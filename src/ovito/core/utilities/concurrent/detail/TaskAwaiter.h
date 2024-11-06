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

    /// Runs a continuation function once the given task reaches the 'finished' state.
    template<bool StructuredConcurrency = false, typename Executor, typename Function>
    void whenTaskFinishes(TaskDependency awaitedTask, Executor&& executor, PromiseBase promise, Function&& function) noexcept {
        OVITO_ASSERT(promise);

        // Make a copy of the task pointer for later.
        TaskPtr awt = awaitedTask.get();

        // Attach to the task to be waited on.
        if(!setAwaitedTask(*promise.task(), std::move(awaitedTask))) {
            // The waiting task has been canceled.
            // Normally, we can now bail out immediately and do not need to wait for the awaited task to finish.
            // In structured concurrency mode, however, we still register a continuation function
            // with the child task just to keep the parent task alive as long as the child task is running.
            if constexpr(StructuredConcurrency) {
                awt->addContinuation([promise = std::move(promise)]() noexcept {});
            }
            return;
        }

        // Run the waiting task's callback method once the awaited task finishes.
        awt->addContinuation([
            this,
            promise = std::move(promise),
            executor = std::forward<Executor>(executor),
            function = std::forward<Function>(function)]() mutable noexcept
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

            // Run the callback method that processes the awaited task's result.
            std::move(executor).execute([function = std::move(function), promise = std::move(promise), finishedTask = std::move(finishedTask)]() mutable noexcept {
                std::invoke(std::move(function), std::move(promise), std::move(finishedTask));
            });
        });
    }

    /// Runs a continuation method of the waiting task class once the given awaited task reaches the 'finished' state.
    template<
        typename TaskClass,
        void(TaskClass::*ContinuationMethod)(PromiseBase, detail::TaskDependency) noexcept,
        typename Executor
    >
    void whenTaskFinishes(TaskDependency awaitedTask, Executor&& executor, PromiseBase promise) noexcept {
        whenTaskFinishes(std::move(awaitedTask), std::forward<Executor>(executor), std::move(promise),
            [](PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
                (static_cast<TaskClass*>(promise.task().get())->*ContinuationMethod)(std::move(promise), std::move(finishedTask));
            }
        );
    }

private:

    /// Moves the dependency on the awaited task out of this object.
    /// Note: Make sure this owning task's mutex is locked while calling this function.
    TaskDependency takeAwaitedTask() noexcept {
        return std::move(_awaitedTask);
    }

    /// Attaches to a task to be waited on.
    bool setAwaitedTask(Task& waitingTask, TaskDependency awaitedTask) noexcept {
        OVITO_ASSERT(awaitedTask);
        Task::MutexLock lock(waitingTask);
        OVITO_ASSERT(!_awaitedTask);
        if(waitingTask.isCanceled()) {
            // Bail out and do not attach to the awaited task if the waiting task is already canceled.
            return false;
        }
        OVITO_ASSERT(!waitingTask.isFinished());
        _awaitedTask = std::move(awaitedTask);
        return true;
    }

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

            // If the waiting task got canceled while it was waiting, put the task into the finished
            // state immediately. This is to run all continuations as soon as possible without waiting
            // for the awaited task to finish (which would eventually release the promise object referring to the waiting task).
            if(!task.isFinished()) {
                task.finishLocked(lock);
                OVITO_ASSERT(!lock); // Note: Task::finishLocked() does not relock the mutex after running the continuations.
                lock.lock();
            }
        }
    }

    /// The task we are waiting for.
    TaskDependency _awaitedTask;
};

} // End of namespace
