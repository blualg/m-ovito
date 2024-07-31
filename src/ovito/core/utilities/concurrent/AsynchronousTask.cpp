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

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include "AsynchronousTask.h"

#if defined(Q_OS_WIN) && (defined(Q_CC_MSVC) || defined(Q_CC_CLANG) || defined(OVITO_BUILD_MONOLITHIC))
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#endif

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AsynchronousTaskBase::AsynchronousTaskBase(State initialState, void* resultsStorage) noexcept : Task(State(initialState | Task::IsAsynchronous), resultsStorage)
{
    QRunnable::setAutoDelete(false);
}

/******************************************************************************
* Destructor.
******************************************************************************/
AsynchronousTaskBase::~AsynchronousTaskBase()
{
    // If task was never submitted for execution, cancel and finish it.
    cancelAndFinish();
}

/******************************************************************************
* Schedules the asynchronous task for execution in a thread pool.
* This function is called by the launchTask() helper.
******************************************************************************/
void AsynchronousTaskBase::operator()()
{
    OVITO_ASSERT(!this->_thisTask);
    OVITO_ASSERT(!this->_threadPool);

    // Inherit execution context from parent task.
    _executionContext = ExecutionContext::current();
    OVITO_ASSERT(_executionContext.isValid());

    // Store a shared_ptr to this task to keep it alive while running.
    _thisTask = this->shared_from_this();

    // Determine the thread pool to use for this task.
    _threadPool = _executionContext.ui().taskManager().chooseThreadPool(*this);

#ifdef QT_BUILDING_UNDER_TSAN
    // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
    // This annotation establishes a happens-before relation with the corresponding __tsan_acquire() call in the worker function executed in the thread pool.
    ::__tsan_release(this);
#endif

    // Submit to thread pool.
    _threadPool->start(this);
}

/******************************************************************************
* Implementation of QRunnable.
******************************************************************************/
void AsynchronousTaskBase::run()
{
#ifdef QT_BUILDING_UNDER_TSAN
    // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
    // This annotation establishes a happens-after relation with the corresponding __tsan_release() call when this runnable is submitted to the thread pool.
    ::__tsan_acquire(this);
#endif

    OVITO_ASSERT(_executionContext.isValid());

    // Execute the work function in the original execution context.
    ExecutionContext::Scope execScope(std::move(_executionContext));

    try {
        // Execute the work function in the scope of this task object.
        Task::Scope taskScope(this);

        perform();

        OVITO_ASSERT(!isFinished());
        setFinished();
    }
    catch(...) {
        OVITO_ASSERT(!isFinished());
        captureExceptionAndFinish();
    }
    _thisTask.reset(); // No need to keep the task object alive any longer.
}

// Instantiate class template to export it from the core DLL.
#if defined(Q_OS_WIN) && (defined(Q_CC_MSVC) || defined(Q_CC_CLANG) || defined(OVITO_BUILD_MONOLITHIC))
    template class OVITO_CORE_EXPORT AsynchronousTask<PipelineFlowState>;
#endif

}   // End of namespace
