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

namespace Ovito {

template<typename R>
class CoroutineTask : public detail::ContinuationTask<R>
{
public:

    /// This is used by the launchTask() utility function.
    using future_type = Future<R>;

    /// Constructor.
    CoroutineTask(std::coroutine_handle<CoroutinePromise<R>> handle) : _handle(handle) {}

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
    std::coroutine_handle<CoroutinePromise<R>> _handle;
};

template<typename Executor, typename FutureType>
class FutureAwaiter
{
    Q_DISABLE_COPY_MOVE(FutureAwaiter)

public:

    explicit FutureAwaiter(Executor&& executor, FutureType&& future) noexcept : _executor(std::forward<Executor>(executor)), _future(std::forward<FutureType>(future)) {
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

    template<typename R>
    void await_suspend(std::coroutine_handle<CoroutinePromise<R>> handle) {
        OVITO_ASSERT(_future);
        auto coroTask = handle.promise().coroTask();
        OVITO_ASSERT(coroTask);
        coroTask->whenTaskFinishes(_future.takeTaskDependency(), std::move(_executor), std::move(handle.promise()), [this](PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            _future = std::decay_t<FutureType>(std::move(finishedTask));
            auto coroTask = static_cast<CoroutineTask<R>*>(promise.task().get());
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

    std::decay_t<FutureType> _future;
    std::decay_t<Executor> _executor;
};

template<typename R>
class CoroutinePromiseBase : public PromiseBase
{
public:

    /// Returns the coroutine task associated with this promise.
    CoroutineTask<R>* coroTask() noexcept {
        OVITO_ASSERT(*this);
        return static_cast<CoroutineTask<R>*>(PromiseBase::task().get());
    }

    /// Creates the object that will be returned to the caller of the coroutine.
    Future<R> get_return_object() {
        // Create the task object associated with the coroutine.
        auto coroTask = std::make_shared<CoroutineTask<R>>(std::coroutine_handle<CoroutinePromise<R>>::from_promise(static_cast<CoroutinePromise<R>&>(*this)));
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

template<typename R>
class CoroutinePromise : public CoroutinePromiseBase<R>
{
public:
    /// Sets the result value of the coroutine.
    template<typename R2>
    void return_value(R2&& value) noexcept {
        this->coroTask()->setResult(std::forward<R2>(value));
        PromiseBase::setFinished();
    }
};

template<>
class CoroutinePromise<void> : public CoroutinePromiseBase<void>
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
auto operator co_await(SharedFuture<T>&& future) noexcept
{
    return FutureAwaiter<InlineExecutor, SharedFuture<T>>(InlineExecutor{}, std::move(future));
}

}   // End of namespace
