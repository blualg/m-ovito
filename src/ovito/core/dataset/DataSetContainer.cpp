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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/UndoStack.h>
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

IMPLEMENT_OVITO_CLASS(DataSetContainer);
DEFINE_REFERENCE_FIELD(DataSetContainer, currentSet);

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
	clearAllReferences();
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void DataSetContainer::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(currentSet)) {

		// Detach old dataset from this container.
		if(DataSet* oldDataSet = static_object_cast<DataSet>(oldTarget)) {
			OVITO_ASSERT(oldDataSet->_container == this);
			oldDataSet->_container = nullptr;
		}

		// Forward signals from the current dataset.
		disconnect(_viewportConfigReplacedConnection);
		disconnect(_renderSettingsReplacedConnection);
		disconnect(_filePathChangedConnection);
		if(currentSet()) {
			currentSet()->_container = this;
			_viewportConfigReplacedConnection = connect(currentSet(), &DataSet::viewportConfigReplaced, this, &DataSetContainer::onViewportConfigReplaced);
			_renderSettingsReplacedConnection = connect(currentSet(), &DataSet::renderSettingsReplaced, this, &DataSetContainer::renderSettingsReplaced);
			_filePathChangedConnection = connect(currentSet(), &DataSet::filePathChanged, this, &DataSetContainer::filePathChanged);
		}

		Q_EMIT dataSetChanged(currentSet());

		// Discard all objects in the vis cache.
		Application::instance()->visCache().reset();

		if(currentSet()) {
			Q_EMIT renderSettingsReplaced(currentSet()->renderSettings());
			Q_EMIT filePathChanged(currentSet()->filePath());
			onViewportConfigReplaced(currentSet()->viewportConfig());			
		}
		else {
			onViewportConfigReplaced(nullptr);
			Q_EMIT renderSettingsReplaced(nullptr);
			Q_EMIT filePathChanged(QString());
		}
	}
	RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* This handler is called when another viewport configuration becomes the active one.
******************************************************************************/
void DataSetContainer::onViewportConfigReplaced(ViewportConfiguration* viewportConfig)
{
	disconnect(_activeViewportChangedConnection);
	if(viewportConfig) {
		// Forward signals from the current viewport config.
		_activeViewportChangedConnection = connect(viewportConfig, &ViewportConfiguration::activeViewportChanged, this, &DataSetContainer::onActiveViewportChanged);
	}
	Q_EMIT viewportConfigReplaced(viewportConfig);
	onActiveViewportChanged(viewportConfig ? viewportConfig->activeViewport() : nullptr);
}

/******************************************************************************
* This handler is called when another viewport becomes the active one.
******************************************************************************/
void DataSetContainer::onActiveViewportChanged(Viewport* activeViewport)
{
	onSceneReplaced(activeViewport ? activeViewport->scene() : nullptr);
	Q_EMIT activeViewportChanged(activeViewport);
}

/******************************************************************************
* This handler is called when another scene becomes the active one.
******************************************************************************/
void DataSetContainer::onSceneReplaced(Scene* newScene)
{
	if(newScene == _activeScene)
		return;
	disconnect(_selectionSetReplacedConnection);
	_activeScene = newScene;
	if(_animationPlayback) {
		_animationPlayback->stopAnimationPlayback();
		_animationPlayback->setScene(newScene);
	}
	if(newScene) {
		// Forward signals from the current scene.
		_selectionSetReplacedConnection = connect(newScene, &Scene::selectionSetReplaced, this, &DataSetContainer::onSelectionSetReplaced);
	}
	Q_EMIT sceneReplaced(newScene);
	onAnimationSettingsReplaced(newScene ? newScene->animationSettings() : nullptr);
	onSelectionSetReplaced(newScene ? newScene->selection() : nullptr);
}

/******************************************************************************
* This handler is called when another selection set becomes the active one.
******************************************************************************/
void DataSetContainer::onSelectionSetReplaced(SelectionSet* newSelectionSet)
{
	if(newSelectionSet == _activeSelectionSet)
		return;
	disconnect(_selectionSetChangedConnection);
	disconnect(_selectionSetChangeCompleteConnection);
	_activeSelectionSet = newSelectionSet;
	if(newSelectionSet) {
		// Forward signals from the current selection set.
		_selectionSetChangedConnection = connect(newSelectionSet, &SelectionSet::selectionChanged, this, &DataSetContainer::selectionChanged);
		_selectionSetChangeCompleteConnection = connect(newSelectionSet, &SelectionSet::selectionChangeComplete, this, &DataSetContainer::selectionChangeComplete);
	}
	Q_EMIT selectionSetReplaced(newSelectionSet);
	Q_EMIT selectionChanged(newSelectionSet);
	Q_EMIT selectionChangeComplete(newSelectionSet);
}

/******************************************************************************
* This handler is called when another animation settings object becomes the active one.
******************************************************************************/
void DataSetContainer::onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings)
{
	if(newAnimationSettings == _activeAnimationSettings)
		return;
	disconnect(_animationCurrentFrameChangedConnection);
	disconnect(_animationIntervalChangedConnection);
	disconnect(_timeFormatChangedConnection);
	_activeAnimationSettings = newAnimationSettings;
	if(newAnimationSettings) {
		// Forward signals from the current animation settings object.
		_animationCurrentFrameChangedConnection = connect(newAnimationSettings, &AnimationSettings::currentFrameChanged, this, &DataSetContainer::currentFrameChanged);
		_animationIntervalChangedConnection = connect(newAnimationSettings, &AnimationSettings::intervalChanged, this, &DataSetContainer::animationIntervalChanged);
		_timeFormatChangedConnection = connect(newAnimationSettings, &AnimationSettings::timeFormatChanged, this, &DataSetContainer::timeFormatChanged);
	}
	if(newAnimationSettings) {
		Q_EMIT animationIntervalChanged(newAnimationSettings->firstFrame(), newAnimationSettings->lastFrame());
		Q_EMIT currentFrameChanged(newAnimationSettings->currentFrame());
		Q_EMIT timeFormatChanged();
	}
	Q_EMIT animationSettingsReplaced(newAnimationSettings);
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
* Loads the given session state file and makes it the current dataset.
******************************************************************************/
bool DataSetContainer::loadDataset(const QString& filename, MainThreadOperation operation)
{
	// Make path absolute.
	QString absoluteFilepath = QFileInfo(filename).absoluteFilePath();

	// Load dataset from file.
	OORef<DataSet> dataSet;

	QFile fileStream(absoluteFilepath);
	if(!fileStream.open(QIODevice::ReadOnly))
		throw Exception(tr("Failed to open session state file '%1' for reading: %2").arg(absoluteFilepath).arg(fileStream.errorString()));

	QDataStream dataStream(&fileStream);
	ObjectLoadStream stream(dataStream, operation);

	dataSet = stream.loadObject<DataSet>();
	stream.close();

	if(!dataSet)
		throw Exception(tr("Session state file '%1' does not contain a dataset.").arg(absoluteFilepath));
		
	OVITO_CHECK_OBJECT_POINTER(dataSet);
	dataSet->setFilePath(absoluteFilepath);
	setCurrentSet(dataSet);
	return true;
}

/******************************************************************************
* Create the animation playback helper object on demand.
******************************************************************************/
SceneAnimationPlayback* DataSetContainer::createAnimationPlayback()
{
	if(!_animationPlayback) {
		_animationPlayback = OORef<SceneAnimationPlayback>::create(userInterface(), activeScene());
		connect(_animationPlayback.get(), &SceneAnimationPlayback::playbackChanged, this, &DataSetContainer::playbackChanged);
	}
	return _animationPlayback;
}

}	// End of namespace
