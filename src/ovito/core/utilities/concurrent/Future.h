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
    explicit operator bool() const { return static_cast<bool>(_task); }

    /// Dissociates this Future from its shared state.
    void reset() {
        _task.reset();
    }

    /// Returns the shared state associated with this Future.
    const TaskPtr& task() const {
        OVITO_ASSERT(_task);
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

    /// Runs the given continuation function once this future has reached either the 'finished' state.
    /// Note that the continuation function will always be executed, even if this future was canceled or set to an error state.
    /// The callable can accept one parameter: a reference to the underlying Task object.
    template<typename Executor, typename Function>
    void finally(Executor&& executor, Function&& f) const {
        // This future must be valid for finally() to work.
        OVITO_ASSERT_MSG(*this, "FutureBase::finally()", "Future must be valid.");
        task()->finally(std::forward<Executor>(executor), std::forward<Function>(f));
    }

    /// Overload of the function above using the inline executor.
    template<typename Function>
    void finally(Function&& f) const { finally(InlineExecutor{}, std::forward<Function>(f)); }

    /// \brief Blocks execution until this future is fulfilled.
    /// Throws an OperationCanceled exception if the future or the task awaiting it got canceled.
    /// Throws an exception if the awaited task has failed to complete.
    void waitForFinished(bool returnEarlyIfCanceled = true) const & {
        if(!Task::waitFor(task(), true, returnEarlyIfCanceled, true))
            throw OperationCanceled();
    }

    /// \brief Blocks execution until this future is fulfilled.
    /// Throws an OperationCanceled exception if the future or the task awaiting it got canceled.
    /// Throws an exception if the awaited task has failed to complete.
    void waitForFinished(bool returnEarlyIfCanceled = true) && {
        if(!Task::waitFor(takeTaskDependency(), true, returnEarlyIfCanceled, true))
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
    Q_DISABLE_COPY(Future)

public:

    using this_type = Future<R>;
    using result_type = R;

    /// The promise type for C++ coroutines returning a Future.
    using promise_type = CoroutinePromise<R, false>;

    /// Default constructor that constructs an invalid Future that is not associated with any shared state.
    Future() noexcept = default;

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
    Future(R2&& val) : FutureBase(std::move(Promise<R>::createImmediate(std::forward<R2>(val))._task)) {}

    /// A future is moveable.
    Future& operator=(Future&& other) noexcept = default;

    /// Create a future that is in the 'fulfilled' state and holds an immediate default-constructed result.
    [[nodiscard]] static Future createImmediateEmpty() {
        return Promise<R>::createImmediateEmpty();
    }

    /// Create a future that is in the 'fulfilled' state and holds an immediate result.
    template<typename V>
    [[nodiscard]] static Future createImmediate(V&& result) {
        return Promise<R>::createImmediate(std::forward<V>(result));
    }

    /// Create a future that is in the 'fulfilled' state and holds an immediate result.
    template<typename... Args>
    [[nodiscard]] static Future createImmediateEmplace(Args&&... args) {
        return Promise<R>::createImmediateEmplace(std::forward<Args>(args)...);
    }

    /// Creates a future that is in the 'exception' state.
    [[nodiscard]] static Future createFailed(const Exception& ex) {
        return Promise<R>::createFailed(ex);
    }

    /// Creates a future that is in the 'exception' state.
    [[nodiscard]] static Future createFailed(Exception&& ex) {
        return Promise<R>::createFailed(std::move(ex));
    }

    /// Creates a future that is in the 'exception' state.
    [[nodiscard]] static Future createFailed(std::exception_ptr ex_ptr) {
        return Promise<R>::createFailed(std::move(ex_ptr));
    }

    /// Create a new Future that is associated with the given task object.
    [[nodiscard]] static Future createFromTask(TaskPtr task) {
        OVITO_ASSERT(task);
        OVITO_ASSERT(task->_resultsStorage != nullptr || (std::is_void_v<R>));
        return Future(std::move(task));
    }

    /// Returns the results computed by the associated Promise.
    /// This function may only be called if the future is in the 'fulfilled' state.
    template<typename R2 = R>
    [[nodiscard]] std::enable_if_t<!std::is_void_v<R2>, R> result() {
        OVITO_ASSERT_MSG(*this, "Future::results()", "Future must be valid.");
        OVITO_ASSERT_MSG(isFinished(), "Future::results()", "Future must be in fulfilled state.");
        OVITO_ASSERT_MSG(!isCanceled(), "Future::results()", "Future must not be canceled.");
        auto taskDep = takeTaskDependency();
        OVITO_ASSERT(!*this);
        taskDep->throwPossibleException();
        return taskDep->template takeResult<R>();
    }

    /// Blocks until the results of this future become available and returns them.
    template<typename R2 = R>
    [[nodiscard]] std::enable_if_t<!std::is_void_v<R2>, R> blockForResult() {
        OVITO_ASSERT_MSG(*this, "Future::blockForResult()", "Future must be valid.");
        waitForFinished();
        return result();
    }

    /// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
    /// The provided continuation function must accept the results of this future or the future itself as an input parameter.
    template<typename Executor, typename Function>
    [[nodiscard]] detail::continuation_future_type<Function, Future>
    then(Executor&& executor, Function&& f);

    /// Overload of the function above using the inline executor.
    template<typename Function>
    [[nodiscard]] decltype(auto) then(Function&& f) { return then(InlineExecutor{}, std::forward<Function>(f)); }

    /// Applies a post-processing function to the future's results, which must returns the same type of value
    /// as the original future. The post-processing function is executed once the future is fulfilled.
    /// Calling postprocess() is equivalent to calling then() and then replacing the parent future with the
    /// continuation future.
    template<typename Executor, typename Function>
    void postprocess(Executor&& executor, Function&& f) {
        *this = then(std::forward<Executor>(executor), std::forward<Function>(f));
    }

protected:

    /// Move constructor taking the promise state pointer from a r-value Promise.
    Future(Promise<R>&& promise) : FutureBase(std::move(promise._task)) {}

    template<typename R2> friend class Future;
    template<typename R2> friend class Promise;
    template<typename R2> friend class SharedFuture;
};

/// Returns a new future that, upon the fulfillment of this future, will be fulfilled by running the given continuation function.
/// The provided continuation function must accept the results of this future as an input parameter.
template<typename R>
template<typename Executor, typename Function>
detail::continuation_future_type<Function, Future<R>>
Future<R>::then(Executor&& executor, Function&& f)
{
    // Infer the exact future & task types to create.
    using result_future_type = detail::continuation_future_type<Function, Future<R>>;
    using continuation_task_type = detail::ContinuationTask<typename result_future_type::result_type>;

    // This future must be valid for then() to work.
    OVITO_ASSERT_MSG(*this, "Future::then()", "Future must be valid.");

    class ThenTask : public continuation_task_type
    {
    public:

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = result_future_type;

        /// Constructor.
        explicit ThenTask(Function&& function) :
            _function(std::forward<Function>(function)) {}

        /// Starts execution of the task.
        void operator()(detail::TaskDependency awaitedTask, Executor&& executor) {
            this->template whenTaskFinishes<ThenTask, &ThenTask::awaitedTaskFinished>(
                std::move(awaitedTask),
                std::forward<Executor>(executor),
                this->shared_from_this());
        }

        /// Callback to be invoked when the awaited task has finished.
        void awaitedTaskFinished(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            OVITO_ASSERT(finishedTask->isFinished());
            OVITO_ASSERT(!this->isFinished() || this->isCanceled());

            // Don't execute continuation function in case an error occurred in the preceding task and unless the continuation function takes a Future.
            // Forward any preceding exception state directly to the continuation task.
            if constexpr(!std::is_invocable_v<Function, Future<R>>) {
                if(finishedTask->exceptionStore()) {
                    {
                        Task::MutexLock lock(*this);
                        this->exceptionLocked(finishedTask->exceptionStore());
                        this->finishLocked(lock);
                    }
                    promise.takeTask();
                    return;
                }
            }

            // Now it's time to execute the continuation function supplied by the user.
            // This assigns the function's return value as result of this continuation task.
            this->fulfillWith(std::move(promise), std::move(_function), Future<R>(std::move(finishedTask)));
        }

    private:

        /// The caller's continuation function to be executed once the awaited task completes.
        std::decay_t<Function> _function;
    };

    return launchTask(
        std::make_shared<ThenTask>(std::forward<Function>(f)),
        this->takeTaskDependency(),
        std::forward<Executor>(executor));
}

}   // End of namespace
