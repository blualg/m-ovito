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
    [[nodiscard]] static SharedFuture createFromTask(TaskPtr task) {
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
    [[nodiscard]] std::enable_if_t<!std::is_void_v<R2>, std::add_lvalue_reference_t<std::add_const_t<R>>> result() const& {
        OVITO_ASSERT_MSG(isValid(), "SharedFuture::results()", "Future must be valid.");
        waitForFinished();
        OVITO_ASSERT_MSG(isFinished(), "SharedFuture::results()", "Future must be in fulfilled state.");
        OVITO_ASSERT_MSG(!isCanceled(), "SharedFuture::results()", "Future must not be canceled.");
        return task()->template getResult<R>();
    }

    /// Returns a copy to the results computed by the associated Promise.
    /// The function blocks until the result become available.
    template<typename R2 = R>
    [[nodiscard]] std::enable_if_t<!std::is_void_v<R2>, R> result() && {
        return result();
    }

    /// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
    /// The provided continuation function must accept the results of this future as an input parameter.
    template<typename Executor, typename Function>
    [[nodiscard]] detail::continuation_future_type<Function, SharedFuture>
    then(Executor&& executor, Function&& f) const;

    /// Overload of the function above using the default inline executor.
    template<typename Function>
    [[nodiscard]] decltype(auto) then(Function&& f) const { return then(InlineExecutor{}, std::forward<Function>(f)); }

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
SharedFuture<R>::then(Executor&& executor, Function&& f) const
{
    // Infer the exact future/promise/task types to create.
    using result_future_type = detail::continuation_future_type<Function,SharedFuture<R>>;
    using continuation_task_type = detail::ContinuationTask<typename result_future_type::result_type>;

    // This future must be valid for then() to work.
    OVITO_ASSERT_MSG(isValid(), "SharedFuture::then()", "Future must be valid.");

    class ThenTask : public continuation_task_type
    {
    public:

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = result_future_type;

        /// Constructor.
        explicit ThenTask(Function&& function) :
            continuation_task_type(Task::NoState),
            _function(std::forward<Function>(function)) {}

        /// Starts execution of the task.
        void operator()(detail::TaskDependency awaitedTask, Executor&& executor) {
            this->template whenTaskFinishes<ThenTask, &ThenTask::awaitedTaskFinished>(
                std::move(awaitedTask),
                std::forward<Executor>(executor),
                this->shared_from_this());
        }

        /// Callback to be invoked when the awaited task has finished.
        void awaitedTaskFinished(PromiseBase promise) noexcept {
            // Lock this task.
            Task::MutexLock lock(*this);

            // Get the task that did just finish.
            detail::TaskDependency finishedTask = this->takeAwaitedTask();

            // Don't need to run continuation function if the continuation task has been canceled in the meantime.
            // Also don't run continuation function if the awaited task was canceled.
            if(!finishedTask || finishedTask->isCanceled())
                return; // Note: The Promise's destructor automatically puts the continuation task into 'canceled' and 'finished' states.

            OVITO_ASSERT(finishedTask->isFinished());
            OVITO_ASSERT(!this->isFinished());
            OVITO_ASSERT(!this->isCanceled());

            // Don't execute continuation function in case an error occurred in the preceding task and unless the continuation function takes a Future.
            // Forward any preceding exception state directly to the continuation task.
            if constexpr(!std::is_invocable_v<Function, SharedFuture<R>>) {
                if(finishedTask->exceptionStore()) {
                    this->exceptionLocked(finishedTask->exceptionStore());
                    this->finishLocked(lock);
                    return;
                }
            }
            lock.unlock();

            // Now it's time to execute the continuation function supplied by the user.
            // This assigns the function's return value as result of this continuation task.
            this->fulfillWith(std::move(promise), std::move(_function), SharedFuture<R>(std::move(finishedTask)));
        }

    private:

        /// The caller's continuation function to be executed once the awaited task completes.
        std::decay_t<Function> _function;
    };

    return launchTask(
        std::make_shared<ThenTask>(std::forward<Function>(f)),
        this->task(),
        std::forward<Executor>(executor));
}

}   // End of namespace
