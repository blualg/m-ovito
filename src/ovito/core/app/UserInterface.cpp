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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "UserInterface.h"

#include <QOperatingSystemVersion>
#include <QAbstractEventDispatcher>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(UserInterface);

/******************************************************************************
* Constructor
******************************************************************************/
UserInterface::UserInterface() : _datasetContainer(OORef<DataSetContainer>::create(_taskManager, *this))
{
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after
* displaying an error message.
******************************************************************************/
void UserInterface::exitWithFatalError(const Exception& ex)
{
    // Display fatal error message to the user.
    reportError(ex, true);

    // Make sure the main event loop is running.
    OVITO_ASSERT(QCoreApplication::instance());
    OVITO_ASSERT(QThread::currentThread()->loopLevel() != 0);

    // This will eventually trigger the QCoreApplication::aboutToQuit signal, which invokes UserInterface::shutdown().
    QCoreApplication::exit(1);
}

/******************************************************************************
* Closes the user interface immediately (without asking user to save changes).
******************************************************************************/
void UserInterface::shutdown()
{
    if(isShuttingDown())
        return;

    // Set up a local execution context (needed for the use of ObjectExecutor below).
    ExecutionContext::Scope execScope(ExecutionContext::Type::Scripting, shared_from_this());

    // Close the dataset container. This should release all objects in the current dataset.
    datasetContainer().clearAllReferences();

    // Tell other systems we are about to shutdown.
    signalAboutToQuit();

    {
        // To prevent queuing up more work while we are shutting down.
        std::lock_guard<std::mutex> lock(_pendingWorkMutex);

        // Terminate all running tasks.
        taskManager().shutdown();
        OVITO_ASSERT(isShuttingDown());

        // Discard all remaining work items.
        _pendingWork = decltype(_pendingWork)();
    }

#ifdef OVITO_USE_SYCL
    // Shuts down the SYCL queue.
    taskManager().shutdownSyclQueue();
#endif

    // Release this UI instance as soon as control returns to the event loop.
    if(_selfGuard) {
        if(QThread::currentThread()->loopLevel() != 0)
            QTimer::singleShot(0, Application::instance(), [s = std::move(_selfGuard)]() {});
        else
            _selfGuard.reset();
    }
}

/******************************************************************************
* Displays the error message(s) stored in the Exception object to the user.
******************************************************************************/
void UserInterface::reportError(const Exception& ex, bool blocking)
{
    if(!ex.traceback().isEmpty())
        qInfo().noquote() << ex.traceback();
    for(auto msg = ex.messages().crbegin(); msg != ex.messages().crend(); ++msg) {
        qInfo().noquote() << "ERROR:" << *msg;
    }
}

/******************************************************************************
* Tells the UI to process any pending events in the event queue and return immediately.
* The function can return true to indicate that the running operation should be canceled.
******************************************************************************/
bool UserInterface::processEvents()
{
    OVITO_ASSERT(QCoreApplication::instance() != nullptr);

    // While control is in the event loop, no context should be active.
    // Temporarily switch back to null contexts here.
    ExecutionContext::Scope execScope(ExecutionContext{});
    Task::Scope taskScope(nullptr);
    UndoSuspender noUndo;

    QCoreApplication::processEvents();
    return false;
}

/******************************************************************************
* Executes the given function at some later time unless the given object is
* destroyed in the meantime or the user interface is shut down.
******************************************************************************/
void UserInterface::submitWork(const RefTarget* contextObject, fu2::unique_function<void() noexcept> function, bool isScriptingContext)
{
    OVITO_ASSERT(contextObject);
    std::lock_guard<std::mutex> lock(_pendingWorkMutex);
    if(isShuttingDown() == false) {
        _pendingWork.emplace(contextObject, std::move(function), isScriptingContext);
        if(_pendingWork.size() == 1) {
            _pendingWorkCondition.notify_one();
            pendingWorkArrived();
        }
    }
}

/******************************************************************************
* Executes pending work items waiting in the queue.
******************************************************************************/
void UserInterface::executePendingWork(std::unique_lock<std::mutex>& lock)
{
    // Check that we are really in the main thread here.
    OVITO_ASSERT(ExecutionContext::isMainThread());

    while(!_pendingWork.empty()) {
        // Grab the next work item from the queue.
        Work work = std::move(_pendingWork.front());
        _pendingWork.pop();
        lock.unlock();

        // Execute work item only if the context object still exists and the user interface is not shutting down.
        // Otherwise, silently cancel the work (which still runs the destructor of the work object).
        if(!isShuttingDown()) {
            if(auto contextObject = work.obj.lock()) {
                // Establish the execution context in which the work was submitted.
                ExecutionContext::Scope execScope(work.isScriptingContext ? ExecutionContext::Type::Scripting : ExecutionContext::Type::Interactive, shared_from_this());

                // Undo recording may still be active if the GUI is currently performing an extended user operation (e.g. Animation Settings dialog may be open).
                // While the asynchronous work is being performed, undo recording should be suspended.
                UndoSuspender noUndo;

                // Execute the work function.
                std::move(work.function)();
            }
        }

        // Continue by grabbing the next work item from the queue.
        lock.lock();
    }
}

/******************************************************************************
* Keeps executing pending work items until quitWorkProcessingLoop() is called.
******************************************************************************/
void UserInterface::enterWorkProcessingLoop(Task* waitingTask, detail::TaskReference& awaitedTask)
{
    std::unique_lock<std::mutex> lock(_pendingWorkMutex);
    _pendingWorkLoopCount++;

    // Register a callback function with the awaited task, which terminates the processing loop when the task gets canceled or finishes.
    detail::FunctionTaskCallback awaitedTaskCallback(awaitedTask.get().get(), [&](int state) {
        if(state & Task::Finished) {
            quitWorkProcessingLoop();
        }
        return true;
    });

    // Register a callback function with the waiting task, which terminates the processing loop in case the waiting task gets canceled.
    detail::FunctionTaskCallback waitingTaskCallback(waitingTask, [&](int state) {
        if(state & (Task::Canceled | Task::Finished)) {
            // When the parent task gets canceled, discard the reference which keeps the awaited task running.
            awaitedTask.reset();
            quitWorkProcessingLoop();
        }
        return true;
    });

    for(;;) {
        // Block until new work arrives in the queue or quitWorkProcessingLoop() is called.
        _pendingWorkCondition.wait(lock, [this]{
            return !_pendingWork.empty() || _pendingWorkLoopCount == 0 || isShuttingDown();
        });

        // Time to quit the loop?
        if(_pendingWorkLoopCount == 0 || isShuttingDown())
            break;

        // Process newly arrived items in the work queue until the queue is drained.
        executePendingWork(lock);
    }

    // Detach callbacks from the two task objects.
    waitingTaskCallback.unregisterCallback();
    awaitedTaskCallback.unregisterCallback();
}

/******************************************************************************
* Stops executing pending work items and makes enterWorkProcessingLoop() return.
******************************************************************************/
void UserInterface::quitWorkProcessingLoop()
{
    std::lock_guard<std::mutex> lock(_pendingWorkMutex);
    _pendingWorkLoopCount--;
    _pendingWorkCondition.notify_all();
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> UserInterface::createAndShowFrameBuffer(int width, int height, bool showRenderingOperationProgress)
{
    return std::make_shared<FrameBuffer>(width, height);
}

/******************************************************************************
* This immediately redraws the viewports reflecting all
* changes made to the scene.
******************************************************************************/
void UserInterface::processViewportUpdateRequests()
{
    if(areViewportUpdatesSuspended())
        return;

    if(DataSet* dataset = datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig()) {
            for(Viewport* vp : viewportConfig->viewports())
                vp->processUpdateRequest();
        }
    }
}

/******************************************************************************
* Flags all viewports for redrawing.
******************************************************************************/
void UserInterface::updateViewports()
{
    // Check if viewport updates are suppressed.
    if(areViewportUpdatesSuspended()) {
        _viewportsNeedUpdate = true;
        return;
    }
    _viewportsNeedUpdate = false;

    if(DataSet* dataset = datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig()) {
            for(Viewport* vp : viewportConfig->viewports())
                vp->updateViewport();
        }
    }
}

/******************************************************************************
* This will resume redrawing of the viewports after a call to suspendViewportUpdates().
******************************************************************************/
void UserInterface::resumeViewportUpdates()
{
    OVITO_ASSERT(areViewportUpdatesSuspended());
    _viewportSuspendCount--;
    if(_viewportSuspendCount == 0) {
        if(_viewportsNeedUpdate)
            updateViewports();
    }
}

/******************************************************************************
* Zooms all visible viewports to the extents of the scene when all scene
* pipelines have been fully evaluated and the extents are known.
******************************************************************************/
void UserInterface::zoomToSceneExtentsWhenReady()
{
    if(DataSet* dataset = datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig())
            viewportConfig->zoomToSceneExtentsWhenReady();
    }
}

/******************************************************************************
* Queries the system's information and graphics capabilities.
******************************************************************************/
QString UserInterface::generateSystemReport()
{
    QString text;
    QTextStream stream(&text, QIODevice::WriteOnly | QIODevice::Text);
    stream << "======= System info =======\n";
    stream << "Current date: " << QDateTime::currentDateTime().toString() << "\n";
    stream << "Application: " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";
    stream << "Operating system: " <<  QOperatingSystemVersion::current().name() << " (" << QOperatingSystemVersion::current().majorVersion() << "." << QOperatingSystemVersion::current().minorVersion() << ")" << "\n";
#if defined(Q_OS_LINUX)
    // Get 'uname' output.
    QProcess unameProcess;
    unameProcess.start("uname", QStringList() << "-m" << "-i" << "-o" << "-r" << "-v", QIODevice::ReadOnly);
    unameProcess.waitForFinished();
    QByteArray unameOutput = unameProcess.readAllStandardOutput();
    unameOutput.replace('\n', ' ');
    stream << "uname output: " << unameOutput << "\n";
    // Get 'lsb_release' output.
    QProcess lsbProcess;
    lsbProcess.start("lsb_release", QStringList() << "-s" << "-i" << "-d" << "-r", QIODevice::ReadOnly);
    lsbProcess.waitForFinished();
    QByteArray lsbOutput = lsbProcess.readAllStandardOutput();
    lsbOutput.replace('\n', ' ');
    stream << "LSB output: " << lsbOutput << "\n";
#endif
    stream << "Processor architecture: " << QSysInfo::currentCpuArchitecture() << "\n";
    stream << "Qt version: " << QT_VERSION_STR << " (" << QSysInfo::buildCpuArchitecture() << ")\n";
#ifdef OVITO_DISABLE_THREADING
    stream << "Multi-threading: disabled\n";
#endif
    stream << "Command line: " << QCoreApplication::arguments().join(' ') << "\n";
    stream << "Python file path: " << PluginManager::instance().pythonDir() << "\n";
    // Let the plugin class add their information to their system report.
    for(Plugin* plugin : PluginManager::instance().plugins()) {
        for(OvitoClassPtr clazz : plugin->classes()) {
            clazz->querySystemInformation(stream, *this);
        }
    }
    return text;
}

}   // End of namespace
