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

template<bool ShowProgress = true, typename InputRange, class Executor, typename StartIterFunc, typename CompleteIterFunc, typename... ResultType>
[[nodiscard]] auto for_each_sequential(
    InputRange&& inputRange,
    Executor&& executor,
    StartIterFunc&& startFunc,
    CompleteIterFunc&& completeFunc,
    ResultType&&... initialResult)
{
    // The final output produced by the loop task.
    using task_result_type = typename first_type<std::tuple<std::decay_t<ResultType>...>>::type;

    // The type of future returned by the start function.
    using output_future_type = return_type<std::decay_t<StartIterFunc>>;

    class ForEachTask : public detail::ContinuationTask<task_result_type>
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
                detail::ContinuationTask<task_result_type>(this_task::ui(), Task::NoState, std::forward<ResultType>(initialResult)...),
                _range(std::forward<InputRange>(inputRange)),
                _executor(std::forward<Executor>(executor)),
                _startFunc(std::forward<StartIterFunc>(startFunc)),
                _completeFunc(std::forward<CompleteIterFunc>(completeFunc)),
                _iterator(std::begin(_range))
        {
        }

        /// Starts execution of the task.
        void operator()() noexcept {
            OVITO_ASSERT(_iterator == std::begin(_range));
            // Begin execution of first iteration.
            if(_iterator != std::end(_range)) {

                // Determine the number of iterations we are going to perform.
                if constexpr(ShowProgress)
                    this->setProgressMaximum(std::distance(_iterator, std::end(_range)));

                _executor.execute([promise = PromiseBase(this->shared_from_this())]() mutable noexcept {
                    static_cast<ForEachTask*>(promise.task().get())->iteration_begin(std::move(promise));
                });
                OVITO_ASSERT_MSG(_iterator == std::begin(_range), "for_each_sequential()", "An executor performing deferred execution is required.");
            }
            else {
                this->setFinished();
            }
        }

        /// Performs the next iteration of the mapping process.
        void iteration_begin(PromiseBase promise) noexcept {

            // Did we already reach the end of the input range?
            if(_iterator != std::end(_range) && !this->isCanceled()) {
                output_future_type future;
                try {
                    Task::Scope taskScope(this);

                    // Report the number of iterations we have performed so far.
                    if constexpr(ShowProgress)
                        this->setProgressValue(std::distance(std::begin(_range), _iterator));

                    // Call the user-provided function with the current loop value and, optionally, the task's result storage
                    if constexpr(!std::is_void_v<task_result_type>) {
                        if constexpr(std::is_invocable_v<std::decay_t<StartIterFunc>, decltype(*std::begin(inputRange)), task_result_type&>)
                            future = std::invoke(_startFunc, *_iterator, detail::ContinuationTask<task_result_type>::resultStorage());
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
                // Schedule next iteration upon completion of the future returned by the user function.
                this->template whenTaskFinishes<ForEachTask, &ForEachTask::iteration_complete>(
                    std::move(future),
                    _executor,
                    std::move(promise));
            }
            else {
                // Inform caller that the task has finished and the result is available.
                this->setFinished();
            }
        }

        // Is called at the end of each iteration, when user function has finished performing its work.
        void iteration_complete(PromiseBase promise, detail::TaskDependency finishedTask) noexcept {
            // Get the task that did just finish and wrap it in a future of the original type.
            output_future_type future(std::move(finishedTask));

            // Check if the awaited future completed with an error.
            if(future.task()->exceptionStore()) {
                this->setException(future.task()->exceptionStore());
                this->setFinished();
                return;
            }

            try {
                Task::Scope taskScope(this);

                // Invoke the user function that completes this iteration by processing the results returned by the future.
                if constexpr(!std::is_void_v<typename output_future_type::result_type>) {
                    if constexpr(!std::is_void_v<task_result_type>) {
                        if constexpr(std::is_invocable_v<CompleteIterFunc, decltype(*_iterator), decltype(std::move(future).result()), task_result_type&>)
                            std::invoke(_completeFunc, *_iterator, std::move(future).result(), detail::ContinuationTask<task_result_type>::resultStorage());
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
            iteration_begin(std::move(promise));
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
