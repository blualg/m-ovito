////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include "detail/TaskReference.h"

namespace Ovito {

/******************************************************************************
* A future that provides access to the value computed by a Promise.
******************************************************************************/
template<typename... R>
class SharedFuture : public FutureBase
{
public:

    using this_type = SharedFuture<R...>;
    using tuple_type = typename Future<R...>::tuple_type;
    using promise_type = typename Future<R...>::promise_type;

    /// Default constructor that constructs an invalid SharedFuture that is not associated with any shared state.
    SharedFuture() noexcept = default;

    /// Move constructor.
    SharedFuture(SharedFuture&& other) noexcept = default;

    /// Copy constructor.
    SharedFuture(const SharedFuture& other) noexcept = default;

    /// Constructor that constructs a shared future from a normal future.
    SharedFuture(Future<R...>&& other) noexcept : FutureBase(std::move(other)) {}

    /// Constructor that constructs a SharedFuture that is associated with the given shared state.
    explicit SharedFuture(TaskPtr p) noexcept : FutureBase(std::move(p)) {}

    /// Constructor that constructs a Future from an existing task dependency.
    explicit SharedFuture(detail::TaskReference&& p) noexcept : FutureBase(std::move(p)) {}

    /// A future may directly be initialized from r-values.
    template<typename... R2, size_t C = sizeof...(R),
        typename = std::enable_if_t<C != 0
            && !std::is_same<std::tuple<std::decay_t<R2>...>, std::tuple<SharedFuture<R...>>>::value
            && !std::is_same<std::tuple<std::decay_t<R2>...>, std::tuple<Future<R...>>>::value
            && !std::is_same<std::tuple<std::decay_t<R2>...>, std::tuple<TaskPtr>>::value>>
    SharedFuture(R2&&... val) noexcept : FutureBase(std::move(promise_type::createImmediate(std::forward<R2>(val)...)._task)) {}

    /// Move assignment operator.
    SharedFuture& operator=(SharedFuture&& other) noexcept = default;

    /// Copy assignment operator.
    SharedFuture& operator=(const SharedFuture& other) noexcept = default;

    /// Returns the results computed by the associated Promise.
    /// This function may only be called after the Promise was fulfilled (and not canceled).
    const std::tuple<R...>& results() const {
        OVITO_ASSERT_MSG(isValid(), "SharedFuture::results()", "Future must be valid.");
        OVITO_ASSERT_MSG(isFinished(), "SharedFuture::results()", "Future must be in fulfilled state.");
        OVITO_ASSERT_MSG(!isCanceled(), "SharedFuture::results()", "Future must not be canceled.");
        OVITO_ASSERT_MSG(std::tuple_size_v<tuple_type> != 0, "SharedFuture::results()", "Future must not be of type <void>.");
        task()->throwPossibleException();
        return task()->template getResults<tuple_type>();
    }

    /// Returns the results computed by the associated Promise.
    /// This function may only be called after the Promise was fulfilled (and not canceled).
    decltype(auto) result() const {
        if constexpr(sizeof...(R) == 1) {
            return std::get<0>(results());
        }
        else {
            task()->throwPossibleException(); 
        }
    }

    /// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
    /// The provided continuation function must accept the results of this future as an input parameter.
    template<typename Executor, typename Function>
    detail::continuation_future_type<Function,SharedFuture>
    then(Executor&& executor, Function&& f);

    /// Overload of the function above using the default inline executor.
    template<typename Function>
    decltype(auto) then(Function&& f) { return then(detail::InlineExecutor{}, std::forward<Function>(f)); }

protected:

    template<typename... R2> friend class Promise;
    template<typename... R2> friend class WeakSharedFuture;
};

/// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
/// The provided continuation function must accept the results of this future as an input parameter.
template<typename... R>
template<typename Executor, typename Function>
detail::continuation_future_type<Function,SharedFuture<R...>> 
SharedFuture<R...>::then(Executor&& executor, Function&& f)
{
    // Infer the exact future/promise/task types to create.
    using result_future_type = detail::continuation_future_type<Function,SharedFuture<R...>>;
    using result_promise_type = typename result_future_type::promise_type;
    using continuation_task_type = detail::ContinuationTask<typename result_promise_type::tuple_type, Task>;

    // This future must be valid for then() to work.
    OVITO_ASSERT_MSG(isValid(), "SharedFuture::then()", "Future must be valid.");

    // Create a task, promise and future for the continuation.
    result_promise_type promise{std::make_shared<continuation_task_type>()};
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
        QMutexLocker locker(&continuationTask->taskMutex());

        // Get the task that did just finish.
        detail::TaskReference finishedTask = continuationTask->takeAwaitedTask();

        // Don't need to run continuation function if the continuation task has been canceled in the meantime.
        // Also don't run continuation function if the preceding task was canceled.
        if(!finishedTask || finishedTask->isCanceled())
            return; // Note: The Promise's destructor automatically puts the continuation task into 'canceled' and 'finished' states.

        OVITO_ASSERT(finishedTask->isFinished());
        OVITO_ASSERT(!continuationTask->isFinished());
        OVITO_ASSERT(!continuationTask->isCanceled());

        // Put the continuation task into the 'started' state.
        continuationTask->startLocked();

        // Don't execute continuation function in case an error occurred in the preceding task.
        // In such a case, copy the exception state to the continuation promise.
        if constexpr(!detail::is_invocable_v<Function, SharedFuture<R...>>) {
            if(finishedTask->exceptionStore()) {
                continuationTask->exceptionLocked(finishedTask->copyExceptionStore());
                continuationTask->finishLocked(locker);
                return;
            }
        }
        locker.unlock();

        // Now it's time to execute the continuation function.
        // Assign the function's return value as result of the continuation task.
        continuationTask->fulfillWith(std::move(promise), std::forward<Function>(f), SharedFuture<R...>(std::move(finishedTask)));
    });

    return future;
}

}   // End of namespace
