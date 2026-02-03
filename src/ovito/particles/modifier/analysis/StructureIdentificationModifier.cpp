////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdmod/modifiers/ColorByTypeModifier.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include "StructureIdentificationModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(StructureIdentificationModifier);
DEFINE_VECTOR_REFERENCE_FIELD(StructureIdentificationModifier, structureTypes);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, colorByType);
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, structureTypes, "Structure types");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, colorByType, "Color particles by structure type");

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool StructureIdentificationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Create an instance of the ParticleType class to represent a structure type.
******************************************************************************/
ElementType* StructureIdentificationModifier::createStructureType(int id, ParticleType::PredefinedStructureType predefType)
{
    DataOORef<ElementType> stype = DataOORef<ElementType>::create();
    stype->initializeType([&]() {
        stype->setNumericId(id);
        stype->setName(ParticleType::getPredefinedStructureTypeName(predefType));
    }, OwnerPropertyRef(&Particles::OOClass(), Particles::StructureTypeProperty));
    addStructureType(stype);
    return stype.get();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void StructureIdentificationModifier::loadFromStream(ObjectLoadStream& stream)
{
    Modifier::loadFromStream(stream);

    // For backward compatibility with OVITO 3.14:
    if(stream.formatVersion() < 30016) {
        stream.expectChunkRange(0, 2);
        stream.closeChunk();
    }
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void StructureIdentificationModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
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
Future<PipelineFlowState> StructureIdentificationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            Particles* particles = state.expectMutableObject<Particles>();
            particles->verifyIntegrity();
            return reuseCachedState(request, particles, std::move(state), cachedState);
        }
        return std::move(state);
    }

    // Phase I: Perform the structure identification. The results are cached in the pipeline node's partial cache.
    auto identificationFuture = request.modificationNode()->partialResultsCache().getOrCompute(state.data(), [&]() {

        // Ask the subclass to create the engine that will perform the structure identification.
        std::shared_ptr<Algorithm> algorithm = createAlgorithm(request, state);

        // Let the algorithm perform the structure identification.
        return algorithm->startAlgorithm(*this, request, state);
    });

    // Phase II: Compute structure statistics.
    return identificationFuture.then(ObjectExecutor(this), [this, state = std::move(state),
                                             createdByNode = request.modificationNodeWeak()](std::shared_ptr<const Algorithm> algorithm) {
        auto modifierParameters = algorithm->getModifierParameters(this);
        // Perform the statistics calculation in a separate thread.
        return asyncLaunch([state = std::move(state), modifierParameters = std::move(modifierParameters),
                                                              algorithm = std::move(algorithm), colorByType = colorByType(),
                                                              createdByNode = std::move(createdByNode)]() mutable {
            // Post-process computed structure classifications.
            PropertyPtr structures = algorithm->postProcessStructureTypes(algorithm->structures(), modifierParameters);
            this_task::throwIfCanceled();

            // Add output property to the particles.
            Particles* particles = state.expectMutableObject<Particles>();
            particles->createProperty(structures);

            // Color particles based on their structural type (if requested).
            if(colorByType) {
                ColorByTypeModifier::colorByType(structures, particles);
                this_task::throwIfCanceled();
            }

            // Compute the structure identification statistics.
            algorithm->computeStructureStatistics(structures, state, createdByNode, modifierParameters);

            return std::move(state);
        });
    });
}

/******************************************************************************
* Algorithm constructor.
******************************************************************************/
StructureIdentificationModifier::Algorithm::Algorithm(const StructureIdentificationModifier& modifier, const PipelineFlowState& input) : _simulationCell(input.getObject<SimulationCell>())
{
    // Get the input particles.
    _particles = input.expectObject<Particles>();
    _particles->verifyIntegrity();

    // Get input particle selection.
    if(modifier.onlySelectedParticles())
        _selection = _particles->expectProperty(Particles::SelectionProperty);

    // Create the output structure property.
    _structures = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, _particles->elementCount(), Particles::StructureTypeProperty);

    // Create deep copies of the structure elements types, because data objects owned by the modifier should
    // not be passed down the data pipeline.
    for(const ElementType* type : modifier.structureTypes()) {
        OVITO_ASSERT(type && type->numericId() == _structures->elementTypes().size());

        // Attach structure types to output particle property.
        _structures->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
    }
}

/******************************************************************************
* Starts the algorithm by launching a worker task.
* The method returns a Future that will hold a shared pointer to the Algorithm instance once the task has been started.
******************************************************************************/
Future<std::shared_ptr<StructureIdentificationModifier::Algorithm>> StructureIdentificationModifier::Algorithm::startAlgorithm(const StructureIdentificationModifier& modifier, const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Perform the structure identification in a separate thread.
    return asyncLaunch([self = shared_from_this()]() mutable
    {
        self->identifyStructures();
        return std::move(self);
    });
}

/******************************************************************************
* Computes the structure identification statistics.
******************************************************************************/
std::vector<int64_t> StructureIdentificationModifier::Algorithm::computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const
{
    // Count the number of particles of each identified type.
    int maxTypeId = 0;
    for(const ElementType* stype : structures->elementTypes()) {
        OVITO_ASSERT(stype->numericId() >= 0);
        maxTypeId = std::max(maxTypeId, stype->numericId());
    }

    std::vector<int64_t> counts(maxTypeId + 1);

#ifdef OVITO_USE_SYCL
    if(!counts.empty() && structures->size() != 0) {
        sycl::buffer<int64_t> countsBuf(counts.data(), counts.size());
        this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            auto countsAcc = countsBuf.get_access(cgh, sycl::write_only);
            SyclBufferAccess<int32_t, access_mode::read> typeAcc(structures, cgh);
            OVITO_SYCL_PARALLEL_FOR(cgh, StructureIdentificationModifier_countTypes)(sycl::range(typeAcc.size()), [=](size_t i) {
                auto t = typeAcc[i];
                if(t >= 0 && t < countsAcc.size())
                    sycl::atomic_ref<int64_t, sycl::memory_order::relaxed, sycl::memory_scope::device>(countsAcc[t]).fetch_add((int64_t)1);
            });
        });
    }
#else
    std::ranges::fill(counts, 0);
    for(auto t : BufferReadAccess<int32_t>(structures)) {
        if(t >= 0 && t <= maxTypeId)
            counts[t]++;
    }
#endif

    // Create the property arrays for the bar chart.
    PropertyPtr typeCounts = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int64, 1, tr("Count"));
    std::ranges::copy(counts, BufferWriteAccess<int64_t, access_mode::discard_write>(typeCounts).begin());
    PropertyPtr typeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int32, 1, tr("Structure type"));
    boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(typeIds).begin(), 0, typeIds->size());

    // Use the structure types as labels for the output bar chart.
    for(const ElementType* type : structures->elementTypes()) {
        if(type->enabled())
            typeIds->addElementType(type);
    }

    // Output a bar chart with the type counts.
    state.createObject<DataTable>(QStringLiteral("structures"), createdByNode, DataTable::BarChart, tr("Structure counts"), std::move(typeCounts), std::move(typeIds));

    return counts;
}

/******************************************************************************
* Adopts existing computation results for an interactive pipeline evaluation.
******************************************************************************/
Future<PipelineFlowState> StructureIdentificationModifier::reuseCachedState(const ModifierEvaluationRequest& request, Particles* particles, PipelineFlowState&& output, const PipelineFlowState& cachedState)
{
    // Adopt the structure count data table from the cached state.
    if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("structures"))) {
        output.addObject(cachedTable);
    }

    // Adopt all global attributes computed by the modifier from the cached state.
    output.adoptAttributesFrom(cachedState, request.modificationNode());

    // Adopt the structure property from the cached state.
    if(DataOORef<const Particles> cachedParticles = cachedState.getObject<Particles>()) {
        const Property* cachedStructures = cachedParticles->getProperty(Particles::StructureTypeProperty);
        const Property* cachedColors = colorByType() ? cachedParticles->getProperty(Particles::ColorProperty) : nullptr;
        return asyncLaunch([output = std::move(output), particles, cachedStructures, cachedColors, cachedParticles = std::move(cachedParticles)]() mutable {
            particles->tryToAdoptProperties(cachedParticles, {cachedStructures, cachedColors}, {particles});
            return std::move(output);
        });
    }

    return std::move(output);
}

}   // End of namespace
