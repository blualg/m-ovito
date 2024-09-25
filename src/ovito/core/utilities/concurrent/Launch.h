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
#include "AsynchronousTask.h"
#include "detail/Latch.h"
#include "detail/TaskWithStorage.h"
#include "detail/ContinuationTask.h"
#include "Future.h"
#include "LaunchTask.h"

namespace Ovito {

/// Runs the given function using the given executor and returns its results as a Future.
/// Note: The function may never execute if the future gets canceled before execution begins.
template<typename Executor, typename Function>
[[nodiscard]] auto launchAsync(Executor&& executor, Function&& function) {

    // Infer the future to create. If the function returns a future, use it as is. Otherwise, wrap the result in a Future.
    using result_future_type = std::conditional_t<detail::is_future_v<std::invoke_result_t<Function>>,
                                                std::invoke_result_t<Function>,
                                                Future<std::invoke_result_t<Function>>>;

    // Determine the type of task object to use.
    using base_task_type = std::conditional_t<detail::is_future_v<std::invoke_result_t<Function>>,
                detail::ContinuationTask<typename result_future_type::result_type, typename Executor::base_task_type>,
                detail::TaskWithStorage<std::invoke_result_t<Function>, typename Executor::base_task_type>>;

    class LaunchTask : public base_task_type
    {
    public:
        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = result_future_type;

        /// Constructor.
        explicit LaunchTask(Function&& function) :
            base_task_type(Task::NoState),
            _function(std::forward<Function>(function)) {}

        /// Starts execution of the task.
        void operator()(Executor&& executor) {
            std::forward<Executor>(executor).execute([promise = typename result_future_type::promise_type(this->shared_from_this())]() mutable noexcept {
                static_cast<LaunchTask*>(promise.task().get())->invokeFunction(std::move(promise));
            });
        }

        /// Runs the user function.
        void invokeFunction(typename result_future_type::promise_type promise) noexcept {
            if(promise.isCanceled())
                return;
            try {
                Task::Scope taskScope(this);
                if constexpr(!detail::is_future_v<std::invoke_result_t<Function>>) {
                    if constexpr(!std::is_void_v<std::invoke_result_t<Function>>)
                        promise.setResult(std::invoke(std::move(_function)));
                    else
                        std::invoke(std::move(_function));
                    promise.setFinished();
                }
                else {
                    this->handleUnwrappedFuture(std::move(promise), std::invoke(std::move(_function)));
                }
            }
            catch(const OperationCanceled&) {}
            catch(...) {
                OVITO_ASSERT(!promise.isFinished());
                promise.captureExceptionAndFinish();
            }
        }

    private:
        /// The function to be executed.
        std::decay_t<Function> _function;
    };

    return launchTask(
        std::make_shared<LaunchTask>(std::forward<Function>(function)),
        std::forward<Executor>(executor));
}

/// Schedules the given function for execution in a worker thread.
template<typename Function>
[[nodiscard]] inline auto asyncLaunch(Function&& f)
{
    using R = std::invoke_result_t<Function>;
    class PackagedTask : public AsynchronousTask<R>
    {
    public:
        explicit PackagedTask(Function&& f) : _func(std::forward<Function>(f)) {}
        virtual void perform() override {
            if constexpr(!std::is_void_v<R>)
                this->setResult(std::invoke(std::move(_func)));
            else
                std::invoke(std::move(_func));
        }
    private:
        std::decay_t<Function> _func;
    };
    return launchTask(std::make_shared<PackagedTask>(std::forward<Function>(f)));
}

/// Executes the given worker function in a worker thread and waits for the result.
/// This function blocks until the worker function has finished executing (even if the waiting task gets canceled).
/// Thus, it's safe to use if the worker function is a lambda capturing some local variables by reference.
template<typename Function>
inline auto asyncLaunchAndJoin(Function&& f)
{
    // Launch the function in a worker thread.
    auto future = asyncLaunch(std::forward<Function>(f));

    // Waits until the task has finished executing (but do not return early when canceled but not yet finished).
    future.waitForFinished(false);

    // Return the result of the function to the caller, if any.
    if constexpr(!std::is_void_v<std::invoke_result_t<Function>>) {
        return std::move(future).result();
    }
}

}   // End of namespace
