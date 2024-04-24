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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/detail/TaskCallback.h>

namespace Ovito {

/**
 * Task type which is created by the MainThreadOperation class.
*/
class MainThreadTask : public Task, public detail::TaskCallback<MainThreadTask>
{
public:

    MainThreadTask(Task* parentTask) noexcept : Task(Task::YieldUI) {
        if(parentTask) {
            // Sanity check: The parent cannot be in the finished state yet when the child task is being created.
            OVITO_ASSERT(!parentTask->isFinished() || parentTask->isCanceled());

            // Inherit the priority status from the parent task.
            if(parentTask->isHighPriorityTask())
                this->setHighPriorityTask();

            // When this sub-task gets canceled, we cancel the parent task too.
            this->registerContinuation([this]() noexcept {
                if(isCanceled() && callbackTask() && !callbackTask()->isCanceled()) {
                    callbackTask()->cancel();
                }
            });

            // Register a callback function to get notified when the parent task gets canceled.
            registerCallback(parentTask, true);
        }
    }

    /// Callback function, which is invoked whenever the state of the parent task changes.
    bool taskStateChangedCallback(int state) noexcept {
        if(state & Canceled)
            this->cancel();
        // When the parent task finishes, we should detach our callback function immediately,
        // because a task object may not have callbacks registered at the end of its lifetime.
        if(state & Finished) {
            OVITO_ASSERT(isFinished());
            return false; // Returning false indicates that the callback wishes to be unregistered.
        }
        return true;
    }
};

/******************************************************************************
* Constructor.
******************************************************************************/
MainThreadOperation::MainThreadOperation(ExecutionContext::Type contextType, UserInterface& userInterface, Kind kind) :
    Promise<void>(std::make_shared<MainThreadTask>(kind == Bound ? this_task::get() : nullptr)),
    ExecutionContext::Scope(contextType, userInterface.shared_from_this()),
    Task::Scope(task())
{
    // Usage of MainThreadOperation is only permitted in the main thread.
    OVITO_ASSERT_MSG(ExecutionContext::isMainThread(), "MainThreadOperation", "MainThreadOperation may only be created in the main thread.");
}

/******************************************************************************
* Destructor.
******************************************************************************/
MainThreadOperation::~MainThreadOperation()
{
    if(TaskPtr task = std::move(_task)) {
        OVITO_ASSERT(this_task::get() == task.get());
        task->setFinished();
    }
}

}   // End of namespace
