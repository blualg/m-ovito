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
#include "detail/ContinuationTask.h"
#include "LaunchTask.h"

namespace Ovito {

/// Asynchronously waits for all futures in the given range to complete (or to get canceled).
/// The function returns a new future that yields the original input range of futures upon completion.
template<typename InputRange, class Executor, typename... ResultType>
[[nodiscard]] auto when_all_futures(
    InputRange&& inputRange,
    Executor&& executor)
{
    // The final output produced by the when-all task.
    using task_result_type = std::decay_t<InputRange>;

    class WhenAllFuturesTask : public detail::ContinuationTask<task_result_type>
    {
    public:

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = Future<task_result_type>;

        /// Constructor.
        WhenAllFuturesTask(
            InputRange&& inputRange,
            Executor&& executor) :
                detail::ContinuationTask<task_result_type>(Task::NoState),
                _range(std::forward<InputRange>(inputRange)),
                _executor(std::forward<Executor>(executor)),
                _iterator(std::begin(_range))
        {
        }

        /// Starts execution of the task.
        void operator()() noexcept {
            OVITO_ASSERT(_iterator == std::begin(_range));
            if(_iterator != std::end(_range)) {

                // Make sure all sub-tasks get canceled when the parent task gets canceled.
                this->finally(_executor, [](Task& task) noexcept {
                    if(task.isCanceled()) {
                        OVITO_ASSERT(!static_cast<WhenAllFuturesTask&>(task)._range.empty());
                        static_cast<WhenAllFuturesTask&>(task)._range.clear();
                    }
                });

                // Begin execution of first iteration.
                _executor.execute([promise = Promise<task_result_type>(this->shared_from_this())]() mutable noexcept {
                    static_cast<WhenAllFuturesTask*>(promise.task().get())->iteration_begin(std::move(promise));
                });
                OVITO_ASSERT_MSG(_iterator == std::begin(_range), "when_all_futures()", "An executor that performs deferred execution is required.");
            }
            else {
                this->template setResult<task_result_type>(std::move(_range));
                this->setFinished();
            }
        }

        /// Performs the next iteration.
        void iteration_begin(PromiseBase promise) noexcept {
            // Did we already reach the end of the input range?
            if(!this->isCanceled()) {
                if(_iterator != std::end(_range)) {
                    // Schedule next iteration upon completion of the future.
                    this->template whenTaskFinishes<WhenAllFuturesTask, &WhenAllFuturesTask::iteration_complete>(
                        std::move(*_iterator), _executor, std::move(promise));
                }
                else {
                    // Inform caller we are done.
                    this->template setResult<task_result_type>(std::move(_range));
                    this->setFinished();
                }
            }
        }

        // Is called at the end of each iteration, when another future has finished.
        void iteration_complete(PromiseBase promise, detail::TaskDependency finishedTask, Task::MutexLock& lock) noexcept {
            // Wrap the task that just finished in a future of the original type.
            *_iterator = std::decay_t<decltype(*_iterator)>(std::move(finishedTask));
            lock.unlock();

            // Continue with next iteration.
            ++_iterator;
            iteration_begin(std::move(promise));
        }

    private:

        /// The range of items to be processed.
        std::decay_t<InputRange> _range;

        /// The executor used for sub-tasks.
        std::decay_t<Executor> _executor;

        /// The iterator pointing to the current item from the range.
        typename std::decay_t<InputRange>::iterator _iterator;
    };

    // Launch the task.
    return launchTask(std::make_shared<WhenAllFuturesTask>(
        std::forward<InputRange>(inputRange),
        std::forward<Executor>(executor)));
}

}   // End of namespace
