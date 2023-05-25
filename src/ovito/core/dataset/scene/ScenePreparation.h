////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief This object performs evaluation of all pipelines in a Scene to prepare them for rendering in the interactive viewports.
 */
class OVITO_CORE_EXPORT ScenePreparation : public RefMaker
{
    OVITO_CLASS(ScenePreparation)

public:

    /// Constructor.
    explicit ScenePreparation(UserInterface& userInterface, Scene* scene = nullptr);

    /// Destructor.
    virtual ~ScenePreparation();

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

    /// Is called when a RefTarget referenced by this object has generated an event.
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
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Scene*, scene, setScene, PROPERTY_FIELD_WEAK_REF | PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// The abstract user interface in which this object operates.
    UserInterface& _userInterface;

    /// The animation frame at which the scene was made ready. This is used to detect time changes.
    int _completedFrame;

    /// The scene that was made ready recently. This is used to detect a change of the active scene.
    Scene* _completedScene;

    /// The current pipeline evaluation that is in progress.
    PipelineEvaluationFuture _pipelineEvaluation;

    /// To get notified when the evaluation of the current data pipeline finishes.
    TaskWatcher _pipelineEvaluationWatcher;

    /// The promise that is fulfilled once the scene is ready.
    Promise<> _promise;

    /// A shared future which reaches the completed state once the scene is ready.
    SharedFuture<> _future;

    /// Indicates that a restart of the preparation has already been scheduled.
    bool _isRestartScheduled = false;

    /// Qt signal/slot connection to the SelectionSet::selectionChanged() signal.
    QMetaObject::Connection _selectionChangedConnection;

    // Qt signal/slot connection to the RenderSettings::settingsChanged() signal.
    QMetaObject::Connection _renderSettingsChangedConnection;
};

}   // End of namespace
