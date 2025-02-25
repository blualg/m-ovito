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

namespace Ovito {

class OVITO_CORE_EXPORT ThreadPoolExecutor
{
public:

    /// Constructor.
    explicit ThreadPoolExecutor(bool highPriority = false) noexcept : _highPriority(highPriority) {}

    /// Executes some work.
    template<typename Function>
    void execute(Function&& f) const noexcept;

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) const noexcept {
        static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
        static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
        static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");
        execute(std::bind_front(std::forward<Function>(f), std::forward<Args>(args)...));
    }

private:

    bool _highPriority;
};

}   // End of namespace
