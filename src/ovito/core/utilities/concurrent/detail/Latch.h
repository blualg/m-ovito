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
#if __cpp_lib_latch || _LIBCPP_STD_VER >= 20
#include <latch>
#endif

namespace Ovito::detail {

// If available, use std::latch implementation from C++20. Otherwise, use our drop-in replacement.
#if __cpp_lib_latch || _LIBCPP_STD_VER >= 20
using Latch = std::latch;
#else

/**
 * Drop-in replacement for std::latch, which is not yet available in GCC 10.
*/
class OVITO_CORE_EXPORT Latch
{
public:

    static constexpr std::ptrdiff_t max() noexcept {
        return std::numeric_limits<std::ptrdiff_t>::max();
    }

    explicit Latch(std::ptrdiff_t initial_count) : _count(initial_count) {
        OVITO_ASSERT(initial_count >= 0);
    }

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    // Decrements the "count" by one. This
    // function requires that "count != 0" when it is called.
    //
    // Memory ordering: For any threads X and Y, any action taken by X
    // before it calls `count_down()` is visible to thread Y after
    // Y's call to `count_down()`, provided Y's call returns `true`.
    void count_down() {
        std::ptrdiff_t count = _count.fetch_sub(1, std::memory_order_acq_rel) - 1;
        OVITO_ASSERT(count >= 0);
        if(count == 0) {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.notify_all();
        }
    }

    // Blocks until the counter reaches zero. This function may be called at most
    // once. On return, `count_down()` will have been called "initial_count"
    // times and the blocking counter may be destroyed.
    //
    // Memory ordering: For any threads X and Y, any action taken by X
    // before X calls `count_down()` is visible to Y after Y returns
    // from `wait()`.
    void wait() {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this]() { return _count == 0; });
   }

private:

    std::mutex _mutex;
    std::condition_variable _condition;
    std::atomic<std::ptrdiff_t> _count;
};

#endif

}   // End of namespace
