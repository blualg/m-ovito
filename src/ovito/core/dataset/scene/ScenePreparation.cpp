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
SharedFuture<> ScenePreparation::whenReady()
{
	if(_sceneReadyPromise.isValid() && _sceneReadyPromise.isFinished()) {
		// Recreate completed async operation if the animation time has changed or if a new scene has been set.
		if(_completedScene != scene() || (scene() && _completedFrame != scene()->animationSettings()->currentFrame()))
			_sceneReadyPromise.reset();
	}

	// Create a new promise, which remains in the unfinished state as long as we are preparing the scene.
	// If an old process got canceled halfway, start over with the scene preparation.
	if(!_sceneReadyPromise.isValid() || _sceneReadyPromise.isCanceled()) {
		_sceneReadyPromise = Promise<>::create<Task>(true);
		_completedScene = scene();
		if(scene()) {
			_completedFrame = scene()->animationSettings()->currentFrame();

			// Emit signal to indicate we are preparing the scene.
			Q_EMIT scenePreparationStarted();

			// Reset the promise to the null state as soon as it gets canceled.
			_sceneReadyPromise.finally(executor(), [this](UNUSED_CONTINUATION_FUNC_PARAM) noexcept {
				// Emit signal to indicate we've finished preparing the scene.
				Q_EMIT scenePreparationFinished();
			});

			// This will call makeReady() soon in order to evaluate all pipelines in the scene.
			makeReadyLater(false);
		}
		else {
			// Set the promise to the fulfilled state if there is no scene to prepare.
			_sceneReadyPromise.setFinished();
			_pipelineEvaluation.reset();
		}
	}

	return _sceneReadyPromise.sharedFuture();
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines in the current scene.
******************************************************************************/
void ScenePreparation::makeReady(bool forceReevaluation)
{
	// Only continue if whenReady() was called before.
	if(!_sceneReadyPromise.isValid()) {
		return;
	}

	// Stop right here if application is about to shut down.
	if(userInterface().isShuttingDown()) {
		return;
	}

	// If scene is already ready, we are done.
	if(_sceneReadyPromise.isFinished() && _completedScene == scene() && (!scene() || _completedFrame == scene()->animationSettings()->currentFrame())) {
		return;
	}

	// Is there already a pipeline evaluation in progress?
	if(_pipelineEvaluation.isValid()) {
		OVITO_ASSERT(scene());

		// Keep waiting for the ongoing pipeline evaluation to complete - unless we are at the different animation time now.
		// Or unless the pipeline has been removed from the scene in the meantime.
		if(!forceReevaluation && _pipelineEvaluation.frame() == scene()->animationSettings()->currentFrame() && _pipelineEvaluation.pipeline() && _pipelineEvaluation.pipeline()->isChildOf(scene())) {
			return;
		}
	}

#if 0 // TODO: Make this work
	// If viewport updates are suspended, we simply wait until they get resumed.
	if(dataset()->viewportConfig()->isSuspended())
		return;
#endif

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

	// If all pipelines are already complete, we are done.
	if(!_pipelineEvaluation.isValid()) {
		// Set the promise to the fulfilled state.
		_sceneReadyPromise.setFinished();
	}
	else {
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
	if(_sceneReadyPromise.isValid() && !_pipelineEvaluation.isCanceled()) {
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
		// If the scene contents change, invalidate any completed state.
		if(_sceneReadyPromise.isValid() && _sceneReadyPromise.isFinished()) {
			_sceneReadyPromise.reset();
			OVITO_ASSERT(!_pipelineEvaluation.isValid());
		}

		if(event.sender() == scene()->animationSettings()) {
			// If the animation time changes, abort ongoing pipeline evaluations.
			_pipelineEvaluationWatcher.reset();
			_pipelineEvaluation.reset();
			// Restart pipeline evaluation:
			makeReadyLater(false);
		}
		else if(_pipelineEvaluation.isValid() && dynamic_object_cast<DataVis>(event.sender()) == nullptr) {
			// If the scene contents change, we should interrupt the pipeline evaluation that is currently in progress.
			// Ignore messages from visual elements, because they usually don't require a pipeline re-evaluation.
			makeReadyLater(true);
		}
	}
	else if(event.type() == ReferenceEvent::PreliminaryStateAvailable && source == scene()) {
		// Update viewport window when a new preliminiary state from one of the data pipelines in the scene
		// becomes available (unless we are playing an animation).
#if 0 // TODO: Make this work
		if(!scene()->animationSettings()->arePreliminaryViewportUpdatesSuspended())
#endif
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
		_pipelineEvaluationWatcher.reset();
		_pipelineEvaluation.reset();
		_sceneReadyPromise.reset();
#if 0 // TODO: Remove dead code
		// Install a signal/slot connection that updates the viewports every time the animation time changes.
		if(field == PROPERTY_FIELD(animationSettings)) {
			disconnect(_updateViewportOnTimeChangeConnection);
			if(animationSettings() && viewportConfig()) {
				_updateViewportOnTimeChangeConnection = connect(animationSettings(), &AnimationSettings::timeChangeComplete, viewportConfig(), &ViewportConfiguration::updateViewports);
				viewportConfig()->updateViewports();
			}
		}
#endif
	}
	RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

}	// End of namespace
