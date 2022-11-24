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
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "UserInterface.h"

#include <QOperatingSystemVersion>
#include <QProcess>

namespace Ovito {

/******************************************************************************
* Closes the user interface and shuts down the entire application after 
* displaying an error message.
******************************************************************************/
void UserInterface::exitWithFatalError(const Exception& ex) 
{
	ex.reportError(true);
	QCoreApplication::exit(1);
}

/******************************************************************************
* Tells the UI to process any pending events in the event queue and return immediately.
* The function can return true to indicate that the running operation should be canceled. 
******************************************************************************/
bool UserInterface::processEvents()
{
	QCoreApplication::processEvents();
	return false;
}

/******************************************************************************
* Creates a frame buffer of the requested size and displays it as a window in the user interface.
******************************************************************************/
std::shared_ptr<FrameBuffer> UserInterface::createAndShowFrameBuffer(int width, int height, MainThreadOperation& renderingOperation) 
{ 
	return std::make_shared<FrameBuffer>(width, height);
}

/******************************************************************************
* Returns the scene that is currently active, i.e., which is shown in the viewport window that is currently selected.
******************************************************************************/
Scene* UserInterface::activeScene() const
{
	if(DataSet* dataset = datasetContainer().currentSet()) {
		if(Viewport* vp = dataset->viewportConfig()->activeViewport()) {
			return vp->scene();
		}
	}
	return nullptr;
}

/******************************************************************************
* Indicates whether the program session is being closed and all task in progress should be canceled.
******************************************************************************/
bool UserInterface::isShuttingDown() const
{
	// If the application is closing down, the current dataset has been removed from the container.
	return datasetContainer().currentSet() == nullptr;
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	unameProcess.start("uname", QStringList() << "-m" << "-i" << "-o" << "-r" << "-v", QIODevice::ReadOnly);
#else
	unameProcess.start("uname -m -i -o -r -v", QIODevice::ReadOnly);
#endif
	unameProcess.waitForFinished();
	QByteArray unameOutput = unameProcess.readAllStandardOutput();
	unameOutput.replace('\n', ' ');
	stream << "uname output: " << unameOutput << "\n";
	// Get 'lsb_release' output.
	QProcess lsbProcess;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	lsbProcess.start("lsb_release", QStringList() << "-s" << "-i" << "-d" << "-r", QIODevice::ReadOnly);
#else
	lsbProcess.start("lsb_release -s -i -d -r", QIODevice::ReadOnly);
#endif
	lsbProcess.waitForFinished();
	QByteArray lsbOutput = lsbProcess.readAllStandardOutput();
	lsbOutput.replace('\n', ' ');
	stream << "LSB output: " << lsbOutput << "\n";
#endif
	stream << "Processor architecture: " << QSysInfo::currentCpuArchitecture() << "\n";
	stream << "Floating-point type: " << (sizeof(FloatType)*8) << "-bit" << "\n";
	stream << "Qt version: " << QT_VERSION_STR << " (" << QSysInfo::buildCpuArchitecture() << ")\n";
#ifdef OVITO_DISABLE_THREADING
	stream << "Multi-threading: disabled\n";
#endif
	stream << "Command line: " << QCoreApplication::arguments().join(' ') << "\n";
	// Let the plugin class add their information to their system report.
	for(Plugin* plugin : PluginManager::instance().plugins()) {
		for(OvitoClassPtr clazz : plugin->classes()) {
			clazz->querySystemInformation(stream, *this);
		}
	}
	return text;
}

}	// End of namespace
