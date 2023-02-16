////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/utilities/BindFront.h>
#include <function2/function2.hpp>
#include "detail/FutureDetail.h"

namespace Ovito {

namespace detail {
    class TaskReference; // Forward declaration
    class TaskCallbackBase;
    template<typename Derived> class TaskCallback;
    template<typename tuple_type, typename task_type> class ContinuationTask;
}

/**
 * \brief The shared state of promises and futures.
 */
class OVITO_CORE_EXPORT Task : public std::enable_shared_from_this<Task>
{
public:

    using MutexLocker = QMutexLocker<QMutex>;

    /// The different states a task can be in.
    enum State {
        NoState        = 0,
        Started        = (1<<0),
        Finished       = (1<<1),
        Canceled       = (1<<2),
        IsProgressing  = (1<<3), // Indicates that the task is derived from ProgressingTask and can report its progress
        IsAsynchronous = (1<<4)  // Indicates that the task is derived from AsynchronousTaskBase and will run in a worker thread.
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

    /// Returns the task object that is the active one in the current thread.
    static Task*& current() noexcept;

    /// RAII helper class that can be used to temporarily set the active task.
    class Scope;

    /// Returns whether this shared state has been canceled by a previous call to cancel().
    bool isCanceled() const { return (_state.load(std::memory_order_relaxed) & Canceled); }

    /// Returns true if the promise is in the 'started' state.
    bool isStarted() const { return (_state.load(std::memory_order_relaxed) & Started); }

    /// Returns true if the promise is in the 'finished' state.
    bool isFinished() const { return (_state.load(std::memory_order_relaxed) & Finished); }

    /// Indicates whether this task's class is derived from the ProgressingTask base class.
    bool isProgressingTask() const { return (_state.load(std::memory_order_relaxed) & IsProgressing); }

    /// Indicates whether this task's class is derived from the AsynchronousTaskBase class.
    bool isAsynchronousTask() const { return (_state.load(std::memory_order_relaxed) & IsAsynchronous); }

    /// \brief Requests cancellation of the task.
    void cancel() noexcept;

    /// \brief Switches the task into the 'started' state.
    /// \return false if the task was already in the 'started' state before.
    bool setStarted() noexcept;

    /// \brief Switches the task into the 'finished' state.
    void setFinished() noexcept;

    /// \brief Puts a finished task back into the started state. This method should be used with care.
    void restart();

    /// \brief Switches the task into the 'exception' state to signal that an exception has occurred.
    ///
    /// This method should be called from within an exception handler. It saves a copy of the current exception
    /// being handled into the task object.
    void captureException() { setException(std::current_exception()); }

    /// \brief Switches the task into the 'exception' state to signal that an exception has occurred.
    /// \param ex The exception to store into the task object.
    void setException(const std::exception_ptr& ex) { setException(std::exception_ptr(ex)); }

    /// \brief Switches the task into the 'exception' state to signal that an exception has occurred.
    /// \param ex The exception to store into the task object.
    void setException(std::exception_ptr&& ex) {
        const MutexLocker locker(&taskMutex());

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
        MutexLocker locker(&taskMutex());

        // Check if task is already canceled or finished.
        if(_state.load() & (Canceled | Finished))
            return;

        exceptionLocked(std::current_exception());
        finishLocked(locker);
    }

    /// Runs the given continuation function once this task has reached either the 'finished' or the 'canceled' state.
    /// Note that the continuation function will always be executed, even if this task was canceled or set to an error state.
    template<typename Executor, typename Function>
    void finally(Executor&& executor, Function&& f) {
        // Must store a shared_ptr to this task in the lambda in order to keep it alive until the user
        // function gets invoked. That's because it might happen at a much later time if the executor uses deferred scheduling.
        addContinuation(std::forward<Executor>(executor),
            [f = std::forward<Function>(f), self = shared_from_this()]() mutable { std::forward<Function>(f)(*self); });
    }

    /// Runs the given continuation function once this task has reached either the 'finished' or the 'canceled' state.
    /// Note that the continuation function will always be executed, even if this task was canceled or set to an error state.
    template<typename Function>
    void finally(Function&& f) {
        addContinuation(detail::InlineExecutor{}, detail::bind_front(std::forward<Function>(f), std::ref(*this)));
    }

    /// Accessor function for the internal results storage.
    /// This overload is used for tasks with a non-empty results tuple.
    template<typename tuple_type>
    const std::enable_if_t<std::tuple_size<tuple_type>::value != 0, tuple_type>& getResults() const {
        OVITO_ASSERT(_resultsStorage != nullptr);
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_hasResultsStored.load());
#endif
        return *static_cast<const tuple_type*>(_resultsStorage);
    }

    /// Accessor function for the internal results storage.
    /// This overload is used for tasks with an empty results tuple (returning void).
    template<typename tuple_type>
    std::enable_if_t<std::tuple_size<tuple_type>::value == 0, tuple_type> getResults() const {
        return {};
    }

    /// Accessor function for the internal results storage.
    template<typename tuple_type>
    tuple_type takeResults() {
        if constexpr(std::tuple_size<tuple_type>::value != 0) {
#ifdef OVITO_DEBUG
            OVITO_ASSERT(_hasResultsStored.exchange(false) == true);
#endif
            OVITO_ASSERT(_resultsStorage != nullptr);
            return std::move(*static_cast<tuple_type*>(_resultsStorage));
        }
        else {
            return {};
        }
    }

    /// \brief Re-throws the exception stored in this task state if an exception was previously set via setException().
    /// \throw The exception stored in the Task (if any).
    void throwPossibleException() {
        if(exceptionStore())
            std::rethrow_exception(exceptionStore());
    }

    /// \brief Returns the internal exception store, which contains an exception object in case the task has failed.
    const std::exception_ptr& exceptionStore() const noexcept { return _exceptionStore; }

    /// \brief Returns a copy of the internal exception store, which contains an exception object in case the task has failed.
    std::exception_ptr copyExceptionStore() const { return std::exception_ptr{exceptionStore()}; }

    /// \brief Suspends execution until the given task has reached the 'finished' state.
    ///        If the awaited task gets canceled while waiting, the task waiting for it gets canceled too.
    /// \param task The task to wait for.
    /// \return false if either \a task or this operation have been canceled.
    [[nodiscard]] static bool waitFor(detail::TaskReference awaitedTask);

protected:

    /// Assigns a tuple of values to the internal results storage of the task.
    template<typename tuple_type, typename... R>
    void setResults(std::tuple<R...>&& value) {
        static_assert(std::tuple_size_v<tuple_type> == std::tuple_size_v<std::tuple<R...>>, "Must assign a compatible tuple");
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_hasResultsStored.exchange(true) == false);
#endif
        if constexpr(std::tuple_size_v<tuple_type> != 0) {
            OVITO_ASSERT(_resultsStorage != nullptr);
            *static_cast<tuple_type*>(_resultsStorage) = std::move(value);
        }
    }

    /// Assigns a single value to the internal results storage of the task.
    template<typename tuple_type, typename value_type>
    void setResults(value_type&& result) {
        setResults<tuple_type>(std::forward_as_tuple(std::forward<value_type>(result)));
    }

    /// Assigns a void value to the internal results storage of the task.
    template<typename tuple_type>
    void setResults() {
        setResults<tuple_type>(std::tuple<>{});
    }

    /// Adds a callback to this task's list, which will get notified during state changes.
    void addCallback(detail::TaskCallbackBase* cb, bool replayStateChanges) noexcept;

    /// Removes a callback from this task's list, which will no longer get notified about state changes.
    void removeCallback(detail::TaskCallbackBase* cb) noexcept;

    /// Registers a callback function that will be run when this task reaches the 'finished' state.
    /// If the task is already in one of these states, the continuation function is invoked immediately.
    template<typename Executor, typename Function>
    void addContinuation(Executor&& executor, Function&& f) {
        MutexLocker locker(&taskMutex());
        // Check if task is already finished.
        if(isFinished()) {
            // Run continuation function immediately.
            locker.unlock();
            std::forward<Executor>(executor).execute(std::forward<Function>(f));
        }
        else {
            // Otherwise, insert into list to run continuation function later.
            registerContinuation(std::forward<Executor>(executor).schedule(std::forward<Function>(f)));
        }
    }

    /// Registers a callback function that will be run when this task reaches the 'finished' state.
    /// Do not call this method if the task is already in the 'finished' state.
    template<typename Function>
    void registerContinuation(Function&& f) {
        OVITO_ASSERT(!isFinished());
        // Insert into list. Will run continuation function once the task finishes.
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        _continuations.emplace_back(std::forward<Function>(f));
#else
        _continuations.push_back(fu2::unique_function<void() noexcept>{std::forward<Function>(f)});
#endif
    }

    /// Puts this task into the 'started' state (without newly locking the task).
    bool startLocked() noexcept;

    /// Puts this task into the 'canceled' state (without newly locking the task).
    void exceptionLocked(std::exception_ptr&& ex) noexcept;

    /// Puts this task into the 'canceled' state (without newly locking the task).
    void cancelLocked(MutexLocker& locker) noexcept;

    /// Puts this task into the 'finished' state (without newly locking the task).
    void finishLocked(MutexLocker& locker) noexcept;

    /// Puts this task into the 'canceled' and 'finished' states (without newly locking the task).
    void cancelAndFinishLocked(MutexLocker& locker) noexcept;

    /// Increments the counter of futures or parent tasks currently waiting for this task to complete.
    void incrementDependentsCount() noexcept { _dependentsCount.ref(); }

    /// Decrements the counter of futures or parent tasks currently waiting for this task to complete.
    /// If this counter reaches zero, the task gets canceled.
    void decrementDependentsCount() noexcept {
        // Automatically cancel this task when there are no one left who depends on it.
        if(!_dependentsCount.deref())
            cancel();
    }

    /// Invokes the registered callback functions.
    void callCallbacks(int state);

    /// Returns the mutex that is used to manage concurrent access to this task.
    QMutex& taskMutex() const { return _mutex; }

    /// The current state this task is in.
    std::atomic_int _state;

    /// The number of other parties currently waiting for this task to complete.
    QAtomicInt _dependentsCount{0};

    /// Used for managing concurrent access to this task.
    mutable QMutex _mutex;

    /// List of continuation functions that will be called when this task enters the 'finished' or the 'canceled' state.
    QVarLengthArray<fu2::unique_function<void() noexcept>, 2> _continuations;

    /// Holds the exception object when this shared state is in the failed state.
    std::exception_ptr _exceptionStore;

    /// Head of linked list of callback functions currently registered to this task.
    detail::TaskCallbackBase* _callbacks = nullptr;

    /// Pointer to a std::tuple<...> storing the result value(s) of this task.
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
    friend class detail::TaskReference;
    friend class detail::TaskCallbackBase;
    template<typename Derived> friend class detail::TaskCallback;
    template<typename tuple_type, typename task_type> friend class detail::ContinuationTask;
    template<typename... R2> friend class Future;
    template<typename... R2> friend class SharedFuture;
    template<typename... R2> friend class Promise;
};

/**
 * RAII helper class that allows setting a task to be the active task temporarily.
 */
class Task::Scope
{
public:

    /// Constructor taking a raw pointer to a task.
    explicit Scope(Task* task) noexcept : _previous(std::exchange(current(), std::move(task))) {}

    /// Constructor taking a smart pointer to a task.
    template<class TaskType>
    explicit Scope(const std::shared_ptr<TaskType>& task) noexcept : Scope(task.get()) {}

    /// Destructor.
    ~Scope() noexcept { current() = std::move(_previous); }

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
