////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/utilities/BindFront.h>
#include "detail/ContinuationTask.h"
#include "LaunchTask.h"

namespace Ovito {

template<typename InputRange, class Executor, typename StartIterFunc, typename CompleteIterFunc, typename... ResultType>
auto for_each_sequential(
	InputRange&& inputRange, 
	Executor&& executor, 
	StartIterFunc&& startFunc, 
	CompleteIterFunc&& completeFunc, 
	ResultType&&... initialResult)
{
	// The type of future returned by the user function.
	using output_future_type = detail::invoke_result_t<StartIterFunc, decltype(*std::begin(inputRange)), std::decay_t<ResultType>&...>; // C++20: Use std::indirect_result_t instead.

	// The output tuple produced by the task.
	using task_tuple_type = std::tuple<std::decay_t<ResultType>...>;

	// Can we report progress because the total number of rerquired iterations is known?
	constexpr bool is_with_progress = std::is_same_v<typename std::iterator_traits<typename InputRange::iterator>::iterator_category, std::random_access_iterator_tag>;

	using task_base_class = std::conditional_t<is_with_progress, ProgressingTask, Task>;

	class ForEachTask : public detail::ContinuationTask<task_tuple_type, task_base_class>
    {
    public:

		/// The type of future associated with this task type. This is used by the launchTask() function.
		using future_type = Future<std::decay_t<ResultType>...>;

		/// Constructor.
		ForEachTask(
			InputRange&& inputRange, 
			Executor&& executor, 
			StartIterFunc&& startFunc, 
			CompleteIterFunc&& completeFunc, 
			ResultType&&... initialResult) : 
				detail::ContinuationTask<task_tuple_type, task_base_class>(Task::Started, std::forward_as_tuple(std::forward<ResultType>(initialResult)...)),
				_range(std::forward<InputRange>(inputRange)),
				_executor(std::forward<Executor>(executor)),
				_startFunc(std::forward<StartIterFunc>(startFunc)),
				_completeFunc(std::forward<CompleteIterFunc>(completeFunc)),
				_iterator(std::begin(_range)) 
		{
			// Determine the number of iterations we are going to perform.
			if constexpr(is_with_progress)
				this->setProgressMaximum(std::distance(_iterator, std::end(_range)));
		}

		/// Starts execution of the task.
		void operator()() noexcept {
			OVITO_ASSERT(_iterator == std::begin(_range));
			// Begin execution of first iteration.
			if(_iterator != std::end(_range)) {
				_executor.execute(detail::bind_front(&ForEachTask::iteration_begin, static_pointer_cast<ForEachTask>(this->shared_from_this())));
				OVITO_ASSERT_MSG(_iterator == std::begin(_range), "for_each_sequential()", "An executor that performs deferred execution is required.");
			}
			else {
				this->setFinished();
			}
		}

		/// Inherit method that provides public read/write access to the internal results storage of this task.
		using detail::ContinuationTask<task_tuple_type, task_base_class>::resultsStorage;

		/// Performs the next iteration of the mapping process.
		void iteration_begin() noexcept {
			// Report the number of iterations we have performed so far.
			if constexpr(is_with_progress)
				this->setProgressValue(std::distance(std::begin(_range), _iterator));

			// Did we already reach the end of the input range?
			if(_iterator != std::end(_range) && !this->isCanceled()) {
				output_future_type future;
				try {
					Task::Scope taskScope(this);
					// Call the user-provided function with the current loop value and, optionally, the task's result storage 
					if constexpr(detail::is_invocable_v<std::decay_t<StartIterFunc>, decltype(*_iterator), std::decay_t<ResultType>&...> && std::tuple_size_v<task_tuple_type> == 1)
						future = std::invoke(_startFunc, *_iterator, this->resultsStorage());
					else
						future = std::invoke(_startFunc, *_iterator);
				}
				catch(...) {
					this->captureExceptionAndFinish();
					return;
				}
				OVITO_ASSERT(future.isValid());
				// Schedule next iteration upon completion of the future returned by the user function.
				this->whenTaskFinishes(future.takeTaskReference(), _executor, detail::bind_front(&ForEachTask::iteration_complete, static_pointer_cast<ForEachTask>(this->shared_from_this())));
			}
			else {
				// Inform caller that the task has finished and the result is available.
				this->setFinished();
			}
		}

		// Is called at the end of each iteration, when user function has finished performing its work.
        void iteration_complete() noexcept {
			// Lock access to this task object.
			QMutexLocker locker(&this->taskMutex());

			// Get the task that did just finish and wrap it in a future of the original type.
			output_future_type future(this->takeAwaitedTask());

			// Stop if the awaited future was canceled.
			if(!future.isValid() || future.isCanceled()) {
				this->cancelAndFinishLocked(locker);
				return;
			}

			// Check if the awaited future completed with an error.
			if(future.task()->exceptionStore()) {
				this->exceptionLocked(future.task()->copyExceptionStore());
				this->finishLocked(locker);
				return;
			}

			locker.unlock();

			try {
				Task::Scope taskScope(this);
				// Invoke the user function that completes this iteration by processing the results returned by the future.
				if constexpr(std::tuple_size_v<typename output_future_type::tuple_type> == 1) {
					if constexpr(detail::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result()), std::decay_t<ResultType>&...> && std::tuple_size_v<task_tuple_type> == 1)
						std::invoke(_completeFunc, *_iterator, std::move(future).result(), resultsStorage());
					else if constexpr(detail::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result())>)
						std::invoke(_completeFunc, *_iterator, std::move(future).result());
					else
						std::invoke(_completeFunc, *_iterator);
				}
				else
					std::invoke(_completeFunc, *_iterator);
			}
			catch(...) {
				this->captureExceptionAndFinish();
				return;
			}

			// Continue with next iteration.
			++_iterator;
			iteration_begin();
        }

	private:

        /// The range of items to be processed.
        std::decay_t<InputRange> _range;

        /// The user function to call with each item of the input range.
        std::decay_t<StartIterFunc> _startFunc;

        /// The user function to call with each result produced by the future.
        std::decay_t<CompleteIterFunc> _completeFunc;

        /// The executor used for sub-tasks.
        std::decay_t<Executor> _executor;

        /// The iterator pointing to the current item from the range.
        typename InputRange::iterator _iterator;
    };

	// Launch the task.
	return launchTask<false>(std::make_shared<ForEachTask>(
        std::forward<InputRange>(inputRange), 
        std::forward<Executor>(executor), 
		std::forward<StartIterFunc>(startFunc),
		std::forward<CompleteIterFunc>(completeFunc),
		std::forward<ResultType>(initialResult)...));
}

}	// End of namespace
