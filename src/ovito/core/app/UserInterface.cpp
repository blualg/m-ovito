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
UserInterface::UserInterface() : _taskManager(this), _datasetContainer(OORef<DataSetContainer>::create(_taskManager, *this))
{
}

/******************************************************************************
* Closes the user interface and shuts down the entire application after
* displaying an error message.
******************************************************************************/
void UserInterface::exitWithFatalError(const Exception& ex)
{
    OVITO_ASSERT(ExecutionContext::isMainThread());

    // Display fatal error message to the user.
    reportError(ex, true);

    // Quit the Qt event loop if it is running.
    if(QCoreApplication::instance() != nullptr) {
        OVITO_ASSERT(QThread::currentThread()->loopLevel() != 0);
        // This will eventually trigger the QCoreApplication::aboutToQuit() signal, which invokes UserInterface::shutdown().
        QCoreApplication::exit(1);
    }
    else {
        // No Qt event loop running. Just shut down the application immediately.
        shutdown();
    }
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

    // Terminate all running tasks and empty the deferred work queue.
    taskManager().shutdown();

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
bool UserInterface::processUIEvents()
{
    // While control is in the event loop, no context should be active.
    // Temporarily switch back to null contexts here.
    ExecutionContext::Scope execScope(ExecutionContext{});
    Task::Scope taskScope(nullptr);
    UndoSuspender noUndo;

    // Process Qt events (only if the main Qt event loop is running).
    if(QCoreApplication::instance() != nullptr && QThread::currentThread()->loopLevel() != 0) {
        QCoreApplication::sendPostedEvents(); // Process events sent by the application
        QCoreApplication::processEvents();    // Process events sent by the OS
    }

    // Process pending work items in our own queue.
    taskManager().executePendingWork();

    return isShuttingDown();
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> UserInterface::createAndShowFrameBuffer(int width, int height)
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
