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
#include <ovito/core/utilities/concurrent/ExecutionContext.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

/**
 * \brief An executor that can be used with Future<>::then(), which runs the closure
 *        routine in the context (and in the thread) of some QObject.
 */
class OVITO_CORE_EXPORT ObjectExecutor
{
public:

	/// Constructor.
	explicit ObjectExecutor(const QObject* obj, bool deferredExecution) noexcept : 
			_executionContext(ExecutionContext::current()), 
			_obj(obj), 
			_deferredExecution(deferredExecution) { 
		OVITO_ASSERT(obj); 
		OVITO_ASSERT(!QCoreApplication::instance() || obj->thread() == QCoreApplication::instance()->thread());
		OVITO_ASSERT(_executionContext.isValid());
	}

	/// Creates some work that can be submitted for execution later.
	template<typename Function>
	auto schedule(Function&& f) {

		/// Helper class that is used by this executor to transmit a callable
		/// to the UI thread where it gets executed in the context of some object.
		class WorkEvent : public QEvent, public ObjectExecutor
		{
		public:

			/// Constructor.
			WorkEvent(ObjectExecutor&& executor, std::decay_t<Function>&& f, TaskPtr task) :
				QEvent(workEventType()), 
				ObjectExecutor(std::move(executor)), 
				_callable(std::move(f)),
				_task(std::move(task)) {}

			/// Event destructor, which runs the work function.
			virtual ~WorkEvent() {
				// Qt should be destroying event objects only in the main thread.
				OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());

				// Execute work only if the context object still exists and the application is not shutting down. 
				// Otherwise, silently cancel the work (but still run the destructor of the callable). 
				if(object() && !QCoreApplication::closingDown()) {
					// Temporarily activate the original execution context under which the work was submitted.
					ExecutionContext::Scope execScope(std::move(_executionContext));

					// Temporarily suspend undo recording, because deferred operations never get recorded by convention.
					UndoSuspender noUndo;

					// Execute the work function.
#ifndef OVITO_MSVC_2017_COMPATIBILITY
					if constexpr(detail::is_invocable_v<Function, Task&>)
						std::move(_callable)(*_task);
					else
						std::move(_callable)();
#else // Workaround for compiler deficiency in MSVC 2017. std::is_invocable<> doesn't return correct results.
					std::move(_callable)(*_task);
#endif
				}
			}

		private:
			std::decay_t<Function> _callable;
			const TaskPtr _task;
		};
				
		if constexpr(detail::is_invocable_v<Function, Task&>) {
			// Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
			return [f = std::forward<Function>(f), executor = *this](Task& task) mutable noexcept {					
				if(executor._deferredExecution || !QCoreApplication::instance() || QThread::currentThread() != QCoreApplication::instance()->thread()) {
					// When not in the main thread, schedule work for later execution in the main thread.
					WorkEvent* event = new WorkEvent(std::move(executor), std::move(f), task.shared_from_this());
					QCoreApplication::postEvent(const_cast<QObject*>(event->object()), event);
				}
				else {
					// When already in the main thread, execute work immediately.

					// Temporarily activate the original execution context under which the work was submitted.
					ExecutionContext::Scope execScope(std::move(executor._executionContext));

					// Temporarily suspend undo recording, because deferred operations never get recorded by convention.
					UndoSuspender noUndo;

					// Execute the work function.
					std::move(f)(task);
				}
			};
		}
		else {
			// Note: Avoiding the use of C++17 capture this-by-copy here, because it is not fully supported by the MSVC 2017 compiler.
			return [f = std::forward<Function>(f), executor = *this]() mutable noexcept {
				OVITO_ASSERT(executor.object()); 
				if(executor._deferredExecution || !QCoreApplication::instance() || QThread::currentThread() != QCoreApplication::instance()->thread()) {
					// When not in the main thread, schedule work for later execution in the main thread.
					WorkEvent* event = new WorkEvent(std::move(executor), std::move(f), nullptr);
					QCoreApplication::postEvent(const_cast<QObject*>(event->object()), event);
				}
				else {
					// When already in the main thread, execute work immediately.

					// Temporarily activate the original execution context under which the work was submitted.
					ExecutionContext::Scope execScope(std::move(executor._executionContext));

					// Temporarily suspend undo recording, because deferred operations never get recorded by convention.
					UndoSuspender noUndo;

					// Execute the work function.
					std::move(f)();
				}
			};
		}
	}

	/// Returns the object this executor is associated with.
	/// Work submitted to this executor will be executed in the context of the object.
	const QObject* object() const { return _obj.data(); }

	/// Returns the unique Qt event type ID used by this class to schedule asynchronous work.
	static QEvent::Type workEventType() {
		static const int _workEventType = QEvent::registerEventType();
		return static_cast<QEvent::Type>(_workEventType);
	}

private:

	/// The object work will be submitted to. Work will be executed in the context of this object,
	/// which means it will be automatically canceled if the object gets deleted before the work 
	/// is done.
	QPointer<const QObject> _obj;

	/// The execution context from which the work has been submitted.
	ExecutionContext _executionContext;

	/// Controls whether execution of the work will be deferred until after control is returned to 
	/// the event loop even if immediate execution would be possible.
	const bool _deferredExecution;
};

}	// End of namespace
