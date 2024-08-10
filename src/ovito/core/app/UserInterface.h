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
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/oo/OvitoObject.h>

namespace Ovito {

/**
 * \brief Abstract interface to the graphical user interface of the application.
 *
 * In OVITO, it is possible to open multiple GUI windows. Each window is a separate UserInterface object.
 * Furthermore, the global Application object is also a UserInterface implementation, which
 * is used while running in console mode or during application startup, when no main window exists yet.
 *
 * Typically, you can access the current UserInterface object via the ExecutionContext::current().ui() method.
 */
class OVITO_CORE_EXPORT UserInterface : public OvitoObject
{
    OVITO_CLASS(UserInterface)

public:

    /// Buttons that can be included in a GUI message box displayed to the user.
    /// One or more buttons can be combined using the bitwise OR operator.
    enum MessageBoxButton
    {
        NoButton = 0x00000000,
        Ok = 0x00000400,
        Cancel = 0x00400000,
        Discard = 0x00800000,
        Yes = 0x00004000,
        No = 0x00010000,
        Apply = 0x02000000,
        Abort = 0x00040000,
        Retry = 0x00080000,
        Ignore = 0x00100000,
    };

    /// Icons to be displayed in a message box displayed to the user.
    enum MessageBoxIcon
    {
        NoIcon = 0,
        InformationIcon = 1,
        WarningIcon = 2,
        CriticalIcon = 3,
        QuestionIcon = 4,
    };

public:

    /// Constructor.
    UserInterface();

    /// Returns the container managing the current dataset.
    DataSetContainer& datasetContainer() const { return *_datasetContainer; }

    /// Sets the viewport input manager of the user interface.
    void setViewportInputManager(ViewportInputManager* manager) { _viewportInputManager = manager; }

    /// Returns the viewport input manager of the user interface.
    ViewportInputManager* viewportInputManager() const { return _viewportInputManager; }

    /// Returns the manager of asynchronous tasks belonging to this user interface.
    TaskManager& taskManager() { return _taskManager; }

    /// Returns the manager of ParameterUnit objects.
    UnitsManager& unitsManager() { return _unitsManager; }

    /// Gives the active viewport the input focus.
    virtual void setViewportInputFocus() {}

    /// Displays a message string in the status bar.
    virtual void showStatusBarMessage(const QString& message, int timeout = 0) {}

    /// Hides any messages currently displayed in the status bar.
    virtual void clearStatusBarMessage() {}

    /// Displays a modal message box to the user. Blocks until the user closes the message box.
    /// This method wraps the QMessageBox class of the Qt library.
    virtual MessageBoxButton showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton = NoButton, const QString& detailedText = {}) { OVITO_ASSERT(false); return defaultButton; }

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

    /// Indicates that exitWithFatalError() has been called and the application is shutting down.
    bool exitingDueToFatalError() const { return _exitingDueToFatalError; }

    /// Aborts all running tasks and closes the user interface as soon as possible (without asking user to save changes).
    void shutdown();

    /// Indicates whether the session is in the process of being closed and all ongoing tasks should be canceled.
    bool isShuttingDown() const { return _taskManager.isShuttingDown(); }

    /// Call this to keep the UI object alive until shutdown() is called on it.
    void keepAliveUntilShutdown() { _selfGuard = this; }

    /// Returns a shared_ptr to this UserInterface object.
    std::shared_ptr<UserInterface> shared_from_this() {
        return std::static_pointer_cast<UserInterface>(OvitoObject::shared_from_this());
    }

    /// Tells the UI to process any pending events in the event queue and return immediately.
    /// The function can return true to indicate that the running operation should be canceled.
    virtual bool processUIEvents();

    /// Immediately redraws the viewports to reflect any changes made to the scene.
    void processViewportUpdateRequests();

    /// Returns the manager of the user interface actions.
    ActionManager* actionManager() const { return _actionManager; }

    /// Queries the system's information and graphics capabilities.
    QString generateSystemReport();

    /// Creates a frame buffer of the requested size for rendering into and displays it in the user interface.
    virtual std::shared_ptr<FrameBuffer> createAndShowFrameBuffer(int width, int height);

    /// Returns the undo stack, which keeps track of changes made by the user to the current dataset.
    /// It may be none if not running as a desktop application.
    UndoStack* undoStack() const { return _undoStack; }

    /// Indicates whether the user has activated auto-key mode and controllers should automatically
    /// generate new animation keys whenever their current value is changed by the user.
    virtual bool isAutoGenerateAnimationKeysEnabled() const { return false; }

    /// Temporarily suspends repainting of the viewports.
    /// To resume redrawing of viewports call resumeViewportUpdates().
    /// You should use the ViewportSuspender RAII helper class to temporarily suspend viewport updates.
    void suspendViewportUpdates() { _viewportSuspendCount++; }

    /// Resumes redrawing of the viewports after a call to suspendViewportUpdates().
    void resumeViewportUpdates();

    /// Returns whether viewport updates are currently suspended.
    bool areViewportUpdatesSuspended() const { return _viewportSuspendCount > 0; }

    /// Suspends updates of the viewports whenever preliminary data pipeline results are available.
    void suspendPreliminaryViewportUpdates() { _preliminaryViewportUpdatesSuspendCount++; }

    /// Resumes updates of the viewports whenever preliminary data pipeline results are available.
    void resumePreliminaryViewportUpdates() {
        OVITO_ASSERT_MSG(_preliminaryViewportUpdatesSuspendCount > 0, "UserInterface::resumePreliminaryViewportUpdates()", "resumePreliminaryViewportUpdates() has been called more often than suspendPreliminaryViewportUpdates().");
        _preliminaryViewportUpdatesSuspendCount--;
    }

    /// Returns whether viewports should be updated whenever preliminary pipeline results are available.
    bool arePreliminaryViewportUpdatesSuspended() const { return _preliminaryViewportUpdatesSuspendCount != 0; }

    /// Flags all viewports for redrawing.
    /// This function does not lead to an immediate repainting of the viewports; instead it schedules a
    /// refresh request, which will be processed at some later time when execution returns to the Qt event loop.
    void updateViewports();

    /// Zooms all visible viewports to the extents of the scene when all scene pipelines have been fully evaluated and the extents are known.
    void zoomToSceneExtentsWhenReady();

    /// Checks (or even modifies) the contents of a DataSet after it has been loaded from a file.
    /// Returns false if loading the DataSet was rejected by the application.
    virtual bool checkLoadedDataset(DataSet* dataset) { return true; }

    /// Executes a functor that performs some actions in an interactive context and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
    /// The 'Isolated' template parameter can be set to true to indicate that the operation should execute independently
    /// from the currently active task, i.e., cancellation of one of the tasks should not affect the other.
    template<bool Isolated = false, typename Function>
    bool handleExceptions(Function&& func) noexcept;

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, the error message is displayed
    /// to the user, and this function returns false.
    template<typename Function>
    bool performActions(UndoableTransaction& transaction, Function&& func) noexcept;

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, all data changes performed by the functor so far will be undone, the error message is displayed
    /// to the user, and this function returns false. If no exception is thrown, all performed actions are committed and this function returns true.
    template<typename Function>
    bool performTransaction(const QString& undoOperationName, Function&& func) noexcept;

protected:

    /// Assigns an ActionManager.
    void setActionManager(ActionManager* manager) { _actionManager = manager; }

    /// Assigns an UndoStack.
    void setUndoStack(UndoStack* undoStack) { _undoStack = undoStack; }

    /// Is called by the TaskManager class after all tasks have been terminated and all nested event loops have been exited.
    void shutdownComplete();

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressText(Task& task, const QString& text);

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressMaximum(Task& task, qlonglong maximum, bool autoReset) {}

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressValue(Task& task, qlonglong value) {}

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressIncrementValue(Task& task, qlonglong increment) {}

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressBeginSubStepsWithWeights(Task& task, std::vector<int>&& weights) {}

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressNextSubStep(Task& task) {}

    /// Gets called by a running task to report its progress status (from any thread).
    virtual void taskProgressEndSubSteps(Task& task) {}

protected:

    /// Hosts the dataset that is currently being edited in this user interface.
    OORef<DataSetContainer> _datasetContainer;

    /// Viewport input manager of the user interface.
    ViewportInputManager* _viewportInputManager = nullptr;

    /// Actions of the user interface.
    ActionManager* _actionManager = nullptr;

    /// Manages the running asynchronous tasks that belong to this user interface.
    TaskManager _taskManager;

    /// The undo stack keeping track of changes made by the user to the current dataset.
    UndoStack* _undoStack = nullptr;

    /// The manager of ParameterUnit objects.
    UnitsManager _unitsManager;

    /// This counter tracks temporary suspension of viewport updates.
    int _viewportSuspendCount = 0;

    /// Counts the number of times preliminary viewport updates have been suspended.
    int _preliminaryViewportUpdatesSuspendCount = 0;

    /// Indicates that exitWithFatalError() has been called and the application is shutting down.
    bool _exitingDueToFatalError = false;

    /// This keeps the UI object itself alive until shutdown() is called.
    OORef<UserInterface> _selfGuard;

    friend class TaskManager; // TaskManager needs to call shutdownComplete()
    friend class Task; // Tasks need to call taskProgressText() etc.
};

}   // End of namespace

#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/utilities/concurrent/ExecutionContext.h>

namespace Ovito {

/// Executes a functor that performs some actions in an interactive context and catches any exceptions thrown during its execution.
/// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
/// The 'Isolated' template parameter can be set to true to indicate that the operation should execute independently
/// from the currently active task, i.e., cancellation of one of the tasks should not affect the other.
template<bool Isolated, typename Function>
bool UserInterface::handleExceptions(Function&& func) noexcept
{
    OVITO_ASSERT(!isBeingDeleted());

    // Note: The MainThreadOperation creates a temporary std::shared_ptr<UserInterface>, which keeps the UI alive until function exit.
    MainThreadOperation operation(ExecutionContext::Type::Interactive, *this,
        Isolated ? MainThreadOperation::Kind::Isolated : MainThreadOperation::Kind::Bound);

    try {
        std::forward<Function>(func)();
        return !operation.isCanceled();
    }
    catch(OperationCanceled) {
        OVITO_ASSERT(operation.isCanceled());
        return false;
    }
    catch(const Exception& ex) {
        reportError(ex);
        return false;
    }
}

/// Executes a functor provided by the caller that performs undoable actions in an interactive context.
/// If an exception is thrown by the functor, the error message is displayed
/// to the user, and this function returns false.
template<typename Function>
bool UserInterface::performActions(UndoableTransaction& transaction, Function&& func) noexcept
{
    OVITO_ASSERT(transaction.operation());
    OVITO_ASSERT(&transaction.userInterface() == this);
    UndoSuspender activateUndo(transaction.operation());
    return handleExceptions(std::forward<Function>(func));
}

/// Executes a functor provided by the caller that performs undoable actions in an interactive context.
/// If an exception is thrown by the functor, all data changes performed by the functor so far will be undone, the error message is displayed
/// to the user, and this function returns false. If no exception is thrown, all performed actions are committed and this function returns true.
template<typename Function>
bool UserInterface::performTransaction(const QString& undoOperationName, Function&& func) noexcept
{
    UndoableTransaction transaction(*this, undoOperationName);
    if(performActions(transaction, std::forward<Function>(func))) {
        transaction.commit();
        return true;
    }
    return false;
}

}   // End of namespace
