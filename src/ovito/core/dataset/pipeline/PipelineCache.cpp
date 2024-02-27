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
#include <ovito/core/dataset/data/TransformingDataVis.h>
#include <ovito/core/dataset/data/TransformedDataObject.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineCache::PipelineCache(RefTarget* owner, bool includeVisElements, bool enableCaching)
    : _ownerObject(owner), _includeVisElements(includeVisElements), _isEnabled(enableCaching)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
PipelineCache::~PipelineCache() // NOLINT
{
}

/******************************************************************************
* Starts a pipeline evaluation or returns a reference to an existing evaluation
* that is currently in progress.
******************************************************************************/
PipelineEvaluationResult PipelineCache::evaluatePipeline(const PipelineEvaluationRequest& request)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(!ownerObject()->isUndoRecording());

    PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());

    // Confirm with the pipeline steps that an evaluation is allowed at this time.
    // This is to prevent infinite recursion in case of user mistakes, e.g. when a Python
    // modifier calls the Pipeline.compute() method while the pipeline is already being computed.
    try {
        // Walks up the pipeline recursively and asks each step if the evaluation is allowed at this time.
        if(pipelineNode)
            pipelineNode->preEvaluationCheck();
        else
            static_object_cast<Pipeline>(ownerObject())->preEvaluationCheck();
    }
    catch(...) {
        return std::current_exception();
    }

    // Bypass cache if it was disabled by its pipeline stage.
    // This may be the case for pipeline stages which can deliver results immediately without caching them.
    if(!isEnabled()) {
        OVITO_ASSERT(pipelineNode); // Cache cannot be disabled for an entire pipeline, only for individual nodes.
        if(pipelineNode)
            return pipelineNode->evaluateInternal(request);
    }

    // Prevent re-entrance into the evaluatePipeline() function.
    OVITO_ASSERT(!_preparingEvaluation);
    if(_preparingEvaluation)
        return Exception(Pipeline::tr("A new pipeline evaluation is not permitted while another pipeline evaluation is already in progress. This error may be the result of an invalid user Python script invoking a function that is not permitted in this context."));

    // Update the animation frames for which we should keep computed pipeline outputs.
    if(!_precomputeAllFrames)
        _requestedIntervals = request.cachingIntervals();
    else
        _requestedIntervals.add(TimeInterval::infinite());

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
                if(future.isValid() && !future.isCanceled()) {
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
    OVITO_ASSERT(pipeline != nullptr || _includeVisElements == false);

    PipelineEvaluationResult evalResult;

    if(!pipelineNode) {
        // Without a pipeline data source, the results will be null (no data collection).
        if(!pipeline->head())
            return PipelineFlowState(nullptr, PipelineStatus::Success);

        if(!_includeVisElements) {
            // When requesting the pipeline output WITHOUT the effect of visualization elements,
            // delegate the evaluation to the head node of the pipeline.
            evalResult = pipeline->head()->evaluate(request);
        }
        else {
            // When requesting the pipeline output WITH the effect of visualization elements,
            // delegate the evaluation to the pipeline's other cache.
            evalResult = pipeline->evaluatePipeline(request);
        }
    }
    else {
        try {
            evalResult = pipelineNode->evaluateInternal(request);
        }
        catch(const Exception& ex) {
            if(request.throwOnError())
                throw;
            pipelineNode->setStatus(ex);

            // Return a failed state as pipeline result.
            return PipelineEvaluationResult(PipelineFlowState(nullptr, pipelineNode->status()), request.interactiveMode() ? PipelineEvaluationResult::EvaluationType::Interactive : PipelineEvaluationResult::EvaluationType::Noninteractive);
        }
    }

    // If the pipeline output should include the effect of visualization elements,
    // let each transforming visualization element alter the data collection.
    if(_includeVisElements) {
        OVITO_ASSERT(!pipelineNode);

#if 0 // TODO
        if(!request.interactiveMode()) {
            future = future.then(*ownerObject(), [this, request](const PipelineFlowState& state) {

                // If requested, turn upstream pipeline errors into hard exceptions, which abort the pipeline evaluation.
                if(request.throwOnError() && state.status().type() == PipelineStatus::Error)
                    throw Exception(state.status().text());

                // Give every visualization element the opportunity to apply an asynchronous data transformation.
                Future<PipelineFlowState> stateFuture;
                if(state) {
                    Pipeline* pipeline = static_object_cast<Pipeline>(ownerObject());
                    // Note: For now, we only process the vis elements of root-level data objects in the data collection.
                    // The implementation may have to be extended in the future to cover also nested data objects and their vis elements.
                    for(const auto& dataObj : state.data()->objects()) {
                        for(DataVis* vis : dataObj->visElements()) {
                            // Let the Pipeline substitude the vis element with another one. Then check if it is a transforming vis element.
                            if(TransformingDataVis* transformingVis = dynamic_object_cast<TransformingDataVis>(pipeline->getReplacementVisElement(vis))) {
                                if(transformingVis->isEnabled()) {
                                    if(!stateFuture.isValid()) {
                                        stateFuture = transformingVis->transformData(request, dataObj, PipelineFlowState(state), _cachedTransformedDataObjects);
                                    }
                                    else {
                                        stateFuture = stateFuture.then(*transformingVis, [request, dataObj, transformingVis, this, pipeline = OORef<Pipeline>(pipeline)](PipelineFlowState&& state) {
                                            // If requested, turn failed vis element transformations into hard exceptions, which abort the pipeline evaluation.
                                            if(request.throwOnError() && state.status().type() == PipelineStatus::Error)
                                                throw Exception(state.status().text());
                                            return transformingVis->transformData(request, dataObj, std::move(state), _cachedTransformedDataObjects);
                                        });
                                    }
                                }
                            }
                        }
                    }
                }
                if(!stateFuture.isValid()) {
                    _cachedTransformedDataObjects.clear();
                    stateFuture = Future<PipelineFlowState>::createImmediate(state);
                }
                else {
                    // Cache the transformed data objects created by transforming visualization elements.
                    stateFuture = stateFuture.then(*ownerObject(), [this, throwOnError = request.throwOnError()](PipelineFlowState&& state) {
                        if(throwOnError && state.status().type() == PipelineStatus::Error)
                            throw Exception(state.status().text());
                        cacheTransformedDataObjects(state);
                        return std::move(state);
                    });
                }
                return stateFuture;
            });
        }
#endif
    }

    // Pre-register the evaluation operation.
    _evaluationsInProgress.push_front({ request.throwOnError(), evalResult.evaluationTypes(), evalResult.validityInterval() });
    auto evaluationInProgress = _evaluationsInProgress.begin();

    // Store the results in this cache after the evaluation completes.
    evalResult.postprocess(*ownerObject(), [this, evaluationInProgress](PipelineFlowState state) {
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
            if(state.stateValidity().contains(ExecutionContext::current().ui().datasetContainer().currentAnimationTime())) {
                if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(ownerObject())) {
                    // Let the pipeline update its list of vis elements.
                    if(!_includeVisElements && isNotPreliminaryResult) {
                        // Only gather vis elements that are present in the pipeline output at the animation time currently shown in the GUI.
                        pipeline->updateVisElementList(state);
                    }
                }
                else {
                    // Adopt the newly computed state as the current interactive cache state.
                    setInteractiveState(state, isNotPreliminaryResult);

                    // Inform downstream pipeline that a new interactive state has been generated.
                    PipelineNode* pipelineNode = static_object_cast<PipelineNode>(ownerObject());
                    if(pipelineNode->performPreliminaryUpdateAfterEvaluation()) {
                        pipelineNode->notifyDependents(ReferenceEvent::PreliminaryStateAvailable);
                    }
                }
            }
        }

        // Return state to the caller.
        return std::move(state);
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
    // Evict existing states from cache that do not overlap with the requested time intervals,
    // or which *do* overlap with the newly computed state and have now become outdated.
    _cachedStates.erase(std::remove_if(_cachedStates.begin(), _cachedStates.end(), [&](const PipelineFlowState& cachedState) {
        if(cachedState.stateValidity().overlap(state.stateValidity()))
            return true;
        return !std::any_of(_requestedIntervals.cbegin(), _requestedIntervals.cend(),
            std::bind(&TimeInterval::overlap, cachedState.stateValidity(), std::placeholders::_1));
    }), _cachedStates.end());

    // Decide whether to store the newly computed state in the cache or not.
    // To keep it, its validity interval must be overlapping with one of the requested time intervals.
    if(std::any_of(_requestedIntervals.cbegin(), _requestedIntervals.cend(),
            std::bind(&TimeInterval::overlap, state.stateValidity(), std::placeholders::_1))) {
        _cachedStates.push_back(state);
    }

    ownerObject()->notifyDependents(ReferenceEvent::PipelineCacheUpdated);
}

/******************************************************************************
* Keeps a copy of the pipeline state for interactive rendering.
******************************************************************************/
void PipelineCache::setInteractiveState(const PipelineFlowState& state, bool isNotPreliminaryResult)
{
    _interactiveState = PipelineFlowState(state.data(), state.status(), TimeInterval::infinite());
    _interactiveStateIsNotPreliminaryResult = isNotPreliminaryResult;
}

/******************************************************************************
* Marks the contents of the cache as outdated and throws away data that is no longer needed.
******************************************************************************/
void PipelineCache::invalidate(TimeInterval keepInterval, bool resetSynchronousCache)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    if(_preparingEvaluation) {
        qWarning() << "Warning: Invalidating the pipeline cache while preparing the evaluation of the pipeline is not allowed. This error may be the result of an invalid user Python script invoking a function that is not permitted in this context.";
        return;
    }

    // Interrupt frame precomputation, which might be in progress.
    _precomputeFramesOperation.reset();
    _allFramesPrecomputed = false;

    // Reduce the validity of ongoing evaluations.
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

    // Reduce the validity interval of the synchronous state cache.
    _interactiveState.intersectStateValidity(keepInterval);
    if(resetSynchronousCache && _interactiveState.stateValidity().isEmpty())
        _interactiveState.reset();

    if(resetSynchronousCache)
        _cachedTransformedDataObjects.clear();
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
    if(interactiveMode && _interactiveState.stateValidity().contains(time))
        return _interactiveState;
    return {};
}

/******************************************************************************
* Populates the internal cache with transformed data objects generated by
* transforming visual elements.
******************************************************************************/
void PipelineCache::cacheTransformedDataObjects(const PipelineFlowState& state)
{
    _cachedTransformedDataObjects.clear();
    if(state.data()) {
        for(const DataObject* o : state.data()->objects()) {
            if(const TransformedDataObject* transformedDataObject = dynamic_object_cast<TransformedDataObject>(o)) {
                _cachedTransformedDataObjects.push_back(transformedDataObject);
            }
        }
    }
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
            invalidate(TimeInterval(ExecutionContext::current().ui().datasetContainer().currentAnimationTime()));
        }
    }
}

/******************************************************************************
* Starts the process of caching the pipeline results for all animation frames.
******************************************************************************/
void PipelineCache::startFramePrecomputation(const PipelineEvaluationRequest& request)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    // Start the animation frame precomputation process if it has been activated.
    if(_precomputeAllFrames && !_precomputeFramesOperation.isValid() && !_allFramesPrecomputed) {
        // Create the async operation object that manages the frame precomputation.
        _precomputeFramesOperation = Promise<>::create<ProgressingTask>(true);

        // Show progress of the operation in the user interface by registering the asynchronous task.
        ExecutionContext::current().ui().taskManager().registerPromise(_precomputeFramesOperation);

        // Determine the number of frames that need to be precomputed.
        PipelineNode* pipelineNode = dynamic_object_cast<PipelineNode>(ownerObject());
        if(!pipelineNode)
            pipelineNode = static_object_cast<Pipeline>(ownerObject())->head();
        if(pipelineNode)
            _precomputeFramesOperation.setProgressMaximum(pipelineNode->numberOfSourceFrames());

        // Automatically reset the async operation object and the current frame precomputation when the
        // task gets canceled by the system.
        _precomputeFramesOperation.finally(*ownerObject(), [this](Task&) noexcept {
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
    OVITO_ASSERT(_precomputeFramesOperation.isValid());
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
        OVITO_ASSERT(!_precomputeFrameFuture.isValid());
        _allFramesPrecomputed = true;
        return;
    }

    // Request the next frame from the input trajectory.
    _precomputeFrameFuture = evaluatePipeline(PipelineEvaluationRequest(nextFrameTime));

    // Wait until input frame is ready.
    _precomputeFrameFuture.finally(*ownerObject(), [this](Task& task) {
        try {
            // If the pipeline evaluation has been canceled for some reason, we interrupt the precomputation process.
            if(ownerObject()->isBeingDeleted() || !_precomputeFramesOperation.isValid() || _precomputeFramesOperation.isFinished() || task.isCanceled()) {
                _precomputeFramesOperation.reset();
                OVITO_ASSERT(!_precomputeFrameFuture.isValid());
                return;
            }
            OVITO_ASSERT(_precomputeFrameFuture.isValid());

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
