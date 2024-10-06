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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/PipelineCache.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineCache::PipelineCache(RefTarget* owner) : _ownerObject(owner)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
PipelineCache::~PipelineCache() // NOLINT
{
}

/******************************************************************************
* Queries the pipeline for the time validity and result type of an evaluation.
******************************************************************************/
void PipelineCache::preevaluatePipeline(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::get()->userInterface());

    PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());

    // Bypass cache if it was disabled by its pipeline stage.
    // This may be the case for pipeline stages which can deliver results immediately.
    if(!isEnabled()) {
        OVITO_ASSERT(pipelineNode); // Cache cannot be disabled for an entire pipeline, only for individual nodes.
        if(pipelineNode)
            pipelineNode->preevaluateInternal(request, evaluationTypes, validityInterval);
        return;
    }

    // Check if we can serve the request immediately by returning one of the cached states.
    for(const PipelineFlowState& state : _cachedStates) {
        if(state.stateValidity().contains(request.time())) {
            validityInterval.intersect(state.stateValidity());
            return;
        }
    }

    // Check if cache contains a valid state for interactive rendering.
    if(request.interactiveMode() && _interactiveState.stateValidity().contains(request.time())) {
        validityInterval.intersect(_interactiveState.stateValidity());
        evaluationTypes = _interactiveStateIsNotPreliminaryResult // Tagging this output state as preliminary if necessary.
                ? PipelineEvaluationResult::EvaluationType::Both
                : PipelineEvaluationResult::EvaluationType::Interactive;
        return;
    }

    if(!pipelineNode) {
        // Delegate request to the pipeline's final output node.
        pipelineNode = static_object_cast<Pipeline>(ownerObject())->head();

        // Without a pipeline data source, the results will be null.
        if(!pipelineNode)
            return;

        pipelineNode->preevaluate(request, evaluationTypes, validityInterval);
    }
    else {
        pipelineNode->preevaluateInternal(request, evaluationTypes, validityInterval);
    }
}

/******************************************************************************
* Starts a pipeline evaluation or returns a reference to an existing evaluation
* that is currently in progress.
******************************************************************************/
PipelineEvaluationResult PipelineCache::evaluatePipeline(const PipelineEvaluationRequest& request)
{
    OVITO_ASSERT(this_task::isMainThread());
    OVITO_ASSERT(this_task::get());

    PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());

    // Confirm with the pipeline steps that an evaluation is allowed at this time.
    // This is to prevent infinite recursion in case of user mistakes, e.g. when a Python
    // modifier calls the Pipeline.compute() method while the pipeline is already being computed.
    try {
        // Walks up the pipeline recursively and asks each step if the evaluation is allowed at this time.
        if(pipelineNode)
            pipelineNode->preEvaluationCheck(request);
        else
            static_object_cast<Pipeline>(ownerObject())->preEvaluationCheck(request);
    }
    catch(...) {
        return std::current_exception();
    }

    // Bypass cache if it was disabled by its pipeline stage.
    // This may be the case for pipeline stages which can deliver results immediately.
    if(!isEnabled()) {
        OVITO_ASSERT(pipelineNode); // Cache cannot be disabled for an entire pipeline, only for individual nodes.
        if(pipelineNode) {
            // Prepare the pipeline evaluation.
            PipelineEvaluationResult::EvaluationTypes evaluationTypes = PipelineEvaluationResult::EvaluationType::Both;
            TimeInterval validityInterval = TimeInterval::infinite();
            pipelineNode->preevaluateInternal(request, evaluationTypes, validityInterval);

            // Request results from the pipeline stage.
            return PipelineEvaluationResult(pipelineNode->evaluateInternal(request), evaluationTypes, validityInterval);
        }
    }

    // Prevent re-entrance into the evaluatePipeline() function.
    OVITO_ASSERT(!_preparingEvaluation);
    if(_preparingEvaluation)
        return Exception(Pipeline::tr("A new pipeline evaluation is not permitted while another pipeline evaluation is already in progress. This error may be the result of an invalid user Python script invoking a function that is not permitted in this context."));

    // Check if we can serve the request immediately by returning one of the cached states.
    for(const PipelineFlowState& state : _cachedStates) {
        if(state.stateValidity().contains(request.time())) {
            startFramePrecomputation(request);
            if(pipelineNode) {
                // Give pipeline node the opportunity to postprocess the cached state before it is returned to the caller.
                return pipelineNode->postprocessCachedState(request, state);
            }
            else {
                return state;
            }
        }
    }

    // Check if cache contains a valid state for interactive rendering.
    if(request.interactiveMode() && _interactiveState.stateValidity().contains(request.time())) {
        return PipelineEvaluationResult(_interactiveState,
            _interactiveStateIsNotPreliminaryResult // Tagging this output state as preliminary if necessary.
                ? PipelineEvaluationResult::EvaluationType::Both
                : PipelineEvaluationResult::EvaluationType::Interactive);
    }

    // Check if there already is an evaluation in progress that is compatible with the new request.
    for(const EvaluationInProgress& eval : _evaluationsInProgress) {
        if(eval.validityInterval.contains(request.time()) && eval.throwOnError == request.throwOnError()) {
            if(eval.evaluationTypes.testFlag(request.interactiveMode() ? PipelineEvaluationResult::EvaluationType::Interactive : PipelineEvaluationResult::EvaluationType::Noninteractive)) {
                SharedFuture<PipelineFlowState> future = eval.future.lock();
                if(future && !future.isCanceled()) {
                    startFramePrecomputation(request);
                    return PipelineEvaluationResult(std::move(future), eval.evaluationTypes, eval.validityInterval);
                }
            }
        }
    }

    // Remove terminated pipeline evaluations from the list of active evaluations.
    _evaluationsInProgress.remove_if([](const EvaluationInProgress& eval) {
        return eval.future.expired();
    });

    // To detect unexpected calls to invalidate() and reentrant function calls.
    _preparingEvaluation = true;
    try {
        PipelineEvaluationResult result = evaluatePipelineImpl(request);

        // From now on, it is okay again to call invalidate().
        _preparingEvaluation = false;

        // Start the process of caching the pipeline results for all animation frames.
        startFramePrecomputation(request);

        return result;
    }
    catch(...) {
        _preparingEvaluation = false;
        throw;
    }
}

/******************************************************************************
* Starts a pipeline evaluation.
******************************************************************************/
PipelineEvaluationResult PipelineCache::evaluatePipelineImpl(const PipelineEvaluationRequest& request)
{
    PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());
    Pipeline* pipeline = !pipelineNode ? static_object_cast<Pipeline>(ownerObject()) : nullptr;
    OVITO_ASSERT(pipeline != nullptr || pipelineNode != nullptr);

    UndoSuspender noUndo;
    PipelineEvaluationResult evalResult;

    if(!pipelineNode) {
        // Delegate request to the pipeline's final output node.
        pipelineNode = pipeline->head();

        // Without a pipeline data source, the results will be null (no data collection).
        if(!pipelineNode)
            return PipelineFlowState(nullptr, PipelineStatus::Success);

        evalResult = pipelineNode->evaluate(request);
    }
    else {
        try {
            // Prepare the pipeline evaluation.
            PipelineEvaluationResult::EvaluationTypes evaluationTypes = PipelineEvaluationResult::EvaluationType::Both;
            TimeInterval validityInterval = TimeInterval::infinite();
            pipelineNode->preevaluateInternal(request, evaluationTypes, validityInterval);

            // Request results from the pipeline stage.
            evalResult = PipelineEvaluationResult(pipelineNode->evaluateInternal(request), evaluationTypes, validityInterval);
        }
        catch(const Exception& ex) {
            if(request.throwOnError())
                throw;
            pipelineNode->setStatus(ex);

            // Return a failed state as pipeline result.
            return PipelineEvaluationResult(PipelineFlowState(nullptr, pipelineNode->status()), request.interactiveMode() ? PipelineEvaluationResult::EvaluationType::Interactive : PipelineEvaluationResult::EvaluationType::Noninteractive);
        }
    }

    // Pre-register the evaluation operation.
    _evaluationsInProgress.push_front({ request.throwOnError(), evalResult.evaluationTypes(), evalResult.validityInterval(), request.cachingIntervals() });
    auto evaluationInProgress = _evaluationsInProgress.begin();

    // Store the results in this cache after the evaluation completes.
    evalResult.postprocess(ObjectExecutor(ownerObject()), [this, evaluationInProgress](PipelineFlowState state) {
        OVITO_ASSERT(!ownerObject()->isUndoRecording());

        // If requested, turn upstream pipeline errors into hard exceptions, which abort the pipeline evaluation.
        if(evaluationInProgress->throwOnError && state.status().type() == PipelineStatus::Error)
            throw Exception(state.status().text());

        // Compute the effective validity of the produced state.
        // It is the interaction of the original interval hint and the actual state validity.
        state.intersectStateValidity(evaluationInProgress->validityInterval);

        if(!state.stateValidity().isEmpty()) {

            // Is the produced output a complete or a preliminary result?
            bool isNotPreliminaryResult = evaluationInProgress->evaluationTypes.testFlag(PipelineEvaluationResult::EvaluationType::Noninteractive);

            // Don't cache preliminary results produced by the upstream pipeline.
            if(isNotPreliminaryResult) {
                // Furthermore, let the cache decide whether the state should be retained or not.
                insertState(state);
            }

            // Post-evaluation work for Pipeline and PipelineNode objects.
            if(state.stateValidity().contains(this_task::ui()->datasetContainer().currentAnimationTime())) {

                // Adopt the newly computed state as the current interactive cache state.
                _interactiveState = state;
                _interactiveStateIsNotPreliminaryResult = isNotPreliminaryResult;

                if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(ownerObject())) {
                    // Let the pipeline update its list of vis elements.
                    if(isNotPreliminaryResult) {
                        // Only gather vis elements that are present in the pipeline output at the animation time currently shown in the GUI.
                        pipeline->updateVisElementList(state);
                    }
                }
                else {
                    // Inform downstream pipeline that a new new state has been generated.
                    PipelineNode* pipelineNode = static_object_cast<PipelineNode>(ownerObject());
                    if(isNotPreliminaryResult && pipelineNode->shouldRefreshViewportsAfterEvaluation()) {
                        pipelineNode->notifyDependents(ReferenceEvent::InteractiveStateAvailable);
                    }
                }
            }
        }

        // Return state to the caller.
        return state;
    });

    // Keep a weak reference to the future associated with this pipeline evaluation.
    evaluationInProgress->future = evalResult;

    return evalResult;
}

/******************************************************************************
* Inserts (or may reject) a pipeline state into the cache.
******************************************************************************/
void PipelineCache::insertState(const PipelineFlowState& state)
{
    // Helper function deciding whether a pipeline state should be inserted into or retained in the cache.
    auto shouldKeepInCache = [&](const PipelineFlowState& state) {
        if(_precomputeAllFrames)
            return true;
        return std::any_of(_evaluationsInProgress.begin(), _evaluationsInProgress.end(), [&](const EvaluationInProgress& eval) {
            return eval.requestedIntervals.overlap(state.stateValidity());
        });
    };

    // Evict existing states from cache that do not overlap with the requested time intervals,
    // or which *do* overlap with the newly computed state and have now become outdated.
    erase_if(_cachedStates, [&](const PipelineFlowState& cachedState) {
        if(cachedState.stateValidity().overlap(state.stateValidity()))
            return true;
        return !shouldKeepInCache(cachedState);
    });

    // Decide whether to store the newly computed state in the cache or not.
    // To keep it, its validity interval must be overlapping with one of the requested time intervals.
    if(shouldKeepInCache(state)) {
        _cachedStates.push_back(state);
    }

    ownerObject()->notifyDependents(ReferenceEvent::PipelineCacheUpdated);
}

/******************************************************************************
* Invalidates the cached results from an interactive pipeline evaluation.
******************************************************************************/
void PipelineCache::invalidateInteractiveState()
{
    OVITO_ASSERT(this_task::isMainThread());

    _interactiveState.setStateValidity(TimeInterval::empty());

    // Invalidate in-flight interactive evaluations.
    for(EvaluationInProgress& evaluation : _evaluationsInProgress) {
        if(evaluation.evaluationTypes == PipelineEvaluationResult::EvaluationType::Interactive)
            evaluation.validityInterval = TimeInterval::empty();
    }
}

/******************************************************************************
* Marks the contents of the cache as outdated and throws away data that is no longer needed.
******************************************************************************/
void PipelineCache::invalidate(TimeInterval keepInterval)
{
    OVITO_ASSERT(this_task::isMainThread());

    if(_preparingEvaluation) {
        qWarning() << "Warning: Invalidating the pipeline cache while preparing the evaluation of the pipeline is not allowed. This error may be the result of an invalid user Python script invoking a function that is not permitted in this context.";
        return;
    }

    if(keepInterval.isInfinite())
        return;

    // Interrupt frame precomputation, which might be in progress.
    _precomputeFramesOperation.reset();
    _allFramesPrecomputed = false;

    // Reduce the validity of in-flight evaluations.
    for(EvaluationInProgress& evaluation : _evaluationsInProgress) {
        evaluation.validityInterval.intersect(keepInterval);
    }

    // Reduce the validity of the cached states. Throw away states that became completely invalid.
    for(PipelineFlowState& state : _cachedStates) {
        state.intersectStateValidity(keepInterval);
        if(state.stateValidity().isEmpty()) {
            state.reset();
        }
    }

    // Reduce the validity interval of the interactive state cache.
    _interactiveState.intersectStateValidity(keepInterval);
}

/******************************************************************************
* Throws away all precomputed data to reduce memory footprint.
******************************************************************************/
void PipelineCache::reset()
{
    OVITO_ASSERT(this_task::isMainThread());

    if(_preparingEvaluation) {
        qWarning() << "Warning: Resetting the pipeline cache while preparing the evaluation of the pipeline is not allowed. This error may be the result of an invalid user Python script invoking a function that is not permitted in this context.";
        OVITO_ASSERT(false);
        return;
    }

    // Interrupt frame precomputation, which might be in progress.
    _precomputeFramesOperation.reset();
    _allFramesPrecomputed = false;

    // Reduce the validity of in-flight evaluations.
    for(EvaluationInProgress& evaluation : _evaluationsInProgress) {
        evaluation.validityInterval = TimeInterval::empty();
    }

    // Throw away cached states.
    _cachedStates.clear();

    // Throw away interactive state cache.
    _interactiveState.reset();

    // Notify PropertiesEditor about a change in the pipeline stage's output.
    ownerObject()->notifyDependents(ReferenceEvent::PipelineCacheUpdated);
}

/******************************************************************************
* Special method used by the FileSource class to replace the contents of the pipeline
* cache with a data collection modified by the user.
******************************************************************************/
void PipelineCache::overrideCache(const DataCollection* dataCollection, const TimeInterval& keepInterval)
{
    OVITO_ASSERT(dataCollection != nullptr);
    OVITO_ASSERT(!keepInterval.isEmpty());

    // Interrupt frame precomputation, which might be in progress.
    _precomputeFramesOperation.reset();
    _allFramesPrecomputed = false;

    // Reduce the validity of the cached states to the current animation time.
    // Throw away states that became completely invalid.
    // Replace the contents of the cache with the given data collection.
    for(PipelineFlowState& state : _cachedStates) {
        state.intersectStateValidity(keepInterval);
        if(state.stateValidity().isEmpty()) {
            state.reset();
        }
        else {
            state.setData(dataCollection);
        }
    }

    _interactiveState.setData(dataCollection);
    _interactiveStateIsNotPreliminaryResult = false;
}

/******************************************************************************
* Looks up the pipeline state for the given animation time.
******************************************************************************/
PipelineFlowState PipelineCache::getAt(AnimationTime time, bool interactiveMode) const
{
    for(const PipelineFlowState& state : _cachedStates) {
        if(state.stateValidity().contains(time))
            return state;
    }
    if(interactiveMode) {
        // Note: Returning this state even if the requested time is not contained in its validity interval.
        // In interactive mode we always return "something" - even if it is not the most accurate state.
        return _interactiveState;
    }
    return {};
}

/******************************************************************************
* Enables or disables the precomputation and caching of all frames of the animation.
******************************************************************************/
void PipelineCache::setPrecomputeAllFrames(bool enable)
{
    if(enable != _precomputeAllFrames) {
        _precomputeAllFrames = enable;
        if(!_precomputeAllFrames) {
            // Interrupt the precomputation process if it is currently in progress.
            _precomputeFramesOperation.reset();

            // Throw away all precomputed data (except frame currently shown in the GUI) to reduce memory footprint.
            invalidate(TimeInterval(this_task::ui()->datasetContainer().currentAnimationTime()));
        }
    }
}

/******************************************************************************
* Starts the process of caching the pipeline results for all animation frames.
******************************************************************************/
void PipelineCache::startFramePrecomputation(const PipelineEvaluationRequest& request)
{
    OVITO_ASSERT(this_task::get());

    // Start the animation frame precomputation process if it has been activated.
    if(_precomputeAllFrames && !_precomputeFramesOperation && !_allFramesPrecomputed) {
        // Create the async operation object that manages the frame precomputation.
        _precomputeFramesOperation = Promise<void>::create();
        if(this_task::isInteractive())
            _precomputeFramesOperation.task()->setIsInteractive();
        _precomputeFramesOperation.task()->setUserInterface(this_task::ui());

        // Determine the number of frames that need to be precomputed.
        PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());
        if(!pipelineNode)
            pipelineNode = static_object_cast<Pipeline>(ownerObject())->head();
        if(pipelineNode)
            _precomputeFramesOperation.setProgressMaximum(pipelineNode->numberOfSourceFrames());

        // Automatically reset the async operation object and the current frame precomputation when the
        // task gets canceled by the system.
        _precomputeFramesOperation.task()->finally(ObjectExecutor(ownerObject()), [this]() noexcept {
            _precomputeFrameFuture.reset();
            _precomputeFramesOperation.reset();
        });

        // Compute the first frame of the trajectory.
        precomputeNextAnimationFrame();
    }
}

/******************************************************************************
* Requests the next frame from the pipeline that needs to be precomputed.
******************************************************************************/
void PipelineCache::precomputeNextAnimationFrame()
{
    OVITO_ASSERT(_precomputeFramesOperation);
    OVITO_ASSERT(!_precomputeFramesOperation.isCanceled());

    // Determine the total number of animation frames.
    PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());
    if(!pipelineNode)
        pipelineNode = static_object_cast<Pipeline>(ownerObject())->head();
    int numSourceFrames = pipelineNode ? pipelineNode->numberOfSourceFrames() : 0;

    // Determine what is the next animation frame that needs to be precomputed.
    int nextFrame = 0;
    AnimationTime nextFrameTime;
    while(nextFrame < numSourceFrames) {
        nextFrameTime = pipelineNode->sourceFrameToAnimationTime(nextFrame);
        const PipelineFlowState& state = getAt(nextFrameTime, false);
        if(!state) break;
        do {
            nextFrameTime = pipelineNode->sourceFrameToAnimationTime(++nextFrame);
        }
        while(state.stateValidity().contains(nextFrameTime) && nextFrame < numSourceFrames);
    }
    _precomputeFramesOperation.setProgressValue(nextFrame);
    _precomputeFramesOperation.setProgressText(Pipeline::tr("Caching trajectory (%1 frames remaining)").arg(numSourceFrames - nextFrame));
    if(nextFrame >= numSourceFrames) {
        // Precomputation of trajectory frames is complete.
        _precomputeFramesOperation.setFinished();
        OVITO_ASSERT(!_precomputeFrameFuture);
        _allFramesPrecomputed = true;
        return;
    }

    // Request the next frame from the input trajectory.
    _precomputeFrameFuture = evaluatePipeline(PipelineEvaluationRequest(nextFrameTime));

    // Wait until input frame is ready.
    _precomputeFrameFuture.finally(ObjectExecutor(ownerObject()), [this](Task& task) noexcept {
        try {
            // If the pipeline evaluation has been canceled for some reason, we interrupt the precomputation process.
            if(ownerObject()->isBeingDeleted() || !_precomputeFramesOperation || _precomputeFramesOperation.isFinished() || task.isCanceled()) {
                _precomputeFramesOperation.reset();
                OVITO_ASSERT(!_precomputeFrameFuture);
                return;
            }
            OVITO_ASSERT(_precomputeFrameFuture);

            // Schedule the pipeline evaluation at the next frame.
            precomputeNextAnimationFrame();
        }
        catch(const Exception&) {
            // In case of an error during pipeline evaluation or the unwrapping calculation,
            // abort the operation.
            _precomputeFramesOperation.setFinished();
        }
    });
}

}   // End of namespace
