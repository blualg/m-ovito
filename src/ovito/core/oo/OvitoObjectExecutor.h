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
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/dataset/UndoStack.h>

namespace Ovito {

/**
 * \brief An executor that can be used with Future<>::then(), which runs the closure
 *        routine in the context (and in the thread) of this RefTarget.
 */
class OVITO_CORE_EXPORT OvitoObjectExecutor
{
public:

	/// Constructor.
	explicit OvitoObjectExecutor(const OvitoObject* obj, bool deferredExecution) noexcept : 
			_executionContext(ExecutionContext::current()), 
			_obj(obj), 
			_deferredExecution(deferredExecution) { 
		OVITO_ASSERT(obj != nullptr); 
		OVITO_ASSERT(_executionContext.isValid());
	}

	/// Creates some work that can be submitted for execution later.
	template<typename Function>
	auto schedule(Function&& f) {

		/// Helper class that is used by this executor to transmit a callable object
		/// to the UI thread where it is executed in the context on a RefTarget.
		class WorkEvent : public QEvent, public OvitoObjectExecutor
		{
		public:

			/// Constructor.
			WorkEvent(OvitoObjectExecutor&& executor, std::decay_t<Function>&& f, TaskPtr task) :
				QEvent(workEventType()), 
				OvitoObjectExecutor(std::move(executor)), 
				_callable(std::move(f)),
				_task(std::move(task)) { OVITO_ASSERT(this->object()); }

			/// Event destructor, which runs the work function.
			virtual ~WorkEvent() {
				// Qt events should only get destroyed in the main thread.
				OVITO_ASSERT(QThread::currentThread() == object()->thread());

				if(!QCoreApplication::closingDown()) {
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
				OVITO_ASSERT(executor.object()); 
				if(executor._deferredExecution || QThread::currentThread() != executor.object()->thread()) {
					// When not in the main thread, schedule work for later execution in the main thread.
					WorkEvent* event = new WorkEvent(std::move(executor), std::move(f), task.shared_from_this());
					QCoreApplication::postEvent(const_cast<OvitoObject*>(event->object()), event);
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
				if(executor._deferredExecution || QThread::currentThread() != executor.object()->thread()) {
					// When not in the main thread, schedule work for later execution in the main thread.
					WorkEvent* event = new WorkEvent(std::move(executor), std::move(f), nullptr);
					QCoreApplication::postEvent(const_cast<OvitoObject*>(event->object()), event);
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
	const OvitoObject* object() const { return _obj.get(); }

	/// Returns the unique Qt event type ID used by this class to schedule asynchronous work.
	static QEvent::Type workEventType() {
		static const int _workEventType = QEvent::registerEventType();
		return static_cast<QEvent::Type>(_workEventType);
	}

private:

	/// The object the work has been submitted to.
	OORef<const OvitoObject> _obj;

	/// The execution context from which the work has been submitted.
	ExecutionContext _executionContext;

	/// Controls whether execution of the work will be deferred until after control is returned to 
	/// the event loop even if immediate execution would be possible.
	const bool _deferredExecution;
};

}	// End of namespace
