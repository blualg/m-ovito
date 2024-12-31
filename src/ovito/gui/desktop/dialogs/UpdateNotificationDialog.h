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

#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * A application-wise service that informs the user about new program updates when they become available.
 */
class OVITO_GUI_EXPORT UpdateNotificationService : public QObject, public GuiApplicationService
{
    Q_OBJECT
    OVITO_CLASS(UpdateNotificationService)

public:

    /// Is called by the system during standalone application startup.
    /// Downloads the news page from the web server and displays it in the command panel.
    void applicationStarting() override;

    /// Is called when a new main window is created.
    /// Stores a reference to the main window.
    void registerActions(ActionManager& actionManager, MainWindow& mainWindow) override;

private:

    /// Extracts the two version strings from the first line of the news webpage.
    /// Pattern of the version strings: <!--vX+.Y+.Z+|vA+.B+.C+-->
    /// where X+.Y+.Z+ are the significant version even shown when "Skip this version" is pressed
    /// and A+.B+.C+ are all program versions shown to every user.
    static QStringList extractVersion(const QString& input);

    /// Called when the web request finishes.
    /// Show the update dialog and set the "ProgramNotice".
    void onWebRequestFinished();

    /// Creates the update popup window.
    /// Checks the newly available version and compares it against the current version
    /// and validates against the "Skip this version" choice by the user.
    void createUpdateDialog(const QStringList& versionMatch) const;

private:

    /// Pointer to the current main window
    QPointer<MainWindow> _mainWindow;
};

}  // namespace Ovito