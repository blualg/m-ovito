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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>

namespace Ovito {

/*
 * \brief Manages all available user interface actions.
 */
class OVITO_GUI_EXPORT WidgetActionManager : public ActionManager
{
    Q_OBJECT

public:

    /// Constructor.
    WidgetActionManager(QObject* parent, MainWindow& mainWindow);

    /// Returns the main window this action manager belongs to.
    MainWindow& mainWindow() const { return static_cast<MainWindow&>(ActionManager::userInterface()); }

private Q_SLOTS:

    /// Is called when the user selects a command in the quick search field.
    void onQuickSearchCommandSelected(const QModelIndex& index);

    void on_Quit_triggered();
    void on_HelpAbout_triggered();
    void on_HelpSystemInfo_triggered();
    void on_HelpShowOnlineHelp_triggered();
    void on_HelpShowScriptingReference_triggered();
    void on_FileOpen_triggered();
    void on_FileSave_triggered();
    void on_FileSaveAs_triggered();
    void on_FileImport_triggered();
    void on_FileRemoteImport_triggered();
    void on_FileExport_triggered();
    void on_FileNewWindow_triggered();
    void on_Settings_triggered();
    void on_AnimationSettings_triggered();
    void on_RenderActiveViewport_triggered();
    void on_ClonePipeline_triggered();
    void on_RenamePipeline_triggered();
    void on_NewPipelineFileSource_triggered();
    void on_ConfigureViewportGraphics_triggered();

private:

    void setupCommandSearch();
};

}   // End of namespace
