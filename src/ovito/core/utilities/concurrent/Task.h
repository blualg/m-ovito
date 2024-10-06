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
#include <ovito/core/utilities/Exception.h>
#include <function2/function2.hpp>
#include "detail/FutureDetail.h"

namespace Ovito {

/// Exception type thrown by a task in case it got canceled.
struct OVITO_CORE_EXPORT OperationCanceled {};

/**
 * \brief The shared state of promises and futures.
 */
class OVITO_CORE_EXPORT Task : public std::enable_shared_from_this<Task>
{
    Q_DISABLE_COPY_MOVE(Task)

public:

    using MutexLock = std::unique_lock<std::mutex>;

    /// The different states a task can be in.
    enum State {
        NoState        = 0,
        Finished       = (1<<0),
        Canceled       = (1<<1),
        IsAsynchronous = (1<<2), // The task is derived from AsynchronousTaskBase and runs in a worker thread.
        HighPriority   = (1<<3), // The task should be executed with higher priority, because it is responsible for real-time GUI updates.
        IsInteractive  = (1<<4), // The task is doing actions initiated by the user in the GUI - in contrast to automated actions performed by a script.
    };

    /// Constructor.
    explicit Task(State initialState = NoState, void* resultsStorage = nullptr) noexcept : _state(initialState), _resultsStorage(resultsStorage) {
#ifdef OVITO_DEBUG
        // In debug builds we keep track of how many task objects exist to check whether they all get destroyed correctly
        // at program termination.
        _globalTaskCounter.fetch_add(1);
#endif
    }

#ifdef OVITO_DEBUG
    /// Destructor.
    ~Task();
#endif

    /// RAII helper class that can be used to temporarily set the active task.
    class Scope;

    /// Returns whether this shared state has been canceled by a previous call to cancel().
    bool isCanceled() const noexcept { OVITO_ASSERT(this); return (_state.load(std::memory_order_acquire) & Canceled); }

    /// Returns true if the promise is in the 'finished' state.
    bool isFinished() const noexcept { OVITO_ASSERT(this); return (_state.load(std::memory_order_acquire) & Finished); }

    /// Indicates whether this task's class is derived from the AsynchronousTaskBase class.
    bool isAsynchronousTask() const noexcept { OVITO_ASSERT(this); return (_state.load(std::memory_order_relaxed) & IsAsynchronous); }

    /// Indicates whether this task runs with elevated priority, because it is responsible for real-time GUI updates.
    bool isHighPriorityTask() const noexcept { OVITO_ASSERT(this); return (_state.load(std::memory_order_relaxed) & HighPriority); }

    /// Makes this task run with elevated priority, because it is responsible for real-time GUI updates.
    void setHighPriorityTask() noexcept { _state.fetch_or(HighPriority, std::memory_order_relaxed); }

    /// Returns whether this task is doing actions initiated by the user in the GUI - in contrast to automated actions performed by a script.
    bool isInteractive() const noexcept { OVITO_ASSERT(this); return (_state.load(std::memory_order_relaxed) & IsInteractive); }

    /// Marks this task as doing actions initiated by the user in the GUI - in contrast to automated actions performed by a script.
    bool setIsInteractive(bool isInteractive = true) noexcept {
        if(isInteractive)
            return _state.fetch_or(IsInteractive, std::memory_order_relaxed) & IsInteractive;
        else
            return _state.fetch_and(~IsInteractive, std::memory_order_relaxed) & IsInteractive;
    }

    /// Associates this task with an abstract user interface.
    void setUserInterface(std::shared_ptr<UserInterface> ui) noexcept { _userInterface = std::move(ui); }

    /// Returns the abstract user interface this task is associated with (if any).
    const std::shared_ptr<UserInterface>& userInterface() const noexcept { return _userInterface; }

    /// \brief Requests cancellation of the task.
    void cancel() noexcept;

    /// \brief Switches the task into the 'finished' state.
    void setFinished() noexcept;

    /// \brief Switches the task into the 'exception' state to signal that an exception has occurred.
    ///
    /// This method should be called from within an exception handler. It saves a copy of the current exception
    /// being handled into the task object.
    void captureException() { setException(std::current_exception()); }

    /// \brief Switches the task into the 'exception' state to signal that an exception has occurred.
    /// \param ex The exception to store into the task object.
    void setException(std::exception_ptr ex) {
        const MutexLock lock(*this);

        // Check if task is already canceled or finished.
        if(_state.load() & (Canceled | Finished))
            return;

        exceptionLocked(std::move(ex));
    }

    /// \brief Switches the task into the 'exception' and the 'finished' states to signal that an exception has occurred.
    ///
    /// This method should be called from within an exception handler. It saves a copy of the current exception
    /// being handled into the task object.
    void captureExceptionAndFinish() {
        MutexLock lock(*this);

        // Check if task is already canceled or finished.
        if(!(_state.load() & (Canceled | Finished))) {
            exceptionLocked(std::current_exception());
        }
        finishLocked(lock);
    }

    /// Accessor function for the internal result storage.
    template<typename R>
    const R& getResult() const {
        OVITO_ASSERT(_resultsStorage != nullptr);
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_hasResultsStored.load());
#endif
        return *static_cast<const R*>(_resultsStorage);
    }

    /// Accessor function for the internal result storage.
    template<typename R>
    R takeResult() {
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_hasResultsStored.exchange(false) == true);
#endif
        OVITO_ASSERT(_resultsStorage != nullptr);
        return std::move(*static_cast<R*>(_resultsStorage));
    }

    /// Re-throws the exception stored in this task state if an exception was previously set via setException().
    void throwPossibleException() {
        if(exceptionStore())
            std::rethrow_exception(exceptionStore());
    }

    /// Returns the internal exception store, which contains an exception object in case the task has failed.
    const std::exception_ptr& exceptionStore() const noexcept { return _exceptionStore; }

    /// \brief Sets the description of this task's work to be displayed in the GUI.
    /// \param progressText The text string that will be displayed in the user interface to describe the current operation performed by task.
    void setProgressText(const QString& progressText);

    /// \brief Sets the current maximum value for progress reporting. The current progress value is reset to zero unless autoReset is false.
    void setProgressMaximum(qlonglong maximum, bool autoReset = true);

    /// \brief Sets the current progress value of the task.
    /// \param progressValue The new value, which must be in the range 0 to progressMaximum().
    void setProgressValue(qlonglong progressValue);

    /// \brief Increments the progress value of the task.
    /// \param increment The number of progress units to add to the current progress value.
    void incrementProgressValue(qlonglong increment = 1);

    /// \brief Sets the current progress value of the task, generating update events only occasionally.
    /// \param progressValue The new value, which must be in the range 0 to progressMaximum().
    /// \param updateEvery Generate an update event only after the method has been called this many times.
    void setProgressValueIntermittent(qlonglong progressValue, int updateEvery = 2000);

    /// \brief Starts a sequence of sub-steps in the progress range of this task.
    ///
    /// This is used for long and complex operation, which consist of several logical sub-steps, each with a separate
    /// duration.
    ///
    /// \param weights A vector of relative weights, one for each sub-step, which will be used to calculate the
    ///                the total progress as sub-steps are completed.
    void beginProgressSubStepsWithWeights(std::vector<int> weights);

    /// \brief Convenience version of the function above, which creates *N* substeps, all with the same weight.
    /// \param nsteps The number of sub-steps in the sequence.
    void beginProgressSubSteps(int nsteps) { beginProgressSubStepsWithWeights(std::vector<int>(nsteps, 1)); }

    /// \brief Completes the current sub-step in the sequence started with beginProgressSubSteps() or
    ///        beginProgressSubStepsWithWeights() and moves to the next one.
    void nextProgressSubStep();

    /// \brief Completes a sub-step sequence started with beginProgressSubSteps() or beginProgressSubStepsWithWeights().
    ///
    /// Call this method after the last sub-step has been completed.
    void endProgressSubSteps();

    /// \brief Suspends execution until the given task has reached the 'finished' or 'canceled' state.
    ///        If the awaited task gets canceled while waiting, the task waiting for it gets canceled too.
    /// \param awaitedTask The task to wait for.
    /// \param throwOnError If the awaited task failed with an error, throw it as an exception.
    /// \param returnEarlyIfCanceled If the awaited task or the waiting task get canceled, return early without waiting for the awaited task to actually finish.
    /// \param cancelWaitingIfAwaitedCanceled Cancel the waiting task if the awaited task got canceled.
    /// \return false if the waiting task got canceled (which may happen due to the awaited task getting canceled).
    [[nodiscard]] static bool waitFor(detail::TaskDependency awaitedTask, bool throwOnError, bool returnEarlyIfCanceled, bool cancelWaitingIfAwaitedCanceled);

    /// Runs the given continuation function once this task has reached either the 'finished' state.
    /// Note that the continuation function will always be executed, even if the task was canceled or set to an error state.
    /// The callable can accept one parameter: a reference to the Task object.
    template<typename Executor, typename Function>
    void finally(Executor&& executor, Function&& f) {
        static_assert(std::is_nothrow_invocable_r_v<void, Function> || std::is_nothrow_invocable_r_v<void, Function, Task&>, "The function must be noexcept and parameter-free or accept a Task reference.");

        if constexpr(std::is_invocable_v<Function, Task&>) {
            addContinuation(
                std::forward<Executor>(executor).schedule(
                    [f = std::forward<Function>(f), task = shared_from_this()]() mutable noexcept {
                        std::invoke(std::move(f), *task);
                    }));
        }
        else {
            addContinuation(
                std::forward<Executor>(executor).schedule(
                    std::forward<Function>(f)));
        }
    }

protected:

    /// Assigns a value to the internal result storage of the task.
    template<typename R, typename R2>
    void setResult(R2&& value) {
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_hasResultsStored.exchange(true) == false); // May assign result only once to the task's storage.
#endif
        OVITO_ASSERT(_resultsStorage != nullptr);
        *static_cast<R*>(_resultsStorage) = std::forward<R2>(value);
    }

    /// Adds a callback to this task's list, which will get notified during state changes.
    void addCallback(detail::TaskCallbackBase* cb, bool replayStateChanges) noexcept;

    /// Removes a callback from this task's list, which will no longer get notified about state changes.
    void removeCallback(detail::TaskCallbackBase* cb) noexcept;

    /// Registers a callback function that will be run when this task reaches the 'finished' state.
    /// If the task is already in this state, the continuation function is invoked immediately.
    template<typename Function>
    void addContinuation(Function&& f) {
        MutexLock lock(*this);
        // Check if task is already finished.
        if(isFinished()) {
            // Run continuation function immediately.
            lock.unlock();
            std::invoke(std::forward<Function>(f));
        }
        else {
            // Otherwise, insert into list to run continuation function later.
            registerContinuation(std::forward<Function>(f));
        }
    }

    /// The type-erased function object type to be used for Task continuation functions.
    using continuation_function = fu2::function_base<
        true, // IsOwning = true: The function object owns the callable object and is responsible for its destruction.
        false, // IsCopyable = false: The function object is not copyable.
        fu2::capacity_fixed<4 * sizeof(std::shared_ptr<OvitoObject>)>, // Capacity: Defines the internal capacity of the function for small functor optimization.
        false, // IsThrowing = false: Do not throw an exception on empty function call, call `std::abort` instead.
        true, // HasStrongExceptGuarantee = true: All objects satisfy the strong exception guarantee
        void() noexcept>;

    /// Registers a callback function that will be run when this task reaches the 'finished' state.
    /// The task's mutex must be locked when calling this method.
    /// Do not call this method if the task is already in the 'finished' state.
    template<typename Function>
    void registerContinuation(Function&& f) {
        OVITO_ASSERT(!isFinished());
        // Insert into list. Will run continuation function once the task finishes.
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        _continuations.emplace_back(std::forward<Function>(f));
#else
        _continuations.push_back(continuation_function{std::forward<Function>(f)});
#endif
    }

    /// Puts this task into the 'canceled' state (without newly locking the task).
    void exceptionLocked(std::exception_ptr ex) noexcept;

    /// Puts this task into the 'canceled' state (without newly locking the task).
    /// The task may simultaneously be put into the 'finished' state as well.
    void cancelLocked(MutexLock& lock) noexcept;

    /// Puts this task into the 'finished' state (without newly locking the task).
    void finishLocked(MutexLock& lock) noexcept;

    /// If the task is not finished yet, cancel and finish it.
    void cancelAndFinish() noexcept;

    /// Invokes the registered callback functions.
    void callCallbacks(int state, MutexLock& lock) noexcept;

    /// Returns the mutex that is used to manage concurrent access to this task.
    operator std::mutex&() const { return _mutex; }

    /// The current state this task is in.
    std::atomic_int _state;

    /// The number of TaskDependency instances currently referring to this task.
    /// When this count drops to zero, the task gets automatically canceled.
    std::atomic_int _dependentsCount{0};

    /// For managing concurrent access to this task's state.
    mutable std::mutex _mutex;

    /// The abstract user interface object this task is associated with. May be null.
    std::shared_ptr<UserInterface> _userInterface;

    /// List of continuation functions that will be invoked when this task finishes (successfully or not).
    QVarLengthArray<continuation_function, 2> _continuations;

    /// Holds the exception object when this shared state is in the failed state.
    std::exception_ptr _exceptionStore;

    /// Head of linked list of callback functions currently registered to this task.
    detail::TaskCallbackBase* _callbacks = nullptr;

    /// Pointer to the storage for the task's results.
    void* _resultsStorage = nullptr;

#ifdef OVITO_DEBUG
    /// Indicates whether the result value of the task has been set.
    std::atomic_bool _hasResultsStored{false};

    /// Global counter of Task instances that exist at a time. Used only in debug builds to detect memory leaks.
    static std::atomic_size_t _globalTaskCounter;
#endif

    friend class FutureBase;
    friend class PromiseBase;
    friend class MainThreadOperation;
    friend class AsynchronousTaskBase;
    friend class detail::TaskDependency;
    friend class detail::TaskCallbackBase;
    friend class detail::TaskAwaiter;
    template<typename Derived> friend class detail::TaskCallback;
    template<typename R, typename TaskBase> friend class detail::ContinuationTask;
    template<typename R2> friend class Future;
    template<typename R2> friend class SharedFuture;
    template<typename R2> friend class Promise;
    friend class MainWindow; // MainWindow::registerProgressTask() needs to access registerContinuation().
};

namespace this_task
{

/// Determines whether the current thread is the main thread of the application.
OVITO_CORE_EXPORT bool isMainThread() noexcept;

/// Returns the task object that is currently active in the current thread.
OVITO_CORE_EXPORT Task*& get() noexcept;

/// Returns the abstract user interface associated with the current task.
OVITO_CORE_EXPORT inline const std::shared_ptr<UserInterface>& ui() noexcept {
    OVITO_ASSERT(get() != nullptr);
    OVITO_ASSERT(get()->userInterface());
    return get()->userInterface();
}

/// Returns whether the current task is doing actions initiated by the user in the GUI - in contrast to automated actions performed by a script.
OVITO_CORE_EXPORT inline bool isInteractive() noexcept {
    OVITO_ASSERT(get() != nullptr);
    return get()->isInteractive();
}

/// Returns whether the current task is doing actions initiated by a script - in contrast to interactive actions performed by the user.
OVITO_CORE_EXPORT inline bool isScripting() noexcept {
    OVITO_ASSERT(get() != nullptr);
    return !get()->isInteractive();
}

/// Changes the UI description string of the current operation.
OVITO_CORE_EXPORT inline void setProgressText(const QString& progressText) {
    OVITO_ASSERT(get() != nullptr);
    get()->setProgressText(progressText);
}

/// Sets the maximum value of the current task.
OVITO_CORE_EXPORT inline void setProgressMaximum(qlonglong maximum, bool autoReset = true) {
    OVITO_ASSERT(get() != nullptr);
    get()->setProgressMaximum(maximum, autoReset);
}

/// Sets the progress value of the current task.
OVITO_CORE_EXPORT inline void setProgressValue(qlonglong progressValue) {
    OVITO_ASSERT(get() != nullptr);
    get()->setProgressValue(progressValue);
}

/// Increments the progress value of the task.
OVITO_CORE_EXPORT inline void incrementProgressValue(qlonglong increment = 1) {
    OVITO_ASSERT(get() != nullptr);
    get()->incrementProgressValue(increment);
}

/// Sets the current progress value of the task, generating update events only occasionally.
OVITO_CORE_EXPORT inline void setProgressValueIntermittent(qlonglong progressValue, int updateEvery = 2000) {
    OVITO_ASSERT(get() != nullptr);
    get()->setProgressValueIntermittent(progressValue, updateEvery);
}

/// Returns whether the current task has been canceled.
OVITO_CORE_EXPORT inline bool isCanceled() noexcept {
    OVITO_ASSERT(get() != nullptr);
    return get()->isCanceled();
}

/// Throws an OperationCanceled exception if a cancellation request was made on the current task.
OVITO_CORE_EXPORT inline void throwIfCanceled() {
    if(isCanceled())
        throw OperationCanceled();
}

/// Cancels the current task and throws an OperationCanceled exception.
OVITO_CORE_EXPORT inline void cancelAndThrow() {
    OVITO_ASSERT(get() != nullptr);
    get()->cancel();
    throw OperationCanceled();
}

/// Starts a sequence of sub-steps in the progress range of this task.
OVITO_CORE_EXPORT inline void beginProgressSubStepsWithWeights(std::vector<int> weights) {
    OVITO_ASSERT(get() != nullptr);
    get()->beginProgressSubStepsWithWeights(std::move(weights));
}

/// Convenience version of the function above, which creates *N* substeps, all with the same weight.
OVITO_CORE_EXPORT inline void beginProgressSubSteps(int nsteps) {
    beginProgressSubStepsWithWeights(std::vector<int>(nsteps, 1));
}

/// Completes the current sub-step in the sequence started with beginProgressSubSteps() or
/// beginProgressSubStepsWithWeights() and moves to the next one.
OVITO_CORE_EXPORT inline void nextProgressSubStep() {
    OVITO_ASSERT(get() != nullptr);
    get()->nextProgressSubStep();
}

/// Completes a sub-step sequence started with beginProgressSubSteps() or beginProgressSubStepsWithWeights().
OVITO_CORE_EXPORT inline void endProgressSubSteps() {
    OVITO_ASSERT(get() != nullptr);
    get()->endProgressSubSteps();
}

}   // End of namespace this_task

/**
 * RAII helper class that allows setting a task to be the active task temporarily.
 */
class Task::Scope
{
public:

    /// Constructor taking a raw pointer to a task.
    explicit Scope(Task* task) noexcept : _previous(std::exchange(this_task::get(), std::move(task))) {}

    /// Constructor taking a smart pointer to a task.
    template<class TaskType>
    explicit Scope(const std::shared_ptr<TaskType>& task) noexcept : Scope(task.get()) {}

    /// Destructor.
    ~Scope() noexcept { this_task::get() = std::move(_previous); }

    /// Not a movable type.
    Scope(Scope&& other) = delete;

    /// Not a copyable type.
    Scope(const Scope& other) = delete;

    /// Not a movable type.
    Scope& operator=(Scope&& other) = delete;

    /// Not a copyable type.
    Scope& operator=(const Scope& other) = delete;

private:

    Task* _previous;
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::TaskPtr);
