////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
 * \brief A smart-pointer referencing a shared Task object, which expresses a dependency on the Task's results.
 *
 * This is used by the classes Future and SharedFuture to express their dependency on a Task.
 * If the number of dependents of the Task reaches zero, the Task is automatically canceled.
 */
class OVITO_CORE_EXPORT TaskReference
{
public:

    /// Default constructor, initializing the smart pointer to null.
    TaskReference() noexcept = default;

    /// Initialization constructor.
    TaskReference(TaskPtr ptr) noexcept : _ptr(std::move(ptr)) {
        if(_ptr) _ptr->incrementDependentsCount();
    }

    /// Copy constructor.
    TaskReference(const TaskReference& other) noexcept : _ptr(other._ptr) {
        if(_ptr) _ptr->incrementDependentsCount();
    }

    /// Move constructor.
    TaskReference(TaskReference&& rhs) noexcept : _ptr(std::move(rhs._ptr)) {}

    /// Destructor.
    ~TaskReference() noexcept {
        if(_ptr) _ptr->decrementDependentsCount();
    }

    // Copy assignment.
    TaskReference& operator=(const TaskReference& rhs) noexcept {
        TaskReference(rhs).swap(*this);
        return *this;
    }

    // Move assignment.
    TaskReference& operator=(TaskReference&& rhs) noexcept {
        TaskReference(std::move(rhs)).swap(*this);
        return *this;
    }

    // Access to pointer value.
    const TaskPtr& get() const noexcept {
        return _ptr;
    }

    void reset() noexcept {
        TaskReference().swap(*this);
    }

    void reset(TaskPtr rhs) noexcept {
        TaskReference(std::move(rhs)).swap(*this);
    }

    inline void swap(TaskReference& rhs) noexcept {
        _ptr.swap(rhs._ptr);
    }

    inline Task& operator*() const noexcept {
        OVITO_ASSERT(_ptr);
        return *_ptr.get();
    }

    inline Task* operator->() const noexcept {
        OVITO_ASSERT(_ptr);
        return _ptr.get();
    }

    explicit operator bool() const { return (bool)_ptr; }

private:

    /// A shared_ptr to the Task object, which keeps the C++ object alive.
    TaskPtr _ptr;
};

}   // End of namespace
