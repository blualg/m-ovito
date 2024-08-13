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
    OVITO_ASSERT(_mutex.try_lock());
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
    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished))
        finishLocked(lock);
}

/******************************************************************************
* Puts this task into the 'finished' state (without newly locking the task).
******************************************************************************/
void Task::finishLocked(MutexLock& lock) noexcept
{
    // Put this task into the 'finished' state.
    auto state = _state.fetch_or(Finished, std::memory_order_seq_cst);

    // Make sure that the result has been set (if not in canceled or error state).
    OVITO_ASSERT_MSG(_exceptionStore || isCanceled() || _hasResultsStored.load() || !_resultsStorage,
        "Task::finishLocked()",
        "Result has not been set for the task. Please check program code setting the task to finished.");

    if(!(state & Finished)) {
        // Inform the registered callbacks.
        callCallbacks(Finished, lock);

        // Note: Move the functions into a new local list first so that we can unlock the mutex.
        decltype(_continuations) continuations = std::move(_continuations);
        OVITO_ASSERT(_continuations.empty());
        lock.unlock();

        // Run all continuation functions.
        for(auto& cont : continuations)
            std::move(cont)();
    }
}

/******************************************************************************
* Requests cancellation of the task.
******************************************************************************/
void Task::cancel() noexcept
{
    if(!isFinished()) {
        MutexLock lock(*this);
        cancelLocked(lock);

        // If this is a synchronous task, finish it right away.
        // If it is an asynchronous task, it will be finished automatically once the task worker function returns.
        if(!isAsynchronousTask()) {
            finishLocked(lock);
        }
    }
}

/******************************************************************************
* If the task is not finished yet, cancel and finish it.
******************************************************************************/
void Task::cancelAndFinish() noexcept
{
    if(!isFinished()) {
        MutexLock lock(*this);
        cancelLocked(lock);
        finishLocked(lock);
    }
}

/******************************************************************************
* Puts this task into the 'canceled' and 'finished' states (without newly locking the task).
******************************************************************************/
void Task::cancelLocked(MutexLock& lock) noexcept
{
    // Make sure the task isn't already finished.
    if(_state.load(std::memory_order_relaxed) & Finished)
        return;

    // Set the canceled flag.
    auto state = _state.fetch_or(Canceled, std::memory_order_seq_cst);

    // Inform the registered callbacks that this task got canceled.
    if(!(state & Canceled)) {
        callCallbacks(Canceled, lock);
    }
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

    MutexLock lock(*this);

    // Insert into linked list of callbacks.
    cb->_nextInList = _callbacks;
    _callbacks = cb;

    // Replay past state changes to the new callback if requested.
    if(replayStateChanges) {
        if(!cb->callStateChanged(_state.load(std::memory_order_relaxed), lock)) {
            // The callback requested to be removed from the list.
            _callbacks = cb->_nextInList;
        }
    }
}

/******************************************************************************
* Invokes the registered callback functions.
******************************************************************************/
void Task::callCallbacks(int state, MutexLock& lock) noexcept
{
    detail::TaskCallbackBase** preceding = &_callbacks;
    for(detail::TaskCallbackBase* cb = _callbacks; cb != nullptr; cb = cb->_nextInList) {
        if(!cb->callStateChanged(state, lock)) {
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
    const MutexLock lock(*this);

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

/*******************************************************x***********************
* Changes the UI description string of the current operation.
******************************************************************************/
void Task::setProgressText(const QString& progressText)
{
    MutexLock lock(*this);
    auto flags = _state.load(std::memory_order_relaxed);
    if(!(flags & (Finished | Canceled))) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressText(*this, progressText);
    }

    // Note: setProgressText() should not yield control to the UI event loop,
    // because it is called in places that cannot tolerate a context switch.
}

/******************************************************************************
* Sets the current maximum value for progress reporting.
* The current progress value is reset to zero unless autoReset is false.
******************************************************************************/
void Task::setProgressMaximum(qlonglong maximum, bool autoReset)
{
    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished)) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressMaximum(*this, maximum, autoReset);
    }
}

/******************************************************************************
* Sets the current progress value of the task.
******************************************************************************/
void Task::setProgressValue(qlonglong value)
{
    UserInterface& ui = ExecutionContext::current().ui();

    MutexLock lock(*this);
    auto flags = _state.load(std::memory_order_relaxed);
    if(flags & Canceled)
        throw OperationCanceled();
    if(!(flags & (Finished | Canceled))) {
        // Notify UI about progress status change.
        ui.taskProgressValue(*this, value);
    }

    // When in the main thread, temporarily yield control back to the event loop to process UI events and
    // keep the UI responsive during long-running tasks.
    if((flags & YieldUI) && !(flags & IsAsynchronous) && ExecutionContext::isMainThread()) {
        lock.unlock();
        if(ui.processUIEvents()) {
            cancel();
        }
    }
}

/******************************************************************************
* Increments the progress value of the task.
******************************************************************************/
void Task::incrementProgressValue(qlonglong increment)
{
    MutexLock lock(*this);
    if((_state.load(std::memory_order_relaxed) & Canceled))
        throw OperationCanceled();
    if(!(_state.load(std::memory_order_relaxed) & Finished)) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressIncrementValue(*this, increment);
    }
}

/******************************************************************************
* Sets the current progress value of the task, generating update events only occasionally.
******************************************************************************/
void Task::setProgressValueIntermittent(qlonglong progressValue, int updateEvery)
{
    if(Q_UNLIKELY((progressValue % updateEvery) == 0))
        setProgressValue(progressValue);
    else if(isCanceled())
        throw OperationCanceled();
}

/******************************************************************************
* Starts a sequence of sub-steps in the progress range of this task.
******************************************************************************/
void Task::beginProgressSubStepsWithWeights(std::vector<int> weights)
{
    OVITO_ASSERT(std::accumulate(weights.cbegin(), weights.cend(), 0) > 0);

    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished)) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressBeginSubStepsWithWeights(*this, std::move(weights));
    }
}

/******************************************************************************
* Completes the current sub-step in the sequence started with beginProgressSubSteps()
* or beginProgressSubStepsWithWeights() and moves to the next one.
******************************************************************************/
void Task::nextProgressSubStep()
{
    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished)) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressNextSubStep(*this);
    }
}

/******************************************************************************
* Completes a sub-step sequence started with beginProgressSubSteps() or
* beginProgressSubStepsWithWeights().
******************************************************************************/
void Task::endProgressSubSteps()
{
    MutexLock lock(*this);
    if(!(_state.load(std::memory_order_relaxed) & Finished)) {
        // Notify UI about progress status change.
        UserInterface& ui = ExecutionContext::current().ui();
        ui.taskProgressEndSubSteps(*this);
    }
}

/******************************************************************************
* Blocks execution until another task finishes.
******************************************************************************/
bool Task::waitFor(detail::TaskDependency awaitedTask, bool throwOnError, bool returnEarlyIfCanceled, bool cancelWaitingIfAwaitedCanceled)
{
    OVITO_ASSERT(awaitedTask);
    OVITO_ASSERT(ExecutionContext::current().isValid());

    // The task this function was called from.
    Task* waitingTask = this_task::get();
    OVITO_ASSERT_MSG(waitingTask != nullptr, "Task::waitFor()", "No active task. This function may only be called from a context having an active task.");

    // Lock access to the waiting task this function was called by.
    MutexLock waitingTaskLock(*waitingTask);

    // No need to wait for the other task if the waiting task was already canceled.
    if(waitingTask->isCanceled())
        return false;

    // You should never invoke waitFor() from a task that has already finished!
    OVITO_ASSERT(!waitingTask->isFinished());

    // Quick check if the awaited task is already finished or canceled.
    MutexLock awaitedTaskLock(*awaitedTask);
    if(awaitedTask->isCanceled()) {
        // If the awaited task was canceled, cancel the waiting task as well.
        if(cancelWaitingIfAwaitedCanceled)
            waitingTask->cancelLocked(waitingTaskLock);
        // Don't wait for the task to finish.
        if(returnEarlyIfCanceled)
            return false;
    }
    if(awaitedTask->isFinished()) {
        // It's ready, no need to wait.
        if(throwOnError)
            awaitedTask->throwPossibleException();
        return true;
    }

    // Create shared pointers on the stack to make sure the two task objects don't get
    // destroyed during or right after the waiting phase and before we access them again below.
    TaskPtr waitingTaskPtr(waitingTask->shared_from_this());
    TaskPtr awaitedTaskPtr(awaitedTask.get());

    waitingTaskLock.unlock();
    awaitedTaskLock.unlock();

    // Is the waiting task running in a thread pool?
    if(waitingTask->isAsynchronousTask()) {
        // Are we really in a worker thread?
        OVITO_ASSERT(!ExecutionContext::isMainThread());
        OVITO_ASSERT(static_cast<AsynchronousTaskBase*>(waitingTask)->threadPool());

        // Work-stealing mechanism to avoid deadlock when running out of threads in this thread pool.
        AsynchronousTaskBase* asyncTask = awaitedTask->isAsynchronousTask() ? static_cast<AsynchronousTaskBase*>(awaitedTask.get().get()) : nullptr;
        if(asyncTask && asyncTask->threadPool() == static_cast<AsynchronousTaskBase*>(waitingTask)->threadPool() && asyncTask->threadPool()->tryTake(asyncTask)) {
            // The task was taken from the current pool. Run it right away in the current thread.

            // Attach a temporary callback to the waiting task.
            detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) {
                if(state & (Task::Canceled | Task::Finished)) {
                    // When the parent task gets canceled, discard the task dependency which keeps the awaited task running.
                    awaitedTask.reset();
                }
                return true;
            });

            // Execute awaited task now.
            asyncTask->run();

            waitingTaskCallback.unregisterCallback();
        }
        else {
            // The task is already executing or scheduled to run in a different thread pool. Wait for it to finish or get canceled.
            std::condition_variable cv;
            std::mutex waitMutex;
            bool done = false;

            // Attach a temporary callback to the waiting task, which sets the wait condition in case the waiting task gets canceled.
            detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) {
                if(state & (Task::Canceled | Task::Finished)) {
                    // When the parent task gets canceled, discard the task dependency which keeps the awaited task running.
                    awaitedTask.reset();
                    // Wake up the waiting thread - but only if we shouldn't wait for the task to completely finish.
                    if(returnEarlyIfCanceled) {
                        {
                            std::lock_guard lock(waitMutex);
                            done = true;
                        }
                        cv.notify_one();
                    }
                }
                return true;
            });

            // Attach a temporary callback function to the awaited task, which sets the wait condition when the task finishes or gets canceled.
            detail::FunctionTaskCallback awaitedTaskCallback(awaitedTaskPtr.get(), [&](int state) {
                if(state & (returnEarlyIfCanceled ? (Task::Finished | Task::Canceled) : Task::Finished)) {
                    {
                        std::lock_guard lock(waitMutex);
                        done = true;
                    }
                    cv.notify_one();
                }
                return true;
            });

            {
                std::unique_lock lock(waitMutex);
                cv.wait(lock, [&]{ return done; });
            }

            waitingTaskCallback.unregisterCallback();
            awaitedTaskCallback.unregisterCallback();
        }
    }
    else {
        // Process all pending work items while waiting for the task to finish.
        ExecutionContext::current().ui().taskManager().processWorkWhileWaiting(waitingTask, awaitedTask, returnEarlyIfCanceled);
    }

    // Check if the waiting task has been canceled.
    waitingTaskLock.lock();
    if(waitingTask->isCanceled())
        return false;

    // Now check if the awaited task has been canceled.
    awaitedTaskLock.lock();

    if(awaitedTaskPtr->isCanceled()) {
        // If the awaited task was canceled, cancel the waiting task as well.
        if(cancelWaitingIfAwaitedCanceled) {
            waitingTask->cancelLocked(waitingTaskLock);
            return false;
        }
        else {
            return true;
        }
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

} // End of namespace

} // End of namespace
