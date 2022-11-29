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

		if(oldTarget) {
			DataSet* oldDataSet = static_object_cast<DataSet>(oldTarget);

			// Stop animation playback for the old dataset.
			oldDataSet->animationSettings()->stopAnimationPlayback();

			if(oldDataSet->_container == this)
				oldDataSet->_container = nullptr;
		}

		// Forward signals from the current dataset.
		disconnect(_viewportConfigReplacedConnection);
		disconnect(_animationSettingsReplacedConnection);
		disconnect(_sceneReplacedConnection);
		disconnect(_renderSettingsReplacedConnection);
		disconnect(_filePathChangedConnection);
		if(currentSet()) {
			_sceneReplacedConnection = connect(currentSet(), &DataSet::sceneReplaced, this, &DataSetContainer::onSceneReplaced);
			_viewportConfigReplacedConnection = connect(currentSet(), &DataSet::viewportConfigReplaced, this, &DataSetContainer::viewportConfigReplaced);
			_animationSettingsReplacedConnection = connect(currentSet(), &DataSet::animationSettingsReplaced, this, &DataSetContainer::animationSettingsReplaced);
			_renderSettingsReplacedConnection = connect(currentSet(), &DataSet::renderSettingsReplaced, this, &DataSetContainer::renderSettingsReplaced);
			_filePathChangedConnection = connect(currentSet(), &DataSet::filePathChanged, this, &DataSetContainer::filePathChanged);
			currentSet()->_container = this;
		}

		Q_EMIT dataSetChanged(currentSet());

		if(currentSet()) {
			Q_EMIT viewportConfigReplaced(currentSet()->viewportConfig());
			Q_EMIT animationSettingsReplaced(currentSet()->animationSettings());
			Q_EMIT renderSettingsReplaced(currentSet()->renderSettings());
			Q_EMIT filePathChanged(currentSet()->filePath());
			onSceneReplaced(currentSet()->scene());
			onAnimationSettingsReplaced(currentSet()->animationSettings());
			onViewportConfigReplaced(currentSet()->viewportConfig());			
		}
		else {
			onSceneReplaced(nullptr);
			onAnimationSettingsReplaced(nullptr);
			Q_EMIT viewportConfigReplaced(nullptr);
			Q_EMIT animationSettingsReplaced(nullptr);
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
		_activeViewportChangedConnection = connect(viewportConfig, &ViewportConfiguration::activeViewportChanged, this, &DataSetContainer::activeViewportChanged);
	}
	Q_EMIT activeViewportChanged(viewportConfig ? viewportConfig->activeViewport() : nullptr);
}

/******************************************************************************
* This handler is called when another scene becomes the active one.
******************************************************************************/
void DataSetContainer::onSceneReplaced(Scene* newScene)
{
	disconnect(_scenePreparationStartedConnection);
	disconnect(_scenePreparationFinishedConnection);
	disconnect(_selectionSetReplacedConnection);
	_activeScene = newScene;
	if(newScene) {
		// Forward signals from the current scene.
		_scenePreparationStartedConnection = connect(newScene, &Scene::scenePreparationStarted, this, &DataSetContainer::scenePreparationStarted);
		_scenePreparationFinishedConnection = connect(newScene, &Scene::scenePreparationFinished, this, &DataSetContainer::scenePreparationFinished);
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
	disconnect(_animationCurrentFrameChangedConnection);
	disconnect(_animationCurrentFrameChangeCompleteConnection);
	disconnect(_animationIntervalChangedConnection);
	if(_activeAnimationSettings) {
		// Stop animation playback before switching to a new animation settings object.
		_activeAnimationSettings->stopAnimationPlayback();
	}
	_activeAnimationSettings = newAnimationSettings;
	if(newAnimationSettings) {
		// Forward signals from the current animation settings object.
		_animationCurrentFrameChangedConnection = connect(newAnimationSettings, &AnimationSettings::currentFrameChanged, this, &DataSetContainer::currentFrameChanged);
		_animationCurrentFrameChangeCompleteConnection = connect(newAnimationSettings, &AnimationSettings::currentFrameChangeComplete, this, &DataSetContainer::currentFrameChangeComplete);
		_animationIntervalChangedConnection = connect(newAnimationSettings, &AnimationSettings::intervalChanged, this, &DataSetContainer::animationIntervalChanged);
	}
	if(newAnimationSettings) {
		Q_EMIT animationIntervalChanged(newAnimationSettings->firstFrame(), newAnimationSettings->lastFrame());
		Q_EMIT currentFrameChanged(newAnimationSettings->currentFrame());
		Q_EMIT currentFrameChangeComplete();
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
* Loads the given session state file and makes it the current dataset.
******************************************************************************/
bool DataSetContainer::loadDataset(const QString& filename, MainThreadOperation operation)
{
	// Make path absolute.
	QString absoluteFilepath = QFileInfo(filename).absoluteFilePath();

	// Load dataset from file.
	OORef<DataSet> dataSet;
	try {

		QFile fileStream(absoluteFilepath);
		if(!fileStream.open(QIODevice::ReadOnly))
			throw Exception(tr("Failed to open session state file '%1' for reading: %2").arg(absoluteFilepath).arg(fileStream.errorString()), this);

		QDataStream dataStream(&fileStream);
		ObjectLoadStream stream(dataStream, operation);

		dataSet = stream.loadObject<DataSet>();
		stream.close();

		if(!dataSet)
			throw Exception(tr("Session state file '%1' does not contain a dataset.").arg(absoluteFilepath), this);
	}
	catch(Exception& ex) {
		// Provide a local context for the error.
		ex.setContext(this);
		throw ex;
	}
	OVITO_CHECK_OBJECT_POINTER(dataSet);
	dataSet->setFilePath(absoluteFilepath);
	setCurrentSet(dataSet);
	return true;
}

#if 0 // TODO: Remove unused code
/******************************************************************************
* Returns the scene that is currently active, i.e., which is displayed in 
* the viewport window that is currently selected.
******************************************************************************/
Scene* DataSetContainer::activeScene() const
{
	if(currentSet()) {
		if(Viewport* vp = currentSet()->viewportConfig()->activeViewport()) {
			return vp->scene();
		}
	}
	return nullptr;
}
#endif

}	// End of namespace
