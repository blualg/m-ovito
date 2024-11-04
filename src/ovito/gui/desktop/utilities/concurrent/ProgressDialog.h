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

namespace Ovito {

class OVITO_GUI_EXPORT ProgressDialog : public QDialog
{
    Q_OBJECT

public:

    /// Destructor.
    virtual ~ProgressDialog();

    /// Constructor.
    explicit ProgressDialog(TaskPtr task, detail::TaskDependency taskDependency, MainWindow& mainWindow, QWidget* parent, const QString& dialogTitle = QString());

    /// Constructor.
    explicit ProgressDialog(detail::TaskDependency taskDependency, MainWindow& mainWindow, const QString& dialogTitle = QString()) :
        ProgressDialog(TaskPtr{}, std::move(taskDependency), mainWindow, &mainWindow, dialogTitle) {}

    /// Creates a progress dialog for the currently running task.
    static void showForCurrentTask(MainWindow& mainWindow, QWidget* parent, const QString& dialogTitle = QString());

    /// Creates a progress dialog for the currently running task.
    static void showForCurrentTask(MainWindow& mainWindow, const QString& dialogTitle = QString()) {
        showForCurrentTask(mainWindow, &mainWindow, dialogTitle);
    }

    /// Creates a progress dialog for a future.
    static void showForFuture(FutureBase&& future, MainWindow& mainWindow, QWidget* parent, const QString& dialogTitle = QString()) {
        new ProgressDialog({}, future.takeTaskDependency(), mainWindow, parent, dialogTitle);
    }

    /// Creates a progress dialog for a future.
    static void showForFuture(FutureBase&& future, MainWindow& mainWindow, const QString& dialogTitle = QString()) {
        showForFuture(std::move(future), mainWindow, &mainWindow, dialogTitle);
    }

    /// Blocks the current thread (which must be the UI thread) until the given future completes.
    /// Returns the result of the future.
    template<typename FutureType>
    static auto blockForFuture(FutureType&& future, MainWindow& mainWindow, const QString& dialogTitle = QString()) {
        new ProgressDialog(future.task(), {}, mainWindow, &mainWindow, dialogTitle);
        if constexpr(!std::is_same_v<typename FutureType::result_type, void>)
            return std::move(future).blockForResult();
        else
            std::move(future).blockForResult();
    }

public:

    /// Runs the given function in the GUI thread once the awaited task has completed successfully.
    /// This may be immediately if the task has already completed.
    template<typename Function>
    void whenDone(Function&& function) {
        static_assert(std::is_invocable_r_v<void, Function>, "Function must be callable with no arguments.");
        static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");
        if(_isDone) {
            function();
        }
        else if(_task || _taskDependency) {
            connect(this, &ProgressDialog::accepted, this, std::forward<Function>(function));
        }
    }

protected:

    /// Is called when the dialog is shown.
    virtual void showEvent(QShowEvent* event) override;

    /// Is called when the user tries to close the dialog.
    virtual void reject() override;

private Q_SLOTS:

    /// Updates the displayed list of running tasks in the dialog.
    void updateTaskList();

private:

    /// The window this display widget is associated with.
    MainWindow& _mainWindow;

    /// The running task displayed in this dialog.
    TaskPtr _task;

    /// The dependency that keeps the task running.
    detail::TaskDependency _taskDependency;

    /// List of per-task display widgets.
    std::vector<std::pair<QLabel*, QProgressBar*>> _taskWidgets;

    /// Indicates that the task has already completed successfully.
    bool _isDone = false;
};

}   // End of namespace
