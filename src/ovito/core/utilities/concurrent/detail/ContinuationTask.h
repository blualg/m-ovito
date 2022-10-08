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
#include "TaskWithStorage.h"
#include "TaskReference.h"
#include "TaskCallback.h"

namespace Ovito::detail {

/**
 * \brief The type of task that is returned by the Future::then() method.
 */
template<typename tuple_type, typename task_type>
class ContinuationTask : public TaskWithStorage<tuple_type, task_type>
{
public:

	/// Constructor.
	explicit ContinuationTask(Task::State initialState = Task::NoState) noexcept : TaskWithStorage<tuple_type, task_type>(initialState) {
		OVITO_ASSERT(!(initialState & Task::Canceled));
		registerFinallyFunction();
	}

	/// Constructor initializing the results storage.
	template<typename InitialValue>
	explicit ContinuationTask(Task::State initialState, InitialValue&& initialResult) noexcept : TaskWithStorage<tuple_type, task_type>(initialState, std::forward<InitialValue>(initialResult)) {
		OVITO_ASSERT(!(initialState & Task::Canceled));
		registerFinallyFunction();
	}

	/// Moves the dependency on the preceding task out of this task object.
	/// Note: Make sure this task's mutex is locked when calling this function.
	TaskReference takeAwaitedTask() noexcept {
		return std::move(_awaitedTask);
	}

	/// Runs the given continuation function once the given task reaches the 'finished' state.
	template<typename Executor, typename Function>
	void whenTaskFinishes(TaskReference awaitedTask, Executor&& executor, Function&& f) noexcept {
		OVITO_ASSERT(awaitedTask);

		// Attach to the task to be waited on.
		QMutexLocker locker(&this->taskMutex());
		OVITO_ASSERT(!_awaitedTask);
		if(this->isCanceled()) {
			locker.unlock();
			// Bail out and do not attach to the input task if this continuation task is already canceled.
			// Still run continuation function, because the caller may depend on it.
#ifndef OVITO_MSVC_2017_COMPATIBILITY
			std::forward<Executor>(executor).schedule(std::forward<Function>(f))();
#else // Workaround for compiler deficiency in MSVC 2017. std::is_invocable<> doesn't return correct results.
			std::forward<Executor>(executor).schedule(std::forward<Function>(f))(*awaitedTask);
#endif
			return;
		}
		OVITO_ASSERT(!this->isFinished());
		_awaitedTask = std::move(awaitedTask);
		TaskPtr t = _awaitedTask.get();
		locker.unlock();

		// Run the work function once the task finishes.
		t->finally(std::forward<Executor>(executor), std::forward<Function>(f));
	}

	/// Sets the result of this task upon completion of the preceding task.
	template<typename Function, typename FutureType>
	void fulfillWith(PromiseBase&& promise, Function&& f, FutureType&& future) noexcept {
		OVITO_ASSERT(!_awaitedTask);
		OVITO_ASSERT(this->isStarted());
		OVITO_ASSERT(!this->isFinished());
		OVITO_ASSERT(promise.task().get() == this);
		OVITO_ASSERT(future.isValid() && future.isFinished());

		// Inspect return value type of the continuation function.
		if constexpr(!detail::returns_future_v<Function, FutureType>) {
			// Continuation function returns a result value or void.
			try {
				// Execute the continuation function in the scope of this task object.
				Task::Scope taskScope(this);

				if constexpr(!detail::returns_void_v<Function, FutureType>) {
					// Function returns non-void results.
					if constexpr(!detail::is_invocable_v<Function, FutureType>)
						if constexpr(is_shared_future_v<FutureType>)
							this->template setResults<tuple_type>(std::apply(std::forward<Function>(f), future.task()->template getResults<typename FutureType::tuple_type>()));
						else
							this->template setResults<tuple_type>(std::apply(std::forward<Function>(f), future.task()->template takeResults<typename FutureType::tuple_type>()));
					else
						this->template setResults<tuple_type>(std::forward<Function>(f)(std::forward<FutureType>(future)));
				}
				else {
					// Function returns void.
					if constexpr(!detail::is_invocable_v<Function, FutureType>)
						std::forward<Function>(f)();
					else
						std::forward<Function>(f)(std::forward<FutureType>(future));
					this->template setResults<tuple_type>();
				}
			}
			catch(...) {
				this->captureException();
			}
			this->setFinished();
		}
		else {
			// The continuation function returns a new future, whose result will be used to fulfill this task.
			std::decay_t<callable_result_t<Function, FutureType>> nextFuture;
			try {
				// Execute the continuation function in the scope of this task object.
				Task::Scope taskScope(this);

				// Call the continuation function with the results of the finished task or the finished future itself.
				if constexpr(!detail::is_invocable_v<Function, FutureType>)
					nextFuture = std::apply(std::forward<Function>(f), std::forward<FutureType>(future).results());
				else
					nextFuture = std::forward<Function>(f)(std::forward<FutureType>(future));
			}
			catch(...) {
				this->captureExceptionAndFinish();
				return;
			}
			OVITO_ASSERT(nextFuture.isValid());
				
			// The new future's task now becomes the one we depend on.
			QMutexLocker locker(&this->taskMutex());
			_awaitedTask = nextFuture.task();
			locker.unlock();

			// Get results from the future's task once it completes and use it as the results of this continuation task.
			nextFuture.finally([promise = std::move(promise)]() mutable noexcept {
				ContinuationTask* thisTask = static_cast<ContinuationTask*>(promise.task().get());

				// Manage access to the task that represents the continuation.
				QMutexLocker locker(&thisTask->taskMutex());

				// Get the task that did just finish.
				TaskReference finishedTask = thisTask->takeAwaitedTask();

				// Bail out if the preceding task has been canceled, or if the continuation has been canceled.
				if(!finishedTask || finishedTask->isCanceled())
					return; // Note: The Promise's destructor automatically puts the continuation task into 'canceled' and 'finished' states if it isn't already.

				OVITO_ASSERT(!thisTask->isFinished());
				OVITO_ASSERT(!thisTask->isCanceled());

				// If the preceding task failed, inherit the error state.
				if(finishedTask->exceptionStore()) {
					thisTask->exceptionLocked(finishedTask->copyExceptionStore());
				}
				else {
					// Adopt result value from the completed future.
					if constexpr(is_shared_future_v<decltype(nextFuture)>)
						thisTask->template setResults<tuple_type>(finishedTask->template getResults<tuple_type>());
					else
						thisTask->template setResults<tuple_type>(finishedTask->template takeResults<tuple_type>());
				}

				thisTask->finishLocked(locker);
			});
		}
	}

private:

	/// Register a callback function with this task, which gets invoked when the task gets canceled.
	void registerFinallyFunction() {
		// When this task gets canceled, we should discard the reference to the task we are waiting for in order to cancel it as well.
		this->registerContinuation([](Task& thisTask) noexcept {
			QMutexLocker locker(&static_cast<ContinuationTask&>(thisTask).taskMutex());
			// Move the dependency on the preceding task out of this object. This may implicitly cancel the 
			// awaited task when the reference goes out of scope.
			auto awaitedTask = static_cast<ContinuationTask&>(thisTask).takeAwaitedTask();
			// Note: It's critical to first unlock the mutex before releasing the reference to the awaited task.
			locker.unlock(); 
		});
	}

	/// The task that must finish first before this task can continue.
	TaskReference _awaitedTask;
};

} // End of namespace
