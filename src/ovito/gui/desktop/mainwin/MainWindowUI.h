////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

class OVITO_GUI_EXPORT MainWindowUI : public UserInterface
{
    OVITO_CLASS(MainWindowUI)

public:

    /// Initialization function.
    void initializeObject();

    /// Destructor.
    ~MainWindowUI();

    /// Indicates whether the main window widget still exists.
    /// The main window may be closed while the UI object still exists.
    bool hasMainWindow() const { return _mainWindow != nullptr; }

    /// Returns the main window widget associated with this UI object.
    /// Note: This method may only be called while the UI object is still associated with a main window.
    MainWindow* mainWindow() const { OVITO_ASSERT(hasMainWindow()); return _mainWindow; }

    /// Displays a message string in the window's status bar.
    virtual void showStatusBarMessage(const QString& message, int timeout = 0) override;

    /// Hides any messages currently displayed in the window's status bar.
    virtual void clearStatusBarMessage() override;

    /// Gives the active viewport the input focus.
    virtual void setViewportInputFocus() override;

    /// Closes the user interface and shuts down the entire application after displaying an error message.
    virtual void exitWithFatalError(const Exception& ex) override;

    /// Creates a frame buffer of the requested size for rendering and displays it in a window in the user interface.
    virtual std::shared_ptr<FrameBuffer> createAndShowFrameBuffer(int width, int height) override;

    /// Shows a progress bar or a similar UI to indicate the current rendering progress and let the user cancel the operation if necessary.
    virtual void showRenderingProgress(const std::shared_ptr<FrameBuffer>& frameBuffer, SharedFuture<void> renderingFuture) override;

    /// \brief Returns whether animation recording is active and animation keys should be automatically generated.
    /// \return \c true if animating is currently turned on and not suspended; \c false otherwise.
    ///
    /// When animating is turned on, controllers should automatically set keys when their value is changed.
    virtual bool isAutoGenerateAnimationKeysEnabled() const override { return _autoKeyModeOn && _animSuspendCount == 0; }

    /// Cancels all running tasks associated with this user interface and closes the user interface as soon as possible (without asking user to save changes).
    virtual bool shutdown() override;

    /// Displays an error message to the user.
    virtual void reportError(const Exception& ex, bool blocking = false) override;

    /// Displays a modal message box to the user. Blocks until the user closes the message box.
    /// This method wraps the QMessageBox class of the Qt library.
    virtual MessageBoxButton showMessageBox(MessageBoxIcon icon, const QString& title, const QString& text, int buttons, MessageBoxButton defaultButton = NoButton, const QString& detailedText = {}) override;

    /// Checks (or even modifies) the contents of a DataSet after it has been loaded from a file.
    /// Returns false if loading the DataSet was rejected by the application.
    virtual bool checkLoadedDataset(DataSet* dataset) override;

    /// Registers a new task progress record with this user interface.
    /// This method gets called when a new TaskProgress instance is created from a running task.
    virtual std::mutex* taskProgressBegin(TaskProgress* progress) override;

    /// Unregisters a task progress record from this user interface.
    /// This method gets called when a previously registered task finishes.
    virtual void taskProgressEnd(TaskProgress* progress) override;

    /// Informs the user interface that a task's progress state has changed.
    virtual void taskProgressChanged(TaskProgress* progress) override;

    /// Lets the caller visit all registered worker tasks that are in progress.
    void visitRunningTasks(std::function<void(const QString&,int,int)> visitor);

    /// \brief Imports a set of files into the current dataset.
    /// \param urls The locations of the files to import.
    /// \param importerType The FileImporter type selected by the user. If null, the file's format will be auto-detected.
    /// \param importerFormat The sub-format name selected by the user, which is supported by the selected importer class.
    /// \throw Exception on error.
    void importFiles(const std::vector<QUrl>& urls, const FileImporterClass* importerType = nullptr, const QString& importerFormat = {});

    /// \brief Save the current dataset.
    /// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
    /// \throw Exception on error.
    ///
    /// If the current dataset has not been assigned a file path, then this method
    /// displays a file selector dialog by calling fileSaveAs() to let the user select a file path.
    bool fileSave();

    /// \brief Lets the user select a new destination filename for the current dataset. Then saves the dataset by calling fileSave().
    /// \param filename If \a filename is an empty string that this method asks the user for a filename. Otherwise
    ///                 the provided filename is used.
    /// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
    /// \throw Exception on error.
    bool fileSaveAs(const QString& filename = QString());

    /// \brief Asks the user if changes made to the dataset should be saved.
    ///
    /// If the current dataset has been changed, this method asks the user if changes should be saved.
    /// If yes, then the dataset is saved by calling fileSave().
    void askForSaveChanges();

    /// The type-erased function object type to be passed to scheduleOperationAfterScenePreparation().
    using operation_function = fu2::function_base<
        true, // IsOwning = true: The function object owns the callable object and is responsible for its destruction.
        false, // IsCopyable = false: The function object is not copyable.
        fu2::capacity_fixed<3 * sizeof(std::shared_ptr<OvitoObject>)>, // Capacity: Defines the internal capacity of the function for small functor optimization.
        false, // IsThrowing = false: Do not throw an exception on empty function call, call `std::abort` instead.
        true, // HasStrongExceptGuarantee = true: All objects satisfy the strong exception guarantee
        void()>;

    /// Waits for the pipelines in the current scene to be fully evaluated, then executes the given operation.
    /// A progress dialog may be displayed while waiting for the scene preparation to complete.
    void scheduleOperationAfterScenePreparation(Scene* scene, const QString& waitingMessage, operation_function&& operation);

private:

    /// Notifies all registered listeners that the progress state of the registered tasks has changed.
    void notifyProgressTasksChanged();

private:

    /// The main window widget associated with this UI object.
    MainWindow* _mainWindow = nullptr;

    /// Indicates whether the user has activated auto-key animation mode.
    bool _autoKeyModeOn = false;

    /// Head of doubly-linked list of all registered task progress records.
    TaskProgress* _progressTasksHead = nullptr;

    /// Tail of doubly-linked list of all registered task progress records.
    TaskProgress* _progressTasksTail = nullptr;

    /// Guards thread-safe access to the task list.
    std::mutex _progressTaskListMutex;

    /// Indicates that a delayed task progress update is underway.
    std::atomic_bool _progressUpdateScheduled{false};

    friend class MainWindow; // Allow direct access to the _mainWindow pointer.
};

// Instantiate class templates.
#ifndef OVITO_BUILD_MONOLITHIC
#if !defined(Core_EXPORTS)
extern template class OVITO_GUI_EXPORT UserInterfaceComponent<MainWindowUI>;
#elif !defined(Q_CC_MSVC) && !defined(Q_CC_CLANG)
template class OVITO_GUI_EXPORT UserInterfaceComponent<MainWindowUI>;
#endif
#endif

}   // End of namespace
