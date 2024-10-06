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
 * Helper function which launches a task by invoking the task's call operator.
 * It returns a future for the task's results.
 *
 * The task class must define a type named 'future_type',
 * which specifies the type of Future<T> or SharedFuture<T> to be returned
 * by the function.
*/
template<class TaskType, typename... Args>
[[nodiscard]] auto launchTask(std::shared_ptr<TaskType> task, Args&&... args)
{
    OVITO_ASSERT(task);

    // The task class must define a type named 'future_type', which specifies what kind of return value the task produces.
    using future_type = typename TaskType::future_type;

    // Inherit the priority status, interactive flag, and user interface from the current task.
    if(const Task* parentTask = this_task::get()) {
        if(parentTask->isHighPriorityTask())
            task->setHighPriorityTask();
        if(parentTask->isInteractive())
            task->setIsInteractive();
        task->setUserInterface(parentTask->userInterface());
    }

    // Check at compile-time whether the task's call operator is defined.
    if constexpr(std::is_invocable_v<TaskType, Args...>) {

        // Make the task the active one.
        Task::Scope taskScope(task);

        // Launch the task by invoking its call operator.
        (*task)(std::forward<Args>(args)...);
    }
    else {
        // Make sure no args have been provided by the caller.
        static_assert(sizeof...(Args) == 0, "The task does not accept any arguments.");
    }

    // Return the future to the caller.
    return future_type::createFromTask(std::move(task));
}

}   // End of namespace
