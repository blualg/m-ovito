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
#include <ovito/core/utilities/concurrent/ExecutionContext.h>

namespace Ovito {

/// The simplest implementation of the Executor concept.
/// The inline executor runs a work function immediately and in place.
/// See ObjectExecutor for another implementation of the executor concept.
struct InlineExecutor
{
    template<typename Function>
    static constexpr void execute(Function&& f) noexcept {
        static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");
        std::invoke(std::forward<Function>(f));
    }

    template<typename Function>
    [[nodiscard]] static constexpr auto schedule(Function&& f) noexcept {
        static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");
        return [f = std::forward<Function>(f), context = ExecutionContext::current()]() mutable noexcept {
            ExecutionContext::Scope execScope(std::move(context));
            std::invoke(std::move(f));
        };
    }
};

} // End of namespace
