////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * This dialog box lets the user configure the settings of the interactive viewport rendering backend.
 */
class ConfigureViewportGraphicsDialog : public QDockWidget, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    explicit ConfigureViewportGraphicsDialog(MainWindowUI& ui, QWidget* parentWindow);

private Q_SLOTS:

    /// Updates the values displayed in the dialog.
    void updateGUI();

    /// Is called when the user selects a different rendering backend.
    void backendSelectionChanged(QAbstractButton* option, bool checked);

protected:

    /// Is called when the dialog window is being closed by the user.
    virtual void closeEvent(QCloseEvent* event) override;

private:

    /// Recreates all viewport windows in the application. This should
    /// be called whenever a different rendering backend has been activated.
    void recreateViewportWindows();

private:

    QButtonGroup* _backendSelectionGroup; /// Group of radio buttons for selecting the rendering backend.
    QStackedWidget* _backendSettingsStack; /// Hosts the settings widgets for the different rendering backends.
    std::map<QString, int> _backendSettingsMap; /// Maps backend identifiers to the stack index of the corresponding settings widget.
};

}   // End of namespace
