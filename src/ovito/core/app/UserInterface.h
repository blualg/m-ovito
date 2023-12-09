////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * \brief Abstract interface to the graphical user interface of the application.
 *
 * Note that is is possible to open multiple GUI windows per process.
 */
class OVITO_CORE_EXPORT UserInterface : public std::enable_shared_from_this<UserInterface>
{
public:

    /// Constructor.
    explicit UserInterface(DataSetContainer& datasetContainer) : _datasetContainer(datasetContainer) {}

    /// Destructor.
    virtual ~UserInterface() = default;

    /// Returns the container managing the current dataset.
    DataSetContainer& datasetContainer() const { return _datasetContainer; }

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

    /// Closes the user interface immediately (without asking user to save changes).
    void shutdown();

    /// Indicates whether the session is in the process of being closed and all ongoing tasks should be canceled.
    bool isShuttingDown() const { return _taskManager.isShuttingDown(); }

    /// Call this to keep the UI object alive until shutdown() is called on it.
    void keepAliveUntilShutdown() { _selfGuard = shared_from_this(); }

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
    virtual std::shared_ptr<FrameBuffer> createAndShowFrameBuffer(int width, int height, bool showRenderingOperationProgress);

    /// Returns the undo stack, which keeps track of changes made by the user to the current dataset.
    /// It may be none if not running as a desktop application.
    UndoStack* undoStack() const { return _undoStack; }

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

    /// Suspends updates of the viewports whenever preliminary data pipeline results are available.
    void suspendPreliminaryViewportUpdates() { _preliminaryViewportUpdatesSuspendCount++; }

    /// \brief Resumes updates of the viewports whenever preliminary data pipeline results are available.
    void resumePreliminaryViewportUpdates() {
        OVITO_ASSERT_MSG(_preliminaryViewportUpdatesSuspendCount > 0, "UserInterface::resumePreliminaryViewportUpdates()", "resumePreliminaryViewportUpdates() has been called more often than suspendPreliminaryViewportUpdates().");
        _preliminaryViewportUpdatesSuspendCount--;
    }

    /// Returns whether viewports should be updated whenever preliminary pipeline results are available.
    bool arePreliminaryViewportUpdatesSuspended() const { return _preliminaryViewportUpdatesSuspendCount != 0; }

    /// Flags all viewports for redrawing.
    ///
    /// This function does not lead to an immediate repainting of the viewports; instead it schedules a
    /// paint event for deferred processing when execution returns to the Qt event loop.
    ///
    /// To update just a single viewport, Viewport::updateViewport() should be used instead.
    ///
    /// To redraw all viewports immediately, also call processViewportUpdateRequests().
    void updateViewports();

    /// Returns whether any of the visible interactive viewports is currently being rendered.
    bool isRenderingInteractiveViewports() const { return _viewportBeingRendered != nullptr; }

    /// Zooms all visible viewports to the extents of the scene when all scene pipelines have been fully evaluated and the extents are known.
    void zoomToSceneExtentsWhenReady();

    /// Executes a functor that performs some actions in an interactive context and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
    template<typename Function>
    bool handleExceptions(Function&& func, bool visibleInUserInterface = false);

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, the error message is displayed
    /// to the user, and this function returns false.
    template<typename Function>
    bool performActions(UndoableTransaction& transaction, Function&& func);

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, all data changes performed by the functor so far will be undone, the error message is displayed
    /// to the user, and this function returns false. If no exception is thrown, all performed actions are committed and this function returns true.
    template<typename Function>
    bool performTransaction(const QString& undoOperationName, Function&& func);

    /// Executes the given function at some later time unless the given object is destroyed in the meantime or the user interface is shut down.
    void submitWork(const QObject* contextObject, fu2::unique_function<void() noexcept> function, bool isScriptingContext);

protected:

    /// Assigns an ActionManager.
    void setActionManager(ActionManager* manager) { _actionManager = manager; }

    /// Assigns an UndoStack.
    void setUndoStack(UndoStack* undoStack) { _undoStack = undoStack; }

    /// This pure virtual method is called from UserInterface::shutdown().
    virtual void signalAboutToQuit() = 0;

    /// This pure virtual method is called from UserInterface::submitWork().
    virtual void pendingWorkArrived() = 0;

    /// Executes pending work items waiting in the deferred execution queue.
    void executePendingWork();

private:

    /// Hosts the dataset that is currently being edited in this user interface.
    DataSetContainer& _datasetContainer;

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

    /// Indicates that the viewports have been invalidated while updates were suspended.
    bool _viewportsNeedUpdate = false;

    /// Counts the number of times preliminary viewport updates have been suspended.
    int _preliminaryViewportUpdatesSuspendCount = 0;

    /// The interactive viewport currently being rendered.
    Viewport* _viewportBeingRendered = nullptr;

    /// This keeps the UI object itself alive until shutdown() is called.
    std::shared_ptr<UserInterface> _selfGuard;

    /// A piece of work that has been submitted for deferred execution in the main thread.
    struct Work {
        /// Constructor.
        template<typename Function>
        Work(QPointer<const QObject> obj_, Function&& function_, bool isScriptingContext_) :
            obj(std::move(obj_)), function(std::forward<Function>(function_)), isScriptingContext(isScriptingContext_) {}

        /// The context object this work is associated with.
        /// If the object is destroyed before the work is executed, the work is canceled.
        QPointer<const QObject> obj;

        /// The function to be executed.
        fu2::unique_function<void() noexcept> function;

        /// Indicates whether the work is being performed in a scripting context or an interactive context.
        bool isScriptingContext;
    };

    /// The queue of all pending work items that have been submitted for deferred execution in the main thread.
    std::queue<Work> _pendingWork;

    /// Manages thread-safe concurrent access to the work queue.
    std::mutex _pendingWorkMutex;

#ifdef OVITO_DEBUG
protected:
    bool _isBeingDestructed = false;
#endif

    friend class Viewport;
};

}   // End of namespace

#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/utilities/concurrent/ExecutionContext.h>

namespace Ovito {

/// Executes a functor that performs some actions in an interactive context and catches any exceptions thrown during its execution.
/// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
template<typename Function>
bool UserInterface::handleExceptions(Function&& func, bool visibleInUserInterface)
{
    OVITO_ASSERT(!_isBeingDestructed);
    MainThreadOperation operation(ExecutionContext::Type::Interactive, *this, visibleInUserInterface); // Note: This creates a temporary std::shared_ptr<UserInterface> to keep the UI alive until function exit.
    try {
        if constexpr(detail::is_invocable_v<Function, MainThreadOperation&>) {
            std::forward<Function>(func)(operation);
        }
        else {
            std::forward<Function>(func)();
        }
        return !operation.isCanceled();
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
bool UserInterface::performActions(UndoableTransaction& transaction, Function&& func)
{
    OVITO_ASSERT(!_isBeingDestructed);
    OVITO_ASSERT(transaction.operation());
    OVITO_ASSERT(&transaction.userInterface() == this);
    UndoSuspender activateUndo(transaction.operation());
    return handleExceptions(std::forward<Function>(func));
}

/// Executes a functor provided by the caller that performs undoable actions in an interactive context.
/// If an exception is thrown by the functor, all data changes performed by the functor so far will be undone, the error message is displayed
/// to the user, and this function returns false. If no exception is thrown, all performed actions are committed and this function returns true.
template<typename Function>
bool UserInterface::performTransaction(const QString& undoOperationName, Function&& func)
{
    OVITO_ASSERT(!_isBeingDestructed);
    UndoableTransaction transaction(*this, undoOperationName);
    if(performActions(transaction, std::forward<Function>(func))) {
        transaction.commit();
        return true;
    }
    return false;
}

}   // End of namespace
