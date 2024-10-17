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

namespace Ovito {

/**
 * \brief A simple cache for a single future.
 *
 * This class is used to cache the result of a computation that
 * depends on a single input value (template parameter 'Key').
 * If the input value changes, the cache is invalidated and the
 * caller-provided function is invoked to launch a new computation.
 * If the input value has not changed, the previously computed result is
 * returned in the form of a future and the caller-provided function is not invoked.
 *
 * The caller-provided function must return a Future or SharedFuture object.
 */
template<typename Key>
class FutureCache
{
public:

    template<typename F>
    [[nodiscard]] auto getOrCompute(const Key& key, F&& f) {

        using FutureType = decltype(f());
        using SharedFutureType = SharedFuture<typename FutureType::result_type>;

        if(!_task || _key != key || _task->isCanceled()) {
            _task.reset();
            FutureType future = std::move(f)();
            _key = key;
            _task = future.task();
            return SharedFutureType(std::move(future));
        }
        else {
            return SharedFutureType(_task);
        }
    }

    void reset() {
        _task.reset();
        _key = Key{};
    }

private:

    TaskPtr _task;
    Key _key;
};

}   // End of namespace
