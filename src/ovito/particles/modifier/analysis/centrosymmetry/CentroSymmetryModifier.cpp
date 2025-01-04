////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "CentroSymmetryModifier.h"
#include <mwm_csp/mwm_csp.h>


namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CentroSymmetryModifier);
OVITO_CLASSINFO(CentroSymmetryModifier, "DisplayName", "Centrosymmetry parameter");
OVITO_CLASSINFO(CentroSymmetryModifier, "Description", "Calculate the lattice centrosymmetry parameter for each particle.");
OVITO_CLASSINFO(CentroSymmetryModifier, "ModifierCategory", "Structure identification");
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, numNeighbors);
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, mode);
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, numNeighbors, "Number of neighbors");
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, mode, "Mode");
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CentroSymmetryModifier, numNeighbors, IntegerParameterUnit, 2, CentroSymmetryModifier::MAX_CSP_NEIGHBORS);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool CentroSymmetryModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void CentroSymmetryModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
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
Future<PipelineFlowState> CentroSymmetryModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(numNeighbors() < 2)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation is invalid. It must be at least 2."));

    if(numNeighbors() > MAX_CSP_NEIGHBORS)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation is too large. Maximum number of neighbors is %1.").arg(MAX_CSP_NEIGHBORS));

    if(numNeighbors() % 2)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation must be a positive and even integer."));

    // Get input data.
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    // In interactive mode, do not perform a real computation. Instead, reuse old results if available in the pipeline cache.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                particles->tryToAdoptProperties(cachedParticles, {
                    cachedParticles->getProperty(Particles::CentroSymmetryProperty)
                }, {particles});
            }
            if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("csp-centrosymmetry"))) {
                state.addObject(cachedTable);
            }
        }
        return std::move(state);
    }

    // Get selection particle property.
    const Property* selection = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Perform the calculation in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            particles,
            selection,
            mode = mode(),
            nneighbors = numNeighbors(),
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Computing centrosymmetry parameters"));

        // Prepare the neighbor list.
        NearestNeighborFinder neighFinder(nneighbors);
        neighFinder.prepare(particles->expectProperty(Particles::PositionProperty), state.getObject<SimulationCell>(), selection);

        // Create output array.
        Property* cspProperty = particles->createProperty(DataBuffer::Uninitialized, Particles::CentroSymmetryProperty);
        BufferWriteAccess<FloatType, access_mode::discard_read_write> cspArray(cspProperty);

        // Perform analysis on each particle.
        BufferReadAccess<SelectionIntType> selectionData(selection);
        parallelFor(particles->elementCount(), 4096, progress, [&](size_t index) {
            if(!selectionData || selectionData[index])
                cspArray[index] = computeCSP(neighFinder, index, mode);
            else
                cspArray[index] = 0.0;
        });

        // Determine histogram bin size based on maximum CSP value.
        const size_t numHistogramBins = 100;
        FloatType cspHistogramBinSize = (cspArray.size() != 0) ? (FloatType(1.01) * *boost::max_element(cspArray) / numHistogramBins) : 0;
        if(cspHistogramBinSize <= 0) cspHistogramBinSize = 1;

        // Perform binning of CSP values.
        PropertyPtr histogramCounts = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, numHistogramBins, Property::Int64, 1, tr("Count"));
        BufferWriteAccess<int64_t, access_mode::read_write> histogramAccess(histogramCounts);
        const auto* sel = selectionData ? selectionData.begin() : nullptr;
        for(const FloatType cspValue : cspArray) {
            OVITO_ASSERT(cspValue >= 0);
            if(!sel || *sel++) {
                int binIndex = cspValue / cspHistogramBinSize;
                if(binIndex < numHistogramBins)
                    histogramAccess[binIndex]++;
            }
        }
        histogramAccess.reset();

        // Create an empty data table for the CSP value histogram to be computed.
        DataTable* histogram = state.createObject<DataTable>(QStringLiteral("csp-centrosymmetry"), createdByNode, DataTable::Line, tr("CSP distribution"), std::move(histogramCounts));
        histogram->setAxisLabelX(tr("CSP"));
        histogram->setIntervalStart(0);
        histogram->setIntervalEnd(cspHistogramBinSize * numHistogramBins);

        return std::move(state);
    });
}

/******************************************************************************
* Computes the centrosymmetry parameter of a single particle.
******************************************************************************/
FloatType CentroSymmetryModifier::computeCSP(NearestNeighborFinder& neighFinder, size_t particleIndex, CSPMode mode)
{
    // Find k nearest neighbor of current atom.
    NearestNeighborFinder::Query<MAX_CSP_NEIGHBORS> neighQuery(neighFinder);
    neighQuery.findNeighbors(particleIndex);

    int numNN = neighQuery.results().size();

    FloatType csp = 0;
    if(mode == CentroSymmetryModifier::ConventionalMode) {
        // R = Ri + Rj for each of npairs i,j pairs among numNN neighbors.
        FloatType pairs[MAX_CSP_NEIGHBORS*MAX_CSP_NEIGHBORS/2];
        FloatType* p = pairs;
        for(auto ij = neighQuery.results().begin(); ij != neighQuery.results().end(); ++ij) {
            for(auto ik = ij + 1; ik != neighQuery.results().end(); ++ik) {
                *p++ = (ik->delta + ij->delta).squaredLength();
            }
        }

        // Find NN/2 smallest pair distances from the list.
        std::partial_sort(pairs, pairs + (numNN/2), p);

        // Centrosymmetry = sum of numNN/2 smallest squared values.
        csp = std::accumulate(pairs, pairs + (numNN/2), FloatType(0), std::plus<FloatType>());
    }
    else {
        // Make sure our own neighbor count limit is consistent with the limit defined in the mwm-csp module.
        OVITO_STATIC_ASSERT(MAX_CSP_NEIGHBORS <= MWM_CSP_MAX_POINTS);

        double P[MAX_CSP_NEIGHBORS][3];
        for(size_t i = 0; i < numNN; i++) {
            auto v = neighQuery.results()[i].delta;
            P[i][0] = (double)v.x();
            P[i][1] = (double)v.y();
            P[i][2] = (double)v.z();
        }

        csp = (FloatType)calculate_mwm_csp(numNN, P);
    }
    OVITO_ASSERT(std::isfinite(csp));

    return csp;
}

}   // End of namespace
