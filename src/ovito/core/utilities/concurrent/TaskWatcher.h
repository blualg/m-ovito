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
#include <ovito/core/utilities/concurrent/detail/TaskCallback.h>

namespace Ovito {

/**
 * \brief Provides a Qt signal/slots interface to an asynchronous task.
 */
class OVITO_CORE_EXPORT TaskWatcher : public QObject, private detail::ProgressTaskCallback<TaskWatcher>
{
	Q_OBJECT

public:

	/// Constructor that creates a watcher that is not associated with
	/// any future/promise yet.
	explicit TaskWatcher(QObject* parent = nullptr) : QObject(parent) {}

	/// Destructor.
	virtual ~TaskWatcher() {
		watch(nullptr, false);
	}

	/// Returns whether this watcher is currently monitoring a task.
	bool isWatching() const { return (bool)_task; }

	/// Returns the task being monitored by this watcher.
	const TaskPtr& task() const { return _task; }

	/// Makes this watcher monitor the given task.
	void watch(const TaskPtr& task, bool pendingAssignment = true);

	/// Detaches this watcher from the task.
	void reset() { 
		if(isWatching())
			watch(nullptr); 
	}

	/// Returns true if the task monitored by this object has been canceled.
	bool isCanceled() const;

	/// Returns true if the task monitored by this object has reached the 'finished' state.
	bool isFinished() const;

	/// Returns the maximum value for the progress of the task monitored by this object.
	qlonglong progressMaximum() const;

	/// Returns the current value for the progress of the task monitored by this object.
	qlonglong progressValue() const;

	/// Returns the status text of the task monitored by this object.
	QString progressText() const;

public Q_SLOTS:

	/// Cancels the operation being watched by this watcher.
	void cancel();

Q_SIGNALS:

	void finished();
	void started();
	void canceled();
	void progressChanged(qlonglong progress, qlonglong maximum);
	void progressTextChanged(const QString& progressText);

private Q_SLOTS:

	void taskStarted();
	void taskCanceled();
	void taskFinished();
	void taskProgressChanged(qlonglong progress, qlonglong maximum);
	void taskTextChanged();

private:

	bool taskStateChangedCallback(int state);
	void taskProgressChangedCallback(qlonglong progress, qlonglong maximum);
	void taskTextChangedCallback();

	/// The task being monitored.
	TaskPtr _task;

	/// Indicates that the task has reached the 'finished' state.
    bool _finished = false;

	template<typename Derived> friend class detail::TaskCallback;
	template<typename Derived> friend class detail::ProgressTaskCallback;
};

}	// End of namespace
