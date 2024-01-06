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

#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include <ovito/core/oo/ObjectExecutor.h>
#include <ovito/core/dataset/data/RegisteredBufferAccess.h>

namespace Ovito {

/******************************************************************************
* Initializes the task manager.
******************************************************************************/
TaskManager::TaskManager(UserInterface* ui) : _ui(ui)
#ifdef OVITO_USE_SYCL
    : _syclQueue(SYCL_NS::default_selector())
#endif
{
    qRegisterMetaType<TaskPtr>("TaskPtr");

    // Execute pending work in the main thread when control returns to the Qt event loop.
    connect(this, &TaskManager::pendingWorkArrived, this, &TaskManager::executePendingWork, Qt::QueuedConnection);
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
TaskManager::~TaskManager()
{
    OVITO_ASSERT(isShuttingDown());
    OVITO_ASSERT(_registeredTasks.empty());
}
#endif

/******************************************************************************
* Registers a future's promise with the progress manager, which will display
* the progress of the background task in the main window.
******************************************************************************/
void TaskManager::registerFuture(const FutureBase& future)
{
    OVITO_ASSERT(future.isValid());
    registerTask(*future.task());
}

/******************************************************************************
* Registers a promise with the progress manager, which will display
* the progress of the background task in the main window.
******************************************************************************/
void TaskManager::registerPromise(const PromiseBase& promise)
{
    OVITO_ASSERT(promise.isValid());
    registerTask(*promise.task());
}

/******************************************************************************
* Registers an asynchronous task with the task manager.
******************************************************************************/
void TaskManager::registerTask(Task& task)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // Remove dead task pointers from the list.
    _registeredTasks.erase(std::remove_if(_registeredTasks.begin(), _registeredTasks.end(), [](const std::weak_ptr<Task>& weakPtr) {
        return weakPtr.expired();
    }), _registeredTasks.end());

#ifdef OVITO_DEBUG
    // Make sure we don't register the same task twice.
    for(const std::weak_ptr<Task>& weakPtr : _registeredTasks) {
        OVITO_ASSERT(weakPtr.lock().get() != &task);
    }
#endif

    // Abort and reject any new tasks when application is shutting down.
    if(isShuttingDown()) {
        task.cancel();
        return;
    }

    // Append the new task to the list.
    _registeredTasks.push_back(task.weak_from_this());

    // Enable logging for the task if requested.
    if(consoleLoggingEnabled())
        task.setLoggingEnabled();

    // Inform listeners about the new task.
    Q_EMIT taskRegistered(task.shared_from_this());
}

/******************************************************************************
* Cancels all running tasks and waits for them to finish.
******************************************************************************/
void TaskManager::shutdown()
{
    OVITO_ASSERT(ExecutionContext::isMainThread());
    OVITO_ASSERT(ExecutionContext::current().isValid());

    // To prevent queuing up more work while we are shutting down.
    std::unique_lock<std::mutex> lock(_mutex);

    // Set flag to indicate we are shutting down.
    OVITO_ASSERT(!isShuttingDown());
    _isShuttingDown = true;

    // Cancel all registered tasks.
    for(const std::weak_ptr<Task>& weakPtr : _registeredTasks) {
        if(TaskPtr task = weakPtr.lock())
            task->cancel();
    }
    _registeredTasks.clear();

    // Process all remaining work items from the queue.
    executePendingWorkLocked(lock);
    OVITO_ASSERT(_pendingWork.empty());
    OVITO_ASSERT(_registeredTasks.empty());

    // Wait until all threads did terminate. That's because canceled asynchronous tasks
    // may still be running in threads until they notice they have been canceled.
    bool result = QThreadPool::globalInstance()->waitForDone();
    OVITO_ASSERT(result);

    // Shuts down the SYCL queue.
#ifdef OVITO_USE_SYCL
    // Close all active SYCL host memory accessors which are associated with NumPy views of properties.
    for(RegisteredBufferAccess* accessor = _registeredBufferAccessors; accessor != nullptr; accessor = accessor->_next) {
        accessor->_syclAccessor = {};
    }

    // Wait for completion of all enqueued tasks in the SYCL queue.
    _syclQueue.wait();
#endif
}

/******************************************************************************
* Executes the given function at some later time unless the given object is
* destroyed in the meantime or the user interface is shut down.
******************************************************************************/
void TaskManager::submitWork(const OvitoObject* contextObject, fu2::unique_function<void() noexcept> function, bool isScriptingContext)
{
    OVITO_ASSERT(contextObject);
    std::lock_guard<std::mutex> lock(_mutex);

    if(isShuttingDown())
        return;

    // Place work item into the queue.
    _pendingWork.emplace(contextObject, std::move(function), isScriptingContext);

    // If the queue became non-empty, notify listeners that work is waiting.
    if(_pendingWork.size() == 1) {
        _pendingWorkCondition.notify_one();

        if(QCoreApplication::instance())
            Q_EMIT pendingWorkArrived();
    }
}

/******************************************************************************
* Executes pending work items waiting in the queue.
******************************************************************************/
void TaskManager::executePendingWorkLocked(std::unique_lock<std::mutex>& lock)
{
    // Check that we are really in the main thread here.
    OVITO_ASSERT(ExecutionContext::isMainThread());

    while(!_pendingWork.empty()) {
        // Grab the next work item from the queue.
        Work work = std::move(_pendingWork.front());
        _pendingWork.pop();
        lock.unlock();

        // Execute work item only if the context object still exists and the user interface is not shutting down.
        // Otherwise, silently cancel the work (which still runs the destructor of the work object).
        if(!isShuttingDown()) {
            if(auto contextObject = work.obj.lock()) {
                // Establish the execution context in which the work was submitted.
                ExecutionContext::Scope execScope(work.isScriptingContext ? ExecutionContext::Type::Scripting : ExecutionContext::Type::Interactive, _ui->shared_from_this());

                // No active task while executing the work.
                Task::Scope taskScope(nullptr);

                // Undo recording may still be active if the GUI is currently performing an extended user operation (e.g. Animation Settings dialog may be open).
                // While the asynchronous work is being performed, undo recording should be suspended.
                UndoSuspender noUndo;

                // Execute the work function.
                std::move(work.function)();
            }
        }

        // Continue by grabbing the next work item from the queue.
        lock.lock();
    }
}

/******************************************************************************
* Keeps executing pending work items until quitWorkProcessingLoop() is called
* or the awaited task has finished.
******************************************************************************/
void TaskManager::processWorkWhileWaiting(Task* waitingTask, detail::TaskReference& awaitedTask)
{
    // This method must only be called from the main thread.
    OVITO_ASSERT(ExecutionContext::isMainThread());

    TaskPtr awaitedTaskPtr(awaitedTask.get());
    std::unique_lock<std::mutex> lock(_mutex);
    bool wasWaitingForTask = _isWaitingForTask;
    _isWaitingForTask = true;
    bool quitFlag = false;

    // Register a callback function with the awaited task, which terminates the processing loop when the task gets canceled or finishes.
    detail::FunctionTaskCallback awaitedTaskCallback(awaitedTask.get().get(), [&](int state) {
        if(state & Task::Finished) {
            quitWorkProcessingLoop(quitFlag);
        }
        return true;
    });

    // Register a callback function with the waiting task, which terminates the processing loop in case the waiting task gets canceled.
    detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) {
        if(state & (Task::Canceled | Task::Finished)) {
            // When the parent task gets canceled, discard the reference which keeps the awaited task running.
            awaitedTask.reset();
            quitWorkProcessingLoop(quitFlag);
        }
        return true;
    });

    std::optional<QEventLoop> eventLoop;
    QEventLoop* previousEventLoop = _localEventLoop;

    for(;;) {
        // Time to quit the loop because the application is shutting down?
        if(isShuttingDown()) {
            // Cancel the waiting task. This will also (likely) cancel the awaited task.
            lock.unlock();
            waitingTask->cancel();
            lock.lock();
            break;
        }

        // Time to quit the loop because an interrupt was requested?
        if(quitFlag || _interruptProcessingLoop)
            break;

        // Create a local event loop if a Qt application has been set up.
        if(QCoreApplication::instance() && !eventLoop) {
            eventLoop.emplace();
            _localEventLoop = &eventLoop.value();
        }

        // Is the Qt main event loop running? If yes, start a local Qt event loop, which processes both Qt events and pending work items.
        // Otherwise, in a non-GUI environment, enter into our own processing loop, which only processes pending work items of this task manager.
        if(eventLoop) {
            // Temporarily switch back to a null context while in the event loop.
            ExecutionContext::Scope execScope(ExecutionContext{});
            // Also switch back to the null task.
            Task::Scope taskScope(nullptr);
            // Also suspend undo recording while in the event loop.
            UndoSuspender noUndo;

            // Enter the local Qt event loop.
            lock.unlock();
            eventLoop->exec();
            lock.lock();
            break;
        }
        else {
            // Block until new work arrives in the queue or quitWorkProcessingLoop() is called.
            _pendingWorkCondition.wait(lock, [&]{
                return !_pendingWork.empty() || quitFlag || _interruptProcessingLoop || isShuttingDown();
            });

            // Time to quit the loop because we are done?
            if(quitFlag || _interruptProcessingLoop)
                break;

            // Process newly arrived items in the work queue until the queue is drained.
            executePendingWorkLocked(lock);
        }
    }

    // Detach callbacks from the two task objects.
    waitingTaskCallback.unregisterCallback();
    awaitedTaskCallback.unregisterCallback();
    _localEventLoop = previousEventLoop;
    _interruptProcessingLoop = false;
    _isWaitingForTask = wasWaitingForTask;

    // Cancel the awaited task (unless it has already finished successfully).
    lock.unlock();
    awaitedTaskPtr->cancel();
}

/******************************************************************************
* Stops executing pending work items and makes processWorkWhileWaiting() return.
******************************************************************************/
void TaskManager::quitWorkProcessingLoop(bool& quitFlag)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if(_isWaitingForTask) {
        quitFlag = true;
        if(_localEventLoop) {
            _localEventLoop->quit();
        }
        else {
            _pendingWorkCondition.notify_all();
        }
    }
}

/******************************************************************************
* Tells the task manager to interrupt the task it is currently waiting for.
******************************************************************************/
void TaskManager::requestInterruption()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if(_isWaitingForTask) {
        _interruptProcessingLoop = true;
        if(_localEventLoop) {
            _localEventLoop->quit();
        }
        else {
            _pendingWorkCondition.notify_all();
        }
    }
}

}   // End of namespace
