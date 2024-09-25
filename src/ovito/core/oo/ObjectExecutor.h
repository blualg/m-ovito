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
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/**
 * \brief An executor that runs the closure routine in the main thread and in the context of a given OvitoObject.
 *        The closure routine won't be executed in case the object gets deleted before the work could be started.
 */
class OVITO_CORE_EXPORT ObjectExecutor
{
public:

    /// This typedef is used by the launchAsync() function to determine the base task type of the executor.
    using base_task_type = Task;

    /// Constructor.
    explicit ObjectExecutor(OOWeakRef<const OvitoObject> contextObject, bool deferredExecution) noexcept :
            _contextObject(std::move(contextObject)),
            _deferredExecution(deferredExecution) {}

    /// Creates some work that can be submitted for execution later.
    template<typename Function>
    [[nodiscard]] auto schedule(Function&& f) {
        static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");
        OVITO_ASSERT(ExecutionContext::current().isValid());
        // Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
        return [f = std::forward<Function>(f), executor = *this, context = ExecutionContext::current()]() mutable noexcept {
            if(OORef<const OvitoObject> target = executor.contextObject().lock()) {
                // When not in the main thread, or if deferred execution was requested, schedule work for later execution in the main thread.
                if(executor._deferredExecution || ExecutionContext::isMainThread() == false) {
                    // Schedule the work for execution later.
                    std::move(context).runDeferred(target, std::forward<Function>(f));
                }
                else { // When already in the main thread, execute work immediately.

                    // Activate the execution context to which the work was submitted.
                    ExecutionContext::Scope execScope(std::move(context));

                    // Temporarily suspend undo recording, because deferred operations never get recorded by convention.
                    UndoSuspender noUndo;

                    // Execute the work function.
                    std::invoke(std::move(f));
                }
            }
        };
    }

    /// Executes work.
    template<typename Function>
    void execute(Function&& f) {
        static_assert(std::is_nothrow_invocable_r_v<void, Function>, "The function must be noexcept.");
        OVITO_ASSERT(ExecutionContext::current().isValid());
        if(OORef<const OvitoObject> target = contextObject().lock()) {
            // If the work was explicitly marked for deferred execution or if we are not running in the
            // main thread, schedule the work for execution later.
            if(_deferredExecution || ExecutionContext::isMainThread() == false) {
                ExecutionContext::current().runDeferred(target, std::forward<Function>(f));
            }
            else {
                // Execute the work immediately.
                // Temporarily suspend undo recording, because asynchronous operations never get recorded by convention.
                UndoSuspender noUndo;
                std::invoke(std::forward<Function>(f));
            }
        }
    }

    /// Returns the object this executor is associated with.
    /// Work submitted to this executor will be executed in the context of the object.
    const OOWeakRef<const OvitoObject>& contextObject() const { return _contextObject; }

private:

    /// The object work will be submitted to. Work will be executed in the context of this object,
    /// which means it will be automatically canceled if the object gets deleted before the work
    /// is done.
    const OOWeakRef<const OvitoObject> _contextObject;

    /// Controls whether execution of the work will be deferred until after control is returned to
    /// the event loop even if immediate execution would be possible.
    const bool _deferredExecution;
};

}   // End of namespace
