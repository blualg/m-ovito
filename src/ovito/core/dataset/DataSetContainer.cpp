////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
		disconnect(_selectionSetReplacedConnection);
		disconnect(_viewportConfigReplacedConnection);
		disconnect(_animationSettingsReplacedConnection);
		disconnect(_renderSettingsReplacedConnection);
		disconnect(_filePathChangedConnection);
		disconnect(_undoStackCleanChangedConnection);
		if(currentSet()) {
			_selectionSetReplacedConnection = connect(currentSet(), &DataSet::selectionSetReplaced, this, &DataSetContainer::onSelectionSetReplaced);
			_viewportConfigReplacedConnection = connect(currentSet(), &DataSet::viewportConfigReplaced, this, &DataSetContainer::viewportConfigReplaced);
			_animationSettingsReplacedConnection = connect(currentSet(), &DataSet::animationSettingsReplaced, this, &DataSetContainer::animationSettingsReplaced);
			_renderSettingsReplacedConnection = connect(currentSet(), &DataSet::renderSettingsReplaced, this, &DataSetContainer::renderSettingsReplaced);
			_filePathChangedConnection = connect(currentSet(), &DataSet::filePathChanged, this, &DataSetContainer::filePathChanged);
			_undoStackCleanChangedConnection = connect(&currentSet()->undoStack(), &UndoStack::cleanChanged, this, &DataSetContainer::modificationStatusChanged);
			currentSet()->_container = this;
		}

		Q_EMIT dataSetChanged(currentSet());

		if(currentSet()) {

			// Prepare scene for display whenever a new dataset becomes active.
			if(Application::instance()->guiMode()) {
				_sceneReadyScheduled = true;
				Q_EMIT scenePreparationBegin();
				_sceneReadyFuture = currentSet()->whenSceneReady().then(currentSet()->executor(), [this]() {
					sceneBecameReady();
				});
			}

			Q_EMIT viewportConfigReplaced(currentSet()->viewportConfig());
			Q_EMIT animationSettingsReplaced(currentSet()->animationSettings());
			Q_EMIT renderSettingsReplaced(currentSet()->renderSettings());
			Q_EMIT filePathChanged(currentSet()->filePath());
			Q_EMIT modificationStatusChanged(currentSet()->undoStack().isClean());
			onSelectionSetReplaced(currentSet()->selection());
			onAnimationSettingsReplaced(currentSet()->animationSettings());
		}
		else {
			onSelectionSetReplaced(nullptr);
			onAnimationSettingsReplaced(nullptr);
			Q_EMIT viewportConfigReplaced(nullptr);
			Q_EMIT animationSettingsReplaced(nullptr);
			Q_EMIT renderSettingsReplaced(nullptr);
			Q_EMIT filePathChanged(QString());
			Q_EMIT modificationStatusChanged(true);
		}
	}
	RefMaker::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool DataSetContainer::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(source == currentSet()) {
		if(Application::instance()->guiMode()) {
			if(event.type() == ReferenceEvent::TargetChanged) {
				// Update viewports as soon as the scene becomes ready.
				if(!_sceneReadyScheduled) {
					_sceneReadyScheduled = true;
					Q_EMIT scenePreparationBegin();
					_sceneReadyFuture = currentSet()->whenSceneReady().then(currentSet()->executor(), [this]() {
						sceneBecameReady();
					});
				}
			}
			else if(event.type() == ReferenceEvent::PreliminaryStateAvailable) {
				// Update viewports when a new preliminiary state from one of the data pipelines
				// becomes available (unless we are playing an animation).
				if(!currentSet()->animationSettings()->arePreliminaryViewportUpdatesSuspended())
					currentSet()->viewportConfig()->updateViewports();
			}
		}
	}
	return RefMaker::referenceEvent(source, event);
}

/******************************************************************************
* Is called when scene of the current dataset is ready to be displayed.
******************************************************************************/
void DataSetContainer::sceneBecameReady()
{
	_sceneReadyScheduled = false;
	_sceneReadyFuture.reset();
	if(currentSet())
		currentSet()->viewportConfig()->updateViewports();
	Q_EMIT scenePreparationEnd();
}

/******************************************************************************
* This handler is invoked when the current selection set of the current dataset
* has been replaced.
******************************************************************************/
void DataSetContainer::onSelectionSetReplaced(SelectionSet* newSelectionSet)
{
	// Forward signals from the current selection set.
	disconnect(_selectionSetChangedConnection);
	disconnect(_selectionSetChangeCompleteConnection);
	if(newSelectionSet) {
		_selectionSetChangedConnection = connect(newSelectionSet, &SelectionSet::selectionChanged, this, &DataSetContainer::selectionChanged);
		_selectionSetChangeCompleteConnection = connect(newSelectionSet, &SelectionSet::selectionChangeComplete, this, &DataSetContainer::selectionChangeComplete);
	}
	Q_EMIT selectionSetReplaced(newSelectionSet);
	Q_EMIT selectionChanged(newSelectionSet);
	Q_EMIT selectionChangeComplete(newSelectionSet);
}

/******************************************************************************
* This handler is invoked when the current animation settings of the current
* dataset have been replaced.
******************************************************************************/
void DataSetContainer::onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings)
{
	// Forward signals from the current animation settings object.
	disconnect(_animationTimeChangedConnection);
	disconnect(_animationTimeChangeCompleteConnection);
	if(newAnimationSettings) {
		_animationTimeChangedConnection = connect(newAnimationSettings, &AnimationSettings::timeChanged, this, &DataSetContainer::timeChanged);
		_animationTimeChangeCompleteConnection = connect(newAnimationSettings, &AnimationSettings::timeChangeComplete, this, &DataSetContainer::timeChangeComplete);
	}
	if(newAnimationSettings) {
		Q_EMIT timeChanged(newAnimationSettings->time());
		Q_EMIT timeChangeComplete();
	}
}

/******************************************************************************
* Creates an empty dataset and makes it the current dataset.
******************************************************************************/
DataSet* DataSetContainer::newDataset()
{
	setCurrentSet(OORef<DataSet>::create(nullptr));
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

#if 0
		// Issue a warning when the floating-point precision of the input file does not match
		// the precision used in this build.
		if(stream.floatingPointPrecision() > sizeof(FloatType)) {
			if(mainWindow()) {
				QString msg = tr("The session state file has been written with a version of this program that uses %1-bit floating-point precision. "
					   "The version of this program that you are currently using only supports %2-bit precision numbers. "
					   "The precision of all numbers stored in the input file will be truncated during loading.").arg(stream.floatingPointPrecision()*8).arg(sizeof(FloatType)*8);
				QMessageBox::warning(mainWindow(), tr("Floating-point precision mismatch"), msg);
			}
		}
#endif

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

}	// End of namespace
