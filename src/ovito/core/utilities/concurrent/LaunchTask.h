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
 * It returns a future to the task's results.
 *
 * The task class must define a type named 'future_type',
 * which specifies the type of Future to be returned
 * by the function.
*/
template<class TaskType, typename... Args>
auto launchTask(std::shared_ptr<TaskType> task, Args&&... args)
{
    // The task class must define a type named 'future_type', which specifies what kind of return value the task produces.
    using future_type = typename TaskType::future_type;

    // Make the task the active one.
    Task::Scope taskScope(task);

    // Launch the task.
    (*task)(std::forward<Args>(args)...);

    // Return the future to the caller.
    return future_type::createFromTask(std::move(task));
}

}   // End of namespace
