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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>

namespace Ovito {

class OVITO_GUI_EXPORT AsyncProgressDialog : public QDialog
{
public:

    /// Constructor.
    explicit AsyncProgressDialog(FutureBase future, MainWindow& mainWindow, QWidget* parent, const QString& dialogTitle = QString());

    /// Constructor.
    explicit AsyncProgressDialog(FutureBase future, MainWindow& mainWindow, const QString& dialogTitle = QString()) :
        AsyncProgressDialog(std::move(future), mainWindow, &mainWindow, dialogTitle) {}

protected:

    /// Is called when the dialog is shown.
    virtual void showEvent(QShowEvent* event) override;

    /// Is called when the user tries to close the dialog.
    virtual void closeEvent(QCloseEvent* event) override;

    /// Is called when the user tries to close the dialog.
    virtual void reject() override;

private Q_SLOTS:

    /// Updates the displayed list of running tasks in the dialog.
    void updateTaskList();

private:

    /// The window this display widget is associated with.
    MainWindow& _mainWindow;

    /// The running task displayed in this dialog.
    FutureBase _future;

    /// List of per-task display widgets.
    std::vector<std::pair<QLabel*, QProgressBar*>> _taskWidgets;
};

}   // End of namespace
