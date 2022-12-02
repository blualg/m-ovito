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
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/dataset/UndoStack.h>

namespace Ovito {

/**
 * \brief Abstract interface to the graphical user interface of the application.
 *
 * Note that is is possible to open multiple GUI windows per process.
 */
class OVITO_CORE_EXPORT UserInterface
{
public:

	/// Constructor.
	explicit UserInterface(DataSetContainer& datasetContainer, TaskManager& taskManager) : _datasetContainer(datasetContainer), _taskManager(taskManager) {}

	/// Destructor.
	virtual ~UserInterface() {}

	/// Returns the container managing the current dataset.
	DataSetContainer& datasetContainer() const { return _datasetContainer; }

	/// Sets the viewport input manager of the user interface.
	void setViewportInputManager(ViewportInputManager* manager) { _viewportInputManager = manager; }

	/// Returns the viewport input manager of the user interface.
	ViewportInputManager* viewportInputManager() const { return _viewportInputManager; }

	/// Returns the manager of asynchronous tasks belonging to this user interface.
	TaskManager& taskManager() const { return _taskManager; }

	/// Returns the manager of ParameterUnit objects.
	UnitsManager& unitsManager() { return _unitsManager; }

	/// Gives the active viewport the input focus.
	virtual void setViewportInputFocus() {}
	
	/// Displays a message string in the status bar.
	virtual void showStatusBarMessage(const QString& message, int timeout = 0) {}

	/// Hides any messages currently displayed in the status bar.
	virtual void clearStatusBarMessage() {}

	/// Displays the error message(s) stored in the Exception object to the user.
	///
	/// In the graphical program mode, this method will display a modal message box.
	/// In console mode, this method just prints the error messages(s) to the console.
	///
	/// Note that, unless 'blocking' is true, the reporting happens asynchronously in GUI mode.
	/// The method returns immediately and the error messaeg is displayed to the user at a later time,
	/// as soon as control returns to the event loop.
	virtual void reportError(const Exception& ex, bool blocking = false);

	/// Closes the user interface and shuts down the entire application after displaying an error message.
	virtual void exitWithFatalError(const Exception& ex);

	/// Tells the UI to process any pending events in the event queue and return immediately.
	/// The function can return true to indicate that the running operation should be canceled. 
	virtual bool processEvents();

	/// Immediately repaints all viewports that have been flagged for an update.
	virtual void processViewportUpdateRequests();

	/// Returns the manager of the user interface actions.
	ActionManager* actionManager() const { return _actionManager; }

	/// Queries the system's information and graphics capabilities.
	QString generateSystemReport();

	/// Creates a frame buffer of the requested size for rendering into and displays it in the user interface.
	virtual std::shared_ptr<FrameBuffer> createAndShowFrameBuffer(int width, int height, MainThreadOperation& renderingOperation);

	/// Returns the undo stack, which keeps track of changes made by the user to the current dataset. 
	/// It may be none if not running as a desktop application.
	UndoStack* undoStack() const { return _undoStack; }

	/// Indicates whether the program session is being closed and all task in progress should be canceled.
	virtual bool isShuttingDown() const;

	/// Indicates whether the user has activated auto-key mode and controllers should automatically
	/// generate new animation keys whenever their current value is changed by the user.
	virtual bool isAutoGenerateAnimationKeysEnabled() const { return false; }

	/// Temporarily suspends repainting of the viewports.
	/// To resume redrawing of viewports call resumeViewportUpdates().
	/// Normally, you should use the ViewportSuspender helper class to suspend viewport update.
	/// It has the advantage of being exception-safe.
	void suspendViewportUpdates() { _viewportSuspendCount++; }

	/// Resumes redrawing of the viewports after a call to suspendViewportUpdates().
	void resumeViewportUpdates();

	/// Returns whether viewport updates are currently suspended.
	bool areViewportUpdatesSuspended() const { return _viewportSuspendCount > 0; }

	/// Flags all viewports for redrawing.
	///
	/// This function does not lead to an immediate repainting of the viewports; instead it schedules a
	/// paint event for deferred processing when execution returns to the Qt event loop.
	///
	/// To update just a single viewport, Viewport::updateViewport() should be used instead.
	///
	/// To redraw all viewports immediately, also call processViewportUpdateRequests().
	void updateViewports();

	/// Executes a functor that performs some actions in an interactive context and catches any exceptions thrown during its execution.
	/// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
	template<typename Function>
	bool handleExceptions(Function&& func) {
		try {
			ExecutionContext::Scope executionScope(ExecutionContext::Type::Interactive, *this);
			std::forward<Function>(func)();
			return true;
		}
		catch(const Exception& ex) {
			reportError(ex);
			return false;
		}
	}

	/// Executes a functor provided by the caller that performs undoable actions in an interactive context.
	/// If an exception is thrown by the functor, all data changes performed by the functor so far will be undone, the error message is displayed 
	/// to the user, and this function returns false. If no exception is thrown, all performed actions are committed and this function returns true.
	template<typename Function>
	bool performTransaction(const QString& undoOperationName, Function&& func) {
		try {
			ExecutionContext::Scope executionScope(ExecutionContext::Type::Interactive, *this);
			UndoableTransaction transaction(undoOperationName);
			std::forward<Function>(func)();
			transaction.commit(*this);
			return true;
		}
		catch(const Exception& ex) {
			reportError(ex);
			return false;
		}
	}

protected:

	/// Assigns an ActionManager.
	void setActionManager(ActionManager* manager) { _actionManager = manager; }

	/// Assigns an UndoStack.
	void setUndoStack(UndoStack* undoStack) { _undoStack = undoStack; }

private:

	/// Hosts the dataset that is currently being edited in this user interface.
	DataSetContainer& _datasetContainer;

	/// Viewport input manager of the user interface.
	ViewportInputManager* _viewportInputManager = nullptr;

	/// Actions of the user interface.
	ActionManager* _actionManager = nullptr;

	/// Manages the running asynchronous tasks that belong to this user interface.
	TaskManager& _taskManager;

	/// The undo stack keeping track of changes made by the user to the current dataset.
	UndoStack* _undoStack = nullptr;

	/// The manager of ParameterUnit objects.
	UnitsManager _unitsManager;

	/// This counter tracks temporary suspension of viewport updates.
	int _viewportSuspendCount = 0;

	/// Indicates that the viewports have been invalidated while updates were suspended.
	bool _viewportsNeedUpdate = false;
};

/**
 * \brief RAII helper class that suspends viewport redrawing while it exists.
 *
 * Use this to make your code exception-safe.
 * Just create an instance of this class on the stack to suspend viewport updates
 * during the lifetime of the class instance.
 */
class OVITO_CORE_EXPORT ViewportSuspender 
{
public:
	ViewportSuspender(UserInterface& userInterface) noexcept : _ui(userInterface) {
		userInterface.suspendViewportUpdates();
	}
	~ViewportSuspender() {
		_ui.resumeViewportUpdates();
	}
private:
	UserInterface& _ui;
};

}	// End of namespace
