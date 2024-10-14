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


#include <ovito/core/Core.h>
#include "TaskDependency.h"
#include "TaskCallback.h"
#include "../Promise.h"

namespace Ovito::detail {

/**
 * \brief Encapsulates the logic of one task waiting for another task to finish.
 */
class TaskAwaiter : private TaskCallbackBase
{
    Q_DISABLE_COPY_MOVE(TaskAwaiter)

public:

    /// Constructor.
    explicit TaskAwaiter(Task& owner) noexcept : TaskCallbackBase(owner, &taskStateChangedCallback) {}

    /// Moves the dependency on the awaited task out of this object.
    /// Note: Make sure this owning task's mutex is locked while calling this function.
    TaskDependency takeAwaitedTask() noexcept {
        return std::move(_awaitedTask);
    }

    /// Returns the task we are currently waiting for.
    const TaskDependency& awaitedTask() noexcept {
        return _awaitedTask;
    }

    /// Runs a continuation method once the given task reaches the 'finished' state.
    template<
        typename TaskClass,
        void(TaskClass::*ContinuationMethod)(PromiseBase, detail::TaskDependency) noexcept,
        typename Executor
    >
    void whenTaskFinishes(TaskDependency awaitedTask, Executor&& executor, PromiseBase promise) noexcept {
        OVITO_ASSERT(awaitedTask);
        OVITO_ASSERT(promise);
        Task& waitingTask = *promise.task();

        // Attach to the task to be waited on.
        Task::MutexLock lock(waitingTask);
        OVITO_ASSERT(!_awaitedTask);
        if(waitingTask.isCanceled()) {
            // Bail out and do not attach to the input task if this continuation task is already canceled.
            return;
        }
        OVITO_ASSERT(!waitingTask.isFinished());
        _awaitedTask = std::move(awaitedTask);
        TaskPtr t = _awaitedTask.get(); // Keep a reference to the awaited task, because after unlocking our mutex, we might loose _awaitedTask.
        lock.unlock();

        // Run the waiting task's callback method once the awaited task finishes.
        t->addContinuation([
                this,
                promise = std::move(promise),
                executor = std::forward<Executor>(executor)]() mutable noexcept
            {
            // Lock access to the waiting task.
            Task::MutexLock lock(*promise.task());

            // Get the awaited task that did just finish.
            TaskDependency finishedTask = takeAwaitedTask();

            // Bail out if the waiting task or the awaited task has been canceled.
            if(!finishedTask || finishedTask->isCanceled()) {
                return; // Note: The Promise's destructor automatically puts the waiting task into 'canceled' and 'finished' states if it isn't already.
            }
            lock.unlock();

            // Invoke the callback method which processes the awaited task's result.
            std::move(executor).execute([promise = std::move(promise), finishedTask = std::move(finishedTask)]() mutable noexcept {
                (static_cast<TaskClass*>(promise.task().get())->*ContinuationMethod)(std::move(promise), std::move(finishedTask));
            });
        });
    }

private:

    /// This function gets invoked when the state of the waiting task changes.
    /// The waiting task's mutex is locked when this function is called.
    static void taskStateChangedCallback(Task& task, TaskCallbackBase& cb, int state, Task::MutexLock& lock) noexcept {
        // Task either gets canceled or finished. Other state changes are not possible.
        OVITO_ASSERT(state & (Task::Finished | Task::Canceled));

        // When this task gets canceled, we discard the reference to the
        // task we are waiting for in order to cancel that one as well.
        TaskAwaiter& self = static_cast<TaskAwaiter&>(cb);

        // Move the dependency on the preceding task out of this object. This may implicitly cancel the
        // awaited task when the reference goes out of scope.
        if(auto awaitedTask = self.takeAwaitedTask()) {
            // Note: It's critical to first unlock the mutex before releasing the reference to the awaited task.
            lock.unlock();
            awaitedTask.reset();
            lock.lock();
        }
    }

    /// The task we are waiting for.
    TaskDependency _awaitedTask;
};

} // End of namespace
