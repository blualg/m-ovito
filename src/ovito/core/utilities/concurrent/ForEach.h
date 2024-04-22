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

template <typename T, typename R, typename ... Args>
R return_type_deducer(R(T::*)(Args...) const);

template <typename T, typename R, typename ... Args>
R return_type_deducer(R(T::*)(Args...));

/// Deduces the return type of a callable object.
template <typename T>
using return_type = decltype(return_type_deducer(std::declval<decltype(&T::operator())>()));

// Extracts the first type from a parameter pack.
// Primary template with a typename T and a variadic template typename Pack
template<typename Tuple>
struct first_type { using type = std::tuple_element_t<0, Tuple>; };

// Partial specialization for when Pack is empty, fallback to void
template<>
struct first_type<std::tuple<>> { using type = void; };

template<typename InputRange, class Executor, typename StartIterFunc, typename CompleteIterFunc, typename... ResultType>
auto for_each_sequential(
    InputRange&& inputRange,
    Executor&& executor,
    StartIterFunc&& startFunc,
    CompleteIterFunc&& completeFunc,
    ResultType&&... initialResult)
{
    // The final output produced by the loop task.
    using task_result_type = typename first_type<std::tuple<ResultType...>>::type;

    // The type of future returned by the start function.
    using output_future_type = return_type<std::decay_t<StartIterFunc>>;

    // Can we report progress because the total number of required iterations is known?
    constexpr bool is_with_progress = std::is_same_v<typename std::iterator_traits<typename std::decay_t<InputRange>::iterator>::iterator_category, std::random_access_iterator_tag>;

    using task_base_class = std::conditional_t<is_with_progress, ProgressingTask, Task>;

    class ForEachTask : public detail::ContinuationTask<task_result_type, task_base_class>
    {
    public:

        /// The type of future associated with this task type. This is used by the launchTask() function.
        using future_type = Future<task_result_type>;

        /// Constructor.
        ForEachTask(
            InputRange&& inputRange,
            Executor&& executor,
            StartIterFunc&& startFunc,
            CompleteIterFunc&& completeFunc,
            ResultType&&... initialResult) :
                detail::ContinuationTask<task_result_type, task_base_class>(Task::NoState, std::forward<ResultType>(initialResult)...),
                _range(std::forward<InputRange>(inputRange)),
                _executor(std::forward<Executor>(executor)),
                _startFunc(std::forward<StartIterFunc>(startFunc)),
                _completeFunc(std::forward<CompleteIterFunc>(completeFunc)),
                _iterator(std::begin(_range))
        {
            // Determine the number of iterations we are going to perform.
            if constexpr(is_with_progress)
                this->setProgressMaximum(std::distance(_iterator, std::end(_range)));

            // Inherit the priority status from the parent task.
            if(this_task::get()->isHighPriorityTask())
                this->setHighPriorityTask();
        }

        /// Starts execution of the task.
        void operator()() noexcept {
            OVITO_ASSERT(_iterator == std::begin(_range));
            // Begin execution of first iteration.
            if(_iterator != std::end(_range)) {
                _executor.execute(std::bind_front(&ForEachTask::iteration_begin, static_pointer_cast<ForEachTask>(this->shared_from_this())));
                OVITO_ASSERT_MSG(_iterator == std::begin(_range), "for_each_sequential()", "An executor that performs deferred execution is required.");
            }
            else {
                this->setFinished();
            }
        }

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
                    if constexpr(!std::is_void_v<task_result_type>) {
                        if constexpr(std::is_invocable_v<std::decay_t<StartIterFunc>, decltype(*std::begin(inputRange)), task_result_type&>)
                            future = std::invoke(_startFunc, *_iterator, detail::ContinuationTask<task_result_type, task_base_class>::resultStorage());
                        else
                            future = std::invoke(_startFunc, *_iterator);
                    }
                    else
                        future = std::invoke(_startFunc, *_iterator);
                }
                catch(...) {
                    this->captureExceptionAndFinish();
                    return;
                }
                OVITO_ASSERT(future.isValid());
                // Schedule next iteration upon completion of the future returned by the user function.
                this->whenTaskFinishes(future.takeTaskDependency(), _executor, std::bind_front(&ForEachTask::iteration_complete, static_pointer_cast<ForEachTask>(this->shared_from_this())));
            }
            else {
                // Inform caller that the task has finished and the result is available.
                this->setFinished();
            }
        }

        // Is called at the end of each iteration, when user function has finished performing its work.
        void iteration_complete() noexcept {
            // Lock access to this task object.
            Task::MutexLocker locker(*this);

            // Get the task that did just finish and wrap it in a future of the original type.
            output_future_type future(this->takeAwaitedTask());

            // Stop if the awaited future was canceled.
            if(!future.isValid() || future.isCanceled()) {
                this->cancelAndFinishLocked(locker);
                return;
            }

            // Check if the awaited future completed with an error.
            if(future.task()->exceptionStore()) {
                this->exceptionLocked(future.task()->exceptionStore());
                this->finishLocked(locker);
                return;
            }

            locker.unlock();

            try {
                Task::Scope taskScope(this);
                // Invoke the user function that completes this iteration by processing the results returned by the future.
                if constexpr(!std::is_void_v<typename output_future_type::result_type>) {
                    if constexpr(!std::is_void_v<task_result_type>) {
                        if constexpr(std::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result()), task_result_type&>)
                            std::invoke(_completeFunc, *_iterator, std::move(future).result(), detail::ContinuationTask<task_result_type, task_base_class>::resultStorage());
                        else if constexpr(std::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result())>)
                            std::invoke(_completeFunc, *_iterator, std::move(future).result());
                        else
                            std::invoke(_completeFunc, *_iterator);
                    }
                    else {
                        if constexpr(std::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result())>)
                            std::invoke(_completeFunc, *_iterator, std::move(future).result());
                        else
                            std::invoke(_completeFunc, *_iterator);
                    }
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
        typename std::decay_t<InputRange>::iterator _iterator;
    };

    // Launch the task.
    return launchTask(std::make_shared<ForEachTask>(
        std::forward<InputRange>(inputRange),
        std::forward<Executor>(executor),
        std::forward<StartIterFunc>(startFunc),
        std::forward<CompleteIterFunc>(completeFunc),
        std::forward<ResultType>(initialResult)...));
}

}   // End of namespace
