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
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include "DeferredObjectExecutor.h"

namespace Ovito {

template<typename Function, typename... Args>
inline void DeferredObjectExecutor::execute(Function&& f, Args&&... args) const& noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    if(contextObject().expired())
        return;

    Application::instance()->taskManager().submitWork([contextObject = contextObject(), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

template<typename Function, typename... Args>
inline void DeferredObjectExecutor::execute(Function&& f, Args&&... args) && noexcept
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    if(contextObject().expired())
        return;

    Application::instance()->taskManager().submitWork([contextObject = std::move(_contextObject), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

}   // End of namespace