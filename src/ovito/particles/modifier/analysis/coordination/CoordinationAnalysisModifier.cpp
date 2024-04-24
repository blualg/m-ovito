////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/SyclFlatSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#ifdef OVITO_USE_SYCL
    #include <ovito/core/utilities/concurrent/SyclParallelFor.h>
    #include <ovito/particles/util/SyclCutoffNeighborFinder.h>
#endif
#include "CoordinationAnalysisModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CoordinationAnalysisModifier);
OVITO_CLASSINFO(CoordinationAnalysisModifier, "ClassNameAlias", "CoordinationNumberModifier");
OVITO_CLASSINFO(CoordinationAnalysisModifier, "Description", "Determine number of neighbors and compute the radial distribution function (RDF).");
OVITO_CLASSINFO(CoordinationAnalysisModifier, "DisplayName", "Coordination analysis");
OVITO_CLASSINFO(CoordinationAnalysisModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(CoordinationAnalysisModifier, cutoff);
DEFINE_PROPERTY_FIELD(CoordinationAnalysisModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(CoordinationAnalysisModifier, computePartialRDF);
DEFINE_PROPERTY_FIELD(CoordinationAnalysisModifier, onlySelected);
SET_PROPERTY_FIELD_LABEL(CoordinationAnalysisModifier, cutoff, "Cutoff radius");
SET_PROPERTY_FIELD_LABEL(CoordinationAnalysisModifier, numberOfBins, "Number of histogram bins");
SET_PROPERTY_FIELD_LABEL(CoordinationAnalysisModifier, computePartialRDF, "Compute partial RDFs");
SET_PROPERTY_FIELD_LABEL(CoordinationAnalysisModifier, onlySelected, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(CoordinationAnalysisModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(CoordinationAnalysisModifier, numberOfBins, IntegerParameterUnit, 4);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
CoordinationAnalysisModifier::CoordinationAnalysisModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _cutoff(3.2),
    _numberOfBins(200),
    _computePartialRDF(false),
    _onlySelected(false)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool CoordinationAnalysisModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void CoordinationAnalysisModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> CoordinationAnalysisModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get input data.
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    // In interactive mode, do not perform a real computation. Instead, reuse old results if available in the pipeline cache.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(DataOORef<const Particles> cachedParticles = cachedState.getObject<Particles>()) {
                if(const Property* cachedCoordination = cachedParticles->getProperty(Particles::CoordinationProperty)) {
                    if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("coordination-rdf"))) {
                        state.addObject(cachedTable);
                    }
                    return asyncLaunch([state = std::move(state), particles, cachedCoordination, cachedParticles = std::move(cachedParticles)]() mutable {
                        particles->tryToAdoptProperties(cachedParticles, {cachedCoordination}, {particles});
                        return std::move(state);
                    });
                }
            }
        }
        return std::move(state);
    }

    // Get selection particle property.
    const Property* selection = onlySelected() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // The number of sampling intervals for the radial distribution function.
    int rdfSampleCount = std::max(numberOfBins(), 4);

    if(cutoff() <= 0)
        throw Exception(tr("Invalid cutoff range value. Cutoff must be positive."));

    // Get particle types if partial RDF calculation has been requested.
    const Property* particleTypes = nullptr;
    boost::container::flat_map<int,QString> uniqueTypes;
    if(computePartialRDF()) {
        particleTypes = particles->getProperty(Particles::TypeProperty);
        if(!particleTypes)
            throw Exception(tr("Calculation of partial RDFs requires the '%1' property, but particles don't have types assigned.").arg(Particles::OOClass().standardPropertyName(Particles::TypeProperty)));

        // Build the set of unique particle type IDs.
        for(const ElementType* pt : particleTypes->elementTypes()) {
#if BOOST_VERSION >= 106200
            uniqueTypes.insert_or_assign(pt->numericId(), pt->name().isEmpty() ? QString::number(pt->numericId()) : pt->name());
#else
            // For backward compatibility with older Boost versions, which do not know insert_or_assign():
            uniqueTypes[pt->numericId()] = pt->name().isEmpty() ? QString::number(pt->numericId()) : pt->name();
#endif
        }
        if(uniqueTypes.empty())
            throw Exception(tr("No particle types have been defined."));
        if(uniqueTypes.size() > 20)
            throw Exception(tr("Calculation of partial RDFs is currently limited to 20 particle types for performance reasons. Your system contains %1 types.").arg(uniqueTypes.size()));
    }

    // Perform the calculation in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            particles,
            particleTypes,
            selection,
            cutoff = cutoff(),
            rdfSampleCount,
            computePartialRDF = computePartialRDF(),
            uniqueTypes = std::move(uniqueTypes),
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        this_task::setProgressText(tr("Coordination analysis"));

        boost::container::flat_set<int> uniqueTypeIds;
        uniqueTypeIds.reserve(uniqueTypes.size());
        for(const auto& t : uniqueTypes)
            uniqueTypeIds.insert(t.first);

        size_t componentCount = computePartialRDF ? (uniqueTypeIds.size() * (uniqueTypeIds.size()+1) / 2) : 1;
        QStringList componentNames;
        if(computePartialRDF) {
            for(auto t1 = uniqueTypes.cbegin(); t1 != uniqueTypes.cend(); ++t1) {
                for(auto t2 = t1; t2 != uniqueTypes.cend(); ++t2) {
                    componentNames.push_back(QStringLiteral("%1-%2").arg(t1->second, t2->second));
                }
            }
        }

        Property* coordinationNumbers = particles->createProperty(DataBuffer::Uninitialized, Particles::CoordinationProperty);
        PropertyPtr rdfY = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, rdfSampleCount, Property::FloatDefault, componentCount, QStringLiteral("g(r)"), 0, std::move(componentNames));

        size_t particleCount = particles->elementCount();
        const size_t typeCount = computePartialRDF ? uniqueTypeIds.size() : 1;
        const size_t binCount = rdfSampleCount;
        const size_t rdfCount = componentCount;
        const FloatType rdfBinSize = cutoff / binCount;

        // Get simulation cell.
        const SimulationCell* simulationCell = state.getObject<SimulationCell>();

#ifdef OVITO_USE_SYCL

        // Prepare the neighbor finder.
        SyclCutoffNeighborFinder neighborFinder;
        neighborFinder.prepare(cutoff, particles->expectProperty(Particles::PositionProperty), simulationCell, selection);

        // Convert set of type IDs into a SYCL-compatible data structure.
        SyclFlatSet uniqueTypeIdsFlat{uniqueTypeIds};

        // Temporary buffer for computing the non-normalized RDF histogram, i.e., counting the number of pairs at each distance.
        DataBufferPtr rdfHistogram = DataBufferPtr::create(DataBuffer::Initialized, binCount, DataBuffer::Int64, rdfCount);

        // Calculate coordination numbers and RDF histogram.
        syclParallelForWithProgress(particleCount, [&](sycl::handler& cgh, auto&& parallel_kernel) {
            SyclBufferAccess<int32_t, access_mode::discard_write> coordinationAcc(coordinationNumbers, cgh);
            SyclBufferAccess<int32_t, access_mode::read> particleTypeAcc(particleTypes, cgh);
            SyclBufferAccess<SelectionIntType, access_mode::read> selectionAcc(selection, cgh);
            SyclBufferAccess<int64_t*, access_mode::read_write> rdfAcc(rdfHistogram, cgh);
            SyclCutoffNeighborFinder::Accessor neighborAcc(neighborFinder, cgh);
            auto uniqueTypeIdsAcc = uniqueTypeIdsFlat.get_access(cgh);
            sycl::local_accessor<int, 2> localHistogram{sycl::range<2>{binCount, rdfCount}, cgh};
            parallel_kernel([=](sycl::nd_item<1> idx, size_t local_problem_size, size_t global_index_offset, auto&& was_canceled) {

                // Parallelized histogram calculation.
                // Phase I: Work-group items cooperate to zero local histogram memory.
                auto grp = idx.get_group();
                for(size_t b = idx.get_local_id(0); b < binCount; b += idx.get_local_range(0)) {
                    for(size_t c = 0; c < rdfCount; c++)
                        localHistogram[b][c] = 0;
                }
                sycl::group_barrier(grp);

                // Phase II: Work-group items each add to the histogram in local memory.
                for(size_t i_local = idx.get_global_id(0); i_local < local_problem_size; i_local += idx.get_global_range(0)) {
                    size_t i = i_local + global_index_offset;

                    if(was_canceled())
                        break;

                    int coordination = 0;

                    // Process only subset of selected particles if a selection has been specified.
                    if(!selectionAcc || selectionAcc[i]) {
                        size_t typeIndex1 = uniqueTypeIdsAcc ? uniqueTypeIdsAcc.index_of(particleTypeAcc[i]) : 0;
                        if(typeIndex1 < typeCount) {
                            neighborAcc.visitNeighbors(i, [&](const SyclCutoffNeighborFinder::Neighbor& neighbor) {
                                coordination++;
                                size_t rdfBin = sycl::min(static_cast<size_t>(neighbor.distance() / rdfBinSize), binCount - 1);

                                // Calculating complete or partial RDF?
                                if(!uniqueTypeIdsAcc) {
                                    sycl::atomic_ref<int, sycl::memory_order::relaxed, sycl::memory_scope::work_group, sycl::access::address_space::local_space>(
                                        localHistogram[rdfBin][0]).fetch_add(1);
                                }
                                else {
                                    size_t typeIndex2 = uniqueTypeIdsAcc.index_of(particleTypeAcc[neighbor.neighborIndex()]);
                                    if(typeIndex2 < typeCount) {
                                        size_t lowerIndex = sycl::min(typeIndex1, typeIndex2);
                                        size_t upperIndex = sycl::max(typeIndex1, typeIndex2);
                                        size_t rdfIndex = (typeCount * lowerIndex) - ((lowerIndex - 1) * lowerIndex) / 2 + upperIndex - lowerIndex;
                                        sycl::atomic_ref<int, sycl::memory_order::relaxed, sycl::memory_scope::work_group, sycl::access::address_space::local_space>(
                                            localHistogram[rdfBin][rdfIndex]).fetch_add(1);
                                    }
                                }
                            });
                        }
                    }

                    // Output coordination number.
                    coordinationAcc[i] = coordination;
                }
                sycl::group_barrier(grp);

                // Phase III: Work-group items cooperate to update histogram in global memory.
                for(size_t b = idx.get_local_id(0); b < binCount; b += idx.get_local_range(0)) {
                    for(size_t c = 0; c < rdfCount; c++) {
                        sycl::atomic_ref<int64_t, sycl::memory_order::relaxed, sycl::memory_scope::device>(
                            rdfAcc[b][c]).fetch_add(static_cast<int64_t>(localHistogram[b][c]));
                    }
                }
            });
        });
#else
        // Prepare the neighbor list.
        CutoffNeighborFinder neighborFinder;
        neighborFinder.prepare(cutoff, particles->expectProperty(Particles::PositionProperty), simulationCell, selection);

        BufferWriteAccess<int32_t, access_mode::discard_write> coordinationData(coordinationNumbers);
        BufferReadAccess<int32_t> particleTypeData(particleTypes);
        BufferReadAccess<SelectionIntType> selectionData(selection);

        // Parallel calculation loop:
        EnumerableThreadSpecific<std::vector<size_t>> threadLocalRDFs;
        parallelForInnerOuter(particleCount, 4096, [&](auto&& iterate) {
            std::vector<size_t>& threadLocalRDF = threadLocalRDFs.create(binCount * rdfCount, 0);
            iterate([&](size_t i) {
                int coordination = 0;
                if(!selectionData || selectionData[i]) {
                    size_t typeIndex1 = computePartialRDF ? uniqueTypeIds.index_of(uniqueTypeIds.find(particleTypeData[i])) : 0;
                    if(typeIndex1 < typeCount) {
                        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next()) {
                            coordination++;
                            if(computePartialRDF) {
                                size_t typeIndex2 = uniqueTypeIds.index_of(uniqueTypeIds.find(particleTypeData[neighQuery.current()]));
                                if(typeIndex2 < typeCount) {
                                    auto [lowerIndex, upperIndex] = std::minmax(typeIndex1, typeIndex2);
                                    size_t rdfIndex = (typeCount * lowerIndex) - ((lowerIndex - 1) * lowerIndex) / 2 + upperIndex - lowerIndex;
                                    OVITO_ASSERT(rdfIndex < rdfCount);
                                    size_t rdfBin = static_cast<size_t>(neighQuery.distance() / rdfBinSize);
                                    threadLocalRDF[rdfIndex + std::min(rdfBin, binCount - 1) * rdfCount]++;
                                }
                            }
                            else {
                                size_t rdfBin = static_cast<size_t>(neighQuery.distance() / rdfBinSize);
                                threadLocalRDF[std::min(rdfBin, binCount - 1)]++;
                            }
                        }
                    }
                }
                coordinationData[i] = coordination;
            });
        });

        // Combine per-thread RDFs into a set of master histograms.
        BufferWriteAccess<FloatType*, access_mode::read_write> rdfData(rdfY);
        threadLocalRDFs.visitEach([&](const std::vector<size_t>& r) {
            OVITO_ASSERT(r.size() == rdfData.size() * rdfData.componentCount());
            auto bin = r.cbegin();
            for(auto iter = rdfData.begin(); iter != rdfData.end(); ++iter)
                *iter += *bin++;
        });

        particleTypeData.reset();
        selectionData.reset();
        coordinationData.reset();
        rdfData.reset();
#endif
        this_task::throwIfCanceled();

        // Helper function that normalizes a RDF histogram.
        auto normalizeRDF = [&](size_t type1Count, size_t type2Count, size_t component = 0, FloatType prefactor = 1) {
            OVITO_ASSERT(simulationCell);
            bool is2D = simulationCell->is2D();
            if(!is2D) {
                prefactor *= FloatType(4.0/3.0) * FLOATTYPE_PI * type1Count / simulationCell->volume3D() * type2Count;
            }
            else {
                prefactor *= FLOATTYPE_PI * type1Count / simulationCell->volume2D() * type2Count;
            }
            if(prefactor == 0.0)
                return;
            OVITO_ASSERT(component < rdfY->componentCount());
#ifdef OVITO_USE_SYCL
            ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                SyclBufferAccess<int64_t*, access_mode::read> histogramAcc(rdfHistogram, cgh);
                SyclBufferAccess<FloatType*, access_mode::discard_write> rdfAcc(rdfY, cgh);
                OVITO_SYCL_PARALLEL_FOR(cgh, normalizeRDF_kernel)(sycl::range(rdfAcc.size()), [=](size_t i) {
                    FloatType r1 = i * rdfBinSize;
                    FloatType r2 = r1 + rdfBinSize;
                    FloatType vol = is2D ? (r2*r2 - r1*r1) : (r2*r2*r2 - r1*r1*r1);
                    rdfAcc[i][component] = histogramAcc[i][component] / (prefactor * vol);
                });
            });
#else
            FloatType r1 = 0;
            BufferWriteAccess<FloatType*, access_mode::read_write> rdfData(rdfY);
            for(FloatType& y : rdfData.componentRange(component)) {
                FloatType r2 = r1 + rdfBinSize;
                FloatType vol = is2D ? (r2*r2 - r1*r1) : (r2*r2*r2 - r1*r1*r1);
                y /= prefactor * vol;
                r1 = r2;
            }
#endif
        };

        if(simulationCell) {
            if(!computePartialRDF) {
                if(selection)
                    particleCount = selection->nonzeroCount();
                normalizeRDF(particleCount, particleCount);
            }
            else {
                // Count number of particles of each type.
                BufferReadAccess<SelectionIntType> selectionAcc(selection);

                std::vector<size_t> particleCounts(typeCount, 0);
                const SelectionIntType* sel = selectionAcc ? selectionAcc.begin() : nullptr;
                for(auto t : BufferReadAccess<int32_t>(particleTypes)) {
                    if(sel && !(*sel++))
                        continue;
                    size_t typeIndex = uniqueTypeIds.index_of(uniqueTypeIds.find(t));
                    if(typeIndex < typeCount)
                        particleCounts[typeIndex]++;
                }
                this_task::throwIfCanceled();

                // Normalize RDFs.
                size_t component = 0;
                for(size_t i = 0; i < particleCounts.size(); i++) {
                    for(size_t j = i; j < particleCounts.size(); j++) {
                        normalizeRDF(particleCounts[i], particleCounts[j], component++, (i == j) ? 1 : 2);
                        this_task::throwIfCanceled();
                    }
                }
            }
        }

        // Output RDF histogram(s).
        DataTable* table = state.createObject<DataTable>(QStringLiteral("coordination-rdf"), createdByNode, DataTable::Line, tr("Radial distribution function"), std::move(rdfY));
        table->setIntervalStart(0);
        table->setIntervalEnd(cutoff);
        table->setAxisLabelX(tr("Pair separation distance"));

        return std::move(state);
    });
}

}   // End of namespace
