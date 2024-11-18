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
#include <ovito/core/oo/OORef.h>
#include <function2/function2.hpp>

namespace Ovito {

/**
 * \brief This class manages execution of asynchronous tasks in a thread pool and
 *        a queue of pending work items to be executed in the main thread.
 */
class OVITO_CORE_EXPORT TaskManager
{
public:

    /// The type-erased function object type to be used for work queue items.
    using work_function_type = fu2::function_base<
        true, // IsOwning = true: The function object owns the callable object and is responsible for its destruction.
        false, // IsCopyable = false: The function object is not copyable.
        fu2::capacity_fixed<4 * sizeof(std::shared_ptr<OvitoObject>)>, // Capacity: Defines the internal capacity of the function for small functor optimization.
        false, // IsThrowing = false: Do not throw an exception on empty function call, call `std::abort` instead.
        true, // HasStrongExceptGuarantee = true: All objects satisfy the strong exception guarantee
        void() noexcept>;

public:

    /// Constructor.
    TaskManager();

#ifdef OVITO_DEBUG
    /// Destructor.
    ~TaskManager();
#endif

    /// Indicates whether the program session is in the process of shutting down.
    bool isShuttingDown() const { return _isShuttingDown; }

#ifdef OVITO_USE_SYCL
    /// Returns the main SYCL out-of-order queue to which work can be submitted.
    sycl::queue& syclQueue() { return _syclQueue; }
#endif

    /// Tells the TaskManager to wait for all in-flight tasks to complete, then shutdown.
    void requestShutdown();

    /// Executes the given function at some later time.
    void submitWork(work_function_type&& function);

    /// Returns the thread pool for executing an asynchronous task.
    QThreadPool* getThreadPool(bool isHighPriorityTask) {
        return isHighPriorityTask ? &_threadPoolUI : &_threadPool;
    }

    /// Changes the maximum number of threads used by the task manager's thread pools.
    void setMaxThreadCount(int maxThreadCount) {
        _threadPool.setMaxThreadCount(maxThreadCount);
        _threadPoolUI.setMaxThreadCount(maxThreadCount);
    }

    /// Returns the maximum number of threads used by the task manager's thread pools.
    int maxThreadCount() const { return _threadPool.maxThreadCount(); }

#ifdef Q_OS_MACOS
    /// Informs the task manager that a native UI dialog (e.g. a QFileDialog) is currently open.
    /// During this time the task manager will not enter a local event loop with
    /// user input event processing, because this would close the native dialog (for unknown reasons / may be a Qt bug?).
    static void setNativeDialogActive(bool active) { _nativeDialogActive = active; }
#else
    static void setNativeDialogActive(bool) {}
#endif

    /// Executes pending work items waiting in the deferred execution queue.
    void executePendingWork();

private:

    /// Is called when the pending work queue becomes non-empty.
    void notifyWorkArrived();

    /// Executes pending work items waiting in the deferred execution queue.
    void executePendingWorkLocked(std::unique_lock<std::mutex>& lock);

    /// Keeps executing pending work items until quitWorkProcessingLoop() is called or the awaited task has finished.
    void processWorkWhileWaiting(Task* waitingTask, detail::TaskDependency& awaitedTask, bool returnEarlyIfCanceled);

    /// Stops executing pending work items and makes processWorkWhileWaiting() return.
    void quitWorkProcessingLoop(bool& quitFlag, std::optional<QEventLoop>& eventLoop);

    /// Waits for all work queues to become empty.
    void shutdownImplementation(std::unique_lock<std::mutex>& lock);

private:

    /// Indicates whether the task manager is in the process of shutting down.
    bool _isShuttingDown = false;

    /// Indicates that this task manager has completed its shutdown procedure.
    bool _shutdownCompleted = false;

    /// Used to keep the Qt main event loop running while the task manager is active.
    std::optional<QEventLoopLocker> _eventLoopLocker;

#ifdef OVITO_USE_SYCL
    /// The main SYCL out-of-order queue for work on the compute device.
    sycl::queue _syclQueue;

    /// The head of the linked list of RegisteredBufferAccess objects associated with this task manager's SYCL queue.
    RegisteredBufferAccess* _registeredBufferAccessors = nullptr;
#endif

    /// The queue of all pending work items that have been submitted for execution in the main thread.
    std::queue<work_function_type> _pendingWork;

    /// Used to signal the arrival of new work items in the main thread queue.
    std::condition_variable _pendingWorkCondition;

    /// Indicates that we are currently waiting for some task to finish in processWorkWhileWaiting().
    TaskPtr _waitingForTask;

    /// Manages thread-safe concurrent access to the work queue.
    std::mutex _mutex;

    /// Pool of threads for executing worker tasks.
    QThreadPool _threadPool;

    /// Pool of threads for executing UI-related tasks, which require a higher priority.
    QThreadPool _threadPoolUI;

#ifdef Q_OS_MACOS
    /// Indicates that a native UI dialog (e.g. a QFileDialog) is currently open.
    /// During this time, the task manager should not enter a local event loop with
    /// user input event processing, because this would close the native dialog.
    /// This behavior may be a bug in the Qt framework, which has been observed so far on the macOS platform.
    static bool _nativeDialogActive;
#endif

    friend class RegisteredBufferAccess;
    friend class Task; // to call TaskManager::processWorkWhileWaiting() from Task::waitFor()
};

}   // End of namespace
