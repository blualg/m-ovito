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
#include "TaskWithStorage.h"
#include "TaskAwaiter.h"
#include "../InlineExecutor.h"
#include "../Promise.h"

namespace Ovito::detail {

/**
 * \brief The type of task that is returned by the Future::then() method.
 */
template<typename R, class TaskBase = Task>
class ContinuationTask : public TaskWithStorage<R, TaskBase>, public TaskAwaiter
{
public:

    /// Delegating constructor.
    explicit ContinuationTask(Task::State initialState = Task::NoState) noexcept : ContinuationTask(initialState, std::nullopt) {}

    /// Constructor initializing the results storage.
    template<typename InitialValue>
    explicit ContinuationTask(Task::State initialState, InitialValue&& initialResult) noexcept :
            TaskWithStorage<R, TaskBase>(initialState, std::forward<InitialValue>(initialResult)),
            TaskAwaiter(static_cast<Task&>(*this))
    {
        OVITO_ASSERT(!(initialState & (Task::Canceled | Task::Finished)));
    }

    /// Sets the result of this task upon completion of the preceding task.
    template<typename Function, typename FutureType>
    void fulfillWith(PromiseBase&& promise, Function&& f, FutureType&& future) noexcept {
        OVITO_ASSERT(!awaitedTask());
        OVITO_ASSERT(promise.task().get() == this);
        OVITO_ASSERT(future.isFinished());

        // Skip the continuation if the task has been canceled.
        if(this->isCanceled())
            return;

        try {
            // Execute the continuation function in the scope of this task object.
            Task::Scope taskScope(this);

            // Inspect return value type of the continuation function.
            if constexpr(!detail::returns_future_v<Function, FutureType>) {
                // Continuation function returns a result value or void.
                if constexpr(!detail::returns_void_v<Function, FutureType>) {
                    // Function returns non-void results.
                    if constexpr(!std::is_invocable_v<Function, FutureType>)
                        if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                            if constexpr(is_shared_future_v<FutureType>)
                                this->setResult(std::invoke(std::forward<Function>(f), future.task()->template getResult<typename FutureType::result_type>()));
                            else
                                this->setResult(std::invoke(std::forward<Function>(f), future.task()->template takeResult<typename FutureType::result_type>()));
                        }
                        else {
                            this->setResult(std::invoke(std::forward<Function>(f)));
                        }
                    else
                        this->setResult(std::invoke(std::forward<Function>(f), std::forward<FutureType>(future)));
                }
                else {
                    // Function returns void.
                    if constexpr(!std::is_invocable_v<Function, FutureType>) {
                        if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                            if constexpr(is_shared_future_v<FutureType>)
                                std::invoke(std::forward<Function>(f), future.task()->template getResult<typename FutureType::result_type>());
                            else
                                std::invoke(std::forward<Function>(f), future.task()->template takeResult<typename FutureType::result_type>());
                        }
                        else {
                            std::invoke(std::forward<Function>(f));
                        }
                    }
                    else {
                        std::invoke(std::forward<Function>(f), std::forward<FutureType>(future));
                    }
                }
                this->setFinished();
            }
            else {
                // The continuation function returns a new future, whose result will be used to fulfill this task.
                std::decay_t<callable_result_t<Function, FutureType>> nextFuture;
                // Call the continuation function with the results of the finished task or the finished future itself.
                if constexpr(!std::is_invocable_v<Function, FutureType>) {
                    if constexpr(!std::is_void_v<typename FutureType::result_type>) {
                        nextFuture = std::invoke(std::forward<Function>(f), std::forward<FutureType>(future).result());
                    }
                    else {
                        std::forward<FutureType>(future).waitForFinished();
                        nextFuture = std::invoke(std::forward<Function>(f));
                    }
                }
                else {
                    nextFuture = std::invoke(std::forward<Function>(f), std::forward<FutureType>(future));
                }
                handleUnwrappedFuture(std::move(promise), std::move(nextFuture));
            }
        }
        catch(...) {
            this->captureExceptionAndFinish();
        }
    }

protected:

    /// Uses the results of the given future to fulfill this task.
    void handleUnwrappedFuture(PromiseBase promise, auto&& future) noexcept {
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 11
        // Workaround for a deficiency in GCC 10: The compiler stops with the error "not a valid template argument...must be a pointer-to-member of the form ‘&X::Y",
        // because finalResultsAvailable() is a template member function. To work around this, we define two non-templated helper functions that forward to the templated one.
        if constexpr(is_shared_future_v<decltype(future)>)
            whenTaskFinishes<ContinuationTask, &ContinuationTask::finalResultsAvailableShared>(
                std::move(future),
                InlineExecutor{},
                std::move(promise));
        else
            whenTaskFinishes<ContinuationTask, &ContinuationTask::finalResultsAvailableExclusive>(
                std::move(future),
                InlineExecutor{},
                std::move(promise));
#else
        // Schedule the continuation task to run once the new future completes.
        // We are passing the type of the future (Future or SharedFuture) to the callback routine via a template parameter,
        // because this information would otherwise get lost when we unpack the task dependency from the future.
        whenTaskFinishes<ContinuationTask, &ContinuationTask::finalResultsAvailable<is_shared_future_v<decltype(future)>>>(
            std::move(future),
            InlineExecutor{},
            std::move(promise));
#endif
    }

private:

    // Workaround for a deficiency in GCC 10 and older.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 11
    void finalResultsAvailableShared(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
        finalResultsAvailable<true>(std::move(promise), std::move(finishedTask));
    }
    void finalResultsAvailableExclusive(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
        finalResultsAvailable<false>(std::move(promise), std::move(finishedTask));
    }
#endif

    /// Callback function which gets invoked once the unwrapped future has completed.
    template<bool IsSharedFuture>
    void finalResultsAvailable(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
        // Lock access to this task.
        Task::MutexLock lock(*this);

        // There is a small chance that the continuation task was canceled in the meantime but hasn't let go of the awaited task yet
        // (because finishing and running the registered continuation functions is not an atomic operation).
        // We need to check for this situation here and bail out if it happened.
        if(this->isFinished())
            return;

        // If the awaited task failed, inherit the error state.
        if(finishedTask->exceptionStore()) {
            this->exceptionLocked(finishedTask->exceptionStore());
        }
        else {
            // Adopt result value from the completed task.
            if constexpr(!std::is_void_v<R>) {
                if constexpr(IsSharedFuture)
                    this->setResult(finishedTask->template getResult<R>());
                else
                    this->setResult(finishedTask->template takeResult<R>());
            }
        }

        this->finishLocked(lock);
    }
};

} // End of namespace
