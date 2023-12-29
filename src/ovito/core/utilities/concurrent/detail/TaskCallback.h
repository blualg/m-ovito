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
#include <ovito/core/utilities/concurrent/Task.h>

namespace Ovito::detail {

class OVITO_CORE_EXPORT TaskCallbackBase
{
private:

    /// Invokes the registered callback function. Delegates the call to the function pointer provided by a derived class.
    bool callStateChanged(int state) noexcept {
        OVITO_ASSERT(this->_stateChanged);
        return this->_stateChanged(this, state);
    }

    /// Invokes the registered callback function. Delegates the call to the function pointer provided by a derived class.
    void callProgressChanged(qlonglong progress, qlonglong maximum) noexcept {
        if(this->_progressChanged)
            this->_progressChanged(this, progress, maximum);
    }

    /// Invokes the registered callback function. Delegates the call to the function pointer provided by a derived class.
    void callTextChanged() noexcept {
        if(this->_textChanged)
            this->_textChanged(this);
    }

protected:

    /// The type of function pointer provided by the derived class.
    using state_changed_fn = bool(TaskCallbackBase* f, int state) noexcept;

    /// The type of function pointer provided by the derived class.
    using progress_changed_fn = void(TaskCallbackBase* f, qlonglong progress, qlonglong maximum) noexcept;

    /// The type of function pointer provided by the derived class.
    using text_changed_fn = void(TaskCallbackBase* f) noexcept;

    /// Constructor to be called by the derived class.
    explicit TaskCallbackBase(
        state_changed_fn* stateChanged,
        progress_changed_fn* progressChanged = nullptr,
        text_changed_fn* textChanged = nullptr) noexcept :
            _stateChanged(stateChanged),
            _progressChanged(progressChanged),
            _textChanged(textChanged) {}

    /// The callback function provided by the derived class.
    state_changed_fn* _stateChanged;

    /// The callback function provided by the derived class.
    progress_changed_fn* _progressChanged;

    /// The callback function provided by the derived class.
    text_changed_fn* _textChanged;

    /// Linked list of callbacks (pointer to next callback object).
    TaskCallbackBase* _nextInList = nullptr;

    friend class Ovito::Task;
    friend class Ovito::ProgressingTask;
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
        OVITO_ASSERT(isRegistered());
        _task->removeCallback(this);
        _task = nullptr;
    }

    /// Returns the task being monitored.
    Task* callbackTask() const { return _task; }

private:

    /// The static function to be registered as callback with the base class.
    static bool stateChangedImpl(TaskCallbackBase* cb, int state) noexcept {
        auto& self = *static_cast<Derived*>(cb);
        bool retval = self.taskStateChangedCallback(state);
        if(!retval)
            self._task = nullptr;
        return retval;
    }

    /// The task being monitored.
    Task* _task = nullptr;
};

template<typename Derived>
class ProgressTaskCallback : protected TaskCallback<Derived>
{
public:

    explicit ProgressTaskCallback() noexcept {
        this->_progressChanged = &ProgressTaskCallback::progressChangedImpl;
        this->_textChanged = &ProgressTaskCallback::textChangedImpl;
    }

private:

    /// The static function to be registered as callback with the base class.
    static void progressChangedImpl(TaskCallbackBase* cb, qlonglong progress, qlonglong maximum) noexcept {
        auto& self = *static_cast<Derived*>(cb);
        self.taskProgressChangedCallback(progress, maximum);
    }

    /// The static function to be registered as callback with the base class.
    static void textChangedImpl(TaskCallbackBase* cb) noexcept {
        auto& self = *static_cast<Derived*>(cb);
        self.taskTextChangedCallback();
    }
};

template<typename F>
class FunctionTaskCallback : public TaskCallback<FunctionTaskCallback<F>>
{
public:

    explicit FunctionTaskCallback(Task* task, F&& func) : _func(std::forward<F>(func)) {
        OVITO_ASSERT(task);
        this->registerCallback(task, true);
    }

private:

    bool taskStateChangedCallback(int state) noexcept {
        return _func(state);
    }

    F _func;

    template<typename Derived> friend class TaskCallback;
};


}   // End of namespace
