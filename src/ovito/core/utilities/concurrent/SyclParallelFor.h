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
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/detail/TaskCallback.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

#ifdef OVITO_USE_SYCL

template<typename CGFunction>
inline bool syclParallelForWithProgress(std::size_t total_problem_size, CGFunction&& cgf)
{
    sycl::queue& queue = ExecutionContext::current().ui().taskManager().syclQueue();

    // Allocate early-exit flag in kernel-accessible host memory.
    using early_exit_flag_t = int;
    early_exit_flag_t* early_exit_flag = sycl::malloc_host<early_exit_flag_t>(1, queue);
    *early_exit_flag = false;

    // Register a callback function with the async task, which sets the early-exit flag when the task gets canceled to signal the SYCL kernel to abort as soon as possible.
    detail::FunctionTaskCallback taskCallback(this_task::get(), [&](int state) {
        if(state & Task::Canceled) {
            *early_exit_flag = true; // TODO: Use C++20 std::atomic_ref here if available.
        }
        return true;
    });

    // Compute work group size.
    std::size_t group_size = std::min((std::size_t)128, queue.get_device().get_info<sycl::info::device::max_work_group_size>());
    std::size_t desired_num_groups = queue.get_device().get_info<sycl::info::device::max_compute_units>() * 4;

    // Break down total problem size into smaller chunks for progress reporting (only in GUI mode).
    std::size_t min_progress_size = 4 * desired_num_groups * group_size;
    std::size_t progress_problem_size = total_problem_size;
    if(Application::guiMode()) {
        progress_problem_size = std::min(total_problem_size, std::max(min_progress_size, total_problem_size / 50));
        this_task::setProgressMaximum(total_problem_size);
    }

    for(std::size_t progress_offset = 0; progress_offset < total_problem_size; progress_offset += progress_problem_size) {
        if(this_task::isCanceled())
            break;

        std::size_t problem_size = std::min(total_problem_size, progress_offset + progress_problem_size) - progress_offset;
        std::size_t default_num_groups = (problem_size + group_size - 1) / group_size;
        std::size_t num_groups = std::min(default_num_groups, desired_num_groups);

        queue.submit([&](sycl::handler& cgh) {
            cgf(cgh, [&](auto&& kernel) {
                cgh.parallel_for(sycl::nd_range<1>{num_groups * group_size, group_size}, [=](sycl::nd_item<1> idx) {
                    kernel(idx, problem_size, progress_offset, [&]() -> bool {
                        return sycl::atomic_ref<early_exit_flag_t, sycl::memory_order_relaxed, sycl::memory_scope::system>(*early_exit_flag).load();
                    });
                });
            });
        }).wait();

        if(Application::guiMode()) {
            this_task::setProgressValue(progress_offset + problem_size);
        }
    }
    taskCallback.unregisterCallback();

    // Release host memory.
    sycl::free(early_exit_flag, queue);

    return !this_task::isCanceled();
}

#endif

}   // End of namespace
