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
#include "Future.h"
#include "detail/FutureDetail.h"
#include "detail/TaskDependency.h"
#include "InlineExecutor.h"

namespace Ovito {

/**
 * A future that provides access to the value computed by a Promise.
 */
template<typename R>
class SharedFuture : public FutureBase
{
public:

    using this_type = SharedFuture<R>;
    using result_type = R;
    using promise_type = typename Future<R>::promise_type;

    /// Default constructor that constructs an invalid SharedFuture that is not associated with any shared state.
    SharedFuture() noexcept = default;

    /// Move constructor.
    SharedFuture(SharedFuture&& other) noexcept = default;

    /// Copy constructor.
    SharedFuture(const SharedFuture& other) noexcept = default;

    /// Constructor that constructs a shared future from a normal future.
    SharedFuture(Future<R>&& other) noexcept : FutureBase(std::move(other)) {}

    /// Constructor that constructs a SharedFuture that is associated with the given shared state.
    explicit SharedFuture(TaskPtr p) noexcept : FutureBase(std::move(p)) {}

    /// Constructor that constructs a Future from an existing task dependency.
    explicit SharedFuture(detail::TaskDependency&& p) noexcept : FutureBase(std::move(p)) {}

    /// A future may directly be initialized from a value.
    template<typename R2 = R,
        typename = std::enable_if_t<!std::is_void_v<R2>
            && !std::is_same_v<std::decay_t<R2>, SharedFuture<R>>
            && !std::is_same_v<std::decay_t<R2>, Future<R>>
            && !std::is_same_v<std::decay_t<R2>, TaskPtr>>>
    SharedFuture(R2&& val) : FutureBase(std::move(promise_type::createImmediate(std::forward<R2>(val))._task)) {}

    /// Create a new SharedFuture that is associated with the given task object.
    static SharedFuture createFromTask(TaskPtr task) {
        OVITO_ASSERT(task);
        OVITO_ASSERT(task->_resultsStorage != nullptr || (std::is_void_v<R>));
        return SharedFuture(std::move(task));
    }

    /// Move assignment operator.
    SharedFuture& operator=(SharedFuture&& other) noexcept = default;

    /// Copy assignment operator.
    SharedFuture& operator=(const SharedFuture& other) noexcept = default;

    /// Returns a const reference to the results computed by the associated Promise.
    /// The function blocks until the result become available.
    template<typename R2 = R>
    std::enable_if_t<!std::is_void_v<R2>, std::add_lvalue_reference_t<std::add_const_t<R>>> result() const& {
        OVITO_ASSERT_MSG(isValid(), "SharedFuture::results()", "Future must be valid.");
        waitForFinished();
        OVITO_ASSERT_MSG(isFinished(), "SharedFuture::results()", "Future must be in fulfilled state.");
        OVITO_ASSERT_MSG(!isCanceled(), "SharedFuture::results()", "Future must not be canceled.");
        return task()->template getResult<R>();
    }

    /// Returns a copy to the results computed by the associated Promise.
    /// The function blocks until the result become available.
    template<typename R2 = R>
    std::enable_if_t<!std::is_void_v<R2>, R> result() && {
        return result();
    }

    /// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
    /// The provided continuation function must accept the results of this future as an input parameter.
    template<typename Executor, typename Function>
    detail::continuation_future_type<Function, SharedFuture>
    then(Executor&& executor, Function&& f);

    /// Overload of the function above using the default inline executor.
    template<typename Function>
    decltype(auto) then(Function&& f) { return then(InlineExecutor{}, std::forward<Function>(f)); }

    /// Applies a post-processing function to the future's results, which must returns the same type of value
    /// as the original future. The post-processing function is executed once the future is fulfilled.
    /// Calling postprocess() is equivalent to calling then() and then replacing the parent future with the
    /// continuation future.
    template<typename Executor, typename Function>
    void postprocess(Executor&& executor, Function&& f) {
        *this = then(std::forward<Executor>(executor), std::forward<Function>(f));
    }

protected:

    template<typename R2> friend class Promise;
    template<typename R2> friend class WeakSharedFuture;
};

/// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
/// The provided continuation function must accept the results of this future as an input parameter.
template<typename R>
template<typename Executor, typename Function>
detail::continuation_future_type<Function, SharedFuture<R>>
SharedFuture<R>::then(Executor&& executor, Function&& f)
{
    // Infer the exact future/promise/task types to create.
    using result_future_type = detail::continuation_future_type<Function,SharedFuture<R>>;
    using result_promise_type = typename result_future_type::promise_type;
    using continuation_task_type = detail::ContinuationTask<typename result_promise_type::result_type, Task>;

    // This future must be valid for then() to work.
    OVITO_ASSERT_MSG(isValid(), "SharedFuture::then()", "Future must be valid.");

    // Inherit the priority flag from the task adding the continuation.
    bool isHighPriority = this_task::get()->isHighPriorityTask();

    // Create a task, promise and future for the continuation.
    // Inherit the priority flag from the parent task.
    result_promise_type promise{std::make_shared<continuation_task_type>(isHighPriority ? Task::HighPriority : Task::NoState)};
    result_future_type future = promise.future();
    continuation_task_type* continuationTask = static_cast<continuation_task_type*>(promise.task().get());

    // Run the following function once the existing task finishes. We'll then invoke the user's continuation function.
    continuationTask->whenTaskFinishes(
            this->task(),
            std::forward<Executor>(executor),
            [f = std::forward<Function>(f), promise = std::move(promise)]() mutable noexcept {

        // Get the task that is about to continue.
        continuation_task_type* continuationTask = static_cast<continuation_task_type*>(promise.task().get());

        // Manage access to the task that represents the continuation.
        Task::MutexLocker locker(*continuationTask);

        // Get the task that did just finish.
        detail::TaskDependency finishedTask = continuationTask->takeAwaitedTask();

        // Don't need to run continuation function if the continuation task has been canceled in the meantime.
        // Also don't run continuation function if the preceding task was canceled.
        if(!finishedTask || finishedTask->isCanceled())
            return; // Note: The Promise's destructor automatically puts the continuation task into 'canceled' and 'finished' states.

        OVITO_ASSERT(finishedTask->isFinished());
        OVITO_ASSERT(!continuationTask->isFinished());
        OVITO_ASSERT(!continuationTask->isCanceled());

        // Don't execute continuation function in case an error occurred in the preceding task.
        // In such a case, copy the exception state to the continuation promise.
        if constexpr(!std::is_invocable_v<Function, SharedFuture<R>>) {
            if(finishedTask->exceptionStore()) {
                continuationTask->exceptionLocked(finishedTask->exceptionStore());
                continuationTask->finishLocked(locker);
                return;
            }
        }
        locker.unlock();

        // Now it's time to execute the continuation function.
        // Assign the function's return value as result of the continuation task.
        continuationTask->fulfillWith(std::move(promise), std::forward<Function>(f), SharedFuture<R>(std::move(finishedTask)));
    });

    return future;
}

}   // End of namespace
