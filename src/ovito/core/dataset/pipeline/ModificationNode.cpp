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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModificationNode);
DEFINE_REFERENCE_FIELD(ModificationNode, modifier);
DEFINE_REFERENCE_FIELD(ModificationNode, input);
DEFINE_REFERENCE_FIELD(ModificationNode, modifierGroup);
SET_PROPERTY_FIELD_LABEL(ModificationNode, modifier, "Modifier");
SET_PROPERTY_FIELD_LABEL(ModificationNode, input, "Input");
SET_PROPERTY_FIELD_LABEL(ModificationNode, modifierGroup, "Group");
SET_PROPERTY_FIELD_CHANGE_EVENT(ModificationNode, input, ReferenceEvent::PipelineChanged);
SET_PROPERTY_FIELD_CHANGE_EVENT(ModificationNode, modifierGroup, ReferenceEvent::PipelineChanged);

/******************************************************************************
 * Returns the global class registry, which allows looking up the
 * ModificationNode subclass for a Modifier subclass.
 ******************************************************************************/
ModificationNode::Registry& ModificationNode::registry()
{
    static Registry singleton;
    return singleton;
}

/******************************************************************************
 * Asks this object to delete itself.
 ******************************************************************************/
void ModificationNode::requestObjectDeletion()
{
    // Detach the node from its input, modifier and group.
    OORef<Modifier> modifier = this->modifier();
    setInput(nullptr);
    setModifier(nullptr);
    setModifierGroup(nullptr);

    // Delete modifier too if there are no more pipeline nodes left that reference the same modifier.
    if(!modifier->someNode())
        modifier->requestObjectDeletion();

    PipelineNode::requestObjectDeletion();
}

/******************************************************************************
 * Throws an exception if the pipeline stage cannot be evaluated at this time.
 * This is called by the system to catch user mistakes that would lead to infinite recursion.
 ******************************************************************************/
void ModificationNode::preEvaluationCheck() const
{
    if(modifier())
        modifier()->preEvaluationCheck();
    if(input())
        input()->preEvaluationCheck();
}

/******************************************************************************
 * Is called when a RefTarget referenced by this object generated an event.
 ******************************************************************************/
bool ModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetEnabledOrDisabled) {
        if(source == modifier() || source == modifierGroup()) {
            // Throw away cached results when the modifier is being disabled.
            _completedEngine.reset();

            // If modifier provides animation frames, the animation interval might change when the
            // modifier gets enabled/disabled.
            if(!isBeingLoaded())
                notifyDependents(ReferenceEvent::AnimationFramesChanged);

            if(!modifierAndGroupEnabled()) {
                // Ignore modifier's status if it is currently disabled.
                if(!modifierGroup() || modifierGroup()->isEnabled())
                    setStatus(PipelineStatus(tr("Modifier is currently turned off.")));
                else
                    setStatus(PipelineStatus(tr("Modifier group is currently turned off.")));
                // Also clear pipeline cache in order to reduce memory footprint when modifier is disabled.
                pipelineCache().invalidate(TimeInterval::empty(), true);
            }

            // Manually generate target changed event when modifier group is being enabled/disabled.
            // That's because events from the group are not automatically propagated.
            if(source == modifierGroup())
                notifyTargetChanged();

            // Propagate enabled/disabled notification events from the modifier or the modifier group.
            return true;
        }
        else if(source == input()) {
            // Inform modifier that the input state has changed if the immediately following input stage was disabled.
            // This is necessary, because we don't receive a PreliminaryStateAvailable signal in this case.
            if(modifier())
                modifier()->notifyDependents(ReferenceEvent::PipelineInputChanged);
        }
    }
    else if(event.type() == ReferenceEvent::TitleChanged && source == modifier()) {
        return true;
    }
    else if(event.type() == ReferenceEvent::ObjectStatusChanged && source == modifier()) {
        // Propagate ObjectStatusChanged events from the modifier to update the pipeline editor UI in case
        // the return value of Modifier::getPipelineEditorShortInfo() changes.
        return true;
    }
    else if(event.type() == ReferenceEvent::PipelineChanged && source == input()) {
        // Propagate pipeline changed events and updates to the preliminary state from upstream.
        return true;
    }
    else if(event.type() == ReferenceEvent::AnimationFramesChanged && (source == input() || source == modifier()) && !isBeingLoaded()) {
        // Propagate animation interval events from the modifier or the upstream pipeline.
        return true;
    }
    else if(event.type() == ReferenceEvent::PipelineInputChanged && source == modifier()) {
        // Whenever the modifier's inputs change, invalidate the cached computation results hold on to any
        // cached results needed for preliminary pipeline evaluation.
        if(_completedEngine)
            _completedEngine->setValidityInterval(TimeInterval::empty());
    }
    else if(event.type() == ReferenceEvent::TargetChanged && (source == input() || source == modifier())) {
        if(source == input()) {
            // Whenever the modifier's inputs change, invalidate the cached async computation results but hold on to any
            // cached results needed for preliminary pipeline evaluation.
            if(_completedEngine)
                _completedEngine->setValidityInterval(TimeInterval::empty());
        }
        else if(source == modifier()) {
            // Whenever the modifier changes, invalidate the cached async computation results
            // unless the engine requests otherwise.
            if(_completedEngine && !_completedEngine->modifierChanged(static_cast<const PropertyFieldEvent&>(event))) {
                _completedEngine->setValidityInterval(TimeInterval::empty());
            }
        }

        // Invalidate cached results when the modifier or the upstream pipeline change.
        TimeInterval validityInterval = static_cast<const TargetChangedEvent&>(event).unchangedInterval();

        // Let the modifier reduce the remaining validity interval if the modifier depends on other animation times.
        if(modifier() && source == input())
            modifier()->restrictInputValidityInterval(validityInterval);

        // Propagate change event to upstream pipeline.
        // Note that this will invoke ModificationNode::notifyDependentsImpl(), which
        // takes care of invalidating the pipeline cache.
        notifyTargetChangedOutsideInterval(validityInterval);

        return false;
    }
    else if(event.type() == ReferenceEvent::PreliminaryStateAvailable && source == input()) {
        // Throw away cached results when the modifier's input changes, unless the engine requests otherwise.
        if(_completedEngine && !_completedEngine->pipelineInputChanged()) _completedEngine.reset();

        // Also discard cached output state.
        pipelineCache().invalidateInteractiveState();

        // Inform modifier that the input state has changed.
        if(modifier())
            modifier()->notifyDependents(ReferenceEvent::PipelineInputChanged);
    }
    return PipelineNode::referenceEvent(source, event);
}

/******************************************************************************
 * Gets called when the data object of the node has been replaced.
 ******************************************************************************/
void ModificationNode::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(modifier)) {
        // Discard cached async computation results.
        _completedEngine.reset();

        // Reset all caches when the modifier is replaced.
        pipelineCache().invalidate(TimeInterval::empty(), true);

        // Update the status of the Modifier when it is detached from the ModificationNode.
        if(Modifier* oldMod = static_object_cast<Modifier>(oldTarget)) {
            oldMod->notifyDependents(ReferenceEvent::ObjectStatusChanged);
            oldMod->notifyDependents(ReferenceEvent::PipelineInputChanged);
        }
        if(Modifier* newMod = static_object_cast<Modifier>(newTarget)) {
            newMod->notifyDependents(ReferenceEvent::ObjectStatusChanged);
            newMod->notifyDependents(ReferenceEvent::PipelineInputChanged);
        }
        notifyDependents(ReferenceEvent::TargetEnabledOrDisabled);

        // The animation length might have changed when the modifier has changed.
        if(!isBeingLoaded())
            notifyDependents(ReferenceEvent::AnimationFramesChanged);
    }
    else if(field == PROPERTY_FIELD(input) && !isBeingLoaded() && !isBeingDeleted()) {
        // Reset all caches when the data input is replaced.
        pipelineCache().invalidate(TimeInterval::empty(), true);
        // Update the status of the Modifier when ModificationNode is inserted/removed into pipeline.
        if(modifier())
            modifier()->notifyDependents(ReferenceEvent::PipelineInputChanged);
        // The animation length might have changed when the pipeline has changed.
        notifyDependents(ReferenceEvent::AnimationFramesChanged);
    }
    else if(field == PROPERTY_FIELD(modifierGroup)) {
        // Register/unregister node with modifier group:
        if(oldTarget) static_object_cast<ModifierGroup>(oldTarget)->unregisterNode(this);
        if(newTarget) static_object_cast<ModifierGroup>(newTarget)->registerNode(this);

        if(!isBeingLoaded() && modifier()) {
            // Whenever the modification node is moved in or out of a modifier group,
            // its effective enabled/disabled status may change. Emulate a corresponding notification event in this case.
            ModifierGroup* oldGroup = static_object_cast<ModifierGroup>(oldTarget);
            ModifierGroup* newGroup = static_object_cast<ModifierGroup>(newTarget);
            if((!oldGroup || oldGroup->isEnabled()) != (!newGroup || newGroup->isEnabled())) {
                modifier()->notifyDependents(ReferenceEvent::TargetEnabledOrDisabled);
            }
        }
    }

    PipelineNode::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
 * Sends an event to all dependents of this RefTarget.
 ******************************************************************************/
void ModificationNode::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        // Invalidate cached results when this modification node or the modifier changes.
        pipelineCache().invalidate(static_cast<const TargetChangedEvent&>(event).unchangedInterval());
    }
    else if(event.type() == ReferenceEvent::ObjectStatusChanged) {
        // Notify the modifier group to update its combined status.
        if(modifierGroup())
            modifierGroup()->modificationNodeStatusChanged();
    }
    PipelineNode::notifyDependentsImpl(event);
}

/******************************************************************************
 * Asks the object for the result of the upstream data pipeline.
 ******************************************************************************/
PipelineEvaluationResult ModificationNode::evaluateInput(const PipelineEvaluationRequest& request) const
{
    // Without a data source, this ModificationNode doesn't produce any data.
    if(!input())
        return PipelineFlowState();

    // Request the input data.
    return input()->evaluate(request);
}

/******************************************************************************
 *  Asks the object for the result of the upstream data pipeline at several animation times.
 ******************************************************************************/
Future<std::vector<PipelineFlowState>> ModificationNode::evaluateInputMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times) const
{
    // This function should only be used to request final pipeline results, not preliminary results.
    OVITO_ASSERT(request.interactiveMode() == false);

    // Without a data source, this ModificationNode doesn't produce any data.
    if(!input())
        return std::vector<PipelineFlowState>(times.size(), PipelineFlowState());

    // Request the data from the input node.
    return input()->evaluateMultiple(request, std::move(times));
}

#if 0 // TODO
/******************************************************************************
 * Returns the results of an immediate and preliminary evaluation of the data pipeline.
 ******************************************************************************/
PipelineFlowState ModificationNode::evaluateSynchronous(const PipelineEvaluationRequest& request)
{
    // If modifier or the modifier group are disabled, bypass cache and forward results of upstream pipeline.
    if(input() && !modifierAndGroupEnabled())
        return input()->evaluateSynchronous(request);

    return PipelineNode::evaluateSynchronous(request);
}
#endif

/******************************************************************************
 * Asks the object for the result of the data pipeline.
 ******************************************************************************/
PipelineEvaluationResult ModificationNode::evaluate(const PipelineEvaluationRequest& request)
{
    // If modifier is disabled, bypass cache and forward results from upstream pipeline.
    if(input() && !modifierAndGroupEnabled())
        return input()->evaluate(request);

    // Otherwise, let the base class call our evaluateInternal() method.
    return PipelineNode::evaluate(request);
}

/******************************************************************************
 * Asks the object for the result of the data pipeline.
 ******************************************************************************/
PipelineEvaluationResult ModificationNode::evaluateInternal(const PipelineEvaluationRequest& request)
{
    // Set up an evaluation request to be passed to the upstream pipeline.
    ModifierEvaluationRequest modifierRequest(request, this);

    // Ask the modifier for the list of animation frames it wants the upstream pipeline to maintain in the cach.
    if(modifierAndGroupEnabled())
        modifier()->inputCachingHints(modifierRequest);

    // Obtain input data at the current frame from the upstream pipeline.
    PipelineEvaluationResult input = evaluateInput(modifierRequest);

    // If the modifier is currently disabled, we can skip it and simply forward the unmodified results from the upstream pipeline.
    if(!modifierAndGroupEnabled())
        return input;

    // Make a copy of the (future) upstream pipeline results. We may need them later in case this modifier fails.
    PipelineEvaluationResult output = input;

    // Check if results exist in the form of an async compute engine, which can be reused as is.
    ModifierEnginePtr existingEngine;
    if(completedEngine()) {
        if(completedEngine()->validityInterval().contains(request.time())) {
            existingEngine = completedEngine();
            output.intersectValidityInterval(existingEngine->validityInterval());
        }
        else if(request.interactiveMode()) {
            // If the compute results exist but are out of date, we can still try to reuse them in interactive mode.
            // This requires flagging the pipeline output as preliminary.
            existingEngine = completedEngine();
            output.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Interactive);
            output.intersectValidityInterval(request.time());
        }
    }

    // Let the modifier function determine the validity interval of its computation
    // and whether it can provide preliminary or full pipeline results.
    if(!modifier()->preEvaluationRun(modifierRequest, output, existingEngine)) {
        OVITO_ASSERT_MSG(modifierRequest.interactiveMode(), "ModificationNode::evaluateInternal", "Modifier must participate in non-interactive pipeline evaluation.");
        // The modifier has indicated that it WON'T participate in the current interactive pipeline evaluation, because it cannot run at an interactive rate.
        // We will skip the modifier and return the upstream pipeline results as is - after marking it as a preliminary result.
        output.setEvaluationTypes(PipelineEvaluationResult::EvaluationType::Interactive);
        return output;
    }

//    qDebug() << "Modifier" << objectTitle() << "interactive=" << request.interactiveMode();

    // Pass the input data on to the modifier function.
    output.postprocess(*this, [this, request = std::move(modifierRequest), existingEngine = std::move(existingEngine)](PipelineFlowState state) -> Future<PipelineFlowState> {
        // Clear the status of the input unless it is an error state, which must be retained.
        state.mutableStatus().resetShortInfo();
        if(state.status().type() != PipelineStatus::Error) {
            state.setStatus(PipelineStatus::Success);
        }

        // Sanity check: With the throwOnError option set, the input data must never be in an error state.
        OVITO_ASSERT(!request.throwOnError() || state.status().type() != PipelineStatus::Error);

        // This ModificationNode becomes a no-op if
        //  - it doesn't have a modifier,
        //  - the modifier is disabled,
        //  - the upstream pipeline did not yield any data, or
        //  - the modifier cannot be evaluated in interactive mode (this is handled by the modifier directly).
        if(!modifierAndGroupEnabled() || !state)
            return state;

        // Check if async computation results exist that can be reused as is.
        if(existingEngine) {
            // Inject the computation results of the engine back into the pipeline, applying them to the new input state.
            existingEngine->applyResults(request, state);
            return state;
        }

        Future<PipelineFlowState> future;
        bool interactiveMode = request.interactiveMode();

        // Ask the modifier to create an async compute engine to perform the computation (may not be supported by some modifiers).
        Future<ModifierEnginePtr> engineFuture = modifier()->createEngine(request, state);
        if(engineFuture.isValid()) {

            // Wait for the compute engine to become available.
            future = engineFuture.then(*this, [this, request = std::move(request), state = std::move(state)](ModifierEnginePtr engine) mutable {

                // Launch the engine (or execute the work in the current thread if it is small).
                auto execFuture = engine->preferSynchronousExecution() ? engine->runImmediately(true) : engine->runAsync(true);

                // Wait for the compute engine to complete.
                return execFuture.then(*this, [this, engine = std::move(engine), request = std::move(request), state = std::move(state)]() mutable {

                    // Apply the computed results to the input pipeline state.
                    engine->applyResults(request, state);

                    // Double-check that engine has applied its state validity interval to the pipeline data.
                    OVITO_ASSERT(engine->validityInterval().contains(state.stateValidity().start()) && engine->validityInterval().contains(state.stateValidity().end()));

                    // Store the compute engine (including the results that it holds) in the modification node for later reuse.
                    setCompletedEngine(std::move(engine));

                    return std::move(state);
                });
            });
        }
        else {
            // Modifier does not use async compute engine. Perform the computation using the future-based interface.
            future = modifier()->evaluateModifier(request, state);
        }

        // Register the task with this pipeline stage to indicate in the UI that this stage is currently performing work.
        if(!interactiveMode)
            registerActiveFuture(future);

        return future;
    });

    // Post-process the modifier results before returning them to the caller.
    // Turn any exception thrown during modifier evaluation into a
    // valid pipeline state with an error code (unless throwOnError is set).
    output.postprocess(*this, [this, input = std::move(input), throwOnError = request.throwOnError(), interactiveMode = request.interactiveMode()](SharedFuture<PipelineFlowState> future) mutable {
        OVITO_ASSERT(future.isFinished() && !future.isCanceled());
        OVITO_ASSERT(input.isFinished() && !input.isCanceled());
        try {
            try {
                const PipelineFlowState& state = future.result();

                // Indicate the status of the modifier calculation in the GUI.
                if(!interactiveMode)
                    setStatus(state.status());

                return state;
            }
            catch(const Exception&) {
                throw;  // Pass through regular exceptions.
            }
            catch(const std::bad_alloc&) {
                throw Exception(tr("Not enough memory."));
            }
            catch(const std::exception& ex) {
                OVITO_ASSERT_MSG(false, "ModificationNode::evaluateInternal()", "Caught an unexpected exception type during modifier evaluation.");
                throw Exception(tr("A non-standard exception occurred: %1").arg(QString::fromLatin1(ex.what())));
            }
            catch(...) {
                OVITO_ASSERT_MSG(false, "ModificationNode::evaluateInternal()", "Caught an unknown exception type during modifier evaluation.");
                throw Exception(tr("An unknown exception occurred."));
            }
        }
        catch(Exception& ex) {
            if(throwOnError)
                throw;

            // Indicate the failure of the modifier calculation in the GUI.
            if(!interactiveMode)
                setStatus(ex);

            ex.prependToMessage(tr("Modifier '%1' reported: ").arg(modifier()->objectTitle()));

            // Fall back to the results produced by the upstream pipeline.
            // Note: This should never throw an exception, because the input pipeline results have already been access successfully.
            try {
                PipelineFlowState state = input.result();
                state.setStatus(PipelineStatus(ex, QStringLiteral(" ")));
                return state;
            }
            catch(...) {
                OVITO_ASSERT_MSG(false, "ModificationNode::evaluateInternal()", "Caught an unexpected exception type during modifier fallback.");
                return PipelineFlowState(nullptr, PipelineStatus(PipelineStatus::Error, tr("An unknown exception occurred.")));
            }
        }
    });

    return output;
}

/******************************************************************************
 * Returns the number of animation frames this pipeline object can provide.
 ******************************************************************************/
int ModificationNode::numberOfSourceFrames() const
{
    OVITO_ASSERT(ExecutionContext::current().isValid());

    if(modifierAndGroupEnabled()) {
        OVITO_ASSERT(modifier() != nullptr);
        return modifier()->numberOfOutputFrames(const_cast<ModificationNode*>(this));
    }
    return input() ? input()->numberOfSourceFrames() : PipelineNode::numberOfSourceFrames();
}

/******************************************************************************
 * Given an animation time, computes the source frame to show.
 ******************************************************************************/
int ModificationNode::animationTimeToSourceFrame(AnimationTime time) const
{
    int frame = input() ? input()->animationTimeToSourceFrame(time) : PipelineNode::animationTimeToSourceFrame(time);
    if(modifierAndGroupEnabled())
        frame = modifier()->animationTimeToSourceFrame(time, frame);
    return frame;
}

/******************************************************************************
 * Given a source frame index, returns the animation time at which it is shown.
 ******************************************************************************/
AnimationTime ModificationNode::sourceFrameToAnimationTime(int frame) const
{
    AnimationTime time = input() ? input()->sourceFrameToAnimationTime(frame) : PipelineNode::sourceFrameToAnimationTime(frame);
    if(modifierAndGroupEnabled())
        time = modifier()->sourceFrameToAnimationTime(frame, time);
    return time;
}

/******************************************************************************
 * Returns the human-readable labels associated with the animation frames.
 ******************************************************************************/
QMap<int, QString> ModificationNode::animationFrameLabels() const
{
    QMap<int, QString> labels = input() ? input()->animationFrameLabels() : PipelineNode::animationFrameLabels();
    if(modifierAndGroupEnabled())
        return modifier()->animationFrameLabels(std::move(labels));
    return labels;
}

/******************************************************************************
 * Returns a short piece information (typically a string or color) to be
 * displayed next to the object's title in the pipeline editor.
 ******************************************************************************/
QVariant ModificationNode::getPipelineEditorShortInfo(Scene* scene) const
{
    QVariant info = ActiveObject::getPipelineEditorShortInfo(scene);
    if(!info.isValid() && modifier()) info.setValue(modifier()->getPipelineEditorShortInfo(scene, const_cast<ModificationNode*>(this)));
    return info;
}

/******************************************************************************
 * Traverses the pipeline from this modifier application up to the source and
 * returns the source object that generates the input data for the pipeline.
 ******************************************************************************/
PipelineNode* ModificationNode::pipelineSource() const
{
    PipelineNode* node = input();
    while(node) {
        if(ModificationNode* modApp = dynamic_object_cast<ModificationNode>(node))
            node = modApp->input();
        else
            break;
    }
    return node;
}

/******************************************************************************
 * Returns the modification node that precedes this node in the pipeline.
 * If this node is referenced by more than one pipeline node (=it is preceded by a pipeline branch),
 * then nullptr is returned.
 ******************************************************************************/
ModificationNode* ModificationNode::getPredecessorModNode() const
{
    int pipelineCount = 0;
    ModificationNode* predecessor = nullptr;
    visitDependents([&](RefMaker* dependent) {
        if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(dependent)) {
            if(modNode->input() == this && !modNode->pipelines(true).empty()) {
                pipelineCount++;
                predecessor = modNode;
            }
        }
        else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->head() == this) {
                if(pipeline->isInScene())
                    pipelineCount++;
            }
        }
    });
    return (pipelineCount <= 1) ? predecessor : nullptr;
}

/******************************************************************************
 * Returns the title of this modification node.
 ******************************************************************************/
QString ModificationNode::objectTitle() const
{
    if(modifier())
        return modifier()->objectTitle();  // Inherit title from modifier.
    else
        return PipelineNode::objectTitle();
}

/******************************************************************************
 * Returns whether the modifier AND the modifier group (if this node is part of one) are enabled.
 ******************************************************************************/
bool ModificationNode::modifierAndGroupEnabled() const
{
    return modifier() && modifier()->isEnabled() && (!modifierGroup() || modifierGroup()->isEnabled());
}

/******************************************************************************
 * Decides whether a preliminary viewport update is performed after this pipeline object has been
 * evaluated but before the rest of the pipeline is complete.
 ******************************************************************************/
bool ModificationNode::performPreliminaryUpdateAfterEvaluation()
{
    return PipelineNode::performPreliminaryUpdateAfterEvaluation() &&
           (!modifier() || modifier()->performPreliminaryUpdateAfterEvaluation());
}

}  // namespace Ovito
