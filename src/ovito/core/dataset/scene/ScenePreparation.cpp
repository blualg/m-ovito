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
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>
#include "ScenePreparation.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ScenePreparation);
DEFINE_REFERENCE_FIELD(ScenePreparation, scene);

/******************************************************************************
* Constructor.
******************************************************************************/
ScenePreparation::ScenePreparation(UserInterface& userInterface, Scene* scene) : _userInterface(userInterface)
{
	// Get notified when an ongoing pipeline evaluation task finishes.
	connect(&_pipelineEvaluationWatcher, &TaskWatcher::finished, this, &ScenePreparation::pipelineEvaluationFinished);

	// Activate the initial scene provided to the constructor.
	setScene(scene);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ScenePreparation::~ScenePreparation()
{
	// This will cancel any pipeline evaluation requests, which might still be in progress.
	clearAllReferences();
}

/******************************************************************************
* Returns a future that gets fulfilled once all data pipelines in the scene
* have been completely evaluated at the current animation time.
******************************************************************************/
SharedFuture<> ScenePreparation::future()
{
	makeReady(false);
	return _future;
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines in the current scene.
******************************************************************************/
void ScenePreparation::makeReady(bool forceReevaluation)
{
	_isRestartScheduled = false;

	// Create a promise, which remains in the unfinished state as long as we are preparing the scene.
	if(!_promise.isValid() || _promise.isCanceled()) {
		_promise = Promise<>::create<Task>(true);
		_future = _promise.sharedFuture();
		_completedScene = scene();
		if(scene()) {
			_completedFrame = scene()->animationSettings()->currentFrame();

			// Emit signal to indicate we are preparing the scene.
			Q_EMIT scenePreparationStarted();
		}
	}

	if(!scene()) {
		// Set the promise to the fulfilled state if there is no scene to prepare.
		_completedScene = nullptr;
		_promise.setFinished();
		_pipelineEvaluation.reset();
		return;
	}

	// Abort if application is about to shut down.
	if(userInterface().isShuttingDown()) {
		_completedScene = nullptr;
		_promise.cancel();
		return;
	}

	// If scene is already ready, we are done.
	if(!forceReevaluation && _promise.isFinished() && _completedScene == scene() && (!scene() || _completedFrame == scene()->animationSettings()->currentFrame())) {
		return;
	}

	// Is there still a pipeline evaluation in progress?
	if(_pipelineEvaluation.isValid() && !forceReevaluation) {
		OVITO_ASSERT(scene());

		// Keep waiting for the ongoing pipeline evaluation to complete - unless we are at the different animation time now.
		// Or unless the pipeline has been removed from the scene in the meantime.
		if(_pipelineEvaluation.time() == scene()->animationSettings()->currentTime() && _pipelineEvaluation.pipeline() && _pipelineEvaluation.pipeline()->isChildOf(scene())) {
			return;
		}
	}

	// If viewport updates are suspended, we simply wait until they get resumed.
	if(userInterface().areViewportUpdatesSuspended())
		return;

	// Hold on to the old evaluation request until a new request has been made 
	// to not loose partial results stored in the pipeline caches.
	PipelineEvaluationFuture oldEvaluation = std::move(_pipelineEvaluation);

	// Request results from all data pipelines in the scene.
	// If at least one of them is not immediately available, we'll have to
	// wait until its evaulation completes.
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset();
	_completedFrame = scene()->animationSettings()->currentFrame();
	_completedScene = scene();
	PipelineEvaluationRequest request(scene()->animationSettings());
	ExecutionContext::Scope executionScope(ExecutionContext::Type::Interactive, userInterface(), nullptr);

	// Go through all pipelines of the scene until we find one 
	// that is not completely evaulated yet.
	scene()->visitObjectNodes([&](PipelineSceneNode* pipeline) {
		// Request visual elements too.
		_pipelineEvaluation = pipeline->evaluateRenderingPipeline(request);
		if(!_pipelineEvaluation.isFinished()) {
			// Wait for this state to become available and return a pending future.
			return false;
		}
		else if(!_pipelineEvaluation.isCanceled()) {
			try { _pipelineEvaluation.results(); }
			catch(...) {
				qWarning() << "ScenePreparation::makeReady(): An exception was thrown in a data pipeline. This should never happen.";
				OVITO_ASSERT(false);
			}
		}
		_pipelineEvaluation.reset();
		return true;
	});

	// Now that a new evaluation request is underway, we can cancel the old request.
	oldEvaluation.reset();

	if(!_pipelineEvaluation.isValid()) {
		// If all pipelines are in the complete state, we are done. The scene is prepared for rendering.

		// Set the promise to the fulfilled state.
		_promise.setFinished();

		// Emit signal to indicate we've finished preparing the scene.
		Q_EMIT scenePreparationFinished();

		// Also update the viewports to reflect the final pipeline state.
		Q_EMIT viewportUpdateRequest();
	}
	else {
		// If one of the pipelines is not complete yet, wait until it is. 
		_pipelineEvaluationWatcher.watch(_pipelineEvaluation.task());
	}
}

/******************************************************************************
* Is called when the pipeline evaluation of a scene node has finished.
******************************************************************************/
void ScenePreparation::pipelineEvaluationFinished()
{
	OVITO_ASSERT(_pipelineEvaluation.isValid());
	OVITO_ASSERT(_pipelineEvaluation.pipeline());
	OVITO_ASSERT(_pipelineEvaluation.isFinished());

	// Query results of the pipeline evaluation to see if an exception has been thrown.
	if(_promise.isValid() && !_pipelineEvaluation.isCanceled()) {
		try {
			_pipelineEvaluation.results();
		}
		catch(...) {
			qWarning() << "ScenePreparation::pipelineEvaluationFinished(): An exception was thrown in a data pipeline. This should never happen.";
			OVITO_ASSERT(false);
		}
	}

	_pipelineEvaluation.reset();
	_pipelineEvaluationWatcher.reset();

	// One of the pipelines in the scene became ready.
	// Check if there are more pending pipelines in the scene.
	makeReady(false);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool ScenePreparation::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(event.type() == ReferenceEvent::TargetChanged && source == scene()) {
		// Ignore changes of visual elements, because they usually don't require a pipeline re-evaluation.
		if(dynamic_object_cast<DataVis>(event.sender()) == nullptr) {
			// If the scene contents change, we interrupt the pipeline evaluation that is currently in progress and start over.
			restartPreparation();
		}
	}
	else if(event.type() == ReferenceEvent::PreliminaryStateAvailable && source == scene()) {
		// Update viewport window when a new preliminiary state from one of the data pipelines in the scene
		// becomes available (unless we are playing an animation).
		if(!userInterface().arePreliminaryViewportUpdatesSuspended())
			Q_EMIT viewportUpdateRequest();
	}
	return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ScenePreparation::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(scene)) {
		restartPreparation();
	}
	RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines next time execution returns to the event loop.
******************************************************************************/
void ScenePreparation::restartPreparation() 
{ 
	// Reset the promise if it was already in the completed state before.
	if(_promise.isValid() && _promise.isFinished()) {
		_promise.reset();
		_future.reset();
	}
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset();
	_completedScene = nullptr;
	if(!_isRestartScheduled) {
		_isRestartScheduled = true;
		QMetaObject::invokeMethod(this, "makeReady", Qt::QueuedConnection, Q_ARG(bool, true));
	}
}


}	// End of namespace
