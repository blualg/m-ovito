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
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/oo/ObjectExecutor.h>

namespace Ovito {

/******************************************************************************
* Initializes the task manager.
******************************************************************************/
TaskManager::TaskManager()
#ifdef OVITO_USE_SYCL
    : _syclQueue(cl::sycl::default_selector())
#endif
{
    qRegisterMetaType<TaskPtr>("TaskPtr");
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
TaskManager::~TaskManager()
{
    for(TaskWatcher* watcher : runningTasks()) {
        OVITO_ASSERT_MSG(watcher->task()->isFinished() || watcher->isCanceled(), "TaskManager destructor",
                        "Some tasks are still in progress while destroying the TaskManager.");
    }
}
#endif

/******************************************************************************
* Registers a future's promise with the progress manager, which will display
* the progress of the background task in the main window.
******************************************************************************/
void TaskManager::registerFuture(const FutureBase& future)
{
    registerTask(future.task());
}

/******************************************************************************
* Registers a promise with the progress manager, which will display
* the progress of the background task in the main window.
******************************************************************************/
void TaskManager::registerPromise(const PromiseBase& promise)
{
    registerTask(promise.task());
}

/******************************************************************************
* Registers an asynchronous task with the task manager.
******************************************************************************/
void TaskManager::registerTask(const TaskPtr& task)
{
    // Execute the function call in the main thread.
    QMetaObject::invokeMethod(this, "addTaskInternal", Q_ARG(TaskPtr, task));
}

/******************************************************************************
* Registers an asynchronous task with the task manager.
******************************************************************************/
void TaskManager::registerTask(Task& task)
{
    // Execute the function call in the main thread.
    QMetaObject::invokeMethod(this, "addTaskInternal", Q_ARG(TaskPtr, task.shared_from_this()));
}

/******************************************************************************
* Registers a promise with the task manager.
******************************************************************************/
void TaskManager::addTaskInternal(const TaskPtr& task)
{
    OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());

    // Abort all new tasks while application shutting down.
    if(isShuttingDown()) {
        task->cancel();
        return;
    }

    // Check if the task has already been registered before.
    // In this case, a TaskWatcher must exist for the task that has been added as a child object to the TaskManager.
    for(QObject* childObject : children()) {
        if(TaskWatcher* watcher = qobject_cast<TaskWatcher*>(childObject)) {
            if(watcher->task() == task) {
                return;
            }
        }
    }

    // Create a task watcher, which will generate start/stop notification signals.
    TaskWatcher* watcher = new TaskWatcher(this);
    connect(watcher, &TaskWatcher::started, this, &TaskManager::taskStartedInternal);
    connect(watcher, &TaskWatcher::finished, this, &TaskManager::taskFinishedInternal);

    // Activate the watcher.
    watcher->watch(task);
}

/******************************************************************************
* Returns the watchers for all currently registered tasks.
******************************************************************************/
QList<TaskWatcher*> TaskManager::registeredTasks() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    return findChildren<TaskWatcher*>(Qt::FindDirectChildrenOnly);
#else
    return findChildren<TaskWatcher*>(QString{}, Qt::FindDirectChildrenOnly);
#endif
}

/******************************************************************************
* Enables or disables printing of task status messages to the console for
* this task manager.
******************************************************************************/
void TaskManager::setConsoleLoggingEnabled(bool enabled)
{
    if(_consoleLoggingEnabled != enabled) {
        _consoleLoggingEnabled = enabled;
        if(enabled) {
            // Foward status messages from the active tasks to the console if logging was enabled.
            for(TaskWatcher* watcher : runningTasks()) {
                connect(watcher, &TaskWatcher::progressTextChanged, this, &TaskManager::taskProgressTextChangedInternal);
            }
        }
    }
}

/******************************************************************************
* Is called when a task has reported a new progress text (only if logging is enabled).
******************************************************************************/
void TaskManager::taskProgressTextChangedInternal(const QString& msg)
{
    if(!msg.isEmpty())
        qInfo().noquote() << "OVITO:" << msg;
}

/******************************************************************************
* Is called when a task has started to run.
******************************************************************************/
void TaskManager::taskStartedInternal()
{
    TaskWatcher* watcher = static_cast<TaskWatcher*>(sender());
    _runningTasks.push_back(watcher);

    // Foward status messages from the task to the console if logging is enabled.
    if(_consoleLoggingEnabled)
        connect(watcher, &TaskWatcher::progressTextChanged, this, &TaskManager::taskProgressTextChangedInternal);

    Q_EMIT taskStarted(watcher);
}

/******************************************************************************
* Is called when a task has finished.
******************************************************************************/
void TaskManager::taskFinishedInternal()
{
    TaskWatcher* watcher = static_cast<TaskWatcher*>(sender());

    if(auto iter = std::find(_runningTasks.begin(), _runningTasks.end(), watcher); iter != _runningTasks.end()) {
        _runningTasks.erase(iter);
    }

    Q_EMIT taskFinished(watcher);

    watcher->reset();
    watcher->deleteLater();

    if(runningTasks().empty()) {
        Q_EMIT allTasksFinished();
    }
}

/******************************************************************************
* Cancels all running tasks and waits for them to finish.
******************************************************************************/
void TaskManager::shutdown()
{
    OVITO_ASSERT(QThread::currentThread() == this->thread());
    OVITO_ASSERT(!_isShuttingDown);

    _isShuttingDown = true;

    for(TaskWatcher* watcher : registeredTasks()) {
        if(watcher->task()) {
            watcher->task()->cancel();
        }
    }

#ifdef OVITO_USE_SYCL
    // Wait for completion of all enqueued tasks in the SYCL queue.
    _syclQueue.wait();
#endif

    // Block with a local event until all running tasks have completed.
    if(!runningTasks().empty()) {
        QEventLoop eventLoop;
        connect(this, &TaskManager::allTasksFinished, &eventLoop, &QEventLoop::quit);

        // Temporarily switch to a null context while in the event loop.
        ExecutionContext::Scope execScope(ExecutionContext{});
        // Also switch back to the null task.
        Task::Scope taskScope(nullptr);

        OVITO_ASSERT(!runningTasks().empty());
        eventLoop.exec();
    }
    OVITO_ASSERT(runningTasks().empty());

    // Wait until all threads did terminate. That's because canceled asynchronous tasks
    // may still be running in threads until they notice they have been canceled.
    bool result = QThreadPool::globalInstance()->waitForDone();
    OVITO_ASSERT(result);

    // Execute all remaining deferred tasks which have not been registered with this task manager.
    QCoreApplication::sendPostedEvents(nullptr, ObjectExecutor::workEventType());
}

}   // End of namespace
