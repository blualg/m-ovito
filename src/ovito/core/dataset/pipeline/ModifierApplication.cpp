////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ModifierApplication);
DEFINE_REFERENCE_FIELD(ModifierApplication, modifier);
DEFINE_REFERENCE_FIELD(ModifierApplication, input);
DEFINE_REFERENCE_FIELD(ModifierApplication, modifierGroup);
SET_PROPERTY_FIELD_LABEL(ModifierApplication, modifier, "Modifier");
SET_PROPERTY_FIELD_LABEL(ModifierApplication, input, "Input");
SET_PROPERTY_FIELD_LABEL(ModifierApplication, modifierGroup, "Group");
SET_PROPERTY_FIELD_CHANGE_EVENT(ModifierApplication, input, ReferenceEvent::PipelineChanged);
SET_PROPERTY_FIELD_CHANGE_EVENT(ModifierApplication, modifierGroup, ReferenceEvent::PipelineChanged);

/******************************************************************************
* Returns the global class registry, which allows looking up the
* ModifierApplication subclass for a Modifier subclass.
******************************************************************************/
ModifierApplication::Registry& ModifierApplication::registry()
{
	static Registry singleton;
	return singleton;
}

/******************************************************************************
* Asks this object to delete itself.
******************************************************************************/
void ModifierApplication::deleteReferenceObject() 
{
	// Detach the modifier application from its input, modifier and group.
	OORef<Modifier> modifier = this->modifier();
	setInput(nullptr);
	setModifier(nullptr);
	setModifierGroup(nullptr);

	// Delete modifier if there are no more modifier applications left.
	if(modifier->modifierApplications().empty())
		modifier->deleteReferenceObject();

	CachingPipelineObject::deleteReferenceObject();
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval ModifierApplication::validityInterval(const PipelineEvaluationRequest& request) const
{
	TimeInterval iv = CachingPipelineObject::validityInterval(request);

	// Take into account the validity interval of the input state.
	if(input())
		iv.intersect(input()->validityInterval(request));

	// Let the modifier determine the local validity interval.
	if(modifierAndGroupEnabled()) 
		iv.intersect(modifier()->validityInterval(ModifierEvaluationRequest(request, this)));

	return iv;
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool ModifierApplication::referenceEvent(RefTarget* source, const ReferenceEvent& event)
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
	else if(event.type() == ReferenceEvent::TargetChanged && (source == input() || source == modifier())) {
		// Invalidate cached results when the modifier or the upstream pipeline change.
		TimeInterval validityInterval = static_cast<const TargetChangedEvent&>(event).unchangedInterval();

		// Let the modifier reduce the remaining validity interval if the modifier depends on other animation times.
		if(modifier() && source == input())
			modifier()->restrictInputValidityInterval(validityInterval);

		// Propagate change event to upstream pipeline.
		// Note that this will invoke ModifierApplication::notifyDependentsImpl(), which
		// takes care of invalidating the pipeline cache.
		notifyTargetChangedOutsideInterval(validityInterval);

		// Trigger a preliminary viewport update if desired by the modifier.
		if(source == modifier() && modifier()->performPreliminaryUpdateAfterChange()) {
			notifyDependents(ReferenceEvent::PreliminaryStateAvailable);
		}

		return false;
	}
	else if(event.type() == ReferenceEvent::PreliminaryStateAvailable && source == input()) {
		pipelineCache().invalidateSynchronousState();
		// Inform modifier that the input state has changed.
		if(modifier())
			modifier()->notifyDependents(ReferenceEvent::PipelineInputChanged);
	}
#ifdef OVITO_QML_GUI
	else if(event.type() == ReferenceEvent::PipelineInputChanged && source == modifier()) {
		// Inform the QML GUI that the modifier's input has changed.
		Q_EMIT modifierInputChanged();
	}
	else if(event.type() == ReferenceEvent::PipelineCacheUpdated && source == input()) {
		// Inform the QML GUI that the modifier's input has changed.
		Q_EMIT modifierInputChanged();
	}
#endif
	return CachingPipelineObject::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data object of the node has been replaced.
******************************************************************************/
void ModifierApplication::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(modifier)) {
		// Reset all caches when the modifier is replaced.
		pipelineCache().invalidate(TimeInterval::empty(), true);

		// Update the status of the Modifier when it is detached from the ModifierApplication.
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
	else if(field == PROPERTY_FIELD(input) && !isBeingLoaded() && !isAboutToBeDeleted()) {
		// Reset all caches when the data input is replaced.
		pipelineCache().invalidate(TimeInterval::empty(), true);
		// Update the status of the Modifier when ModifierApplication is inserted/removed into pipeline.
		if(modifier())
			modifier()->notifyDependents(ReferenceEvent::PipelineInputChanged);
		// The animation length might have changed when the pipeline has changed.
		notifyDependents(ReferenceEvent::AnimationFramesChanged);
	}
	else if(field == PROPERTY_FIELD(modifierGroup)) {
		// Register/unregister modapp with modifier group:
		if(oldTarget) static_object_cast<ModifierGroup>(oldTarget)->unregisterModApp(this);
		if(newTarget) static_object_cast<ModifierGroup>(newTarget)->registerModApp(this);

		if(!isBeingLoaded() && modifier()) {
			// Whenever the modifier application is moved in or out of a modifier group, 
			// its effective enabled/disabled status may change. Emulate a corresponding notification event in this case.
			ModifierGroup* oldGroup = static_object_cast<ModifierGroup>(oldTarget);
			ModifierGroup* newGroup = static_object_cast<ModifierGroup>(newTarget);
			if((!oldGroup || oldGroup->isEnabled()) != (!newGroup || newGroup->isEnabled())) {
				modifier()->notifyDependents(ReferenceEvent::TargetEnabledOrDisabled);
			}
		}
	}

	CachingPipelineObject::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Sends an event to all dependents of this RefTarget.
******************************************************************************/
void ModifierApplication::notifyDependentsImpl(const ReferenceEvent& event)
{
	if(event.type() == ReferenceEvent::TargetChanged) {
		// Invalidate cached results when this modifier application or the modifier changes.
		pipelineCache().invalidate(static_cast<const TargetChangedEvent&>(event).unchangedInterval());
	}
#ifdef OVITO_QML_GUI
	else if(event.type() == ReferenceEvent::PipelineCacheUpdated) {
		// Inform the QML GUI that the modifier's results are available.
		Q_EMIT modifierResultsComplete();
	}
#endif
	CachingPipelineObject::notifyDependentsImpl(event);
}

/******************************************************************************
* Asks the object for the result of the upstream data pipeline.
******************************************************************************/
SharedFuture<PipelineFlowState> ModifierApplication::evaluateInput(const PipelineEvaluationRequest& request) const
{
	// Without a data source, this ModifierApplication doesn't produce any data.
	if(!input())
		return PipelineFlowState();

	// Request the input data.
	return input()->evaluate(request);
}

/******************************************************************************
*  Asks the object for the result of the upstream data pipeline at several animation times.
******************************************************************************/
Future<std::vector<PipelineFlowState>> ModifierApplication::evaluateInputMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times) const
{
	// Without a data source, this ModifierApplication doesn't produce any data.
	if(!input())
		return std::vector<PipelineFlowState>(times.size(), PipelineFlowState());

	// Request the input data.
	return input()->evaluateMultiple(request, std::move(times));
}

/******************************************************************************
* Returns the results of an immediate and preliminary evaluation of the data pipeline.
******************************************************************************/
PipelineFlowState ModifierApplication::evaluateSynchronous(const PipelineEvaluationRequest& request)
{
	// If modifier or the modifier group are disabled, bypass cache and forward results of upstream pipeline.
	if(input() && !modifierAndGroupEnabled())
		return input()->evaluateSynchronous(request);

	return CachingPipelineObject::evaluateSynchronous(request);
}

/******************************************************************************
* Asks the object for the result of the data pipeline.
******************************************************************************/
SharedFuture<PipelineFlowState> ModifierApplication::evaluate(const PipelineEvaluationRequest& request)
{
	// If modifier is disabled, bypass cache and forward results of upstream pipeline.
	if(input() && !modifierAndGroupEnabled())
		return input()->evaluate(request);
	
	// Otherwise, let the base class call our evaluateInternal() method.
	return CachingPipelineObject::evaluate(request);
}

/******************************************************************************
* Asks the object for the result of the data pipeline.
******************************************************************************/
Future<PipelineFlowState> ModifierApplication::evaluateInternal(const PipelineEvaluationRequest& request)
{
	// Set up the evaluation request for the upstream pipeline.
	ModifierEvaluationRequest modifierRequest(request, this);

	// Ask the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
	if(modifierAndGroupEnabled())
		modifier()->inputCachingHints(modifierRequest.modifiableCachingIntervals(), this);

	// Obtain input data and pass it on to the modifier.
	return evaluateInput(modifierRequest)
		.then(executor(), [this, modifierRequest](PipelineFlowState inputData) -> Future<PipelineFlowState> {

			// Clear the status of the input unless it is an error.
			if(inputData.status().type() != PipelineStatus::Error) {
				inputData.setStatus(PipelineStatus());
			}
			else if(modifierRequest.breakOnError()) {
				// Skip all following modifiers once an error has occured along the pipeline.
				return inputData;
			}

			// Without a modifier, this ModifierApplication becomes a no-op.
			// The same is true when the Modifier is disabled or if the input data is invalid.
			if(!modifierAndGroupEnabled() || !inputData)
				return inputData;

			Future<PipelineFlowState> future;
			try {
				// Let the modifier do its job.
				future = modifier()->evaluate(modifierRequest, inputData);
				// Register the task with this pipeline stage.
				registerActiveFuture(future);
			}
			catch(...) {
				future = Future<PipelineFlowState>::createFailed(std::current_exception());
			}

			// Post-process the modifier results before returning them to the caller.
			// Turn any exception that was thrown during modifier evaluation into a
			// valid pipeline state with an error code.
			return future.then(executor(), [this, inputData = std::move(inputData)](Future<PipelineFlowState> future) mutable {
				OVITO_ASSERT(future.isFinished() && !future.isCanceled());
				try {
					try {
						PipelineFlowState state = future.result();
						if(inputData.status().type() != PipelineStatus::Error || state.status().type() == PipelineStatus::Success)
							setStatus(state.status());
						else
							setStatus(PipelineStatus());
						return state;
					}
					catch(const Exception&) {
						throw;
					}
					catch(const std::bad_alloc&) {
						throw Exception(tr("Not enough memory."));
					}
					catch(const std::exception& ex) {
						qWarning() << "WARNING: Modifier" << modifier() << "has thrown a non-standard exception:" << ex.what();
						OVITO_ASSERT(false);
						throw Exception(tr("Exception: %1").arg(QString::fromLatin1(ex.what())));
					}
				}
				catch(Exception& ex) {
					setStatus(PipelineStatus(PipelineStatus::Error, ex.messages().join(QChar('\n'))));
					ex.prependGeneralMessage(tr("Modifier '%1' reported:").arg(modifier()->objectTitle()));
					inputData.setStatus(PipelineStatus(PipelineStatus::Error, ex.messages().join(QChar(' '))));
					return std::move(inputData);
				}
				catch(...) {
					OVITO_ASSERT_MSG(false, "ModifierApplication::evaluate()", "Caught an unexpected exception type during modifier evaluation.");
					PipelineStatus status(PipelineStatus::Error, tr("Unknown exception caught during evaluation of modifier '%1'.").arg(modifier()->objectTitle()));
					setStatus(status);
					inputData.setStatus(status);
					return std::move(inputData);
				}
			});
		});
}

/******************************************************************************
* Lets the pipeline stage compute a preliminary result in a synchronous fashion.
******************************************************************************/
PipelineFlowState ModifierApplication::evaluateInternalSynchronous(const PipelineEvaluationRequest& request)
{
	OVITO_ASSERT(!isUndoRecording());

	PipelineFlowState state;
	
	if(input()) {
		// First get the preliminary results from the upstream pipeline.
		state = input()->evaluateSynchronous(request);
		try {
			if(!state)
				throw Exception(tr("Modifier input is empty."));

			// Apply modifier:
			if(modifierAndGroupEnabled())
				modifier()->evaluateSynchronous(ModifierEvaluationRequest(request, this), state);
		}
		catch(const Exception& ex) {
			// Turn exceptions thrown during modifier evaluation into an error pipeline state.
			state.setStatus(PipelineStatus(PipelineStatus::Error, ex.messages().join(QStringLiteral(": "))));
		}
		catch(const std::bad_alloc&) {
			// Turn exceptions thrown during modifier evaluation into an error pipeline state.
			state.setStatus(PipelineStatus(PipelineStatus::Error, tr("Not enough memory.")));
		}
		catch(const std::exception& ex) {
			qWarning() << "WARNING: Modifier" << modifier() << "has thrown a non-standard exception:" << ex.what();
			OVITO_ASSERT(false);
			state.setStatus(PipelineStatus(PipelineStatus::Error, tr("Exception: %1").arg(QString::fromLatin1(ex.what()))));
		}
		catch(...) {
			OVITO_ASSERT_MSG(false, "ModifierApplication::evaluateSynchronous()", "Caught an unexpected exception type during preliminary modifier evaluation.");
			// Turn exceptions thrown during modifier evaluation into an error pipeline state.
			state.setStatus(PipelineStatus(PipelineStatus::Error, tr("Unknown exception caught during evaluation of modifier '%1'.").arg(modifier()->objectTitle())));
		}
	}

	return state;
}

/******************************************************************************
* Returns the number of animation frames this pipeline object can provide.
******************************************************************************/
int ModifierApplication::numberOfSourceFrames() const
{
	if(modifierAndGroupEnabled()) {
		OVITO_ASSERT(modifier() != nullptr);
		return modifier()->numberOfOutputFrames(const_cast<ModifierApplication*>(this));
	}
	return input() ? input()->numberOfSourceFrames() : CachingPipelineObject::numberOfSourceFrames();
}

/******************************************************************************
* Given an animation time, computes the source frame to show.
******************************************************************************/
int ModifierApplication::animationTimeToSourceFrame(AnimationTime time) const
{
	int frame = input() ? input()->animationTimeToSourceFrame(time) : CachingPipelineObject::animationTimeToSourceFrame(time);
	if(modifierAndGroupEnabled())
		frame = modifier()->animationTimeToSourceFrame(time, frame);
	return frame;
}

/******************************************************************************
* Given a source frame index, returns the animation time at which it is shown.
******************************************************************************/
AnimationTime ModifierApplication::sourceFrameToAnimationTime(int frame) const
{
	AnimationTime time = input() ? input()->sourceFrameToAnimationTime(frame) : CachingPipelineObject::sourceFrameToAnimationTime(frame);
	if(modifierAndGroupEnabled())
		time = modifier()->sourceFrameToAnimationTime(frame, time);
	return time;
}

/******************************************************************************
* Returns the human-readable labels associated with the animation frames.
******************************************************************************/
QMap<int, QString> ModifierApplication::animationFrameLabels() const
{
	QMap<int, QString> labels = input() ? input()->animationFrameLabels() : CachingPipelineObject::animationFrameLabels();
	if(modifierAndGroupEnabled())
		return modifier()->animationFrameLabels(std::move(labels));
	return labels;
}

/******************************************************************************
* Returns a short piece information (typically a string or color) to be 
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant ModifierApplication::getPipelineEditorShortInfo(Scene* scene) const 
{
	QVariant info = ActiveObject::getPipelineEditorShortInfo(scene);
	if(!info.isValid() && modifier())
		info.setValue(modifier()->getPipelineEditorShortInfo(scene, const_cast<ModifierApplication*>(this)));
	return info;
}

/******************************************************************************
* Traverses the pipeline from this modifier application up to the source and
* returns the source object that generates the input data for the pipeline.
******************************************************************************/
PipelineObject* ModifierApplication::pipelineSource() const
{
	PipelineObject* obj = input();
	while(obj) {
		if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(obj))
			obj = modApp->input();
		else
			break;
	}
	return obj;
}

/******************************************************************************
* Returns the modifier application that precedes this modifier application in the pipeline.
* If this modifier application is referenced by more than one pipeline object (=it is preceded by a pipeline branch),
* then nullptr is returned.
******************************************************************************/
ModifierApplication* ModifierApplication::getPredecessorModApp() const
{
	int pipelineCount = 0;
	ModifierApplication* predecessor = nullptr;
	visitDependents([&](RefMaker* dependent) {
		if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent)) {
			if(modApp->input() == this && !modApp->pipelines(true).empty()) {
				pipelineCount++;
				predecessor = modApp;
			}
		}
		else if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dependent)) {
            if(pipeline->dataProvider() == this) {
				if(pipeline->isInScene())
		    		pipelineCount++;
			}
		}
	});
	return (pipelineCount <= 1) ? predecessor : nullptr;
}

}	// End of namespace
