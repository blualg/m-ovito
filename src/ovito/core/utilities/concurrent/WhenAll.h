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
#include "detail/ContinuationTask.h"
#include "InlineExecutor.h"
#include "LaunchTask.h"

namespace Ovito {

/// Asynchronously waits for all futures in the given range to complete (or to get canceled).
/// The function returns a new future that yields the original input range of futures upon completion.
template<typename InputRange, typename... ResultType>
[[nodiscard]] auto when_all_futures(InputRange&& inputRange)
{
    // The final output produced by the when-all task.
    using task_result_type = std::decay_t<InputRange>;

    class WhenAllFuturesTask : public detail::ContinuationTask<task_result_type>
    {
        Q_DISABLE_COPY_MOVE(WhenAllFuturesTask)

    public:

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = Future<task_result_type>;

        /// Constructor.
        explicit WhenAllFuturesTask(
            InputRange&& inputRange) :
                detail::ContinuationTask<task_result_type>(Task::NoState, std::forward<InputRange>(inputRange)),
                _taskCallback(*this, &taskStateChangedCallback),
                _iterator(std::begin(this->resultStorage()))
        {
            // All futures must be valid.
            OVITO_ASSERT(boost::algorithm::all_of(this->resultStorage(), [](const auto& future) { return (bool)future; }));
        }

        /// Starts execution of the task.
        void operator()() noexcept {
            OVITO_ASSERT(_iterator == std::begin(this->resultStorage()));
            if(_iterator != std::end(this->resultStorage())) {
                // Begin execution of first iteration.
                Task::MutexLock lock(*this);
                this->iteration_begin(this->shared_from_this(), lock);
            }
            else {
                this->setFinished();
            }
        }

        /// Performs the next iteration.
        void iteration_begin(PromiseBase promise, Task::MutexLock& lock) noexcept {
            OVITO_ASSERT(!this->isFinished() && !this->isCanceled());

            // Did we already reach the end of the input range?
            if(_iterator != std::end(this->resultStorage())) {
                // Take next future from the range.
                detail::TaskDependency awaitedTask = _iterator->takeTaskDependency();
                OVITO_ASSERT(awaitedTask && !*_iterator);
                lock.unlock();

                // Schedule next iteration upon completion of the future.
                this->template whenTaskFinishes<WhenAllFuturesTask, &WhenAllFuturesTask::iteration_complete>(
                    std::move(awaitedTask), InlineExecutor{}, std::move(promise));
            }
            else {
                this->finishLocked(lock);
            }
        }

        // Is called at the end of each iteration, when another future has finished.
        void iteration_complete(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            Task::MutexLock lock(*this);

            OVITO_ASSERT(!this->isFinished() && !this->isCanceled());
            OVITO_ASSERT(_iterator != std::end(this->resultStorage()));
            OVITO_ASSERT(!*_iterator);

            // Wrap the task that just finished back into a future of the original type.
            *_iterator = std::decay_t<decltype(*_iterator)>(std::move(finishedTask));
            OVITO_ASSERT(*_iterator);

            // Continue with next iteration.
            ++_iterator;
            iteration_begin(std::move(promise), lock);
        }

    private:

        /// This function gets invoked when the state of this task changes.
        /// The task's mutex is locked when this function is called.
        static void taskStateChangedCallback(Task& task, detail::TaskCallbackBase& cb, int state, Task::MutexLock& lock) noexcept {
            if(state & Task::Canceled) {
                WhenAllFuturesTask& self = static_cast<WhenAllFuturesTask&>(task);
                self._iterator = std::end(self.resultStorage());
                OVITO_ASSERT(self.isCanceled());
                // Note: It's critical to first unlock the mutex before releasing the references to the awaited tasks.
                lock.unlock();
                // Discard all dependencies on the awaited tasks.
                for(auto& future : self.resultStorage())
                    future.reset();
                lock.lock();
            }
        }

    private:

        /// The iterator pointing to the current item in the range of futures.
        typename std::decay_t<InputRange>::iterator _iterator;

        /// Used to get notified when the state of this task changes.
        detail::TaskCallbackBase _taskCallback;
    };

    // Launch the task.
    return launchTask(std::make_shared<WhenAllFuturesTask>(std::forward<InputRange>(inputRange)));
}

}   // End of namespace
