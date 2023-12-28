////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "TaskWatcher.h"
#include "Task.h"
#include "ProgressingTask.h"

namespace Ovito {

void TaskWatcher::watch(Task* task, bool pendingAssignment)
{
    OVITO_ASSERT_MSG(QThread::currentThread() == this->thread(), "TaskWatcher::watch", "Function may only be called from the thread the TaskWatcher belongs to.");
    OVITO_ASSERT(QCoreApplication::instance() != nullptr);

    if(task == _task.get())
        return;

    if(isRegistered())
        unregisterCallback();

    if(_task) {
        // This is to prevent notifications from the old task still waiting in the event loop
        // to reach the TaskWatcher after the new task has been assigned.
        if(pendingAssignment) {
            _finished = false;
            QCoreApplication::removePostedEvents(this);
        }
    }
    if(task) {
        _task = task->shared_from_this();
        registerCallback(_task.get(), true); // Request replay of state changes of the running task.
    }
    else _task.reset();
}

/// Cancels the operation being watched by this watcher.
void TaskWatcher::cancel()
{
    if(isWatching())
        task()->cancel();
}

bool TaskWatcher::taskStateChangedCallback(int state)
{
    if(state & Task::Started)
        QMetaObject::invokeMethod(this, "taskStarted", Qt::QueuedConnection);
    if(state & Task::Canceled)
        QMetaObject::invokeMethod(this, "taskCanceled", Qt::QueuedConnection);
    if(state & Task::Finished)
        QMetaObject::invokeMethod(this, "taskFinished", Qt::QueuedConnection);
    return true;
}

void TaskWatcher::taskProgressChangedCallback(qlonglong progress, qlonglong maximum)
{
    QMetaObject::invokeMethod(this, "taskProgressChanged", Qt::QueuedConnection, Q_ARG(qlonglong, progress), Q_ARG(qlonglong, maximum));
}

void TaskWatcher::taskTextChangedCallback()
{
    QMetaObject::invokeMethod(this, "taskTextChanged", Qt::QueuedConnection);
}

void TaskWatcher::taskFinished()
{
    if(isWatching()) {
        _finished = true;
        Q_EMIT finished();
    }
}

void TaskWatcher::taskCanceled()
{
    if(isWatching()) {
        Q_EMIT canceled();
    }
}

void TaskWatcher::taskStarted()
{
    if(isWatching()) {
        _finished = false; // Need to reset interal finished flag, in case task is run a second time.
        Q_EMIT started();
    }
}

void TaskWatcher::taskProgressChanged(qlonglong progress, qlonglong maximum)
{
    if(isWatching() && !task()->isCanceled())
        Q_EMIT progressChanged(progress, maximum);
}

void TaskWatcher::taskTextChanged()
{
    if(isWatching() && !task()->isCanceled())
        Q_EMIT progressTextChanged(static_cast<ProgressingTask*>(task().get())->progressText());
}

bool TaskWatcher::isCanceled() const
{
    return isWatching() ? task()->isCanceled() : false;
}

bool TaskWatcher::isFinished() const
{
    return _finished;
}

qlonglong TaskWatcher::progressMaximum() const
{
    return (isWatching() && task()->isProgressingTask()) ? static_cast<ProgressingTask*>(task().get())->progressMaximum() : 0;
}

qlonglong TaskWatcher::progressValue() const
{
    return (isWatching() && task()->isProgressingTask()) ? static_cast<ProgressingTask*>(task().get())->progressValue() : 0;
}

QString TaskWatcher::progressText() const
{
    return (isWatching() && task()->isProgressingTask()) ? static_cast<ProgressingTask*>(task().get())->progressText() : QString();
}

}   // End of namespace
