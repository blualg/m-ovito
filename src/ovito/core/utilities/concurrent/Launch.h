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
#include "AsynchronousTask.h"
#include "detail/Latch.h"
#include "Future.h"

namespace Ovito {

/// Schedules the given function for execution in a worker thread.
template<typename Function>
inline auto asyncLaunch(Function&& f, bool showInUserInterface = false)
{
    using R = std::invoke_result_t<Function>;
    class PackagedTask : public AsynchronousTask<R>
    {
    public:
        PackagedTask(Function&& f) noexcept : _func(std::forward<Function>(f)) {}
        virtual void perform() override {
            if constexpr(!std::is_void_v<R>)
                this->setResult(std::invoke(std::move(_func)));
            else
                std::invoke(std::move(_func));
        }
    private:
        std::decay_t<Function> _func;
    };
    auto task = std::make_shared<PackagedTask>(std::forward<Function>(f));
    return task->launch(showInUserInterface);
}

/// Executes the given function in a worker thread and waits for the result.
/// This function blocks and should be used whenever the lambda function captures some
/// local variables by reference.
template<typename Function>
inline auto asyncLaunchAndJoin(Function&& f, bool showInUserInterface = false) {
    detail::Latch latch(1);
    auto future = asyncLaunch([&latch, f = std::forward<Function>(f)]() mutable {
        try {
            if constexpr(!std::is_void_v<std::invoke_result_t<Function>>) {
                auto result = std::invoke(std::move(f));
                latch.count_down();
                return result;
            }
            else {
                std::move(f)();
                latch.count_down();
            }
        }
        catch(...) {
            latch.count_down();
            throw;
        }
    }, showInUserInterface);
    try {
        if constexpr(!std::is_void_v<std::invoke_result_t<Function>>) {
            auto result = future.result();
            latch.wait();
            return result;
        }
        else {
            future.waitForFinished();
            latch.wait();
        }
    }
    catch(...) {
        latch.wait();
        throw;
    }
}

}   // End of namespace
