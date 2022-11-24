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
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(Scene);
DEFINE_REFERENCE_FIELD(Scene, animationSettings);
DEFINE_REFERENCE_FIELD(Scene, selection);
DEFINE_PROPERTY_FIELD(Scene, orbitCenterMode);
DEFINE_PROPERTY_FIELD(Scene, userOrbitCenter);
SET_PROPERTY_FIELD_LABEL(Scene, animationSettings, "Animation Settings");
SET_PROPERTY_FIELD_LABEL(Scene, selection, "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
Scene::Scene(ObjectCreationParams params, AnimationSettings* animationSettings) : SceneNode(params),
	_orbitCenterMode(ORBIT_SELECTION_CENTER),
	_userOrbitCenter(Point3::Origin())
{
	setNodeName("Scene");
	setAnimationSettings(animationSettings);

	// The root node does not need a transformation controller.
	setTransformationController(nullptr);

	// Create child objects for animation settings and node selection set.
	if(params.createSubObjects()) {
		if(!this->animationSettings())
			setAnimationSettings(OORef<AnimationSettings>::create(params));
		setSelection(OORef<SelectionSet>::create(params));
	}

	// Get notified when the current pipeline evaluation task finishes.
	connect(&_pipelineEvaluationWatcher, &TaskWatcher::finished, this, &Scene::pipelineEvaluationFinished);

#if 0 // TODO: Make this work
	// Get notified whenever viewport updates are re-enabled.
	if(dataset()->viewportConfig()) {
		_viewportUpdateResumedConnection = connect(dataset()->viewportConfig(), &ViewportConfiguration::viewportUpdateResumed, this, &Scene::onViewportUpdatesResumed);
	}

	// In case the global viewport configuration gets replaced, update the signal connection.
	connect(dataset(), &DataSet::viewportConfigReplaced, this, [&](ViewportConfiguration* newViewportConfiguration) {
		disconnect(_viewportUpdateResumedConnection);
		if(newViewportConfiguration)
			_viewportUpdateResumedConnection = connect(newViewportConfiguration, &ViewportConfiguration::viewportUpdateResumed, this, &Scene::onViewportUpdatesResumed);
	});
#endif
}

/******************************************************************************
* Destructor.
******************************************************************************/
Scene::~Scene()
{
	// Stop pipeline evaluation, which might still be in progress.
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset();
}

/******************************************************************************
* Searches the scene for a node with the given name.
******************************************************************************/
SceneNode* Scene::getNodeByName(const QString& nodeName) const
{
	SceneNode* result = nullptr;
	visitChildren([nodeName, &result](SceneNode* node) -> bool {
		if(node->nodeName() == nodeName) {
			result = node;
			return false;
		}
		return true;
	});
	return result;
}

/******************************************************************************
* Generates a name for a node that is unique throughout the scene.
******************************************************************************/
QString Scene::makeNameUnique(QString baseName) const
{
	// Remove any existing digits from end of base name.
	if(baseName.size() > 2 &&
		baseName.at(baseName.size()-1).isDigit() && baseName.at(baseName.size()-2).isDigit())
		baseName.chop(2);

	// Keep appending different numbers until we arrive at a unique name.
	for(int i = 1; ; i++) {
		QString newName = baseName + QString::number(i).rightJustified(2, '0');
		if(getNodeByName(newName) == nullptr)
			return newName;
	}
}

/******************************************************************************
* Returns a future that is triggered once all data pipelines in the scene
* have been completely evaluated at the current animation time.
******************************************************************************/
SharedFuture<> Scene::whenReady()
{
	OVITO_CHECK_OBJECT_POINTER(animationSettings());

	int frame = animationSettings()->currentFrame();
	if(_sceneReadyPromise.isValid()) {
		// The promise should never be in the canceled state, because we automatically reset it when it gets canceled (see below).
		OVITO_ASSERT(!_sceneReadyPromise.isCanceled());

		// Recreate async operation object if the animation time has changed.
		if(_sceneReadyPromise.isFinished() && _sceneReadyFrame != frame)
			_sceneReadyPromise.reset();
		else
			_sceneReadyFrame = frame;
	}

	// Create a new promise to represent the process of making the scene ready.
	if(!_sceneReadyPromise.isValid()) {
		_sceneReadyPromise = Promise<>::create<Task>(true);

		// Emit signal to indicate we are preparing the scene.
		Q_EMIT scenePreparationStarted();

		// Reset the promise to the null state as soon as it gets canceled.
		_sceneReadyPromise.finally(executor(), [this](UNUSED_CONTINUATION_FUNC_PARAM) {
			// Emit signal to indicate we finished preparing the scene.
			Q_EMIT scenePreparationFinished();
			// Start over the scene preparation if it was canceled halfway.
			if(_sceneReadyPromise.isCanceled())
				_sceneReadyPromise.reset();
		});

		_sceneReadyFrame = frame;

		// This will call makeReady() soon in order to evaluate all pipelines in the scene.
		makeReadyLater(false);
	}

	return _sceneReadyPromise.sharedFuture();
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines in the current scene.
******************************************************************************/
void Scene::makeReady(bool forceReevaluation)
{
	// Make sure whenReady() was called before.
	if(!_sceneReadyPromise.isValid()) {
		return;
	}

#if 0 // TODO: Make this work
	// Stop right here if application is about to shut down.
	if(ui().isShuttingDown()) {
		return;
	}

	// Stop right here if application is about to shut down.
	if(!dataset()->isActive()) {
		return;
	}
#endif

	// If scene is already ready, we are done.
	if(_sceneReadyPromise.isFinished() && _sceneReadyFrame == animationSettings()->currentFrame()) {
		return;
	}

	// Is there already a pipeline evaluation in progress?
	if(_pipelineEvaluation.isValid()) {
		// Keep waiting for the current pipeline evaluation to finish unless we are at the different animation time now.
		// Or unless the pipeline was removed from the scene in the meantime.
		if(!forceReevaluation && _pipelineEvaluation.frame() == animationSettings()->currentFrame() && _pipelineEvaluation.pipeline() && _pipelineEvaluation.pipeline()->isChildOf(this)) {
			return;
		}
	}

#if 0 // TODO: Make this work
	// If viewport updates are suspended, we simply wait until they get resumed.
	if(dataset()->viewportConfig()->isSuspended())
		return;
#endif

	// Request results from all data pipelines in the scene.
	// If at least one of them is not immediately available, we'll have to
	// wait until its evaulation completes.
	PipelineEvaluationFuture oldEvaluation = std::move(_pipelineEvaluation);
	_pipelineEvaluationWatcher.reset();
	_pipelineEvaluation.reset();
	_sceneReadyFrame = animationSettings()->currentFrame();
	PipelineEvaluationRequest request(animationSettings());

	visitObjectNodes([&](PipelineSceneNode* pipeline) {
		// Request visual elements too.
		_pipelineEvaluation = pipeline->evaluateRenderingPipeline(request);
		if(!_pipelineEvaluation.isFinished()) {
			// Wait for this state to become available and return a pending future.
			return false;
		}
		else if(!_pipelineEvaluation.isCanceled()) {
			try { _pipelineEvaluation.results(); }
			catch(...) {
				qWarning() << "DataSet::makeSceneReady(): An exception was thrown in a data pipeline. This should never happen.";
				OVITO_ASSERT(false);
			}
		}
		_pipelineEvaluation.reset();
		return true;
	});

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
void Scene::pipelineEvaluationFinished()
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
			qWarning() << "Scene::pipelineEvaluationFinished(): An exception was thrown in a data pipeline. This should never happen.";
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
* Sends an event to all dependents of this RefTarget.
******************************************************************************/
void Scene::notifyDependentsImpl(const ReferenceEvent& event)
{
	if(event.type() == ReferenceEvent::TargetChanged) {
		// If any of the scene pipelines change, the scene-ready state needs to be reset (unless it's still unfulfilled).
		if(_sceneReadyPromise.isValid() && _sceneReadyPromise.isFinished()) {
			_sceneReadyPromise.reset();
			OVITO_ASSERT(!_pipelineEvaluation.isValid());
		}

		// If any of the scene pipelines change, we should interrupt the pipeline evaluation that is currently in progress.
		// Ignore messages from visual elements, because they usually don't require a pipeline re-evaluation.
		if(_pipelineEvaluation.isValid() && dynamic_object_cast<DataVis>(event.sender()) == nullptr) {
			// Restart pipeline evaluation:
			makeReadyLater(true);
		}
	}

	SceneNode::notifyDependentsImpl(event);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool Scene::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(event.type() == ReferenceEvent::TargetChanged) {
		if(source == animationSettings()) {
			// If the animation time changes, we should interrupt any pipeline evaluation that is currently in progress.
			if(_pipelineEvaluation.isValid() && _pipelineEvaluation.time() != animationSettings()->currentTime()) {
				_pipelineEvaluationWatcher.reset();
				_pipelineEvaluation.reset();
				// Restart pipeline evaluation:
				makeReadyLater(false);
			}
		}
	}
	else if(event.type() == ReferenceEvent::AnimationFramesChanged && !isBeingLoaded()) {
		// Automatically adjust scene's animation interval to length of loaded source animations.
		if(animationSettings() && animationSettings()->autoAdjustInterval()) {
			UndoSuspender noUndo;
			animationSettings()->adjustAnimationInterval();
		}
	}
	else if(event.type() == ReferenceEvent::RequestGoToAnimationTime) {
		int frame = static_cast<const RequestGoToAnimationTimeEvent&>(event).time().frame();
		if(animationSettings() && frame >= animationSettings()->firstFrame() && frame <= animationSettings()->lastFrame())
			animationSettings()->setCurrentFrame(frame);
	}

	return SceneNode::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void Scene::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(selection)) {
		Q_EMIT selectionSetReplaced(selection());
	}

#if 0 // TODO: Make this work
	// Install a signal/slot connection that updates the viewports every time the animation time has changed.
	if(field == PROPERTY_FIELD(animationSettings)) {
		disconnect(_updateViewportOnTimeChangeConnection);
		if(animationSettings() && viewportConfig()) {
			_updateViewportOnTimeChangeConnection = connect(animationSettings(), &AnimationSettings::timeChangeComplete, viewportConfig(), &ViewportConfiguration::updateViewports);
			viewportConfig()->updateViewports();
		}
	}
#endif

	SceneNode::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void Scene::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(orbitCenterMode) || field == PROPERTY_FIELD(userOrbitCenter)) {
		Q_EMIT cameraOrbitCenterChanged();
	}
	SceneNode::propertyChanged(field);
}

/******************************************************************************
* Is called whenver viewport updates are resumed.
******************************************************************************/
void Scene::onViewportUpdatesResumed()
{
	makeReadyLater(true);
}

/******************************************************************************
* Returns the world space point around which the viewport camera orbits.
******************************************************************************/
Point3 Scene::orbitCenter(Viewport* vp) const
{
	OVITO_ASSERT(vp != nullptr);
	OVITO_ASSERT(animationSettings());

	// Update orbiting center.
	if(orbitCenterMode() == ORBIT_SELECTION_CENTER) {
		AnimationTime time = animationSettings()->currentTime();
		Box3 selectionBoundingBox;
		for(SceneNode* node : selection()->nodes()) {
			selectionBoundingBox.addBox(node->worldBoundingBox(time, vp));
		}
		if(!selectionBoundingBox.isEmpty())
			return selectionBoundingBox.center();
		else {
			Box3 sceneBoundingBox = worldBoundingBox(time, vp);
			if(!sceneBoundingBox.isEmpty())
				return sceneBoundingBox.center();
		}
	}
	else if(orbitCenterMode() == ORBIT_USER_DEFINED) {
		return _userOrbitCenter;
	}
	return Point3::Origin();
}

}	// End of namespace
