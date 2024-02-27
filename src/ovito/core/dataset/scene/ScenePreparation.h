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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/rendering/RenderSettings.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief This object performs evaluation of all pipelines in a Scene to prepare them for rendering in the interactive viewports.
 */
class OVITO_CORE_EXPORT ScenePreparation : public QObject, public RefMaker
{
    OVITO_CLASS(ScenePreparation)
    Q_OBJECT

public:

    /// Constructor.
    explicit ScenePreparation(UserInterface& userInterface, Scene* scene = nullptr);

    /// Returns the abstract user interface in which this object operates.
    UserInterface& userInterface() const { return _userInterface; }

    /// Returns a future that gets fulfilled once the scene is ready.
    SharedFuture<> future();

Q_SIGNALS:

    /// Is emitted whenever the scene is being made ready for rendering after it was changed in some way.
    void scenePreparationStarted();

    /// Is emitted whenever the scene became ready for rendering.
    void scenePreparationFinished();

    /// Is emitted whenever its time to repaint the viewports showing the active scene.
    void viewportUpdateRequest();

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Requests the (re-)evaluation of all data pipelines next time execution returns to the event loop.
    void restartPreparation();

private Q_SLOTS:

    /// Is called when the evaluation of a pipeline in the scene has finished.
    void pipelineEvaluationFinished();

    /// Is called whenever a new RenderSettings object becomes active.
    void renderSettingsReplaced(RenderSettings* newRenderSettings);

private:

    /// Requests the (re-)evaluation of all data pipelines in the current scene.
    Q_INVOKABLE void makeReady(bool forceReevaluation);

private:

    /// The scene being prepared.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(Scene*, scene, setScene);

    /// The active render settings.
    DECLARE_REFERENCE_FIELD(RenderSettings*, renderSettings);

    /// The active scene node selection set.
    DECLARE_REFERENCE_FIELD(SelectionSet*, selectionSet);

    /// The abstract user interface in which this object operates.
    UserInterface& _userInterface;

    /// The animation frame at which the scene was made ready. This is used to detect time changes.
    int _completedFrame;

    /// The scene that was made ready recently. This is used to detect a change of the active scene.
    Scene* _completedScene;

    /// The current pipeline evaluation that is in progress.
    SharedFuture<PipelineFlowState> _pipelineEvaluationFuture;

    /// The pipeline that is currently being evaluated.
    OORef<Pipeline> _currentPipeline;

    /// The animation time at which the current pipeline is being evaluated.
    AnimationTime _currentTime;

    /// To get notified when the evaluation of the current data pipeline finishes.
    TaskWatcher _pipelineEvaluationWatcher;

    /// The promise that is fulfilled once the scene is ready.
    Promise<> _promise;

    /// A shared future which reaches the completed state once the scene is ready.
    SharedFuture<> _future;

    /// Indicates that a restart of the preparation has already been scheduled.
    bool _isRestartScheduled = false;
};

}   // End of namespace
