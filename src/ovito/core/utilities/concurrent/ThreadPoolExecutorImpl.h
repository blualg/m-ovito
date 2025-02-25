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
#include "ThreadPoolExecutor.h"

namespace Ovito {

template<typename Function>
inline void ThreadPoolExecutor::execute(Function&& f) const noexcept
{
    static_assert(std::is_invocable_v<Function>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");

    // Choose the thread pool to use for executing the work.
    QThreadPool* threadPool = Application::instance()->taskManager().getThreadPool(_highPriority);

    // Wrap the callable function in a Qt runner object.
    struct Runner : public QRunnable
    {
        std::decay_t<Function> f;
        explicit Runner(Function&& f) : f(std::forward<Function>(f)) {}
        virtual void run() final override {
#ifdef QT_BUILDING_UNDER_TSAN
            // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
            // This annotation establishes a happens-after relation with the corresponding __tsan_release() call when this runnable is submitted to the thread pool.
            ::__tsan_acquire(this);
#endif
            std::invoke(std::move(f));
        }
    };
    Runner* runner = new Runner(std::forward<Function>(f));

#ifdef QT_BUILDING_UNDER_TSAN
    // Workaround for a false positive error by TSAN, which doesn't know the internals of the QThreadPool implementation (unless Qt itself was built with TSAN support).
    // This annotation establishes a happens-before relation with the corresponding __tsan_acquire() call in the worker function executed in the thread pool.
    ::__tsan_release(runner);
#endif

    // Submit runner to the thread pool.
    threadPool->start(runner);
}

}   // End of namespace