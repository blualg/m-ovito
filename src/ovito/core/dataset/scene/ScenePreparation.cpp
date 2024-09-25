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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/app/Application.h>
#include "ScenePreparation.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ScenePreparation);
DEFINE_REFERENCE_FIELD(ScenePreparation, scene);
DEFINE_REFERENCE_FIELD(ScenePreparation, renderSettings);
DEFINE_REFERENCE_FIELD(ScenePreparation, selectionSet);

/******************************************************************************
* Constructor.
******************************************************************************/
void ScenePreparation::initializeObject(UserInterface& userInterface, Scene* scene)
{
    RefMaker::initializeObject();

    _userInterface = &userInterface;

    // Activate the initial scene provided to the constructor.
    setScene(scene);

    // Get notified when a different rendering settings object becomes active.
    connect(&userInterface.datasetContainer(), &DataSetContainer::renderSettingsReplaced, this, &ScenePreparation::renderSettingsReplaced);
    renderSettingsReplaced(userInterface.datasetContainer().currentSet() ? userInterface.datasetContainer().currentSet()->renderSettings() : nullptr);
}

/******************************************************************************
* Returns a future that gets fulfilled once all data pipelines in the scene
* have been completely evaluated at the current animation time.
******************************************************************************/
SharedFuture<void> ScenePreparation::future()
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
    _restartTimer.stop();

    // Create a promise, which remains in the unfinished state as long as we are preparing the scene.
    if(!_promise || _promise.isCanceled() || _promise.isFinished()) {
        _promise = Promise<void>(std::make_shared<Task>(userInterface().shared_from_this(), Task::NoState));
        _future = _promise.sharedFuture();
        if(scene()) {
            // Emit signal to indicate we are preparing the scene.
            Q_EMIT scenePreparationStarted();
        }
    }

    if(!scene()) {
        // Set the promise to the fulfilled state if there is no scene to prepare.
        _promise.setFinished();
        _pipelineEvaluationFuture.reset();
        _currentPipeline = {};
        return;
    }

    // Abort if application is about to shut down.
    if(userInterface().isShuttingDown()) {
        _pipelineEvaluationFuture.reset();
        _promise.cancel();
        return;
    }

    // Is there still a pipeline evaluation in progress?
    if(_pipelineEvaluationFuture && !forceReevaluation) {
        OVITO_ASSERT(scene());

        // Keep waiting for the ongoing pipeline evaluation to complete - unless we are at the different animation time now.
        // Or unless the pipeline has been removed from the scene in the meantime.
        if(_currentTime == scene()->animationSettings()->currentTime() && _currentPipeline && _currentPipeline->isChildOf(scene())) {
            return;
        }
    }

    // Hold on to the old evaluation request until a new request has been made
    // to not loose partial results stored in the pipeline caches.
    SharedFuture<PipelineFlowState> oldEvaluation = std::move(_pipelineEvaluationFuture);

    // Request results from all data pipelines in the scene.
    // If at least one of them is not immediately available, we'll have to
    // wait until its evaluation completes.
    _currentPipeline.reset();
    _pipelineEvaluationFuture.reset();
    PipelineEvaluationRequest request(scene()->animationSettings()->currentTime());

    // Pipeline evaluation must be done in a valid execution context and with an active task object.
    // We use an isolated execution context to avoid interference with other ongoing tasks.
    MainThreadOperation operation(userInterface(), MainThreadOperation::Kind::Isolated);

    // Go through all pipelines of the scene until we find one
    // that is not completely evaluated yet.
    scene()->visitPipelines([&](Pipeline* pipeline) {
        _pipelineEvaluationFuture = pipeline->evaluatePipeline(request);
        if(!_pipelineEvaluationFuture.isFinished()) {
            // Wait for this state to become available and return a pending future.
            _currentPipeline = pipeline;
            _currentTime = request.time();
            return false;
        }
        else if(!_pipelineEvaluationFuture.isCanceled()) {
            try { _pipelineEvaluationFuture.waitForFinished(); }
            catch(const Exception& ex) {
                qWarning() << "ScenePreparation::makeReady(): Pipeline evaluation raised an exception.";
                ex.logError();
            }
            catch(...) {
                qWarning() << "ScenePreparation::makeReady(): Pipeline evaluation raised an exception.";
            }
        }
        _pipelineEvaluationFuture.reset();
        return true;
    });

    // Now that a new evaluation request is underway, we can cancel the old request.
    oldEvaluation.reset();

    if(!_pipelineEvaluationFuture) {
        // If all pipelines are in the ready state, we are done. The scene is prepared for rendering.

        // Set the promise to the fulfilled state to signal that the scene is prepared.
        _promise.setFinished();

        // Update the viewports to reflect the final pipeline state.
        Q_EMIT viewportUpdateRequest(false);

        // Also emit a Qt signal to indicate we've finished preparing the scene.
        // Note: This must come AFTER refreshing the viewports to make animation playback work correctly.
        Q_EMIT scenePreparationFinished();
    }
    else {
        OVITO_ASSERT(_currentPipeline);

        // If one of the pipelines is not complete yet, wait until it is.
        // Then start over to see if there are more pipelines that need to be evaluated.
        _pipelineEvaluationFuture.finally(ObjectExecutor(this), [this](Task& task) noexcept {
            // Make sure we are still waiting for the same future that just reached the completed state.
            if(_pipelineEvaluationFuture && _pipelineEvaluationFuture.task().get() == &task && _currentPipeline) {
                pipelineEvaluationFinished();
            }
        });
    }
}

/******************************************************************************
* Is called when the pipeline evaluation of a scene node has finished.
******************************************************************************/
void ScenePreparation::pipelineEvaluationFinished()
{
    OVITO_ASSERT(_pipelineEvaluationFuture);
    OVITO_ASSERT(_pipelineEvaluationFuture.isFinished());
    OVITO_ASSERT(_currentPipeline);

    // Query results of the pipeline evaluation to see if an exception has been thrown.
    if(_promise && !_pipelineEvaluationFuture.isCanceled()) {
        try {
            _pipelineEvaluationFuture.task()->throwPossibleException();
        }
        catch(...) {
            qWarning() << "ScenePreparation::pipelineEvaluationFinished(): An exception was thrown in a data pipeline. This should never happen.";
            OVITO_ASSERT(false);
        }
    }

    _currentPipeline.reset();
    _pipelineEvaluationFuture.reset();

    // One of the pipelines in the scene became ready.
    // Check if there are more pending pipelines in the scene.
    makeReady(false);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
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
    else if(event.type() == ReferenceEvent::TargetChanged && source == renderSettings()) {
        // Repaint viewports whenever the user changes the current render settings.
        Q_EMIT viewportUpdateRequest(false);
    }
    else if(event.type() == ReferenceEvent::TargetChanged && source == selectionSet()) {
        // Repaint viewports whenever the user selects a different object in the scene.
        Q_EMIT viewportUpdateRequest(false);
    }
    else if(event.type() == ReferenceEvent::InteractiveStateAvailable && source == scene()) {
        // Update viewport window when a new interactive state from one of the data pipelines in the scene
        // becomes available (unless we are playing an animation).
        if(!userInterface().arePreliminaryViewportUpdatesSuspended()) {
            Q_EMIT viewportUpdateRequest(true);
        }
    }
    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ScenePreparation::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(scene)) {
        restartPreparation(true);
        _selectionSet.set(this, PROPERTY_FIELD(selectionSet), scene() ? scene()->selection() : nullptr);
    }
    RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called whenever a new RenderSettings object becomes active.
******************************************************************************/
void ScenePreparation::renderSettingsReplaced(RenderSettings* newRenderSettings)
{
    _renderSettings.set(this, PROPERTY_FIELD(renderSettings), newRenderSettings);
    // Repaint viewports.
    Q_EMIT viewportUpdateRequest(false);
}

/******************************************************************************
* Requests the (re-)evaluation of all data pipelines next time execution returns to the event loop.
******************************************************************************/
void ScenePreparation::restartPreparation(bool restartImmediately)
{
    // Reset the promise if it was already in the completed state before.
    if(_promise && _promise.isFinished()) {
        _promise.reset();
        _future.reset();
    }
    // Note: Not resetting pipelineEvaluationFuture here, because we want to keep the in-flight evaluation request going until a new request has been made.
    _currentPipeline.reset();
    if(scene() && autoRestart() && !isBeingInitializedOrDeleted()) {
        // Force an immediate restart if there is no Qt event loop running.
        if(!QCoreApplication::instance()) {
            restartImmediately = true;
        }
        else if(_pipelineEvaluationFuture && _currentTime != scene()->animationSettings()->currentTime()) {
            // Force an immediate restart if the animation time has changed.
            restartImmediately = true;
        }
        if(!restartImmediately) {
            if(!_isRestartScheduled) {
                _isRestartScheduled = true;
                // Restart pipeline evaluation after a short delay if an evaluation is already in flight to avoid excessive requests.
                if(_pipelineEvaluationFuture && !_restartTimer.isActive())
                    _restartTimer.start(100, Qt::CoarseTimer, this);
                else
                    QMetaObject::invokeMethod(this, "makeReady", Qt::QueuedConnection, Q_ARG(bool, true));
            }
        }
        else {
            makeReady(true);
        }
    }
    else {
        _pipelineEvaluationFuture.reset();
        _future.reset();
        _promise.reset();
    }
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void ScenePreparation::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _restartTimer.timerId()) {
        makeReady(true);
    }
}

}   // End of namespace
