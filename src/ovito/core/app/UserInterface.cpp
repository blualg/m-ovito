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
#include <QProcess>

namespace Ovito {

/******************************************************************************
* Closes the user interface and shuts down the entire application after
* displaying an error message.
******************************************************************************/
void UserInterface::exitWithFatalError(const Exception& ex)
{
    // Display fata error message to the user.
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

    // Close the dataset container. This should release all data objects.
    datasetContainer().clearAllReferences();

    // Tell other systems we are about to shutdown.
    signalAboutToQuit();

    {
        // To prevent queing more work while we are shutting down.
        std::lock_guard<std::mutex> lock(_pendingWorkMutex);

        // Terminate all running tasks.
        taskManager().shutdown();
    }
    OVITO_ASSERT(isShuttingDown());

    // Release this UI instance as soon as control returns to the event loop.
    if(_selfGuard) {
        if(QThread::currentThread()->loopLevel() != 0)
            ObjectExecutor(Application::instance(), true).execute([s = std::move(_selfGuard)]() mutable noexcept {});
        else
            _selfGuard.reset();
    }
}

/******************************************************************************
* Displays the error message(s) stored in the Exception object to the user.
******************************************************************************/
void UserInterface::reportError(const Exception& ex, bool blocking)
{
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
    // While control is in the event loop, no context should be active.
    // Temporarily switch back to null contexts here.
    ExecutionContext::Scope execScope(ExecutionContext{});
    Task::Scope taskScope(nullptr);
    UndoSuspender noUndo;

    QCoreApplication::processEvents();
    return false;
}

/******************************************************************************
* Executes pending work items waiting in the queue.
******************************************************************************/
void UserInterface::executePendingWork()
{
    OVITO_ASSERT(!QCoreApplication::instance() || QThread::currentThread() == QCoreApplication::instance()->thread());

    std::unique_lock<std::mutex> lock(_pendingWorkMutex);
    while(!_pendingWork.empty()) {
        Work work = std::move(_pendingWork.front());
        _pendingWork.pop();
        lock.unlock();

        // Execute work only if the context object still exists and the user interface is not shutting down.
        // Otherwise, silently cancel the work (but still run the destructor of the callable).
        if(!work.obj.isNull() && !isShuttingDown()) {
            // Activate the execution context to which the work was submitted.
            ExecutionContext::Scope execScope(work.isScriptingContext ? ExecutionContext::Type::Scripting : ExecutionContext::Type::Interactive, shared_from_this());

            // Undo recording may still be active if the GUI is currently performing a extended user operation (e.g. Animation Settings dialog).
            // While the asynchronous work is being performed, undo recording should be suspended.
            UndoSuspender noUndo;

            // Execute the work function.
            std::move(work.function)();
        }

        lock.lock();
    }
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
