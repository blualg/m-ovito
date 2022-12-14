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
#include "detail/TaskWithStorage.h"
#include "Future.h"
#include "TaskManager.h"

namespace Ovito {

class OVITO_CORE_EXPORT AsynchronousTaskBase : public ProgressingTask, public QRunnable
{
public:

	/// Constructor.
	AsynchronousTaskBase(State initialState = NoState, void* resultsStorage = nullptr) noexcept;

	/// Destructor.
	~AsynchronousTaskBase();

    /// Returns the thread pool this task has been submitted to for execution (if any).
    QThreadPool* threadPool() const { return _submittedToPool; }

	/// This virtual function is responsible for computing the results of the task.
	virtual void perform() = 0;

private:

	/// Implementation of QRunnable.
	virtual void run() final override;

	/// Submits the task for execution to a thread pool.
	void startInThreadPool(QThreadPool* pool, bool showInUserInterface);

	/// Runs the task's work function immediately in the current thread.
	void startInThisThread(bool showInUserInterface);

	/// A shared pointer to the task itself, which is used to keep the C++ object alive
	/// while the task is transferred to and executed in a thread pool.
	TaskPtr _thisTask;

    /// The thread pool this task has been submitted to for execution (if any).
    QThreadPool* _submittedToPool = nullptr;

	/// The execution context that this task inherits from its parent task.
	ExecutionContext _executionContext;

	friend class Task;
	template<typename... R> friend class AsynchronousTask;
};

template<typename... R>
class AsynchronousTask : public detail::TaskWithStorage<std::tuple<R...>, AsynchronousTaskBase>
{
public:

	/// Schedules the task for execution in the global thread pool and returns a future for the task's results.
	Future<R...> runAsync(bool showInUserInterface) {
#ifndef OVITO_DISABLE_THREADING
		// Submit the task for execution in a worker thread.
		return runAsync(QThreadPool::globalInstance(), showInUserInterface);
#else
		// If multi-threading is not available, run the task immediately.
		return runImmediately();
#endif
	}

#ifndef OVITO_DISABLE_THREADING
	/// Submits the task to a thread pool for execution and returns a future for the task's results.
	Future<R...> runAsync(QThreadPool* pool, bool showInUserInterface) {
		this->startInThreadPool(pool, showInUserInterface);
		return Future<R...>::createFromTask(this->shared_from_this());
	}
#endif

	/// Schedules the given function for execution in a worker thread.
	/// The function should accept a reference to a ProgressingTask as a parameter.
	template<typename Function>
	static Future<R...> runAsync(Function&& f, bool showInUserInterface) {
		class FuncAsyncTask : public AsynchronousTask {
		public:
			FuncAsyncTask(Function&& f) : _func(std::forward<Function>(f)) {}
			virtual void perform() override { std::invoke(std::move(_func), *this); }
		private:
			std::decay_t<Function> _func;
		};
		auto task = std::make_shared<FuncAsyncTask>(std::forward<Function>(f));
		return task->runAsync(showInUserInterface);
	}

	/// Runs the given function in a separate worker thread and waits until the function returns.
	/// Returns false if execution has been canceled due to cancelation of the task calling this function.
	template<typename Function>
	static bool runAsyncAndJoin(Function&& f, bool showInUserInterface) {
		QWaitCondition wc;
		QMutex waitMutex;
		bool done = false;
		auto future = AsynchronousTask::runAsync([&wc, &waitMutex, &done, f = std::forward<Function>(f)](ProgressingTask& task) {
			std::move(f)();
			QMutexLocker locker(&waitMutex);
			done = true;
			wc.wakeAll();
		}, showInUserInterface);
		bool result = std::move(future).waitForFinished();
		waitMutex.lock();
		if(!done)
			wc.wait(&waitMutex);
		waitMutex.unlock();
		return result;
	}

	/// Runs the task in place and returns a future for the task's results.
	Future<R...> runImmediately(bool showInUserInterface) {
		this->startInThisThread(showInUserInterface);
		return Future<R...>::createFromTask(this->shared_from_this());
	}

	/// Sets the result value of the task.
	template<typename... R2>
	void setResult(R2&&... result) {
		this->template setResults<std::tuple<R...>>(std::forward_as_tuple(std::forward<R2>(result)...));
	}
};

}	// End of namespace
