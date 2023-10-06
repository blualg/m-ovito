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
 * \brief Composite class template that packages a Task together with the storage for the task's results tuple.
 *
 * \tparam Tuple The std::tuple<...> type of the results storage.
 *
 * The task gets automatically configured to use the internal results storage provided by this class.
 */
template<class Tuple, class TaskBase>
#ifndef Q_CC_MSVC
class TaskWithStorage : public TaskBase, private Tuple
#else
class TaskWithStorage : public TaskBase
#endif
{
public:

    /// \brief Constructor assigning the task's results storage and forwarding any extra arguments to the task class constructor.
    /// \param initialResult The value to assign to the results storage tuple.
    template<typename InitialValue>
    explicit TaskWithStorage(Task::State initialState, InitialValue&& initialResult) :
#ifndef Q_CC_MSVC
    TaskBase(initialState, static_cast<Tuple*>(this)), Tuple(std::forward<InitialValue>(initialResult))
#else
    TaskBase(initialState, &_tuple), _tuple(std::forward<InitialValue>(initialResult))
#endif
    {
#ifdef OVITO_DEBUG
        // This is used in debug builds to detect programming errors and explicitly keep track of whether a result has been assigned to the task.
        this->_hasResultsStored = true;
#endif
    }

    /// \brief Constructor which leaves results storage uninitialized.
    explicit TaskWithStorage(Task::State initialState = Task::NoState) noexcept :
#ifndef Q_CC_MSVC
    TaskBase(initialState, std::tuple_size_v<Tuple> != 0 ? static_cast<Tuple*>(this) : nullptr) {}
#else
    TaskBase(initialState, std::tuple_size_v<Tuple> != 0 ? &_tuple : nullptr) {}
#endif

protected:

    /// Provides direct read/write access to the internal results tuple.
    Tuple& resultsTupleStorage() {
#ifndef Q_CC_MSVC
        return static_cast<Tuple&>(*this);
#else
        return _tuple;
#endif
    }

    /// Provides direct read/write access to the first tuple element of the internal results storage.
    decltype(auto) resultsStorage() {
        if constexpr(std::tuple_size_v<Tuple> != 0)
            return std::get<0>(resultsTupleStorage());
    }

private:

#ifdef Q_CC_MSVC
    Tuple _tuple;
#endif
};

}   // End of namespace
