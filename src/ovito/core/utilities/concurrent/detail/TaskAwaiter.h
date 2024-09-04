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

namespace Ovito::detail {

/**
 * \brief Encapsulates the logic of one task waiting for another task to finish.
 */
class OVITO_CORE_EXPORT TaskAwaiter : private TaskCallbackBase
{
    Q_DISABLE_COPY_MOVE(TaskAwaiter)

public:

    /// Constructor.
    explicit TaskAwaiter(Task& owner) noexcept : TaskCallbackBase(&taskStateChangedCallback) {
        // Insert into own linked list of callbacks.
        _nextInList = owner._callbacks;
        owner._callbacks = this;
    }

    /// Moves the dependency on the awaited task out of this object.
    /// Note: Make sure this owning task's mutex is locked while calling this function.
    TaskDependency takeAwaitedTask() noexcept {
        return std::move(_awaitedTask);
    }

    /// Returns the task we are currently waiting for.
    const TaskDependency& awaitedTask() noexcept {
        return _awaitedTask;
    }

    /// Runs the given continuation function once the given task reaches the 'finished' state.
    template<typename Executor, typename Function>
    void whenTaskFinishes(TaskDependency awaitedTask, Executor&& executor, PromiseBase promise, Function&& f) noexcept {
        OVITO_ASSERT(awaitedTask);
        OVITO_ASSERT(promise.isValid());
        Task& waitingTask = *promise.task();

        // Attach to the task to be waited on.
        Task::MutexLock lock(waitingTask);
        OVITO_ASSERT(!_awaitedTask);
        if(waitingTask.isCanceled()) {
            // Bail out and do not attach to the input task if this continuation task is already canceled.
            return;
        }
        OVITO_ASSERT(!waitingTask.isFinished());
        setAwaitedTask(std::move(awaitedTask));

        // Run the function once the task finishes. Store the promise and pass it to the function.
        this->awaitedTask().get()->addContinuation(
            std::move(lock),
            std::forward<Executor>(executor),
            [promise=std::move(promise), f=std::forward<Function>(f)]() mutable noexcept {
                std::invoke(std::move(f), std::move(promise));
            });
    }

    /// Runs the given continuation method once the given task reaches the 'finished' state.
    template<typename DerivedClass, void(DerivedClass::*ContinuationMethod)(PromiseBase), typename Executor>
    void whenTaskFinishes(TaskDependency awaitedTask, Executor&& executor, PromiseBase promise) noexcept {
        whenTaskFinishes(std::move(awaitedTask), std::forward<Executor>(executor), std::move(promise), [](PromiseBase promise) {
            (static_cast<DerivedClass*>(promise.task().get())->*ContinuationMethod)(std::move(promise));
        });
    }

protected:

    /// Sets the task to be waited for.
    /// Note: The waiting task's mutex must be locked when calling this function.
    void setAwaitedTask(TaskDependency awaitedTask) noexcept {
        _awaitedTask = std::move(awaitedTask);
    }

private:

    /// This function gets invoked when the state of the waiting task changes.
    /// The waiting task's mutex is locked when this function is called.
    static bool taskStateChangedCallback(TaskCallbackBase* f, int state, Task::MutexLock& lock) noexcept {
        // Task either gets canceled or finished. Other state changes are not possible.
        OVITO_ASSERT(state & (Task::Finished | Task::Canceled));

        // When this task gets canceled, we discard the reference to the
        // task we are waiting for in order to cancel that one as well.
        TaskAwaiter* self = static_cast<TaskAwaiter*>(f);

        // Move the dependency on the preceding task out of this object. This may implicitly cancel the
        // awaited task when the reference goes out of scope.
        if(auto awaitedTask = self->takeAwaitedTask()) {
            // Note: It's critical to first unlock the mutex before releasing the reference to the awaited task.
            lock.unlock();
            awaitedTask.reset();
            lock.lock();
        }

        // When this task finishes, we should detach our callback function immediately,
        // because a task object may not have callbacks registered at the end of its lifetime.
        if(state & Task::Finished)
            return false; // Returning false indicates that the callback wishes to be unregistered.
        return true;
    }

    /// The task we are waiting for.
    TaskDependency _awaitedTask;
};

} // End of namespace
