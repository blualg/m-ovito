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

namespace Ovito {

class OVITO_CORE_EXPORT ThreadPoolExecutor
{
public:

    /// Constructor.
    ThreadPoolExecutor(bool highPriority = false) noexcept : _highPriority(highPriority) {}

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

    /// Creates some work that can be submitted for execution later.
    template<typename Function>
    [[nodiscard]] auto schedule(Function&& f) const noexcept {
        // Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
        return [f = std::forward<Function>(f), executor = *this]<typename... Args>(Args&&... args) mutable noexcept {
            std::move(executor).execute(std::move(f), std::forward<Args>(args)...);
        };
    }

private:

    bool _highPriority;
};

}   // End of namespace

#include <ovito/core/app/Application.h>

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