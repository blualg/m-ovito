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
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, numNeighbors);
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, mode);
DEFINE_PROPERTY_FIELD(CentroSymmetryModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, numNeighbors, "Number of neighbors");
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, mode, "Mode");
SET_PROPERTY_FIELD_LABEL(CentroSymmetryModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CentroSymmetryModifier, numNeighbors, IntegerParameterUnit, 2, CentroSymmetryModifier::MAX_CSP_NEIGHBORS);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
CentroSymmetryModifier::CentroSymmetryModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _numNeighbors(12),
    _mode(ConventionalMode),
    _onlySelectedParticles(false)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool CentroSymmetryModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the modifier's results.
******************************************************************************/
Future<ModifierEnginePtr> CentroSymmetryModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Get modifier input.
    const Particles* particles = input.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);
    const SimulationCell* simCell = input.expectObject<SimulationCell>();

    if(numNeighbors() < 2)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation is invalid. It must be at least 2."));

    if(numNeighbors() > MAX_CSP_NEIGHBORS)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation is too large. Maximum number of neighbors is %1.").arg(MAX_CSP_NEIGHBORS));

    if(numNeighbors() % 2)
        throw Exception(tr("The number of neighbors to take into account in the centrosymmetry calculation must be a positive and even integer."));

    // Get particle selection.
    const Property* selectionProperty = onlySelectedParticles() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Create an empty data table for the CSP value histogram to be computed.
    DataOORef<DataTable> histogram = DataOORef<DataTable>::create(DataTable::Line, tr("CSP distribution"));
    histogram->setIdentifier(input.generateUniqueIdentifier<DataTable>(QStringLiteral("csp-centrosymmetry")));
    histogram->setCreatedByNode(request.modificationNode());
    histogram->setAxisLabelX(tr("CSP"));

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    return std::make_shared<CentroSymmetryEngine>(request, particles, posProperty, selectionProperty, simCell, numNeighbors(), mode(), std::move(histogram));
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void CentroSymmetryModifier::CentroSymmetryEngine::perform()
{
    setProgressText(tr("Computing centrosymmetry parameters"));

    // Prepare the neighbor list.
    NearestNeighborFinder neighFinder(_nneighbors);
    if(!neighFinder.prepare(positions(), cell(), selection()))
        return;

    // Access output array.
    BufferWriteAccess<FloatType, access_mode::discard_read_write> cspArray(csp());

    // Perform analysis on each particle.
    BufferReadAccess<SelectionIntType> selectionData(selection());
    parallelForWithProgress(positions()->size(), [&](size_t index) {
        if(!selectionData || selectionData[index])
            cspArray[index] = computeCSP(neighFinder, index, _mode);
        else
            cspArray[index] = 0.0;
    });
    if(isCanceled())
        return;

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
    _histogram->setY(std::move(histogramCounts));
    _histogram->setIntervalStart(0);
    _histogram->setIntervalEnd(cspHistogramBinSize * numHistogramBins);

    // Release data that is no longer needed.
    selectionData.reset();
    _positions.reset();
    _selection.reset();
    _simCell.reset();
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

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void CentroSymmetryModifier::CentroSymmetryEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    Particles* particles = state.expectMutableObject<Particles>();
    if(_inputFingerprint.hasChanged(particles))
        throw Exception(tr("Cached modifier results are obsolete, because the number or the storage order of input particles has changed."));

    // Output per-particle CSP values.
    particles->createProperty(csp());

    // Output CSP distribution histogram.
    state.addObjectWithUniqueId<DataTable>(_histogram);
}

}   // End of namespace
