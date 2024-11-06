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
#include "InlineExecutor.h"
#include "LaunchTask.h"
#include "detail/ContinuationTask.h"
#include "detail/FutureDetail.h"

namespace Ovito {

template<typename R, bool StructuredConcurrency>
class CoroutineTask : public detail::ContinuationTask<R>
{
public:

    /// This is used by the launchTask() utility function.
    using future_type = std::conditional_t<StructuredConcurrency, SCFuture<R>, Future<R>>;

    /// Constructor.
    CoroutineTask(std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>> handle) : _handle(handle) {}

    /// Destructor.
    ~CoroutineTask() {
        // Free the coroutine state if it is still attached to this task.
        if(_handle)
            _handle.destroy();
    }

    /// Detaches the coroutine state from this task after the coroutine has finished running.
    void detachFromCoroutine() noexcept {
        _handle = {};
    }

    /// Resumes the coroutine associated with this task.
    void resumeCoroutine(PromiseBase promise) noexcept {
        OVITO_ASSERT(promise);
        OVITO_ASSERT(_handle);
        OVITO_ASSERT(!_handle.promise());
        OVITO_ASSERT(!this->isFinished());
        static_cast<PromiseBase&>(_handle.promise()) = std::move(promise);
        OVITO_ASSERT(_handle.promise().task().get() == this);
        Task::Scope taskScope(this);
        _handle.resume();
    }

private:

    /// The handle to the coroutine associated with this task.
    std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>> _handle;
};

template<typename Executor, typename FutureType>
class FutureAwaiter
{
    Q_DISABLE_COPY_MOVE(FutureAwaiter)

public:

    /// Enable structured concurrency mode if the future type is a SCFuture.
    static constexpr bool UsingStructuredConcurrency = detail::is_structured_future_v<FutureType>;

    explicit FutureAwaiter(Executor&& executor, FutureType future) noexcept : _executor(std::forward<Executor>(executor)), _future(std::move(future)) {
        OVITO_ASSERT(_future);
    }

    bool await_ready() const noexcept {
        if constexpr(std::is_same_v<std::decay_t<Executor>, InlineExecutor>) {
            return _future.isFinished();
        }
        else {
            return false;
        }
    }

    template<typename R, bool SC>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R, SC>> handle) {
        OVITO_ASSERT(_future);
        auto coroTask = handle.promise().coroTask();
        OVITO_ASSERT(coroTask);
        coroTask->template whenTaskFinishes<UsingStructuredConcurrency>(_future.takeTaskDependency(), std::move(_executor), std::move(handle.promise()), [this](PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            _future = FutureType{std::move(finishedTask)};
            auto coroTask = static_cast<CoroutineTask<R, SC>*>(promise.task().get());
            if(!coroTask->isCanceled())
                coroTask->resumeCoroutine(std::move(promise));
        });
    }

    decltype(auto) await_resume() {
        OVITO_ASSERT(_future.isFinished());
        if constexpr(!std::is_same_v<typename FutureType::result_type, void>)
            return std::move(_future).result();
        else
            std::move(_future).takeTaskDependency()->throwPossibleException();
    }

private:

    FutureType _future;
    std::decay_t<Executor> _executor;
};

template<typename Executor>
class ExecutorAwaiter
{
    Q_DISABLE_COPY_MOVE(ExecutorAwaiter)

public:

    explicit ExecutorAwaiter(Executor&& executor) noexcept : _executor(std::forward<Executor>(executor)) {}

    bool await_ready() const noexcept { return false; }

    template<typename R, bool SC>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R, SC>> handle) {
        auto coroTask = handle.promise().coroTask();
        OVITO_ASSERT(coroTask);
        std::move(_executor).execute([promise = std::move(handle.promise())]() mutable noexcept {
            auto coroTask = static_cast<CoroutineTask<R, SC>*>(promise.task().get());
            if(!coroTask->isCanceled())
                coroTask->resumeCoroutine(std::move(promise));
        });
    }

    void await_resume() {}

private:

    std::decay_t<Executor> _executor;
};

template<typename R, bool StructuredConcurrency>
class CoroutinePromiseBase : public PromiseBase
{
public:

    /// Returns the coroutine task associated with this promise.
    CoroutineTask<R, StructuredConcurrency>* coroTask() noexcept {
        OVITO_ASSERT(*this);
        return static_cast<CoroutineTask<R, StructuredConcurrency>*>(PromiseBase::task().get());
    }

    /// Creates the object that will be returned to the caller of the coroutine.
    std::conditional_t<StructuredConcurrency, SCFuture<R>, Future<R>> get_return_object() {
        // Create the task object associated with the coroutine.
        auto coroTask = std::make_shared<CoroutineTask<R, StructuredConcurrency>>(std::coroutine_handle<CoroutinePromise<R, StructuredConcurrency>>::from_promise(static_cast<CoroutinePromise<R, StructuredConcurrency>&>(*this)));
        this->_task = coroTask;
        return launchTask(std::move(coroTask));
    }

    /// Gets called when coroutine throws an exception and nothing catches it.
    void unhandled_exception() noexcept {
        PromiseBase::captureExceptionAndFinish();
    }

    /// Make it an eagerly-started coroutine.
    std::suspend_never initial_suspend() noexcept { return {}; }

    /// Gets called when the coroutine terminates by any means.
    std::suspend_never final_suspend() noexcept {
        OVITO_ASSERT(*this);
        coroTask()->detachFromCoroutine();
        return {};
    }
};

template<typename R, bool StructuredConcurrency>
class CoroutinePromise : public CoroutinePromiseBase<R, StructuredConcurrency>
{
public:
    /// Sets the result value of the coroutine.
    template<typename R2>
    void return_value(R2&& value) noexcept {
        this->coroTask()->setResult(std::forward<R2>(value));
        PromiseBase::setFinished();
    }
};

template<bool StructuredConcurrency>
class CoroutinePromise<void, StructuredConcurrency> : public CoroutinePromiseBase<void, StructuredConcurrency>
{
public:
    /// Completes the coroutine.
    void return_void() noexcept {
        PromiseBase::setFinished();
    }
};

template<typename T>
auto operator co_await(Future<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, Future<T>>(InlineExecutor{}, std::move(future));
}

template<typename T>
auto operator co_await(SCFuture<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, SCFuture<T>>(InlineExecutor{}, std::move(future));
}

template<typename T>
auto operator co_await(SharedFuture<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, SharedFuture<T>>(InlineExecutor{}, std::move(future));
}

}   // End of namespace
