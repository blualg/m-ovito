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

#include <ovito/core/Core.h>
#include <ovito/core/app/UserInterface.h>
#include "AsynchronousTask.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AsynchronousTaskBase::AsynchronousTaskBase(State initialState, void* resultsStorage) noexcept : ProgressingTask(State(initialState | Task::IsAsynchronous), resultsStorage) 
{
	QRunnable::setAutoDelete(false);
}	

/******************************************************************************
* Destructor.
******************************************************************************/
AsynchronousTaskBase::~AsynchronousTaskBase() 
{
	// If task was never submitted for execution, cancel and finish it.
	if(!isFinished()) {
		cancel();
		setFinished();
	}
}

/******************************************************************************
* Submits the task for execution to a thread pool.
******************************************************************************/
void AsynchronousTaskBase::startInThreadPool(QThreadPool* pool, bool showInUserInterface) 
{
	OVITO_ASSERT(pool);
	OVITO_ASSERT(!this->_thisTask);
	OVITO_ASSERT(!this->_submittedToPool);
	OVITO_ASSERT(!this->isStarted());

	// Store a shared_ptr to this task to keep it alive while running.
	this->_thisTask = this->shared_from_this();
	this->_submittedToPool = pool;
	
	// Inherit execution context from parent task.
	_executionContext = ExecutionContext::current();
	OVITO_ASSERT(_executionContext.isValid());

	// Register task with UI task manager if requested.
	if(showInUserInterface) {
		_executionContext.ui().taskManager().registerTask(*this);
	}
	
	// Mark this task as started.
	this->setStarted();
	
	// Submit to thread pool.
	pool->start(this);
}

/******************************************************************************
* Runs the task's work function immediately in the current thread.
******************************************************************************/
void AsynchronousTaskBase::startInThisThread(bool showInUserInterface) 
{
	OVITO_ASSERT(!this->_thisTask);
	OVITO_ASSERT(!this->_submittedToPool);
	OVITO_ASSERT(!this->isStarted());

	// Inherit execution context from parent task.
	_executionContext = ExecutionContext::current();
	OVITO_ASSERT(_executionContext.isValid());

	// Register task with UI task manager if requested.
	if(showInUserInterface) {
		_executionContext.ui().taskManager().registerTask(*this);
	}

	// Mark this task as started.
	this->setStarted();

	// Execute it now.
	this->run();
}

/******************************************************************************
* Implementation of QRunnable.
******************************************************************************/
void AsynchronousTaskBase::run() 
{
	OVITO_ASSERT(isStarted());
	OVITO_ASSERT(_executionContext.isValid());
	
	ExecutionContext::Scope execScope(_executionContext);
	try {
		// Execute the work function in the scope of this task object.
		Task::Scope taskScope(this);

		perform();
	}
	catch(...) {
		captureException();
	}
	setFinished();
	_thisTask.reset(); // No need to keep the task object alive any longer.
}

}	// End of namespace
