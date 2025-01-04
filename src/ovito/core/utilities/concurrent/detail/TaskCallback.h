////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito::detail {

class OVITO_CORE_EXPORT TaskCallbackBase
{
public:

    /// The type of function pointer provided by the derived class.
    using state_changed_fn = void(Task& task, TaskCallbackBase& f, int state, Task::MutexLock& lock) noexcept;

    /// Constructor to be called by the derived class.
    explicit TaskCallbackBase(state_changed_fn* stateChanged) noexcept : _stateChanged(stateChanged) { OVITO_ASSERT(stateChanged); }

    /// Constructor to be called by the derived class.
    explicit TaskCallbackBase(Task& task, state_changed_fn* stateChanged) noexcept : _stateChanged(stateChanged), _nextInList(std::exchange(task._callbacks, this)) {
        OVITO_ASSERT(stateChanged);
    }

private:

    /// Invokes the registered callback function. Delegates the call to the function pointer provided by a derived class.
    void callStateChanged(Task& task, int state, Task::MutexLock& lock) noexcept {
        OVITO_ASSERT(_stateChanged);
        _stateChanged(task, *this, state, lock);
    }

protected:

    /// The callback function provided by the derived class.
    state_changed_fn* _stateChanged;

    /// Linked list of callbacks (pointer to next callback object).
    TaskCallbackBase* _nextInList = nullptr;

    friend class Ovito::Task;
};

template<typename Derived>
class TaskCallback : protected TaskCallbackBase
{
public:

    explicit TaskCallback() noexcept : TaskCallbackBase(&TaskCallback::stateChangedImpl) {}

    ~TaskCallback() {
        if(isRegistered())
            _task->removeCallback(this);
    }

    bool isRegistered() const { return _task != nullptr; }

    void registerCallback(Task* task, bool replayStateChanges) {
        OVITO_ASSERT(task != nullptr && !isRegistered());
        _task = task;
        task->addCallback(this, replayStateChanges);
    }

    void unregisterCallback() {
        if(isRegistered()) {
            _task->removeCallback(this);
            _task = nullptr;
        }
    }

    /// Returns the task being monitored.
    Task* callbackTask() const { return _task; }

private:

    /// The static function to be registered as callback with the base class.
    static void stateChangedImpl(Task& task, TaskCallbackBase& cb, int state, Task::MutexLock& lock) noexcept {
        auto& self = static_cast<Derived&>(cb);
        OVITO_ASSERT(self.callbackTask() == &task);
        self.taskStateChangedCallback(state, lock);
        if(state & Task::Finished)
            self._task = nullptr;
    }

    /// The task being monitored.
    Task* _task = nullptr;
};

template<typename F>
class FunctionTaskCallback : public TaskCallback<FunctionTaskCallback<F>>
{
public:

    static_assert(std::is_nothrow_invocable_r_v<void, F, int>, "The function must be noexcept.");

    explicit FunctionTaskCallback(Task* task, F&& func) : _func(std::forward<F>(func)) {
        OVITO_ASSERT(task);
        this->registerCallback(task, true);
    }

private:

    void taskStateChangedCallback(int state, Task::MutexLock& lock) noexcept {
        _func(state);
    }

    F _func;

    template<typename Derived> friend class TaskCallback;
};


}   // End of namespace
