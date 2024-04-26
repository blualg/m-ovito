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
#include "Future.h"
#include "LaunchTask.h"

namespace Ovito {

/// Schedules the given function for execution in a worker thread.
template<typename Function>
[[nodiscard]] inline auto asyncLaunch(Function&& f)
{
    using R = std::invoke_result_t<Function>;
    class PackagedTask : public AsynchronousTask<R>
    {
    public:
        /// The type of future associated with this task type. This typedef is used by the launchTask() function.
        using future_type = Future<R>;
        /// Constructor.
        PackagedTask(Function&& f) : _func(std::forward<Function>(f)) {}
        /// Worker thread entry point.
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

/// Executes the given function in a worker thread and waits for the result.
/// This function blocks and should be used whenever the lambda function captures some
/// local variables by reference.
template<typename Function>
inline auto asyncLaunchAndJoin(Function&& f) {
    auto future = asyncLaunch(std::forward<Function>(f));
    future.waitForFinished(false); // This waits until the task has finished executing (does not return early when canceled but not finished).
    if constexpr(!std::is_void_v<std::invoke_result_t<Function>>)
        return std::move(future).result();
}

/// Schedules the given function for execution with the given executor.
template<typename Executor, typename Function>
[[nodiscard]] inline auto executorLaunch(Executor&& executor, Function&& f)
{
    using R = std::invoke_result_t<Function>;
    class PackagedTask : public detail::TaskWithStorage<R>
    {
    public:
        /// The type of future associated with this task type. This typedef is used by the launchTask() function.
        using future_type = Future<R>;
        /// Constructor.
        PackagedTask(Function&& f) : detail::TaskWithStorage<R>(Task::NoState), _func(std::forward<Function>(f)) {}
        /// Start routine invoked by launchTask().
        void operator()(Executor&& executor) {
            // Let the executor invoke our run() method. Use a promise to properly finish the task
            // in case the executor does never call run().
            std::forward<Executor>(executor).execute([promise = Promise<R>(this->shared_from_this())]() noexcept {
                static_cast<PackagedTask*>(promise.task().get())->run();
            });
        }
        /// Execution routine.
        void run() noexcept {
            try {
                if(!this->isCanceled()) {
                    Task::Scope taskScope(this);
                    if constexpr(!std::is_void_v<R>)
                        this->setResult(std::invoke(std::move(_func)));
                    else
                        std::invoke(std::move(_func));
                }
                this->setFinished();
            }
            catch(...) {
                this->captureExceptionAndFinish();
            }
        }
    private:
        std::decay_t<Function> _func;
    };
    return launchTask(std::make_shared<PackagedTask>(std::forward<Function>(f)), std::forward<Executor>(executor));
}

}   // End of namespace
