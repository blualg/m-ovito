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
 * \brief An executor that runs the closure routine in the main thread and in the context of a given OvitoObject.
 *        The closure routine won't be executed in case the object gets deleted before the work could be started.
 *        Execution always happens deferred, i.e., when controls returns to the event loop.
 */
class OVITO_CORE_EXPORT ObjectExecutor
{
public:

    /// This typedef is used by the launchAsync() function to determine the base task type of the executor.
    using base_task_type = Task;

    /// Constructor.
    explicit ObjectExecutor(OOWeakRef<const OvitoObject> contextObject, std::shared_ptr<UserInterface> ui = this_task::ui()) noexcept : _contextObject(std::move(contextObject)), _ui(std::move(ui)) {}

    /// Creates some work that can be submitted for execution later.
    template<typename Function>
    [[nodiscard]] auto schedule(Function&& f) const& {
        // Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
        return [f = std::forward<Function>(f), executor = *this]<typename... Args>(Args&&... args) mutable noexcept {
            if(!executor.contextObject().expired())
                std::move(executor).execute(std::move(f), std::forward<Args>(args)...);
        };
    }

    /// Creates some work that can be submitted for execution later.
    template<typename Function>
    [[nodiscard]] auto schedule(Function&& f) && {
        // Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
        return [f = std::forward<Function>(f), executor = std::move(*this)]<typename... Args>(Args&&... args) mutable noexcept {
            if(!executor.contextObject().expired())
                std::move(executor).execute(std::move(f), std::forward<Args>(args)...);
        };
    }

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) const&;

    /// Executes some work.
    template<typename Function, typename... Args>
    void execute(Function&& f, Args&&... args) &&;

    /// Returns the object this executor is associated with.
    /// Work submitted to this executor will be executed in the context of the object.
    const OOWeakRef<const OvitoObject>& contextObject() const { return _contextObject; }

private:

    /// The object work will be submitted to. Work will be executed in the context of this object,
    /// which means it will be automatically canceled if the object gets deleted before the work
    /// is done.
    OOWeakRef<const OvitoObject> _contextObject;

    /// The abstract user interface object that is used to submit work to the main thread.
    std::shared_ptr<UserInterface> _ui;
};

}   // End of namespace

#include <ovito/core/app/UserInterface.h>

namespace Ovito {

template<typename Function, typename... Args>
inline void ObjectExecutor::execute(Function&& f, Args&&... args) const&
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    _ui->taskManager().submitWork([contextObject = contextObject(), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

template<typename Function, typename... Args>
inline void ObjectExecutor::execute(Function&& f, Args&&... args) &&
{
    static_assert(std::is_invocable_v<Function, Args...>, "The function must be invocable with the right arguments.");
    static_assert(std::is_invocable_r_v<void, Function, Args...>, "The function must return void.");
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Args...>, "The function must be noexcept.");

    _ui->taskManager().submitWork([contextObject = std::move(_contextObject), f = std::forward<Function>(f), ...args = std::forward<Args>(args)]() mutable noexcept {
        if(OORef<const OvitoObject> target = contextObject.lock())
            std::invoke(std::move(f), std::move(args)...);
    });
}

}   // End of namespace