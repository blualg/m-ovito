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
#include "detail/TaskWithStorage.h"
#include "detail/Latch.h"
#include "Future.h"
#include "TaskManager.h"

namespace Ovito {

class OVITO_CORE_EXPORT AsynchronousTaskBase : public ProgressingTask, public QRunnable
{
public:

    /// Constructor.
    AsynchronousTaskBase(State initialState = NoState, void* resultsStorage = nullptr) noexcept;

    /// Destructor.
    ~AsynchronousTaskBase();

    /// Returns the thread pool this task has been submitted to for execution (if any).
    QThreadPool* threadPool() const { return _submittedToPool; }

    /// This virtual function is responsible for computing the results of the task.
    virtual void perform() = 0;

private:

    /// Implementation of QRunnable.
    virtual void run() final override;

    /// Submits the task for execution to a thread pool.
    void startInThreadPool(bool showInUserInterface);

    /// Runs the task's work function immediately in the current thread.
    void startInThisThread(bool showInUserInterface);

    /// A shared pointer to the task itself, which is used to keep the C++ object alive
    /// while the task is transferred to and executed in a thread pool.
    TaskPtr _thisTask;

    /// The thread pool this task has been submitted to for execution (if any).
    QThreadPool* _submittedToPool = nullptr;

    /// The execution context that this task inherits from its parent task.
    ExecutionContext _executionContext;

    friend class Task;
    template<typename... R> friend class AsynchronousTask;
};

template<typename... R>
class AsynchronousTask : public detail::TaskWithStorage<std::tuple<R...>, AsynchronousTaskBase>
{
public:

    /// Schedules the task for execution in the global thread pool and returns a future for the task's results.
    Future<R...> runAsync(bool showInUserInterface) {
#ifndef OVITO_DISABLE_THREADING
        // Submit the task for execution in a worker thread.
        this->startInThreadPool(showInUserInterface);
        return Future<R...>::createFromTask(this->shared_from_this());
#else
        // If multi-threading is not available, run the task immediately.
        return runImmediately();
#endif
    }

    /// Schedules the given function for execution in a worker thread.
    template<typename Function>
    static Future<R...> runAsync(Task::AsynchronousTaskType asyncTaskType, Function&& f, bool showInUserInterface = false) {
        class FuncAsyncTask : public AsynchronousTask {
        public:
            FuncAsyncTask(Function&& f) : _func(std::forward<Function>(f)) {}
            virtual void perform() override {
                if constexpr(std::tuple_size_v<typename Future<R...>::tuple_type> != 0)
                    setResult(std::move(_func)());
                else
                    std::move(_func)();
            }
        private:
            std::decay_t<Function> _func;
        };
        auto task = std::make_shared<FuncAsyncTask>(std::forward<Function>(f));
        task->setAsyncTaskType(asyncTaskType);
        return task->runAsync(showInUserInterface);
    }

    /// Schedules the given function for execution in a worker thread.
    template<typename Function>
    static Future<R...> runAsync(Function&& f, bool showInUserInterface = false) {
        Task::AsynchronousTaskType asyncTaskType = this_task::get() ? this_task::get()->asyncTaskType() : Task::AsynchronousTaskType::DefaultAsyncTask;
        return runAsync(asyncTaskType, std::forward<Function>(f), showInUserInterface);
    }

    /// Executes the given function in a worker thread and waits for the result.
    /// This function is blocking and should be used when the lambda function captures some
    /// local variables by reference.
    template<typename Function>
    static auto runAsyncAndJoin(Function&& f, bool showInUserInterface = false) {
        detail::Latch latch(1);
        Future<R...> future = runAsync([&latch, f = std::forward<Function>(f)]() mutable {
            try {
                if constexpr(sizeof...(R) != 0) {
                    auto result = std::move(f)();
                    latch.count_down();
                    return result;
                }
                else {
                    std::move(f)();
                    latch.count_down();
                }
            }
            catch(...) {
                latch.count_down();
                throw;
            }
        }, showInUserInterface);
        try {
            if constexpr(sizeof...(R) == 1) {
                auto result = future.result();
                latch.wait();
                return result;
            }
            else {
                future.waitForFinished();
                latch.wait();
            }
        }
        catch(...) {
            latch.wait();
            throw;
        }
    }

    /// Runs the task in place and returns a future for the task's results.
    Future<R...> runImmediately(bool showInUserInterface) {
        this->startInThisThread(showInUserInterface);
        return Future<R...>::createFromTask(this->shared_from_this());
    }

    /// Sets the result value of the task.
    template<typename... R2>
    void setResult(R2&&... result) {
        this->template setResults<std::tuple<R...>>(std::forward_as_tuple(std::forward<R2>(result)...));
    }
};

}   // End of namespace


#ifndef OVITO_BUILD_MONOLITHIC

#include <ovito/core/dataset/pipeline/PipelineFlowState.h>

namespace Ovito {

// Instantiate class template to export it from the core DLL.
#if !defined(Core_EXPORTS)
    extern template class OVITO_CORE_EXPORT AsynchronousTask<PipelineFlowState>;
#elif !defined(Q_CC_MSVC) && !defined(Q_CC_CLANG)
    template class OVITO_CORE_EXPORT AsynchronousTask<PipelineFlowState>;
#endif

}   // End of namespace

#endif
