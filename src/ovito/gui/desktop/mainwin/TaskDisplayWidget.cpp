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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ElidedTextLabel.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/app/Application.h>
#include "TaskDisplayWidget.h"

namespace Ovito {

/******************************************************************************
* Constructs the widget and associates it with the main window.
******************************************************************************/
TaskDisplayWidget::TaskDisplayWidget(MainWindow* mainWindow) : _mainWindow(mainWindow)
{
    setVisible(false);

    QHBoxLayout* progressWidgetLayout = new QHBoxLayout(this);
    progressWidgetLayout->setContentsMargins(10,0,0,0);
    progressWidgetLayout->setSpacing(0);
    _progressTextDisplay = new ElidedTextLabel(Qt::ElideLeft);
    _progressTextDisplay->setLineWidth(0);
    _progressTextDisplay->setAlignment(Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));
    _progressTextDisplay->setAutoFillBackground(true);
    _progressTextDisplay->setMargin(2);
    _progressTextDisplay->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored);
    progressWidgetLayout->addWidget(_progressTextDisplay);
    _progressBar = new QProgressBar(this);
    _progressBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    progressWidgetLayout->addWidget(_progressBar);
    progressWidgetLayout->addStrut(_progressTextDisplay->sizeHint().height());
    setMinimumHeight(_progressTextDisplay->minimumSizeHint().height());

    connect(&mainWindow->taskManager(), &TaskManager::taskRegistered, this, &TaskDisplayWidget::taskRegistered, Qt::QueuedConnection);
    connect(&Application::instance()->taskManager(), &TaskManager::taskRegistered, this, &TaskDisplayWidget::taskRegistered, Qt::QueuedConnection);
    connect(this, &QObject::destroyed, _progressTextDisplay, &QObject::deleteLater);
}

/******************************************************************************
* Is called when a new task has been registered with the TaskManager.
******************************************************************************/
void TaskDisplayWidget::taskRegistered(const TaskPtr& task)
{
    // Don't display the task if it is already finished.
    if(task->isFinished())
        return;

    // Create a task watcher object that will monitor the task's progress.
    TaskWatcher* taskWatcher = new TaskWatcher(this);

    // Self-destruction after task has finished.
    connect(taskWatcher, &TaskWatcher::finished, taskWatcher, &QObject::deleteLater);

    // Listen to task events.
    connect(taskWatcher, &TaskWatcher::started, this, &TaskDisplayWidget::taskStarted);
    connect(taskWatcher, &TaskWatcher::finished, this, &TaskDisplayWidget::taskFinished);
    connect(taskWatcher, &TaskWatcher::progressChanged, this, &TaskDisplayWidget::taskProgressChanged);
    connect(taskWatcher, &TaskWatcher::progressTextChanged, this, &TaskDisplayWidget::taskProgressChanged);

    // Start watching the task.
    taskWatcher->watch(task);
}

/******************************************************************************
* Is called when a task has started to run.
******************************************************************************/
void TaskDisplayWidget::taskStarted()
{
    // Show progress indicator only if the task doesn't finish within 200 milliseconds.
    if(isHidden()) {
        if(!_delayTimer.isActive())
            _delayTimer.start(200, Qt::CoarseTimer, this);
    }
    else {
        updateIndicator();
    }
}

/******************************************************************************
* Is called when a task has finished.
******************************************************************************/
void TaskDisplayWidget::taskFinished()
{
    updateIndicator();
}

/******************************************************************************
* Is called when the progress of a task has changed
******************************************************************************/
void TaskDisplayWidget::taskProgressChanged()
{
    updateIndicator();
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void TaskDisplayWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _delayTimer.timerId()) {
        OVITO_ASSERT(_delayTimer.isActive());
        _delayTimer.stop();
        updateIndicator();
    }
    QWidget::timerEvent(event);
}

/******************************************************************************
* Shows or hides the progress indicator widgets and updates the displayed information.
******************************************************************************/
void TaskDisplayWidget::updateIndicator()
{
    if(TaskWatcher* watcher = pickVisibleTask()) {
        if(!_delayTimer.isActive()) {
            qlonglong maximum = watcher->progressMaximum();
            if(maximum < (qlonglong)std::numeric_limits<int>::max()) {
                _progressBar->setRange(0, (int)maximum);
                _progressBar->setValue((int)watcher->progressValue());
            }
            else {
                _progressBar->setRange(0, 1000);
                _progressBar->setValue((int)(watcher->progressValue() * 1000ll / maximum));
            }
            _progressTextDisplay->setText(watcher->progressText());
            show();
        }
    }
    else {
        _delayTimer.stop();
        hide();
    }
}

/******************************************************************************
* From all currently running tasks, picks which one should be displayed in the status bar.
******************************************************************************/
TaskWatcher* TaskDisplayWidget::pickVisibleTask() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    auto taskWatchers = findChildren<TaskWatcher*>(Qt::FindDirectChildrenOnly);
#else
    auto taskWatchers = findChildren<TaskWatcher*>(QString{}, Qt::FindDirectChildrenOnly);
#endif

    TaskWatcher* selectedTask = nullptr;
    for(TaskWatcher* watcher : taskWatchers) {
        if(!watcher->task()->isFinished()) {
            if(watcher->progressMaximum() != 0)
                return watcher;
            else if(watcher->progressText().isEmpty() == false)
                selectedTask = watcher;
        }
    }
    return selectedTask;
}

}   // End of namespace
