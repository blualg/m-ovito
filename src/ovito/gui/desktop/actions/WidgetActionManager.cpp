////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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
#include <ovito/core/dataset/UndoStack.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/desktop/viewport/input/XFormModes.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/ClonePipelineDialog.h>
#include <ovito/gui/desktop/dialogs/AnimationSettingsDialog.h>
#include "WidgetActionManager.h"

namespace Ovito {

/******************************************************************************
* Initializes the WidgetActionManager.
******************************************************************************/
WidgetActionManager::WidgetActionManager(QObject* parent, MainWindow& mainWindow) : ActionManager(parent, mainWindow)
{
	createViewportModeAction(ACTION_XFORM_MOVE_MODE, new MoveMode(mainWindow.viewportInputManager()), tr("Move"), "edit_mode_move", tr("Move objects."));
	createViewportModeAction(ACTION_XFORM_ROTATE_MODE, new RotateMode(mainWindow.viewportInputManager()), tr("Rotate"), "edit_mode_rotate", tr("Rotate objects."));

	connect(getAction(ACTION_QUIT), &QAction::triggered, this, &WidgetActionManager::on_Quit_triggered);
	connect(getAction(ACTION_HELP_ABOUT), &QAction::triggered, this, &WidgetActionManager::on_HelpAbout_triggered);
	connect(getAction(ACTION_HELP_GRAPHICS_SYSINFO), &QAction::triggered, this, &WidgetActionManager::on_HelpSystemInfo_triggered);
	connect(getAction(ACTION_HELP_SHOW_ONLINE_HELP), &QAction::triggered, this, &WidgetActionManager::on_HelpShowOnlineHelp_triggered);
	connect(getAction(ACTION_HELP_SHOW_SCRIPTING_HELP), &QAction::triggered, this, &WidgetActionManager::on_HelpShowScriptingReference_triggered);
	connect(getAction(ACTION_FILE_OPEN), &QAction::triggered, this, &WidgetActionManager::on_FileOpen_triggered);
	connect(getAction(ACTION_FILE_SAVE), &QAction::triggered, this, &WidgetActionManager::on_FileSave_triggered);
	connect(getAction(ACTION_FILE_SAVEAS), &QAction::triggered, this, &WidgetActionManager::on_FileSaveAs_triggered);
	connect(getAction(ACTION_FILE_IMPORT), &QAction::triggered, this, &WidgetActionManager::on_FileImport_triggered);
	connect(getAction(ACTION_FILE_REMOTE_IMPORT), &QAction::triggered, this, &WidgetActionManager::on_FileRemoteImport_triggered);
	connect(getAction(ACTION_FILE_EXPORT), &QAction::triggered, this, &WidgetActionManager::on_FileExport_triggered);
	connect(getAction(ACTION_FILE_NEW_WINDOW), &QAction::triggered, this, &WidgetActionManager::on_FileNewWindow_triggered);
	connect(getAction(ACTION_SETTINGS_DIALOG), &QAction::triggered, this, &WidgetActionManager::on_Settings_triggered);
	connect(getAction(ACTION_ANIMATION_SETTINGS), &QAction::triggered, this, &WidgetActionManager::on_AnimationSettings_triggered);
	connect(getAction(ACTION_RENDER_ACTIVE_VIEWPORT), &QAction::triggered, this, &WidgetActionManager::on_RenderActiveViewport_triggered);
	connect(getAction(ACTION_EDIT_CLONE_PIPELINE), &QAction::triggered, this, &WidgetActionManager::on_ClonePipeline_triggered);
	connect(getAction(ACTION_EDIT_RENAME_PIPELINE), &QAction::triggered, this, &WidgetActionManager::on_RenamePipeline_triggered);
	connect(getAction(ACTION_NEW_PIPELINE_FILESOURCE), &QAction::triggered, this, &WidgetActionManager::on_NewPipelineFileSource_triggered);

	setupCommandSearch();
}

/******************************************************************************
* Handles ACTION_EDIT_CLONE_PIPELINE command
******************************************************************************/
void WidgetActionManager::on_ClonePipeline_triggered()
{
	if(PipelineSceneNode* pipeline = dynamic_object_cast<PipelineSceneNode>(dataset()->selection()->firstNode())) {
		ClonePipelineDialog dialog(pipeline, &mainWindow());
		dialog.exec();
	}
}

/******************************************************************************
* Handles ACTION_EDIT_RENAME_PIPELINE command
******************************************************************************/
void WidgetActionManager::on_RenamePipeline_triggered()
{
	if(OORef<PipelineSceneNode> pipeline = dynamic_object_cast<PipelineSceneNode>(dataset()->selection()->firstNode())) {
		QString oldPipelineName = pipeline->objectTitle();
		bool ok;
		QString pipelineName = QInputDialog::getText(&mainWindow(), tr("Rename pipeline"), tr("New pipeline name:                                         "), QLineEdit::Normal, oldPipelineName, &ok).trimmed();
		if(ok && pipelineName != oldPipelineName) {
			UndoableTransaction::handleExceptions(dataset()->undoStack(), tr("Rename pipeline"), [&]() {
				pipeline->setNodeName(pipelineName);
			});
		}
	}
}

/******************************************************************************
* Handles ACTION_NEW_PIPELINE_FILESOURCE command
******************************************************************************/
void WidgetActionManager::on_NewPipelineFileSource_triggered()
{
	if(!dataset()) return;

	UndoableTransaction::handleExceptions(dataset()->undoStack(), tr("Create pipeline"), [&]() {

#ifndef OVITO_BUILD_PROFESSIONAL
		dataset()->throwException(tr("OVITO Pro is required to insert more than one pipeline into the scene. Please visit <a href=\"https://www.ovito.org/about/ovito-pro/\">www.ovito.org</a> for more information on the extended version of our software."));
#endif

		// Do not create any animation keys.
		AnimationSuspender animSuspender(dataset());
		// Pause viewport updates while updating the scene.
		ViewportSuspender noUpdates(dataset());

		// Create the FileSource.
		OORef<FileSource> fileSource = OORef<FileSource>::create(dataset());

		// Create pipeline scene node.
		OORef<PipelineSceneNode> pipeline = OORef<PipelineSceneNode>::create(dataset());
		pipeline->setDataProvider(fileSource);

		// Insert pipeline into scene.
		dataset()->scene()->addChildNode(pipeline);

		// Select new object in the scene.
		dataset()->selection()->setNode(pipeline);
	});	
}

/******************************************************************************
* Handles the ACTION_ANIMATION_SETTINGS command.
******************************************************************************/
void WidgetActionManager::on_AnimationSettings_triggered()
{
	AnimationSettingsDialog(dataset()->animationSettings(), &mainWindow()).exec();
}

}	// End of namespace
