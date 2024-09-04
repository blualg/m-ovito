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

namespace Ovito::detail {

/**
 * \brief The type of task that is returned by the Future::then() method.
 */
template<typename R>
class ContinuationTask : public TaskWithStorage<R>, public TaskAwaiter
{
public:

    /// Delegating constructor.
    explicit ContinuationTask(Task::State initialState) noexcept : ContinuationTask(initialState, std::nullopt) {}

    /// Constructor initializing the results storage.
    template<typename InitialValue>
    explicit ContinuationTask(Task::State initialState, InitialValue&& initialResult) noexcept :
            TaskWithStorage<R>(initialState, std::forward<InitialValue>(initialResult)),
            TaskAwaiter(static_cast<Task&>(*this))
    {
        OVITO_ASSERT(!(initialState & (Task::Canceled | Task::Finished)));
    }

    /// Sets the result of this task upon completion of the preceding task.
    template<typename Function, typename FutureType>
    void fulfillWith(PromiseBase&& promise, Function&& f, FutureType&& future) noexcept {
        OVITO_ASSERT(!awaitedTask());
        OVITO_ASSERT(!this->isFinished());
        OVITO_ASSERT(promise.task().get() == this);
        OVITO_ASSERT(future.isValid() && future.isFinished());
        OVITO_ASSERT(!this->isFinished());

        // Execute the continuation function in the scope of this task object.
        Task::Scope taskScope(this);

        // Inspect return value type of the continuation function.
        if constexpr(!detail::returns_future_v<Function, FutureType>) {
            // Continuation function returns a result value or void.
            try {
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
            catch(...) {
                this->captureExceptionAndFinish();
            }
        }
        else {
            // The continuation function returns a new future, whose result will be used to fulfill this task.
            std::decay_t<callable_result_t<Function, FutureType>> nextFuture;
            try {
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
            }
            catch(...) {
                this->captureExceptionAndFinish();
                return;
            }
            OVITO_ASSERT(nextFuture.isValid());

            whenTaskFinishes<ContinuationTask, &ContinuationTask::finalResultsAvailable<is_shared_future_v<decltype(nextFuture)>>>(
                nextFuture.takeTaskDependency(),
                InlineExecutor{},
                std::move(promise));
        }
    }

private:

    /// Callback function which gets invoked once the unwrapped future has completed.
    template<bool IsSharedFuture>
    void finalResultsAvailable(PromiseBase promise) noexcept {
        // Lock access to this task object.
        Task::MutexLock lock(*this);

        // Get the task that did just finish.
        TaskDependency finishedTask = takeAwaitedTask();

        // Bail out if the preceding task has been canceled, or if the continuation has been canceled.
        if(!finishedTask || finishedTask->isCanceled()) {
            return; // Note: The Promise's destructor automatically puts the continuation task into 'canceled' and 'finished' states if it isn't already.
        }

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
