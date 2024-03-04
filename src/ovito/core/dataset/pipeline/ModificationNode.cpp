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
                pipelineCache().reset();
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
            // This is necessary, because we don't receive an InteractiveStateAvailable signal in this case.
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
        // Propagate pipeline changed events from upstream.
        return true;
    }
    else if(event.type() == ReferenceEvent::AnimationFramesChanged && (source == input() || source == modifier()) && !isBeingLoaded()) {
        // Propagate animation interval events from the modifier or the upstream pipeline.
        return true;
    }
    else if(event.type() == ReferenceEvent::TargetChanged && (source == input() || source == modifier())) {

        // Invalidate cached results when the modifier or the upstream pipeline change.
        TimeInterval validityInterval = static_cast<const TargetChangedEvent&>(event).unchangedInterval();

        // Let the modifier further reduce the remaining validity interval, e.g., if the modifier depends on other animation times.
        if(modifier() && source == input())
            modifier()->restrictInputValidityInterval(validityInterval);

        // Propagate change event to upstream pipeline.
        // Note that this will invoke ModificationNode::notifyDependentsImpl(), which
        // takes care of invalidating the pipeline cache.
        notifyTargetChangedOutsideInterval(validityInterval);

        // Refresh interactive viewports if requested by the modifier.
        if(source == modifier() && modifier()->shouldRefreshViewportsAfterChange()) {
            notifyDependents(ReferenceEvent::InteractiveStateAvailable);
        }

        return false;
    }
    else if(event.type() == ReferenceEvent::InteractiveStateAvailable && source == input()) {

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

        // Reset all caches when the modifier is replaced.
        pipelineCache().reset();

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
        // Reset cache when the upstream pipeline is being replaced.
        pipelineCache().reset();
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
        // Invalidate cached results when this modification node itself or its associated modifier change.
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

    // Let the modifier function determine the validity interval of its computation
    // and whether it can provide preliminary or full pipeline results.
    if(!modifier()->preEvaluationRun(modifierRequest, output)) {
        // The modifier has indicated that it WON'T participate in the current interactive pipeline evaluation, because it cannot run at an interactive rate.
        // We will skip the modifier and return the upstream pipeline results as is - after marking it as a preliminary result.
        OVITO_ASSERT_MSG(modifierRequest.interactiveMode(), "ModificationNode::evaluateInternal", "All modifiers must participate in a non-interactive pipeline evaluation.");
        OVITO_ASSERT(output.evaluationTypes() == PipelineEvaluationResult::EvaluationType::Interactive);
        return output;
    }

    // Pass the input data on to the modifier function.
    output.postprocess(*this, [this, request = std::move(modifierRequest)](PipelineFlowState state) -> Future<PipelineFlowState> {
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
            return std::move(state);

        // Let the modifier compute its results.
        Future<PipelineFlowState> future = modifier()->evaluateModifier(request, std::move(state));
        OVITO_ASSERT(future.isValid());

        // Register the task with this pipeline stage to indicate in the UI that this stage is currently performing work.
        if(!request.interactiveMode())
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
    if(!info.isValid() && modifier())
        info.setValue(modifier()->getPipelineEditorShortInfo(scene, const_cast<ModificationNode*>(this)));
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
bool ModificationNode::shouldRefreshViewportsAfterEvaluation()
{
    return modifier() && modifier()->shouldRefreshViewportsAfterEvaluation();
}

}  // namespace Ovito
