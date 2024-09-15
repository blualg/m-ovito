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

namespace Ovito {

class OVITO_CORE_EXPORT AsynchronousTaskBase : public Task, public QRunnable
{
public:

    /// Constructor.
    AsynchronousTaskBase(OORef<UserInterface> ui, State initialState, void* resultsStorage) noexcept;

    /// Destructor.
    ~AsynchronousTaskBase();

    /// Returns the thread pool this task has been submitted to for execution (if any).
    QThreadPool* threadPool() const { return _threadPool; }

    /// Performs the work of the task.
    virtual void perform() = 0;

    /// Schedules the asynchronous task for execution in a thread pool.
    /// This function is called by the launchTask() helper.
    void operator()();

private:

    /// Implementation of QRunnable interface.
    virtual void run() final override;

    /// A shared pointer to the task itself, which is used to keep the C++ object alive
    /// while the task is transferred to and executed in a thread pool.
    TaskPtr _thisTask;

    /// The thread pool this task has been submitted to for execution (if any).
    QThreadPool* _threadPool = nullptr;

    friend class Task; // Allow Task::waitFor() to invoke the run() method directly (as part of the work-stealing mechanism).
};

template<typename R>
class AsynchronousTask : public detail::TaskWithStorage<R, AsynchronousTaskBase>
{
public:

    /// The type of future associated with this task type. This typedef is used by the launchTask() function.
    using future_type = Future<R>;

    /// Constructor
    AsynchronousTask() : detail::TaskWithStorage<R, AsynchronousTaskBase>(
        this_task::ui(),
        this_task::get()->isHighPriorityTask() ? Task::HighPriority : Task::NoState,
        std::nullopt) {}
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
