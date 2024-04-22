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
#include "Promise.h"
#include "detail/FutureDetail.h"
#include "detail/TaskDependency.h"
#include "detail/ContinuationTask.h"
#include "InlineExecutor.h"

namespace Ovito {

/**
 * \brief Base class for futures, which provides access to the results of an asynchronous task.
 */
class OVITO_CORE_EXPORT FutureBase
{
public:

    /// Destructor.
    ~FutureBase() { reset(); }

    /// Returns true if the shared state associated with this Future has been canceled.
    bool isCanceled() const { return task()->isCanceled(); }

    /// Returns true if the shared state associated with this Future has been fulfilled.
    bool isFinished() const { return task()->isFinished(); }

    /// Returns true if this Future is associated with a shared state.
    bool isValid() const { return (bool)_task.get(); }

    /// Dissociates this Future from its shared state.
    void reset() {
        _task.reset();
    }

    /// Returns the shared state associated with this Future.
    /// Use isValid() to make sure it has one before calling this function.
    const TaskPtr& task() const {
        OVITO_ASSERT(isValid());
        return _task.get();
    }

    /// Moves the task dependency out of this future, which invalidates the future.
    detail::TaskDependency takeTaskDependency() noexcept { return std::move(_task); }

    /// Moves the task dependency out of this future, which invalidates the future.
    operator detail::TaskDependency() && noexcept { return takeTaskDependency(); }

    /// Move constructor.
    FutureBase(FutureBase&& other) noexcept = default;

    /// Copy constructor.
    FutureBase(const FutureBase& other) noexcept = default;

    /// A future is moveable.
    FutureBase& operator=(FutureBase&& other) noexcept = default;

    /// Copy assignment.
    FutureBase& operator=(const FutureBase& other) noexcept = default;

    /// Runs the given continuation function once this future has reached either the 'finished' or the 'canceled' state.
    /// Note that the continuation function will always be executed, even if this future was canceled or set to an error state.
    /// The callable must accept one parameter: a reference to the underlying Task object.
    template<typename Executor, typename Function>
    void finally(Executor&& executor, Function&& f) const noexcept {
        OVITO_ASSERT_MSG(isValid(), "FutureBase::finally()", "Future must be valid.");
        task()->finally(std::forward<Executor>(executor), std::forward<Function>(f));
    }

    /// Runs the given continuation function once this future has reached either the 'finished' or the 'canceled' state.
    /// Note that the continuation function will always be executed, even if this future was canceled or set to an error state.
    /// The callable must accept one parameter: a reference to the underlying Task object.
    template<typename Function>
    void finally(Function&& f) const noexcept {
        OVITO_ASSERT_MSG(isValid(), "FutureBase::finally()", "Future must be valid.");
        task()->finally(std::forward<Function>(f));
    }

    /// \brief Blocks execution until this future is fulfilled.
    /// Throws an OperationCanceled exception if the future or the task awaiting it got canceled.
    /// Throws an exception if the awaited task has failed to complete.
    void waitForFinished() const & {
        if(!Task::waitFor(task(), true))
            throw OperationCanceled();
    }

    /// \brief Blocks execution until this future is fulfilled.
    /// Throws an OperationCanceled exception if the future or the task awaiting it got canceled.
    /// Throws an exception if the awaited task has failed to complete.
    void waitForFinished() && {
        if(!Task::waitFor(takeTaskDependency(), true))
            throw OperationCanceled();
    }

protected:

    /// Default constructor creating a future without a shared state.
    FutureBase() noexcept = default;

    /// Constructor that creates a Future associated with a share state.
    explicit FutureBase(TaskPtr&& p) noexcept : _task(std::move(p)) {}

    /// Constructor that creates a Future from an existing task reference.
    explicit FutureBase(detail::TaskDependency&& p) noexcept : _task(std::move(p)) {}

private:

    /// Reference to the shared state, which also expresses the strong dependency on this task's results.
    detail::TaskDependency _task;
};

/**
 * \brief A typed future, which provides access to the results of an asynchronous task.
 */
template<typename R>
class Future : public FutureBase
{
public:

    using this_type = Future<R>;
    using result_type = R;
    using promise_type = Promise<R>;

    /// Default constructor that constructs an invalid Future that is not associated with any shared state.
    Future() noexcept = default;

    /// A future is not copy-constructible.
    Future(const Future& other) = delete;

    /// A future is move-constructible.
    Future(Future&& other) noexcept = default;

    /// Constructs a Future that is associated with the given shared state. This is mostly for internal use.
    explicit Future(TaskPtr p) noexcept : FutureBase(std::move(p)) {}

    /// Constructs a Future from an existing task reference. This is mostly for internal use.
    explicit Future(detail::TaskDependency&& p) noexcept : FutureBase(std::move(p)) {}

    /// A future may directly be initialized from a value.
    template<typename R2 = R,
        typename = std::enable_if_t<!std::is_void_v<R2>
            && !std::is_same_v<std::decay_t<R2>, Future<R>>
            && !std::is_same_v<std::decay_t<R2>, TaskPtr>>>
    Future(R2&& val) : FutureBase(std::move(promise_type::createImmediate(std::forward<R2>(val))._task)) {}

    /// A future is moveable.
    Future& operator=(Future&& other) noexcept = default;

    /// A future is not copy assignable.
    Future& operator=(const Future& other) = delete;

    /// Creates a future that is in the 'canceled' state.
    static Future createCanceled() {
        return promise_type::createCanceled();
    }

    /// Create a future that is in the 'fulfilled' state and holds an immediate default-constructed result.
    static Future createImmediateEmpty() {
        return promise_type::createImmediateEmpty();
    }

    /// Create a future that is in the 'fulfilled' state and holds an immediate result.
    template<typename V>
    static Future createImmediate(V&& result) {
        return promise_type::createImmediate(std::forward<V>(result));
    }

    /// Create a future that is in the 'fulfilled' state and holds an immediate result.
    template<typename... Args>
    static Future createImmediateEmplace(Args&&... args) {
        return promise_type::createImmediateEmplace(std::forward<Args>(args)...);
    }

    /// Creates a future that is in the 'exception' state.
    static Future createFailed(const Exception& ex) {
        return promise_type::createFailed(ex);
    }

    /// Creates a future that is in the 'exception' state.
    static Future createFailed(Exception&& ex) {
        return promise_type::createFailed(std::move(ex));
    }

    /// Creates a future that is in the 'exception' state.
    static Future createFailed(std::exception_ptr ex_ptr) {
        return promise_type::createFailed(std::move(ex_ptr));
    }

    /// Create a new Future that is associated with the given task object.
    static Future createFromTask(TaskPtr task) {
        OVITO_ASSERT(task);
        OVITO_ASSERT(task->_resultsStorage != nullptr || (std::is_void_v<R>));
        return Future(std::move(task));
    }

    /// Returns the results computed by the associated Promise.
    /// The function blocks until the result become available.
    template<typename R2 = R>
    std::enable_if_t<!std::is_void_v<R2>, R> result() {
        OVITO_ASSERT_MSG(isValid(), "Future::results()", "Future must be valid.");
        waitForFinished();
        OVITO_ASSERT_MSG(isFinished(), "Future::results()", "Future must be in fulfilled state.");
        OVITO_ASSERT_MSG(!isCanceled(), "Future::results()", "Future must not be canceled.");
        return takeTaskDependency()->template takeResult<R>();
    }

    /// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
    /// The provided continuation function must accept the results of this future or the future itself as an input parameter.
    template<typename Executor, typename Function>
    detail::continuation_future_type<Function, Future>
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

#ifndef Q_CC_GNU
protected:
#else
// This is a workaround for what is likely a bug in the GCC compiler, which doesn't respect the
// template friend class declarations made below. The AsynchronousTask<void> template specialization
// doesn't seem to get access to the Future constructor.
public:
#endif

    /// Move constructor taking the promise state pointer from a r-value Promise.
    Future(promise_type&& promise) : FutureBase(std::move(promise._task)) {}

    template<typename R2> friend class Future;
    template<typename R2> friend class Promise;
    template<typename R2> friend class SharedFuture;
    template<typename R2> friend class AsynchronousTask;
    friend class AsynchronousTaskBase;
};

/// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
/// The provided continuation function must accept the results of this future as an input parameter.
template<typename R>
template<typename Executor, typename Function>
detail::continuation_future_type<Function, Future<R>>
Future<R>::then(Executor&& executor, Function&& f)
{
    // Infer the exact future/promise/task types to create.
    using result_future_type = detail::continuation_future_type<Function, Future<R>>;
    using result_promise_type = typename result_future_type::promise_type;
    using continuation_task_type = detail::ContinuationTask<typename result_promise_type::result_type, Task>;

    // This future must be valid for then() to work.
    OVITO_ASSERT_MSG(isValid(), "Future::then()", "Future must be valid.");

    // Inherit the priority flag from the task adding the continuation.
    bool isHighPriority = this_task::get()->isHighPriorityTask();

    // Create a task, promise and future for the continuation.
    result_promise_type promise{std::make_shared<continuation_task_type>(isHighPriority ? Task::HighPriority : Task::NoState)};
    result_future_type future = promise.future();
    continuation_task_type* continuationTask = static_cast<continuation_task_type*>(promise.task().get());

    // Run the following function once the existing task finishes. We'll then invoke the user's continuation function.
    continuationTask->whenTaskFinishes(
            this->takeTaskDependency(), // The reference to the existing task is moved from this future into the continuation task.
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

        // Don't execute continuation function in case an error occurred in the preceding task and unless the continuation function takes a Future.
        // Forward any preceding exception state directly to the continuation task.
        if constexpr(!std::is_invocable_v<Function, Future<R>>) {
            if(finishedTask->exceptionStore()) {
                continuationTask->exceptionLocked(finishedTask->exceptionStore());
                continuationTask->finishLocked(locker);
                return;
            }
        }
        locker.unlock();

        // Now it's time to execute the continuation function supplied by the user.
        // Assign the function's return value as result of the continuation task.
        continuationTask->fulfillWith(std::move(promise), std::forward<Function>(f), Future<R>(std::move(finishedTask)));
    });

    return future;
}

}   // End of namespace
