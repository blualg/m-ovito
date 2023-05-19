////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/particles/objects/BondsObject.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "ExpandSelectionModifier.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ExpandSelectionModifier);
DEFINE_PROPERTY_FIELD(ExpandSelectionModifier, mode);
DEFINE_PROPERTY_FIELD(ExpandSelectionModifier, cutoffRange);
DEFINE_PROPERTY_FIELD(ExpandSelectionModifier, numNearestNeighbors);
DEFINE_PROPERTY_FIELD(ExpandSelectionModifier, numberOfIterations);
SET_PROPERTY_FIELD_LABEL(ExpandSelectionModifier, mode, "Mode");
SET_PROPERTY_FIELD_LABEL(ExpandSelectionModifier, cutoffRange, "Cutoff distance");
SET_PROPERTY_FIELD_LABEL(ExpandSelectionModifier, numNearestNeighbors, "N");
SET_PROPERTY_FIELD_LABEL(ExpandSelectionModifier, numberOfIterations, "Number of iterations");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ExpandSelectionModifier, cutoffRange, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(ExpandSelectionModifier, numNearestNeighbors, IntegerParameterUnit, 1, ExpandSelectionModifier::MAX_NEAREST_NEIGHBORS);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ExpandSelectionModifier, numberOfIterations, IntegerParameterUnit, 1);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ExpandSelectionModifier::ExpandSelectionModifier(ObjectInitializationFlags flags) : AsynchronousModifier(flags),
    _mode(CutoffRange),
    _cutoffRange(3.2),
    _numNearestNeighbors(1),
    _numberOfIterations(1)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ExpandSelectionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<ParticlesObject>();
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the
* modifier's results.
******************************************************************************/
Future<AsynchronousModifier::EnginePtr> ExpandSelectionModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Get the input particles.
    const ParticlesObject* particles = input.expectObject<ParticlesObject>();
    particles->verifyIntegrity();

    // Get the particle positions.
    const PropertyObject* posProperty = particles->expectProperty(ParticlesObject::PositionProperty);

    // Get the current particle selection.
    const PropertyObject* inputSelection = particles->expectProperty(ParticlesObject::SelectionProperty);

    // Get simulation cell.
    const SimulationCellObject* inputCell = input.expectObject<SimulationCellObject>();

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    if(mode() == CutoffRange) {
        return std::make_shared<ExpandSelectionCutoffEngine>(request, particles, posProperty, inputCell, inputSelection, numberOfIterations(), cutoffRange());
    }
    else if(mode() == NearestNeighbors) {
        return std::make_shared<ExpandSelectionNearestEngine>(request, particles, posProperty, inputCell, inputSelection, numberOfIterations(), numNearestNeighbors());
    }
    else if(mode() == BondedNeighbors) {
        particles->expectBonds()->verifyIntegrity();
        return std::make_shared<ExpandSelectionBondedEngine>(request, particles, posProperty, inputCell, inputSelection, numberOfIterations(), particles->expectBondsTopology());
    }
    else {
        throw Exception(tr("Invalid selection expansion mode."));
    }
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionEngine::perform()
{
    setProgressText(tr("Expanding particle selection"));

    setNumSelectedParticlesInput(_inputSelection->size() - boost::count(ConstBufferAccess<SelectionIntType>(_inputSelection), 0));

    beginProgressSubSteps(_numIterations);
    for(int i = 0; i < _numIterations; i++) {
        if(i != 0) {
            _inputSelection = outputSelection();
            setOutputSelection(_inputSelection.makeCopy());
            nextProgressSubStep();
        }
        expandSelection();
        if(isCanceled()) return;
    }
    endProgressSubSteps();

    setNumSelectedParticlesOutput(outputSelection()->size() - boost::count(ConstBufferAccess<SelectionIntType>(outputSelection()), 0));

    // Release data that is no longer needed.
    _positions.reset();
    _inputSelection.reset();
    _simCell.reset();
}

/******************************************************************************
* Performs one iteration of the selection expansion.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionNearestEngine::expandSelection()
{
    if(_numNearestNeighbors > MAX_NEAREST_NEIGHBORS)
        throw Exception(tr("Invalid parameter. The expand selection modifier can expand the selection only to the %1 nearest neighbors of particles. This limit is set at compile time.").arg(MAX_NEAREST_NEIGHBORS));

    // Prepare the neighbor list.
    NearestNeighborFinder neighFinder(_numNearestNeighbors);
    if(!neighFinder.prepare(positions(), simCell(), {}))
        return;

    OVITO_ASSERT(inputSelection() != outputSelection());
    ConstBufferAccess<SelectionIntType> inputSelectionArray(inputSelection());
    BufferAccess<SelectionIntType> outputSelectionArray(outputSelection());
    parallelForWithProgress(positions()->size(), [&](size_t index) {
        if(!inputSelectionArray[index]) return;

        NearestNeighborFinder::Query<MAX_NEAREST_NEIGHBORS> neighQuery(neighFinder);
        neighQuery.findNeighbors(index);
        OVITO_ASSERT(neighQuery.results().size() <= _numNearestNeighbors);

        for(auto n = neighQuery.results().begin(); n != neighQuery.results().end(); ++n) {
            outputSelectionArray[n->index] = 1;
        }
    });
}

/******************************************************************************
* Performs one iteration of the selection expansion.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionBondedEngine::expandSelection()
{
    BufferAccess<SelectionIntType> outputSelectionArray(outputSelection());
    ConstBufferAccess<SelectionIntType> inputSelectionArray(inputSelection());
    ConstBufferAccess<ParticleIndexPair> bondTopologyArray(_bondTopology);

    size_t particleCount = inputSelection()->size();
    parallelForWithProgress(_bondTopology->size(), [&](size_t index) {
        size_t index1 = bondTopologyArray[index][0];
        size_t index2 = bondTopologyArray[index][1];
        if(index1 >= particleCount || index2 >= particleCount)
            return;
        if(inputSelectionArray[index1])
            outputSelectionArray[index2] = 1;
        if(inputSelectionArray[index2])
            outputSelectionArray[index1] = 1;
    });
}

/******************************************************************************
* Performs one iteration of the selection expansion.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionCutoffEngine::expandSelection()
{
    // Prepare the neighbor list.
    CutoffNeighborFinder neighborListBuilder;
    if(!neighborListBuilder.prepare(_cutoffRange, positions(), simCell(), {}))
        return;

    BufferAccess<SelectionIntType> outputSelectionArray(outputSelection());
    ConstBufferAccess<SelectionIntType> inputSelectionArray(inputSelection());

    parallelForWithProgress(positions()->size(), [&](size_t index) {
        if(!inputSelectionArray[index]) return;
        for(CutoffNeighborFinder::Query neighQuery(neighborListBuilder, index); !neighQuery.atEnd(); neighQuery.next()) {
            outputSelectionArray[neighQuery.current()] = 1;
        }
    });
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    // Get the output particles.
    ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
    if(_inputFingerprint.hasChanged(particles))
        throw Exception(tr("Cached modifier results are obsolete, because the number or the storage order of input particles has changed."));

    // Output the selection property.
    particles->createProperty(outputSelection());

    QString msg = tr("Added %1 particles to selection.\n"
            "Old selection count was: %2\n"
            "New selection count is: %3")
                    .arg(numSelectedParticlesOutput() - numSelectedParticlesInput())
                    .arg(numSelectedParticlesInput())
                    .arg(numSelectedParticlesOutput());
    state.setStatus(PipelineStatus(PipelineStatus::Success, std::move(msg)));
}

}   // End of namespace
