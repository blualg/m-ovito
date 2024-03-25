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
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ReferenceConfigurationModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ReferenceConfigurationModifier);
DEFINE_REFERENCE_FIELD(ReferenceConfigurationModifier, referenceConfiguration);
DEFINE_PROPERTY_FIELD(ReferenceConfigurationModifier, affineMapping);
DEFINE_PROPERTY_FIELD(ReferenceConfigurationModifier, useMinimumImageConvention);
DEFINE_PROPERTY_FIELD(ReferenceConfigurationModifier, useReferenceFrameOffset);
DEFINE_PROPERTY_FIELD(ReferenceConfigurationModifier, referenceFrameNumber);
DEFINE_PROPERTY_FIELD(ReferenceConfigurationModifier, referenceFrameOffset);
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, referenceConfiguration, "Reference Configuration");
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, affineMapping, "Affine mapping");
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, useMinimumImageConvention, "Use minimum image convention");
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, useReferenceFrameOffset, "Use reference frame offset");
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, referenceFrameNumber, "Reference frame number");
SET_PROPERTY_FIELD_LABEL(ReferenceConfigurationModifier, referenceFrameOffset, "Reference frame offset");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ReferenceConfigurationModifier, referenceFrameNumber, IntegerParameterUnit, 0);

// This class can be removed in a future version of OVITO:
IMPLEMENT_CREATABLE_OVITO_CLASS(ReferenceConfigurationModifierApplication);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ReferenceConfigurationModifier::ReferenceConfigurationModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _affineMapping(NO_MAPPING),
    _useReferenceFrameOffset(false),
    _referenceFrameNumber(0),
    _referenceFrameOffset(-1),
    _useMinimumImageConvention(true)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ReferenceConfigurationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
bool ReferenceConfigurationModifier::preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        result.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Interactive);
    else
        result.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Noninteractive);

    if(useReferenceFrameOffset()) {
        // Results will only be valid for the duration of the current frame when using a relative offset.
        result.intersectValidityInterval(request.time());
    }

    return true;
}

/******************************************************************************
* Throws an exception if the pipeline stage cannot be evaluated at this time.
* This is called by the system to catch user mistakes that would lead to infinite recursion.
******************************************************************************/
void ReferenceConfigurationModifier::preEvaluationCheck() const
{
    // Walk up the reference config pipeline and ask each step if evaluation is allowed at this time.
    if(referenceConfiguration())
        referenceConfiguration()->preEvaluationCheck();
}

/******************************************************************************
* Asks the modifier for the set of animation time intervals that should be
* cached by the upstream pipeline.
******************************************************************************/
void ReferenceConfigurationModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    // Only need to communicate caching hints when reference configuration is provided by the upstream pipeline.
    if(!referenceConfiguration()) {
        if(useReferenceFrameOffset()) {
            // When using a relative reference configuration, we need to build the corresponding set of shifted time intervals.
            for(const TimeInterval& iv : TimeIntervalUnion(request.cachingIntervals())) {
                int startFrame = request.modificationNode()->animationTimeToSourceFrame(iv.start());
                int endFrame = request.modificationNode()->animationTimeToSourceFrame(iv.end());
                AnimationTime shiftedStartTime = request.modificationNode()->sourceFrameToAnimationTime(startFrame + referenceFrameOffset());
                AnimationTime shiftedEndTime = request.modificationNode()->sourceFrameToAnimationTime(endFrame + referenceFrameOffset());
                request.modifiableCachingIntervals().add(TimeInterval(shiftedStartTime, shiftedEndTime));
            }
        }
        else {
            // When using a static reference configuration, ask the upstream pipeline to cache the corresponding animation frame.
            request.modifiableCachingIntervals().add(request.modificationNode()->sourceFrameToAnimationTime(referenceFrameNumber()));
        }
    }

    Modifier::inputCachingHints(request);
}

/******************************************************************************
* Is called by the ModifierApplication to let the modifier adjust the
* time interval of a TargetChanged event received from the upstream pipeline
* before it is propagated to the downstream pipeline.
******************************************************************************/
void ReferenceConfigurationModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);

    if(!referenceConfiguration()) {
        // If the upstream pipeline changes, all computed output frames of the modifier become invalid.
        iv.setEmpty();
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ReferenceConfigurationModifier::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == referenceConfiguration()) {
        // If the reference configuration state changes in some way, all output frames of the modifier
        // become invalid --over the entire animation time interval.
        notifyTargetChanged();
        return false;
    }
    return Modifier::referenceEvent(source, event);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ReferenceConfigurationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            Particles* particles = state.expectMutableObject<Particles>();
            particles->verifyIntegrity();
            reuseCachedState(request, particles, state, cachedState);
        }
        return std::move(state);
    }

    // What is the reference frame number to use?
    int referenceFrame;
    if(useReferenceFrameOffset()) {
        // Determine the current frame, preferably from the marker attribute stored in the pipeline flow state.
        // If the source frame attribute is not present, fall back to inferring it from the current animation time.
        int currentFrame = state.data() ? state.data()->sourceFrame() : -1;
        if(currentFrame < 0)
            currentFrame = request.modificationNode()->animationTimeToSourceFrame(request.time());

        // Use frame offset relative to current configuration.
        referenceFrame = currentFrame + referenceFrameOffset();
    }
    else {
        // Use a constant, user-specified frame as reference configuration.
        referenceFrame = referenceFrameNumber();
    }

    // Obtain the reference positions of the particles, either from the upstream pipeline or from a user-specified reference data source.
    PipelineEvaluationResult refState;
    if(!referenceConfiguration()) {
        // Set up the pipeline request for obtaining the reference configuration.
        ModifierEvaluationRequest referenceRequest = request;
        referenceRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(referenceFrame));
        inputCachingHints(referenceRequest);

        // Send the request to the upstream pipeline.
        refState = request.modificationNode()->evaluateInput(referenceRequest);
    }
    else {
        if(referenceConfiguration()->numberOfSourceFrames() > 0) {
            if(referenceFrame < 0 || referenceFrame >= referenceConfiguration()->numberOfSourceFrames()) {
                if(referenceFrame > 0)
                    throw Exception(tr("Requested reference frame number %1 is out of range. "
                        "The loaded reference configuration contains only %2 frame(s).").arg(referenceFrame).arg(referenceConfiguration()->numberOfSourceFrames()));
                else
                    throw Exception(tr("Requested reference frame %1 is out of range. Cannot perform calculation at the current animation time.").arg(referenceFrame));
            }

            // Convert frame to animation time.
            AnimationTime referenceTime = referenceConfiguration()->sourceFrameToAnimationTime(referenceFrame);

            // Set up the pipeline request for obtaining the reference configuration.
            PipelineEvaluationRequest referenceRequest(referenceTime);
            referenceRequest.setThrowOnError(request.throwOnError());

            // Send the request to the pipeline branch.
            refState = referenceConfiguration()->evaluate(referenceRequest);
        }
        else {
            // Create an empty state for the reference configuration if it is yet to be specified by the user.
            refState = PipelineFlowState{};
        }
    }

    // Wait for the reference configuration to become available.
    return refState.then(*this, [this, request, state = std::move(state), referenceFrame](const PipelineFlowState& referenceInput) mutable {

        // Make sure the obtained reference configuration is valid and ready to use.
        if(referenceInput.status().type() == PipelineStatus::Error)
            throw Exception(tr("Reference configuration is not available: %1").arg(referenceInput.status().text()));
        if(!referenceInput)
            throw Exception(tr("Reference configuration has not been specified yet or is empty. Please pick a reference simulation file."));

        // Make sure we really got back the requested reference frame.
        if(int sourceFrame = referenceInput.data()->sourceFrame(); sourceFrame != -1 && sourceFrame != referenceFrame) {
            if(referenceFrame > 0)
                throw Exception(tr("Requested reference frame %1 is out of range. Make sure the loaded reference configuration file contains a sufficent number of frames.").arg(referenceFrame));
            else
                throw Exception(tr("Requested reference frame %1 is out of range. Cannot perform calculation at the current animation time.").arg(referenceFrame));
        }

        // Let subclass create the compute engine.
        std::unique_ptr<Engine> engine = createEngine(request, state, referenceInput);

        // Perform the actual calculation in a separate thread.
        return AsynchronousTask<PipelineFlowState>::runAsync([
                state = std::move(state),
                engine = std::move(engine)]() mutable
        {
            // Run the algorithm.
            engine->perform(state);
            return std::move(state);
        }, true);
    });
}

/******************************************************************************
* Constructor.
******************************************************************************/
ReferenceConfigurationModifier::Engine::Engine(
    ConstPropertyPtr positions, const SimulationCell* simCell,
    ConstPropertyPtr refPositions, const SimulationCell* simCellRef,
    ConstPropertyPtr identifiers, ConstPropertyPtr refIdentifiers,
    AffineMappingType affineMapping, bool useMinimumImageConvention) :
    _positions(std::move(positions)),
    _refPositions(std::move(refPositions)),
    _identifiers(std::move(identifiers)),
    _refIdentifiers(std::move(refIdentifiers)),
    _affineMapping(affineMapping),
    _useMinimumImageConvention(useMinimumImageConvention)
{
    // Clone the input simulation cells, because we need to slightly adjust for the computation.
    CloneHelper cloneHelper;
    _simCell = cloneHelper.cloneObject(simCell, false);
    _simCellRef = cloneHelper.cloneObject(simCellRef, false);

    // Automatically disable PBCs in Z direction for 2D systems.
    if(_simCell->is2D()) {
        _simCell->setPbcFlags(_simCell->hasPbc(0), _simCell->hasPbc(1), false);
        // Make sure the matrix is invertible.
        AffineTransformation m = _simCell->matrix();
        m.column(2) = Vector3(0,0,1);
        _simCell->setCellMatrix(m);
        m = _simCellRef->matrix();
        m.column(2) = Vector3(0,0,1);
        _simCellRef->setCellMatrix(m);
    }

    if(affineMapping != NO_MAPPING) {
        if(cell()->volume3D() < FLOATTYPE_EPSILON || refCell()->volume3D() < FLOATTYPE_EPSILON)
            throw Exception(tr("Simulation cell is degenerate in either the deformed or the reference configuration."));
    }

    // PBCs flags of the current configuration always override PBCs flags
    // of the reference config.
    _simCellRef->setPbcFlags(_simCell->pbcFlags());
    _simCellRef->setIs2D(_simCell->is2D());

    // Precompute matrices for transforming points/vector between the two configurations.
    _refToCurTM = cell()->matrix() * refCell()->inverseMatrix();
    _curToRefTM = refCell()->matrix() * cell()->inverseMatrix();
}

/******************************************************************************
* Determines the mapping between particles in the reference configuration and
* the current configuration and vice versa.
******************************************************************************/
void ReferenceConfigurationModifier::Engine::buildParticleMapping(bool requireCompleteCurrentToRefMapping, bool requireCompleteRefToCurrentMapping)
{
    // Build particle-to-particle index maps.
    _currentToRefIndexMap.resize(positions()->size());
    _refToCurrentIndexMap.resize(refPositions()->size());
    if(identifiers() && refIdentifiers()) {
        OVITO_ASSERT(identifiers()->size() == positions()->size());
        OVITO_ASSERT(refIdentifiers()->size() == refPositions()->size());

        // Build map of particle identifiers in reference configuration.
        std::map<IdentifierIntType, size_t> refMap;
        size_t index = 0;
        BufferReadAccess<IdentifierIntType> refIdentifiersArray(refIdentifiers());
        for(auto id : refIdentifiersArray) {
            if(refMap.insert(std::make_pair(id, index)).second == false)
                throw Exception(tr("Particles with duplicate identifiers detected in reference configuration."));
            index++;
        }
        this_task::throwIfCanceled();

        // Check for duplicate identifiers in current configuration
        std::map<IdentifierIntType, size_t> currentMap;
        index = 0;
        BufferReadAccess<IdentifierIntType> identifiersArray(identifiers());
        for(auto id : identifiersArray) {
            if(currentMap.insert(std::make_pair(id, index)).second == false)
                throw Exception(tr("Particles with duplicate identifiers detected in current configuration."));
            index++;
        }
        this_task::throwIfCanceled();

        // Build index maps.
        auto id = identifiersArray.cbegin();
        for(auto& mappedIndex : _currentToRefIndexMap) {
            auto iter = refMap.find(*id);
            if(iter != refMap.end())
                mappedIndex = iter->second;
            else if(requireCompleteCurrentToRefMapping)
                throw Exception(tr("Particle ID %1 does exist in the current configuration but not in the reference configuration.").arg(*id));
            else
                mappedIndex = std::numeric_limits<size_t>::max();
            ++id;
        }
        this_task::throwIfCanceled();

        id = refIdentifiersArray.cbegin();
        for(auto& mappedIndex : _refToCurrentIndexMap) {
            auto iter = currentMap.find(*id);
            if(iter != currentMap.end())
                mappedIndex = iter->second;
            else if(requireCompleteRefToCurrentMapping)
                throw Exception(tr("Particle ID %1 does exist in the reference configuration but not in the current configuration.").arg(*id));
            else
                mappedIndex = std::numeric_limits<size_t>::max();
            ++id;
        }
    }
    else {
        // Deformed and reference configuration must contain the same number of particles.
        if(positions()->size() != refPositions()->size())
            throw Exception(tr("Cannot perform calculation. Numbers of particles in reference configuration and current configuration do not match."));

        // When particle identifiers are not available, assume the storage order of particles in the
        // reference configuration and the current configuration are the same and use trivial 1-to-1 mapping.
        std::iota(_refToCurrentIndexMap.begin(), _refToCurrentIndexMap.end(), size_t(0));
        std::iota(_currentToRefIndexMap.begin(), _currentToRefIndexMap.end(), size_t(0));
    }

    this_task::throwIfCanceled();
}

}   // End of namespace
