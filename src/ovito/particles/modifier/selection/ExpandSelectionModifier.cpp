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
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include "ExpandSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ExpandSelectionModifier);
OVITO_CLASSINFO(ExpandSelectionModifier, "DisplayName", "Expand selection");
OVITO_CLASSINFO(ExpandSelectionModifier, "Description", "Select particles that are neighbors of already selected particles.");
OVITO_CLASSINFO(ExpandSelectionModifier, "ModifierCategory", "Selection");
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
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ExpandSelectionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void ExpandSelectionModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
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
Future<PipelineFlowState> ExpandSelectionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get the input particles.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    // In interactive mode, do not perform a real computation. Instead, reuse old results if available in the pipeline cache.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                Particles* mutableParticles = state.expectMutableObject<Particles>();
                mutableParticles->tryToAdoptProperties(cachedParticles, {
                    cachedParticles->getProperty(Particles::SelectionProperty),
                }, {mutableParticles});
            }
            // Adopt all global attributes computed by the modifier from the cached state.
            state.adoptAttributesFrom(cachedState, request.modificationNode());
        }
        return std::move(state);
    }

    // Get the particle positions.
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);

    // Get the current particle selection.
    const Property* inputSelection = particles->expectProperty(Particles::SelectionProperty);

    // Get simulation cell (optional).
    const SimulationCell* inputCell = state.getObject<SimulationCell>();

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    std::unique_ptr<ExpandSelectionEngine> engine;
    if(mode() == CutoffRange) {
        engine = std::make_unique<ExpandSelectionCutoffEngine>(request.modificationNode(), posProperty, inputCell, inputSelection, numberOfIterations(), cutoffRange());
    }
    else if(mode() == NearestNeighbors) {
        engine = std::make_unique<ExpandSelectionNearestEngine>(request.modificationNode(), posProperty, inputCell, inputSelection, numberOfIterations(), numNearestNeighbors());
    }
    else if(mode() == BondedNeighbors) {
        particles->expectBonds()->verifyIntegrity();
        engine = std::make_unique<ExpandSelectionBondedEngine>(request.modificationNode(), posProperty, inputCell, inputSelection, numberOfIterations(), particles->expectBondsTopology());
    }
    else {
        throw Exception(tr("Invalid selection expansion mode."));
    }

    // Perform the calculation in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            engine = std::move(engine)]() mutable
    {
        // Compute the expanded selection.
        engine->perform();
        this_task::throwIfCanceled();

        // Get the output particles.
        Particles* particles = state.expectMutableObject<Particles>();

        // Output the selection property.
        particles->createProperty(engine->outputSelection());

        // Report the number of newly selected particles as a pipeline attribute.
        state.addAttribute(QStringLiteral("ExpandSelection.num_added"), QVariant::fromValue(engine->numSelectedParticlesOutput() - engine->numSelectedParticlesInput()), engine->createdByNode());

        state.setStatus(tr("Added %1 particles to selection.\n"
                "Old selection count was: %2\n"
                "New selection count is: %3")
                        .arg(engine->numSelectedParticlesOutput() - engine->numSelectedParticlesInput())
                        .arg(engine->numSelectedParticlesInput())
                        .arg(engine->numSelectedParticlesOutput()));

        return std::move(state);
    });
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionEngine::perform()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Expanding particle selection"));

    setNumSelectedParticlesInput(_inputSelection->nonzeroCount());

    progress.beginSubSteps(_numIterations);
    for(int i = 0; i < _numIterations; i++) {
        if(i != 0) {
            _inputSelection = outputSelection();
            setOutputSelection(_inputSelection.makeCopy());
            progress.nextSubStep();
        }
        expandSelection(progress);
    }
    progress.endSubSteps();

    setNumSelectedParticlesOutput(outputSelection()->nonzeroCount());
}

/******************************************************************************
* Performs one iteration of the selection expansion.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionNearestEngine::expandSelection(TaskProgress& progress)
{
    if(_numNearestNeighbors > MAX_NEAREST_NEIGHBORS)
        throw Exception(tr("Invalid parameter. The expand selection modifier can expand the selection only to the %1 nearest neighbors of particles. This limit is set at compile time.").arg(MAX_NEAREST_NEIGHBORS));

    // Prepare the neighbor list.
    NearestNeighborFinder neighFinder(_numNearestNeighbors, positions(), simCell(), {});

    OVITO_ASSERT(inputSelection() != outputSelection());
    BufferReadAccess<SelectionIntType> inputSelectionArray(inputSelection());
    BufferWriteAccess<SelectionIntType, access_mode::write> outputSelectionArray(outputSelection());
    parallelFor(positions()->size(), 4096, progress, [&](size_t index) {
        if(!inputSelectionArray[index])
            return;

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
void ExpandSelectionModifier::ExpandSelectionBondedEngine::expandSelection(TaskProgress& progress)
{
    BufferWriteAccess<SelectionIntType, access_mode::write> outputSelectionArray(outputSelection());
    BufferReadAccess<SelectionIntType> inputSelectionArray(inputSelection());
    BufferReadAccess<ParticleIndexPair> bondTopologyArray(_bondTopology);

    size_t particleCount = inputSelection()->size();
    parallelFor(_bondTopology->size(), 4096, progress, [&](size_t index) {
        size_t index1 = bondTopologyArray[index][0];
        size_t index2 = bondTopologyArray[index][1];
        if(index1 >= particleCount || index2 >= particleCount)
            return;
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
        // TSAN complains about the following, because more than one thread may concurrently flag the same item as selected.
        // But this DOES NOT represent a race condition, because it doesn't matter which thread sets the selection flag first.
        if(inputSelectionArray[index1])
            outputSelectionArray[index2] = 1;
        if(inputSelectionArray[index2])
            outputSelectionArray[index1] = 1;
#else
        // Workaround for false positive TSAN warning about data race.
    #if defined(__cpp_lib_atomic_ref)
        // The atomic_ref class is not yet available in all C++20 compilers.
        if(inputSelectionArray[index1])
            std::atomic_ref(outputSelectionArray[index2]).store(1, std::memory_order_release);
        if(inputSelectionArray[index2])
            std::atomic_ref(outputSelectionArray[index1]).store(1, std::memory_order_release);
    #else
        // If the atomic_ref class is not yet supported by the compiler (e.g. Apple Clang), use GCC intrinsics instead.
        if(inputSelectionArray[index1])
            __atomic_store_n(&outputSelectionArray[index2], 1, __ATOMIC_RELEASE);
        if(inputSelectionArray[index2])
            __atomic_store_n(&outputSelectionArray[index1], 1, __ATOMIC_RELEASE);
    #endif
#endif
    });
}

/******************************************************************************
* Performs one iteration of the selection expansion.
******************************************************************************/
void ExpandSelectionModifier::ExpandSelectionCutoffEngine::expandSelection(TaskProgress& progress)
{
    // Prepare the neighbor finder.
    CutoffNeighborFinder neighborListBuilder(_cutoffRange, positions(), simCell(), {});

    BufferWriteAccess<SelectionIntType, access_mode::write> outputSelectionArray(outputSelection());
    BufferReadAccess<SelectionIntType> inputSelectionArray(inputSelection());

    parallelFor(positions()->size(), 4096, progress, [&](size_t index) {
        if(!inputSelectionArray[index])
            return;

        for(CutoffNeighborFinder::Query neighQuery(neighborListBuilder, index); !neighQuery.atEnd(); neighQuery.next()) {
            outputSelectionArray[neighQuery.current()] = 1;
        }
    });
}

}   // End of namespace
