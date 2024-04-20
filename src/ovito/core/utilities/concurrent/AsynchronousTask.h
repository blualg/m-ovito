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
#include "Future.h"
#include "TaskManager.h"

namespace Ovito {

class OVITO_CORE_EXPORT AsynchronousTaskBase : public ProgressingTask, public QRunnable
{
public:

    /// Constructor.
    AsynchronousTaskBase(State initialState, void* resultsStorage) noexcept;

    /// Destructor.
    ~AsynchronousTaskBase();

    /// Returns the thread pool this task has been submitted to for execution (if any).
    QThreadPool* threadPool() const { return _threadPool; }

    /// This virtual function is responsible for computing the results of the task.
    virtual void perform() = 0;

private:

    /// Implementation of QRunnable.
    virtual void run() final override;

    /// Submits the task for execution to a thread pool.
    void runInThreadPool(bool showInUserInterface);

    /// Runs the task's work function immediately in the current thread.
    void runInThisThread(bool showInUserInterface);

    /// A shared pointer to the task itself, which is used to keep the C++ object alive
    /// while the task is transferred to and executed in a thread pool.
    TaskPtr _thisTask;

    /// The thread pool this task has been submitted to for execution (if any).
    QThreadPool* _threadPool = nullptr;

    /// The execution context that this task inherits from its parent task.
    ExecutionContext _executionContext;

    friend class Task;
    template<typename R> friend class AsynchronousTask;
};

template<typename R>
class AsynchronousTask : public detail::TaskWithStorage<R, AsynchronousTaskBase>
{
public:

    /// Constructor
    AsynchronousTask() : detail::TaskWithStorage<R, AsynchronousTaskBase>(this_task::get()->isHighPriorityTask() ? Task::HighPriority : Task::NoState, std::nullopt) {}

    /// Schedules the task for execution in a thread pool and returns a future for the task's results.
    Future<R> launch(bool showInUserInterface) {
#ifndef OVITO_DISABLE_THREADING
        // Submit the task for execution in a worker thread.
        this->runInThreadPool(showInUserInterface);
        return Future<R>::createFromTask(this->shared_from_this());
#else
        // If multi-threading is not available, run the task immediately.
        this->runInThisThread(showInUserInterface);
        return Future<R>::createFromTask(this->shared_from_this());
#endif
    }

    /// Sets the result value of the task.
    template<typename R2>
    void setResult(R2&& result) {
        Task::setResult<R>(std::forward<R2>(result));
    }
};

}   // End of namespace


#if !defined(OVITO_BUILD_MONOLITHIC) && defined(Q_OS_WIN)

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
