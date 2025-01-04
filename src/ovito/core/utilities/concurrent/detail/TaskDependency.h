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

/**
 * \brief This class represents a dependency on a task's results.
 *
 * It is used by the classes Future and SharedFuture to express their dependency on some task.
 * It is also used by the Future::then() method to represent the dependency of the continuation task on the preceding task.
 *
 * A task is kept going as long as at least one dependency on it exists.
 * If the number of dependents reaches zero, the task gets automatically canceled.
 */
class OVITO_CORE_EXPORT TaskDependency
{
public:

    /// Default constructor, initializing the smart pointer to null.
    TaskDependency() noexcept = default;

    /// Initialization constructor.
    TaskDependency(TaskPtr task) noexcept : _task(std::move(task)) {
        if(_task) _task->_dependentsCount.fetch_add(1, std::memory_order_relaxed);
    }

    /// Copy constructor.
    TaskDependency(const TaskDependency& other) noexcept : _task(other._task) {
        if(_task) _task->_dependentsCount.fetch_add(1, std::memory_order_relaxed);
    }

    /// Move constructor.
    TaskDependency(TaskDependency&& rhs) noexcept : _task(std::move(rhs._task)) {}

    /// Destructor.
    ~TaskDependency() noexcept {
        if(_task) {
            // Automatically cancel the task when there are no one left depending on its results.
            if(_task->_dependentsCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                _task->cancel();
        }
    }

    // Copy assignment.
    TaskDependency& operator=(const TaskDependency& rhs) noexcept {
        TaskDependency(rhs).swap(*this);
        return *this;
    }

    // Move assignment.
    TaskDependency& operator=(TaskDependency&& rhs) noexcept {
        TaskDependency(std::move(rhs)).swap(*this);
        return *this;
    }

    // Access to pointer value.
    const TaskPtr& get() const noexcept {
        return _task;
    }

    void reset() noexcept {
        TaskDependency().swap(*this);
    }

    void reset(TaskPtr rhs) noexcept {
        TaskDependency(std::move(rhs)).swap(*this);
    }

    inline void swap(TaskDependency& rhs) noexcept {
        _task.swap(rhs._task);
    }

    inline Task& operator*() const noexcept {
        OVITO_ASSERT(_task);
        return *_task.get();
    }

    inline Task* operator->() const noexcept {
        OVITO_ASSERT(_task);
        return _task.get();
    }

    explicit operator bool() const { return (bool)_task; }

private:

    /// A std::shared_ptr to the Task object, which keeps the C++ object alive.
    TaskPtr _task;
};

}   // End of namespace
