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
#include <ovito/core/app/UserInterface.h>
#include "Task.h"
#include "Future.h"
#include "AsynchronousTask.h"
#include "detail/TaskCallback.h"

namespace Ovito {

#ifdef OVITO_DEBUG
std::atomic_size_t Task::_globalTaskCounter{0};
#endif

#ifdef OVITO_DEBUG
/*******************************************************x***********************
* Destructor.
******************************************************************************/
Task::~Task()
{
    // No-op destructor in release builds.

    // Check if the mutex is currently locked.
    // This should never be the case while destroying the promise state.
    OVITO_ASSERT(_mutex.tryLock());
    _mutex.unlock();

    // At the end of their lifetime, tasks must always end up in the finished state.
    OVITO_ASSERT(isFinished());

    // All registered callbacks should have been unregistered by now.
    OVITO_ASSERT(_callbacks == nullptr);

    // In debug builds we keep track of how many task objects exist to check whether they all get destroyed correctly
    // at program termination.
    _globalTaskCounter.fetch_sub(1);
}
#endif

/******************************************************************************
* Switches the task into the 'finished' state.
******************************************************************************/
void Task::setFinished() noexcept
{
    MutexLocker locker(&taskMutex());
    if(!isFinished())
        finishLocked(locker);
}

/******************************************************************************
* Puts this task into the 'finished' state (without newly locking the task).
******************************************************************************/
void Task::finishLocked(MutexLocker& locker) noexcept
{
    OVITO_ASSERT(!isFinished());

    // Put this task into the 'finished' state.
    _state.fetch_or(Finished, std::memory_order_relaxed);

    // Make sure that the result has been set (if not in canceled or error state).
    OVITO_ASSERT_MSG(_exceptionStore || isCanceled() || _hasResultsStored.load() || !_resultsStorage,
        "Task::finishLocked()",
        "Result has not been set for the task. Please check program code setting the task to finished.");

    // Inform the registered callbacks.
    callCallbacks(Finished);

    // Note: Move the functions into a new local list first so that we can unlock the mutex.
    decltype(_continuations) continuations = std::move(_continuations);
    OVITO_ASSERT(_continuations.empty());
    locker.unlock();

    // Run all continuation functions.
    for(auto& cont : continuations)
        std::move(cont)();
}

/******************************************************************************
* Requests cancellation of the task.
******************************************************************************/
void Task::cancel() noexcept
{
    MutexLocker locker(&taskMutex());
    cancelAndFinishLocked(locker);
}

/******************************************************************************
* Puts this task into the 'canceled' and 'finished' states (without newly locking the task).
******************************************************************************/
void Task::cancelAndFinishLocked(MutexLocker& locker) noexcept
{
    // Put this task into the 'finished' state.
    auto state = _state.fetch_or(Finished, std::memory_order_relaxed);

    // Do nothing if task was already in the 'finished' state.
    if(state & Finished)
        return;

    // Put the task into the 'canceled' state as well.
    state = _state.fetch_or(Canceled, std::memory_order_relaxed);

    // Inform the registered callbacks.
    callCallbacks(!(state & Canceled) ? (Canceled | Finished) : Finished);

    // Note: Move the functions into a new local list first so that we can unlock the mutex.
    decltype(_continuations) continuations = std::move(_continuations);
    OVITO_ASSERT(_continuations.empty());
    locker.unlock();

    // Run the continuation functions.
    for(auto& cont : continuations)
        std::move(cont)();
}

/******************************************************************************
* Puts this task into the 'exception' state to signal that an error has occurred.
******************************************************************************/
void Task::exceptionLocked(std::exception_ptr ex) noexcept
{
    OVITO_ASSERT(ex != std::exception_ptr());

    // Make sure the task isn't already canceled or finished.
    OVITO_ASSERT(!(_state.load(std::memory_order_relaxed) & (Canceled | Finished)));

    _exceptionStore = std::move(ex);
}

/******************************************************************************
* Adds a callback to this task's list, which will get notified during state changes.
******************************************************************************/
void Task::addCallback(detail::TaskCallbackBase* cb, bool replayStateChanges) noexcept
{
    OVITO_ASSERT(cb != nullptr);

    const MutexLocker locker(&_mutex);

    // Insert into linked list of callbacks.
    cb->_nextInList = _callbacks;
    _callbacks = cb;

    // Replay past state changes to the new callback if requested.
    if(replayStateChanges) {
        if(!cb->callStateChanged(_state.load(std::memory_order_relaxed))) {
            // The callback requested to be removed from the list.
            _callbacks = cb->_nextInList;
        }
    }
}

/******************************************************************************
* Invokes the registered callback functions.
******************************************************************************/
void Task::callCallbacks(int state)
{
    detail::TaskCallbackBase** preceding = &_callbacks;
    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList) {
        if(!cb->callStateChanged(state)) {
            // The callback requested to be removed from the list.
            *preceding = cb->_nextInList;
        }
        else preceding = &cb->_nextInList;
    }
}

/******************************************************************************
* Removes a callback from this task's list, which will no longer get notified about state changes.
******************************************************************************/
void Task::removeCallback(detail::TaskCallbackBase* cb) noexcept
{
    const MutexLocker locker(&_mutex);

    // Remove from linked list of callbacks.
    if(_callbacks == cb) {
        _callbacks = cb->_nextInList;
    }
    else {
        for(detail::TaskCallbackBase* cb2 = _callbacks; cb2 != nullptr; cb2 = cb2->_nextInList) {
            if(cb2->_nextInList == cb) {
                cb2->_nextInList = cb->_nextInList;
                return;
            }
        }
        OVITO_ASSERT(false); // Callback was not found in linked list. Did you try to remove a callback that was never added?
    }
}

/******************************************************************************
* Blocks execution until another task finishes.
******************************************************************************/
bool Task::waitFor(detail::TaskReference awaitedTask, bool throwOnError)
{
    OVITO_ASSERT(awaitedTask);
    OVITO_ASSERT(ExecutionContext::current().isValid());

    // The task this function was called from.
    Task* waitingTask = this_task::get();
    OVITO_ASSERT_MSG(waitingTask != nullptr, "Task::waitFor()", "No active task. This function may only be called from a task worker function or some other context with an active task.");

    // Lock access to the waiting task (this function was called from).
    QMutexLocker waitingTaskLocker(&waitingTask->taskMutex());

    // No need to wait for the other task if the waiting task was already canceled.
    if(waitingTask->isCanceled())
        return false;

    // You should never invoke waitFor() from a task that has already finished!
    OVITO_ASSERT(!waitingTask->isFinished());

    // Quick check if the awaited task has already finished.
    QMutexLocker awaitedTaskLocker(&awaitedTask->taskMutex());
    if(awaitedTask->isFinished()) {
        if(awaitedTask->isCanceled()) {
            // If the awaited was canceled, cancel the waiting task as well.
            waitingTask->cancelAndFinishLocked(waitingTaskLocker);
            return false;
        }
        else {
            // It's ready, no need to wait.
            if(throwOnError)
                awaitedTask->throwPossibleException();
            return true;
        }
    }

    // Create shared pointers on the stack to make sure the two task objects don't get
    // destroyed during or right after the waiting phase and before we access them again below.
    TaskPtr waitingTaskPtr(waitingTask->shared_from_this());
    TaskPtr awaitedTaskPtr(awaitedTask.get());

    waitingTaskLocker.unlock();
    awaitedTaskLocker.unlock();

    // Is the waiting task running in a thread pool?
    if(waitingTask->isAsynchronousTask() && static_cast<AsynchronousTaskBase*>(waitingTask)->threadPool()) {
        // Are we really in a worker thread?
        OVITO_ASSERT(!ExecutionContext::isMainThread());

        // Work-stealing mechanism to avoid deadlock when running out of threads in this thread pool.
        AsynchronousTaskBase* asyncTask = awaitedTask->isAsynchronousTask() ? static_cast<AsynchronousTaskBase*>(awaitedTask.get().get()) : nullptr;
        if(asyncTask && asyncTask->threadPool() == static_cast<AsynchronousTaskBase*>(waitingTask)->threadPool() && asyncTask->threadPool()->tryTake(asyncTask)) {
            // The task was taken from the pool. Run it right away in the current thread.
            asyncTask->run();
        }
        else {
            // The task is already running in a thread pool. Wait for it to finish.
            QWaitCondition wc;
            QMutex waitMutex;
            std::atomic_bool alreadyDone{false};

            // Register a callback function with the waiting task, which sets the wait condition in case the waiting task gets canceled.
            detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) {
                if(state & (Task::Canceled | Task::Finished)) {
                    // When the parent task gets canceled, discard the reference which keeps the awaited task running.
                    awaitedTask.reset();
                    QMutexLocker locker(&waitMutex);
                    alreadyDone.store(true);
                    wc.wakeAll();
                }
                return true;
            });

            // Register a callback function with the awaited task, which sets the wait condition when the task gets canceled or finishes.
            detail::FunctionTaskCallback awaitedTaskCallback(awaitedTaskPtr.get(), [&](int state) {
                if(state & Task::Finished) {
                    QMutexLocker locker(&waitMutex);
                    alreadyDone.store(true);
                    wc.wakeAll();
                }
                return true;
            });

            waitMutex.lock();
            // Last minute check if the awaited task has already completed:
            if(!alreadyDone.load()) {
                // Wait now until one of the tasks are done.
                wc.wait(&waitMutex);
            }
            waitMutex.unlock();

            waitingTaskCallback.unregisterCallback();
            awaitedTaskCallback.unregisterCallback();
        }
    }
    else {
        // Process all pending work items while waiting for the task to finish.
        ExecutionContext::current().ui().taskManager().processWorkWhileWaiting(waitingTask, awaitedTask);
    }

    // Check if the waiting task has been canceled.
    waitingTaskLocker.relock();
    if(waitingTask->isCanceled())
        return false;

    // Now check if the awaited task has been canceled.
    awaitedTaskLocker.relock();

    // When the waiting loop was interrupted because the application is shutting down, cancel
    // all tasks immediately.
    if(ExecutionContext::current().ui().isShuttingDown())
       awaitedTaskPtr->cancelAndFinishLocked(awaitedTaskLocker);

    if(awaitedTaskPtr->isCanceled()) {
        // If the awaited task was canceled, cancel the waiting task as well.
        waitingTask->cancelAndFinishLocked(waitingTaskLocker);
        return false;
    }

    OVITO_ASSERT(awaitedTaskPtr->isFinished());

    if(throwOnError)
        awaitedTaskPtr->throwPossibleException();

    return true;
}

namespace this_task {

/*******************************************************x***********************
* Returns the task object that is the active one in the current thread.
******************************************************************************/
Task*& get() noexcept
{
    // The active task in the current thread.
    static thread_local Task* _current = nullptr;

    return _current;
}

/*******************************************************x***********************
* Changes the description string of the active task.
******************************************************************************/
void setProgressText(const QString& progressText)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->setProgressText(progressText);
    }
}

/*******************************************************x***********************
* Sets the current maximum value for progress reporting.
******************************************************************************/
void setProgressMaximum(qlonglong maximum, bool autoReset)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->setProgressMaximum(maximum, autoReset);
    }
}

/*******************************************************x***********************
* Sets the current progress value of the task.
******************************************************************************/
void setProgressValue(qlonglong progressValue)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->setProgressValue(progressValue);
    }
    else {
        if(task->isCanceled())
            throw OperationCanceled();
    }
}

/*******************************************************x***********************
* Increments the progress value of the task.
******************************************************************************/
void incrementProgressValue(qlonglong increment)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->incrementProgressValue(increment);
    }
    else {
        if(task->isCanceled())
            throw OperationCanceled();
    }
}

/*******************************************************x***********************
* Sets the current progress value of the task, generating update events only occasionally.
******************************************************************************/
void setProgressValueIntermittent(qlonglong progressValue, int updateEvery)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->setProgressValueIntermittent(progressValue, updateEvery);
    }
    else {
        if(task->isCanceled())
            throw OperationCanceled();
    }
}

/*******************************************************x***********************
* Starts a sequence of sub-steps in the progress range of this task.
******************************************************************************/
void beginProgressSubStepsWithWeights(std::vector<int> weights)
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->beginProgressSubStepsWithWeights(std::move(weights));
    }
}

/*******************************************************x***********************
* Completes the current sub-step in the sequence started with beginProgressSubSteps() or
* beginProgressSubStepsWithWeights() and moves to the next one.
******************************************************************************/
void nextProgressSubStep()
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->nextProgressSubStep();
    }
}

/*******************************************************x***********************
* Completes a sub-step sequence started with beginProgressSubSteps() or beginProgressSubStepsWithWeights().
******************************************************************************/
void endProgressSubSteps()
{
    Task* task = get();
    OVITO_ASSERT(task != nullptr);
    if(task->isProgressingTask()) {
        static_cast<ProgressingTask*>(task)->endProgressSubSteps();
    }
}

} // End of namespace

} // End of namespace
