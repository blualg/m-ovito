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

/**
 * \brief Composite class template that packages a Task together with the storage for the task's results.
 */
template<class R, class TaskBase = Task>
class TaskWithStorage : public TaskBase
{
public:

    /// Constructor assigning the task's results storage.
    template<typename... Args>
    explicit TaskWithStorage(Task::State initialState, Args&&... args)
        : TaskBase(initialState, &_result), _result{std::forward<Args>(args)...}
    {
#ifdef OVITO_DEBUG
        // This is used in debug builds to detect programming errors and explicitly keep track of whether a result has been assigned to the task.
        this->_hasResultsStored = true;
#endif
    }

    /// Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState, std::nullopt_t) : TaskBase(initialState, &_result) {}

    /// Assigns a value to the internal result storage of the task.
    template<typename R2>
    void setResult(R2&& value) {
        TaskBase::template setResult<R>(std::forward<R2>(value));
    }

protected:

    /// Provides direct read/write access to the internal results.
    R& resultStorage() { return _result; }

private:

    R _result;
};

/**
 * \brief Composite class template that packages a Task together with the storage for the task's results.
 */
template<class TaskBase>
class TaskWithStorage<void, TaskBase> : public TaskBase
{
public:

    /// \brief Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState) : TaskBase(initialState, nullptr) {}

    /// \brief Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState, std::nullopt_t) : TaskBase(initialState, nullptr) {}
};

}   // End of namespace
