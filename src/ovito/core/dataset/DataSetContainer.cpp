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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/utilities/io/FileManager.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DataSetContainer);
DEFINE_REFERENCE_FIELD(DataSetContainer, currentSet);
DEFINE_REFERENCE_FIELD(DataSetContainer, activeViewportConfig);
DEFINE_REFERENCE_FIELD(DataSetContainer, activeViewport);
DEFINE_REFERENCE_FIELD(DataSetContainer, activeScene);
DEFINE_REFERENCE_FIELD(DataSetContainer, activeSelectionSet);
DEFINE_REFERENCE_FIELD(DataSetContainer, activeAnimationSettings);

/******************************************************************************
* Initializes the dataset manager.
******************************************************************************/
DataSetContainer::DataSetContainer(TaskManager& taskManager, UserInterface& userInterface) : _taskManager(taskManager), _userInterface(userInterface)
{
}

/******************************************************************************
* Destructor.
******************************************************************************/
DataSetContainer::~DataSetContainer()
{
    setCurrentSet(nullptr);

    OVITO_ASSERT(activeViewportConfig() == nullptr);
    OVITO_ASSERT(activeViewport() == nullptr);
    OVITO_ASSERT(activeScene() == nullptr);
    OVITO_ASSERT(activeAnimationSettings() == nullptr);
    OVITO_ASSERT(activeSelectionSet() == nullptr);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool DataSetContainer::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == currentSet()) {
        if(event.type() == ReferenceEvent::ReferenceChanged) {
            const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
            if(refEvent.field() == PROPERTY_FIELD(DataSet::viewportConfig)) {
                _activeViewportConfig.set(this, PROPERTY_FIELD(activeViewportConfig), currentSet()->viewportConfig());
            }
            else if(refEvent.field() == PROPERTY_FIELD(DataSet::renderSettings)) {
                Q_EMIT renderSettingsReplaced(currentSet()->renderSettings());
            }
        }
        else if(event.type() == ReferenceEvent::TargetChanged) {
            const TargetChangedEvent& changedEvent = static_cast<const TargetChangedEvent&>(event);
            if(changedEvent.field() == PROPERTY_FIELD(DataSet::filePath)) {
                Q_EMIT filePathChanged(currentSet()->filePath());
            }
        }
    }
    else if(source == activeViewportConfig()) {
        if(event.type() == ReferenceEvent::ReferenceChanged) {
            const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
            if(refEvent.field() == PROPERTY_FIELD(ViewportConfiguration::activeViewport)) {
                _activeViewport.set(this, PROPERTY_FIELD(activeViewport), activeViewportConfig()->activeViewport());
            }
        }
    }
    else if(source == activeViewport()) {
        if(event.type() == ReferenceEvent::ReferenceChanged) {
            const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
            if(refEvent.field() == PROPERTY_FIELD(Viewport::scene)) {
                _activeScene.set(this, PROPERTY_FIELD(activeScene), activeViewport()->scene());
            }
        }
    }
    else if(source == activeAnimationSettings()) {
        if(event.type() == ReferenceEvent::TargetChanged) {
            const TargetChangedEvent& changedEvent = static_cast<const TargetChangedEvent&>(event);
            if(changedEvent.field() == PROPERTY_FIELD(AnimationSettings::currentFrame)) {
                Q_EMIT currentFrameChanged(activeAnimationSettings()->currentFrame());
            }
            else if(changedEvent.field() == PROPERTY_FIELD(AnimationSettings::firstFrame) || changedEvent.field() == PROPERTY_FIELD(AnimationSettings::lastFrame)) {
                Q_EMIT animationIntervalChanged(activeAnimationSettings()->firstFrame(), activeAnimationSettings()->lastFrame());
            }
        }
    }
    else if(source == activeSelectionSet()) {
        if(event.type() == ReferenceEvent::ReferenceChanged || event.type() == ReferenceEvent::ReferenceAdded || event.type() == ReferenceEvent::ReferenceRemoved) {
            const ReferenceFieldEvent& refEvent = static_cast<const ReferenceFieldEvent&>(event);
            if(refEvent.field() == PROPERTY_FIELD(SelectionSet::nodes)) {
                Q_EMIT selectionChanged(activeSelectionSet());
                if(!_selectionChangeCompleteTimer.isActive())
                    _selectionChangeCompleteTimer.start(0, Qt::CoarseTimer, this);
            }
        }
    }
    return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DataSetContainer::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(currentSet)) {

        // Inform clients about the change.
        Q_EMIT dataSetChanged(currentSet());

        // Discard all objects in the vis cache.
        Application::instance()->visCache().reset();

        if(currentSet()) {
            Q_EMIT renderSettingsReplaced(currentSet()->renderSettings());
            Q_EMIT filePathChanged(currentSet()->filePath());
            _activeViewportConfig.set(this, PROPERTY_FIELD(activeViewportConfig), currentSet()->viewportConfig());
        }
        else {
            Q_EMIT renderSettingsReplaced(nullptr);
            Q_EMIT filePathChanged({});
            _activeViewportConfig.set(this, PROPERTY_FIELD(activeViewportConfig), nullptr);
        }
    }
    else if(field == PROPERTY_FIELD(activeViewportConfig)) {
        Q_EMIT viewportConfigReplaced(activeViewportConfig());
        _activeViewport.set(this, PROPERTY_FIELD(activeViewport), activeViewportConfig() ? activeViewportConfig()->activeViewport() : nullptr);
    }
    else if(field == PROPERTY_FIELD(activeViewport)) {
        Q_EMIT activeViewportChanged(activeViewport());
        _activeScene.set(this, PROPERTY_FIELD(activeScene), activeViewport() ? activeViewport()->scene() : nullptr);
    }
    else if(field == PROPERTY_FIELD(activeScene)) {
        if(_animationPlayback) {
            _animationPlayback->stopAnimationPlayback();
            _animationPlayback->setScene(activeScene());
        }
        Q_EMIT sceneReplaced(activeScene());
        _activeAnimationSettings.set(this, PROPERTY_FIELD(activeAnimationSettings), activeScene() ? activeScene()->animationSettings() : nullptr);
        _activeSelectionSet.set(this, PROPERTY_FIELD(activeSelectionSet), activeScene() ? activeScene()->selection() : nullptr);
    }
    else if(field == PROPERTY_FIELD(activeSelectionSet)) {
        Q_EMIT selectionSetReplaced(activeSelectionSet());
        Q_EMIT selectionChanged(activeSelectionSet());
        Q_EMIT selectionChangeComplete(activeSelectionSet());
    }
    else if(field == PROPERTY_FIELD(activeAnimationSettings)) {
        Q_EMIT animationSettingsReplaced(activeAnimationSettings());
        if(activeAnimationSettings()) {
            Q_EMIT animationIntervalChanged(activeAnimationSettings()->firstFrame(), activeAnimationSettings()->lastFrame());
            Q_EMIT currentFrameChanged(activeAnimationSettings()->currentFrame());
            Q_EMIT timeFormatChanged();
        }
    }
    RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void DataSetContainer::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _selectionChangeCompleteTimer.timerId()) {
        OVITO_ASSERT(_selectionChangeCompleteTimer.isActive());
        _selectionChangeCompleteTimer.stop();
        Q_EMIT selectionChangeComplete(activeSelectionSet());
    }
}

/******************************************************************************
* Creates an empty dataset and makes it the current dataset.
******************************************************************************/
DataSet* DataSetContainer::newDataset()
{
    setCurrentSet(OORef<DataSet>::create());
    return currentSet();
}

/******************************************************************************
* Loads the given session state file.
******************************************************************************/
OORef<DataSet> DataSetContainer::loadDataset(const QString& filename)
{
    // Make path absolute.
    QString absoluteFilepath = QFileInfo(filename).absoluteFilePath();

    // Load dataset from file.
    OORef<DataSet> dataSet;

    QFile fileStream(absoluteFilepath);
    if(!fileStream.open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open session state file '%1' for reading: %2").arg(absoluteFilepath).arg(fileStream.errorString()));

    QDataStream dataStream(&fileStream);
    ObjectLoadStream stream(dataStream);

    dataSet = stream.loadObject<DataSet>();
    stream.close();

    if(!dataSet)
        throw Exception(tr("Session state file '%1' does not contain a dataset.").arg(absoluteFilepath));

    dataSet->setFilePath(absoluteFilepath);
    return dataSet;
}

/******************************************************************************
* Create the animation playback helper object on demand.
******************************************************************************/
SceneAnimationPlayback* DataSetContainer::createAnimationPlayback()
{
    if(!_animationPlayback) {
        _animationPlayback = OORef<SceneAnimationPlayback>::create(userInterface());
        connect(_animationPlayback.get(), &SceneAnimationPlayback::playbackChanged, this, &DataSetContainer::playbackChanged);
    }
    return _animationPlayback;
}

/******************************************************************************
* Starts or stops animation playback in the viewports.
******************************************************************************/
void DataSetContainer::setAnimationPlayback(bool on)
{
    if(on) {
        startAnimationPlayback(
            (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
            ? -1 : 1);
    }
    else {
        stopAnimationPlayback();
    }
}

}   // End of namespace
