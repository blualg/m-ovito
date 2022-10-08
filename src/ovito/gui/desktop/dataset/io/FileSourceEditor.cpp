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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanActionParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/gui/desktop/dialogs/ModalPropertiesEditorDialog.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/gui/desktop/dialogs/ImportRemoteFileDialog.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/app/PluginManager.h>
#include "FileSourceEditor.h"
#include "FileSourcePlaybackRateEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(FileSourceEditor);
SET_OVITO_OBJECT_EDITOR(FileSource, FileSourceEditor);

/******************************************************************************
* Sets up the UI of the editor.
******************************************************************************/
void FileSourceEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
	// Create a rollout.
	QWidget* rollout = createRollout(tr("External file"), rolloutParams, "manual:scene_objects.file_source");

	// Create the rollout contents.
	QVBoxLayout* layout = new QVBoxLayout(rollout);
	layout->setContentsMargins(4,4,4,4);
	layout->setSpacing(4);

	QVBoxLayout* sublayout;

	QToolBar* toolbar = new QToolBar(rollout);
	toolbar->setStyleSheet("QToolBar { padding: 0px; margin: 0px; border: 0px none black; }");
	layout->addWidget(toolbar);

	toolbar->addAction(QIcon::fromTheme("file_import_object_changefile"), tr("Pick new file"), this, SLOT(onPickLocalInputFile()));
#ifdef OVITO_SSH_CLIENT
	toolbar->addAction(QIcon::fromTheme("file_import_remote"), tr("Pick new remote file"), this, SLOT(onPickRemoteInputFile()));
#endif
	toolbar->addAction(QIcon::fromTheme("file_import_object_reload"), tr("Reload file"), this, SLOT(onReloadFrame()));
	toolbar->addAction(QIcon::fromTheme("file_import_object_refresh_animation"), tr("Update trajectory frames"), this, SLOT(onReloadAnimation()));
	QAction* preloadTrajAction = toolbar->addAction(QIcon::fromTheme("file_cache_pipeline_output"), tr("Load entire trajectory into memory"));
	BooleanActionParameterUI* preloadTrajectoryUI = new BooleanActionParameterUI(this, PROPERTY_FIELD(FileSource::pipelineTrajectoryCachingEnabled), preloadTrajAction);

	QGroupBox* sourceBox = new QGroupBox(tr("Data source"), rollout);
	layout->addWidget(sourceBox);
	QGridLayout* gridlayout1 = new QGridLayout(sourceBox);
	gridlayout1->setContentsMargins(4,4,4,4);
	gridlayout1->setColumnStretch(1,1);
	gridlayout1->setVerticalSpacing(2);
	_filenameLabel = new QLineEdit();
	_filenameLabel->setReadOnly(true);
	_filenameLabel->setFrame(false);
	QLabel* label = new QLabel(tr("Current file:"));
	int maxLabelWidth = label->sizeHint().width();
	gridlayout1->addWidget(label, 0, 0);
	gridlayout1->addWidget(_filenameLabel, 0, 1);
	_sourcePathLabel = new QLineEdit();
	_sourcePathLabel->setReadOnly(true);
	_sourcePathLabel->setFrame(false);
	label = new QLabel(tr("Directory:"));
	maxLabelWidth = std::max(label->sizeHint().width(), maxLabelWidth);
	gridlayout1->addWidget(label, 1, 0);
	gridlayout1->addWidget(_sourcePathLabel, 1, 1);

	QGroupBox* wildcardBox = new QGroupBox(tr("File sequence"), rollout);
	layout->addWidget(wildcardBox);
	QGridLayout* gridlayout2 = new QGridLayout(wildcardBox);
	gridlayout2->setContentsMargins(4,4,4,4);
	gridlayout2->setVerticalSpacing(2);
	gridlayout2->setColumnStretch(1, 1);
	_wildcardPatternTextbox = new QLineEdit();
	connect(_wildcardPatternTextbox, &QLineEdit::returnPressed, this, &FileSourceEditor::onWildcardPatternEntered);

	label = new QLabel(tr("Search pattern:"));
	maxLabelWidth = std::max(label->sizeHint().width(), maxLabelWidth);
	gridlayout2->addWidget(label, 0, 0);
	gridlayout2->addWidget(_wildcardPatternTextbox, 0, 1);

	BooleanParameterUI* autoGenerateFilePatternUI = new BooleanParameterUI(this, PROPERTY_FIELD(FileSource::autoGenerateFilePattern));
	autoGenerateFilePatternUI->checkBox()->setText(tr("auto-generate"));
	gridlayout2->addWidget(autoGenerateFilePatternUI->checkBox(), 1, 0);
	maxLabelWidth = std::max(autoGenerateFilePatternUI->checkBox()->sizeHint().width(), maxLabelWidth);

	_fileSeriesLabel = new ElidedTextLabel(Qt::ElideRight);
	QFont smallFont = _fileSeriesLabel->font();
#ifdef Q_OS_MACOS
	smallFont.setPointSize(std::max(6, smallFont.pointSize() - 3));
#elif defined(Q_OS_LINUX)
	smallFont.setPointSize(std::max(6, smallFont.pointSize() - 2));
#else
	smallFont.setPointSize(std::max(6, smallFont.pointSize() - 1));
#endif
	_fileSeriesLabel->setFont(smallFont);
	gridlayout2->addWidget(_fileSeriesLabel, 1, 1);

	if(!parentEditor()) {

		QGroupBox* trajectoryBox = new QGroupBox(tr("Trajectory"), rollout);
		layout->addWidget(trajectoryBox);
		QGridLayout* gridlayout3 = new QGridLayout(trajectoryBox);
		gridlayout3->setContentsMargins(4,4,4,4);
		gridlayout3->setVerticalSpacing(2);
		gridlayout3->setColumnStretch(1, 1);

		label = new QLabel(tr("Current frame:"));
		maxLabelWidth = std::max(label->sizeHint().width(), maxLabelWidth);
		gridlayout3->addWidget(label, 0, 0);
		_framesListBox = new QComboBox();
		_framesListBox->setEditable(false);
		// To improve performance of drop-down list display:
		_framesListBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		static_cast<QListView*>(_framesListBox->view())->setUniformItemSizes(true);
		static_cast<QListView*>(_framesListBox->view())->setLayoutMode(QListView::Batched);
		_framesListModel = new QStringListModel(this);
		_framesListBox->setModel(_framesListModel);
		connect(_framesListBox, qOverload<int>(&QComboBox::activated), this, &FileSourceEditor::onFrameSelected);
		gridlayout3->addWidget(_framesListBox, 0, 1);
		_timeSeriesLabel = new ElidedTextLabel(Qt::ElideRight);
		_timeSeriesLabel->setFont(smallFont);
		gridlayout3->addWidget(_timeSeriesLabel, 1, 1);

 		label = new QLabel(tr("Playback ratio:"));
		maxLabelWidth = std::max(label->sizeHint().width(), maxLabelWidth);
		gridlayout3->addWidget(label, 2, 0);

		QHBoxLayout* sublayout = new QHBoxLayout();
		sublayout->setContentsMargins(0,0,0,0);
		sublayout->setSpacing(6);
		gridlayout3->addLayout(sublayout, 2, 1);

		_playbackRatioDisplay = new QLabel(tr("1 / 1"));
		sublayout->addWidget(_playbackRatioDisplay);
		sublayout->addStretch(1);
		_editPlaybackBtn = new QPushButton(tr("Change..."));
		sublayout->addWidget(_editPlaybackBtn);
		connect(_editPlaybackBtn, &QPushButton::clicked, this, [&]() {
			if(!editObject()) return;
			ModalPropertiesEditorDialog(editObject(), new FileSourcePlaybackRateEditor(), container(), 
				mainWindow(), tr("Configure Trajectory Playback"), tr("Change trajectory playback"), "manual:scene_objects.file_source.configure_playback").exec();
			updateDisplayedInformation();
		});
		
		gridlayout3->setColumnMinimumWidth(0, maxLabelWidth);
	}
	gridlayout1->setColumnMinimumWidth(0, maxLabelWidth);
	gridlayout2->setColumnMinimumWidth(0, maxLabelWidth);

	QGroupBox* statusBox = new QGroupBox(tr("Status"), rollout);
	layout->addWidget(statusBox);
	sublayout = new QVBoxLayout(statusBox);
	sublayout->setContentsMargins(4,4,4,4);
	_statusLabel = new StatusWidget(rollout);
	sublayout->addWidget(_statusLabel);

	// Show settings editor of importer class.
	new SubObjectParameterUI(this, PROPERTY_FIELD(FileSource::importer), rolloutParams.after(rollout));

	// Whenever a new FileSource gets loaded into the editor:
	connect(this, &PropertiesEditor::contentsReplaced, this, [this, con = QMetaObject::Connection()](RefTarget* editObject) mutable {
		disconnect(con);

		// Update displayed information.
		updateFramesList();
		updateDisplayedInformation();

		// Update the frames list displayed in the UI whenever it changes.
		con = editObject ? connect(static_object_cast<FileSource>(editObject), &FileSource::framesListChanged, this, &FileSourceEditor::updateFramesList) : QMetaObject::Connection();
	});
}

/******************************************************************************
* Is called when the user presses the "Pick local input file" button.
******************************************************************************/
void FileSourceEditor::onPickLocalInputFile()
{
	FileSource* fileSource = static_object_cast<FileSource>(editObject());
	if(!fileSource) return;

	try {
		QUrl newSourceUrl;
		OvitoClassPtr importerType;
		QString importerFormat;

		// Put code in a block: Need to release dialog before loading new input file.
		{
			// Offer only file importer types that are compatible with a FileSource.
			auto importerClasses = PluginManager::instance().metaclassMembers<FileImporter>(FileSourceImporter::OOClass());

			// Let the user select a file by displaying a dialog window.
			ImportFileDialog dialog(importerClasses, dataset(), container()->window(), tr("Pick input file"), false);

			// Select the previously imported file in the file dialog.
			if(fileSource->dataCollectionFrame() >= 0 && fileSource->dataCollectionFrame() < fileSource->frames().size()) {
				const QUrl& url = fileSource->frames()[fileSource->dataCollectionFrame()].sourceFile;
				if(url.isLocalFile()) {
#ifndef Q_OS_LINUX
					dialog.selectFile(url.toLocalFile());
#else
					// Workaround for bug in QFileDialog on Linux (Qt 6.2.4) crashing in exec() when selectFile() is called before (OVITO issue #216).
					dialog.setDirectory(QFileInfo(url.toLocalFile()).dir());
#endif
				}
			}
			if(dialog.exec() != QDialog::Accepted)
				return;

			newSourceUrl = dialog.urlToImport();
			std::tie(importerType, importerFormat) = dialog.selectedFileImporter();
		}

		// Set the new input location.
		importNewFile(fileSource, newSourceUrl, importerType, importerFormat, createOperation());
	}
	catch(const Exception& ex) {
		ex.reportError();
	}
}

/******************************************************************************
* Is called when the user presses the "Pick remote input file" button.
******************************************************************************/
void FileSourceEditor::onPickRemoteInputFile()
{
	FileSource* fileSource = static_object_cast<FileSource>(editObject());
	if(!fileSource) return;

	try {
		QUrl newSourceUrl;
		OvitoClassPtr importerType;
		QString importerFormat;

		// Put code in a block: Need to release dialog before loading new input file.
		{
			// Offer only file importer types that are compatible with a FileSource.
			auto importerClasses = PluginManager::instance().metaclassMembers<FileImporter>(FileSourceImporter::OOClass());

			// Let the user select a new URL.
			ImportRemoteFileDialog dialog(importerClasses, dataset(), container()->window(), tr("Pick source"));
			QUrl oldUrl;
			if(fileSource->dataCollectionFrame() >= 0 && fileSource->dataCollectionFrame() < fileSource->frames().size())
				oldUrl = fileSource->frames()[fileSource->dataCollectionFrame()].sourceFile;
			else if(!fileSource->sourceUrls().empty())
				oldUrl = fileSource->sourceUrls().front();
			dialog.selectFile(oldUrl);
			if(dialog.exec() != QDialog::Accepted)
				return;

			newSourceUrl = dialog.urlToImport();
			std::tie(importerType, importerFormat) = dialog.selectedFileImporter();
		}

		// Set the new input location.
		importNewFile(fileSource, newSourceUrl, importerType, importerFormat, createOperation());
	}
	catch(const Exception& ex) {
		ex.reportError();
	}
}

/******************************************************************************
* Loads a new file into the FileSource.
******************************************************************************/
bool FileSourceEditor::importNewFile(FileSource* fileSource, const QUrl& url, OvitoClassPtr importerType, const QString& importerFormat, MainThreadOperation operation)
{
	OVITO_ASSERT(operation.isCurrent());
	OORef<FileImporter> importer;

	// Create file importer instance.
	if(!importerType) {

		// Detect file format.
		Future<OORef<FileImporter>> importerFuture = FileImporter::autodetectFileFormat(fileSource->dataset(), url, fileSource->importer());
		if(!importerFuture.waitForFinished())
			return false;

		importer = importerFuture.result();
		if(!importer)
			fileSource->throwException(tr("Could not detect the format of the file to be imported. The format might not be supported."));
	}
	else {
		// Caller has provided a specific importer type.
		// First, try to reuse existing importer if it is of the requested type.
		if(fileSource->importer() && &fileSource->importer()->getOOClass() == importerType) {
			importer = fileSource->importer();
		}
		else {
			// Create a new importer instance.
			importer = static_object_cast<FileImporter>(importerType->createInstance(fileSource->dataset()));
			if(!importer)
				return false;
		}
		importer->setSelectedFileFormat(importerFormat);
	}

	// The importer must be a FileSourceImporter.
	OORef<FileSourceImporter> newImporter = dynamic_object_cast<FileSourceImporter>(importer);
	if(!newImporter)
		fileSource->throwException(tr("The selected file type is not compatible."));

	// Ask user whether existing data objects should be maintained.
	bool keepExistingDataCollection = false;
	if(fileSource->dataCollection() && fileSource->userHasChangedDataCollection()) {
		QMessageBox msgBox(QMessageBox::Question, tr("Import new file"), 
			tr("Do you want to keep your changes?"),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
			parentWindow());
		msgBox.setDefaultButton(QMessageBox::Yes);
		msgBox.setEscapeButton(QMessageBox::Cancel);
		msgBox.setInformativeText(tr("<p>Select <b>Yes</b> to preserve any adjustments you've made to "
			"visual elements, particle types, etc. Data will be refreshed from the newly picked file.</p>"
			"<p>Select <b>No</b> to start over and reset all visual elements and data objects to their standard state.</p>"
			"<p>In either case, modifiers you have added to the pipeline will be preserved.</p>"));
		int result = msgBox.exec();
		if(result == QMessageBox::Cancel)
			return false; // Operation canceled by user.
		else if(result == QMessageBox::Yes)
			keepExistingDataCollection = true;
	}

	// Temporarily suppress viewport updates while setting up the newly imported data.
	ViewportSuspender noVPUpdate(fileSource->dataset()->viewportConfig());

	// Show the optional user interface (which is provided by the corresponding FileImporterEditor class) for the new importer.
	for(OvitoClassPtr clazz = &newImporter->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
		OvitoClassPtr editorClass = PropertiesEditor::registry().getEditorClass(clazz);
		if(editorClass && editorClass->isDerivedFrom(FileImporterEditor::OOClass())) {
			OORef<FileImporterEditor> editor = dynamic_object_cast<FileImporterEditor>(editorClass->createInstance());
			if(editor) {
				if(!editor->inspectNewFile(newImporter, url, mainWindow()))
					return false;
			}
		}
	}

	// Set the new input location.
	return fileSource->setSource({url}, std::move(newImporter), false, keepExistingDataCollection);
}

/******************************************************************************
* Is called when the user presses the Reload frame button.
******************************************************************************/
void FileSourceEditor::onReloadFrame()
{
	if(FileSource* fileSource = static_object_cast<FileSource>(editObject())) {
		// Request a complete reloading of the current frame from the external file,
		// including a refresh of the file from the remote location if it is not a 
		// local file.
		fileSource->reloadFrame(true, fileSource->dataCollectionFrame());
	}
}

/******************************************************************************
* Is called when the user presses the Reload animation button.
******************************************************************************/
void FileSourceEditor::onReloadAnimation()
{
	if(FileSource* fileSource = static_object_cast<FileSource>(editObject())) {
		// Let the FileSource update the list of source animation frames.
		// After the update is complete, jump to the last of the newly added animation frames.
		int oldFrameCount = fileSource->frames().size();
		fileSource->updateListOfFrames(true).finally(fileSource->executor(), [fileSource, oldFrameCount](Task& task) {
			if(!task.isCanceled() && fileSource->frames().size() > oldFrameCount) {
				TimePoint time = fileSource->sourceFrameToAnimationTime(fileSource->frames().size() - 1);
				fileSource->dataset()->animationSettings()->setTime(time);
			}
		});
	}
}

/******************************************************************************
* This is called when the user has changed the source URL.
******************************************************************************/
void FileSourceEditor::onWildcardPatternEntered()
{
	FileSource* fileSource = static_object_cast<FileSource>(editObject());
	OVITO_CHECK_OBJECT_POINTER(fileSource);

	undoableTransaction(tr("Change wildcard pattern"), [this, fileSource]() {
		if(!fileSource->importer())
			return;

		QString pattern = _wildcardPatternTextbox->text().trimmed();
		if(pattern.isEmpty())
			return;

		QUrl newUrl;
		if(!fileSource->sourceUrls().empty()) newUrl = fileSource->sourceUrls().front();
		QFileInfo fileInfo(newUrl.path());
		fileInfo.setFile(fileInfo.dir(), pattern);
		newUrl.setPath(fileInfo.filePath());
		if(!newUrl.isValid())
			throwException(tr("URL is not valid."));

		fileSource->setSource({newUrl}, fileSource->importer(), false);
	});
	updateDisplayedInformation();
}

/******************************************************************************
* Updates the displayed status information.
******************************************************************************/
void FileSourceEditor::updateDisplayedInformation()
{
	_deferredDisplayUpdatePending = false;

	FileSource* fileSource = static_object_cast<FileSource>(editObject());
	if(!fileSource) {
		// Disable all UI controls if no file source exists.
		_wildcardPatternTextbox->clear();
		_wildcardPatternTextbox->setEnabled(false);
		_sourcePathLabel->setText(QString());
		_filenameLabel->setText(QString());
		_statusLabel->clearStatus();
		if(_framesListBox) {
			_framesListBox->clear();
			_framesListBox->setEnabled(false);
		}
		if(_playbackRatioDisplay)
			_playbackRatioDisplay->setText(QString());
		if(_editPlaybackBtn)
			_editPlaybackBtn->setEnabled(false);
		return;
	}

	QString wildcardPattern;
	if(!fileSource->sourceUrls().empty()) {
		wildcardPattern = fileSource->sourceUrls().front().fileName();
	}
	_wildcardPatternTextbox->setText(wildcardPattern);
	_wildcardPatternTextbox->setEnabled(true);

	_sourcePathLabel->setText(fileSource->currentDirectoryPath());
	_filenameLabel->setText(fileSource->currentFileName());

	if(_timeSeriesLabel) {
		if(!fileSource->frames().empty())
			_timeSeriesLabel->setText(tr("Showing frame %1 of %2").arg(fileSource->dataCollectionFrame()+1).arg(fileSource->frames().count()));
		else
			_timeSeriesLabel->setText(tr("No frames available"));
	}

	if(_playbackRatioDisplay) {
		if(fileSource->restrictToFrame() < 0)
			_playbackRatioDisplay->setText(tr("%1 / %2").arg(fileSource->playbackSpeedNumerator()).arg(fileSource->playbackSpeedDenominator()));
		else
			_playbackRatioDisplay->setText(tr("single frame"));
	}

	if(_framesListBox) {
		_framesListBox->setCurrentIndex(fileSource->dataCollectionFrame());
	}

	_statusLabel->setStatus(fileSource->status());
}

/******************************************************************************
* Updates the list of trajectory frames displayed in the UI.
******************************************************************************/
void FileSourceEditor::updateFramesList()
{
	FileSource* fileSource = static_object_cast<FileSource>(editObject());

	if(!fileSource) {
		// Disable all UI controls if no file source exists.
		if(_fileSeriesLabel)
			_fileSeriesLabel->setText(QString());
		if(_editPlaybackBtn)
			_editPlaybackBtn->setEnabled(false);
		return;
	}

	// Gets the number of files matching the wildcard pattern.
	if(fileSource->numberOfFiles() == 0)
		_fileSeriesLabel->setText(tr("Found no matching file"));
	else if(fileSource->numberOfFiles() == 1)
		_fileSeriesLabel->setText(tr("Found 1 matching file"));
	else
		_fileSeriesLabel->setText(tr("Found %1 matching files").arg(fileSource->numberOfFiles()));

	if(_framesListBox) {
		QStringList stringList;
		stringList.reserve(fileSource->frames().size());
		for(const FileSourceImporter::Frame& frame : fileSource->frames())
			stringList.push_back(frame.label);
		_framesListModel->setStringList(std::move(stringList));
		_framesListBox->setCurrentIndex(fileSource->dataCollectionFrame());
		_framesListBox->setEnabled(_framesListBox->count() > 1);
	}

	if(_editPlaybackBtn) {
		_editPlaybackBtn->setEnabled(fileSource->frames().size() > 1);
	}
}

/******************************************************************************
* Is called when the user has selected a certain frame in the frame list box.
******************************************************************************/
void FileSourceEditor::onFrameSelected(int index)
{
	FileSource* fileSource = static_object_cast<FileSource>(editObject());
	if(!fileSource) return;

	if(fileSource->restrictToFrame() < 0) {
		dataset()->animationSettings()->setTime(fileSource->sourceFrameToAnimationTime(index));
	}
	else {
		undoableTransaction(tr("Select static frame"), [&]() {
			fileSource->setRestrictToFrame(index);
		});
	}
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool FileSourceEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(source == editObject()) {
		if(event.type() == ReferenceEvent::ObjectStatusChanged || event.type() == ReferenceEvent::TitleChanged || event.type() == ReferenceEvent::ReferenceChanged) {
			if(!_deferredDisplayUpdatePending) {
				_deferredDisplayUpdatePending = true;
				QTimer::singleShot(200, this, &FileSourceEditor::updateDisplayedInformation);
			}
		}
	}
	return PropertiesEditor::referenceEvent(source, event);
}

}	// End of namespace
