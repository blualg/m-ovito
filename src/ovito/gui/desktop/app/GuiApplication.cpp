////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dataset/GuiDataSetContainer.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/ApplicationService.h>
#include "GuiApplication.h"

// Registers the embedded Qt resource files embedded in a statically linked executable at application startup. 
// Following the Qt documentation, this needs to be placed outside of any C++ namespace.
static void registerQtResources()
{
#ifdef OVITO_BUILD_MONOLITHIC
	Q_INIT_RESOURCE(guibase);
	Q_INIT_RESOURCE(gui);
#endif
}

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
GuiApplication::GuiApplication() : StandaloneApplication(_fileManager), UserInterface(_globalDatasetContainer, StandaloneApplication::taskManager()), 
	_fileManager(StandaloneApplication::taskManager()),
	_globalDatasetContainer(StandaloneApplication::taskManager(), *this)
{
	// Register Qt resources.
	::registerQtResources();

	// Activate our icon theme.
	QIcon::setFallbackThemeName("ovito-light");
}

/******************************************************************************
* Defines the program's command line parameters.
******************************************************************************/
void GuiApplication::registerCommandLineParameters(QCommandLineParser& parser)
{
	StandaloneApplication::registerCommandLineParameters(parser);

	parser.addOption(QCommandLineOption(QStringList{{"nogui"}}, tr("Run in console mode without displaying a graphical user interface.")));
	parser.addOption(QCommandLineOption(QStringList{{"noviewports"}}, tr("Do not create any viewports (for debugging purposes only).")));
}

/******************************************************************************
* Interprets the command line parameters provided to the application.
******************************************************************************/
bool GuiApplication::processCommandLineParameters()
{
	if(!StandaloneApplication::processCommandLineParameters())
		return false;

	// Check if program was started in console mode.
	if(!_cmdLineParser.isSet("nogui")) {
		// Enable GUI mode by default.
		_consoleMode = false;
		_headlessMode = false;
	}
	else {
		// Activate console mode.
		_consoleMode = true;
#if defined(Q_OS_LINUX)
		// On Unix/Linux, console mode means headless mode if no X server is available.
		if(!qEnvironmentVariableIsEmpty("DISPLAY")) {
			_headlessMode = false;
		}
#elif defined(Q_OS_MACOS)
		// Don't let Qt move the app to the foreground when running in console mode.
		::setenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1", 1);
		_headlessMode = false;
#elif defined(Q_OS_WIN)
		// On Windows, there is always an OpenGL implementation available for background rendering.
		_headlessMode = false;
#endif
	}

	return true;
}

/******************************************************************************
* Create the global instance of the right QCoreApplication derived class.
******************************************************************************/
void GuiApplication::createQtApplication(int& argc, char** argv)
{
	// Verify that the OpenGLSceneRenderer class has registered the right default surface format.
	OVITO_ASSERT(QSurfaceFormat::defaultFormat().depthBufferSize() == 24 && QSurfaceFormat::defaultFormat().stencilBufferSize() == 1);

	if(headlessMode()) {
		StandaloneApplication::createQtApplication(argc, argv);
	}
	else {
		// OVITO prefers the "C" locale over the system's default locale.
		QLocale::setDefault(QLocale::c());

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		// Enable high-resolution toolbar icons on hi-dpi screens.
		QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && !defined(Q_OS_MACOS)
		QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

#if defined(Q_OS_LINUX)
		// Enforce Fusion UI style on Linux.
		qunsetenv("QT_STYLE_OVERRIDE");
		QApplication::setStyle("Fusion");
#endif

		new QApplication(argc, argv);

		// Verify that a global sharing OpenGL context has been created by the Qt application as requested.
		OVITO_ASSERT(QOpenGLContext::globalShareContext() != nullptr);
	}

	// Process events sent to the Qt application by the OS.
	QCoreApplication::instance()->installEventFilter(this);
}

/******************************************************************************
* Prepares application to start running.
******************************************************************************/
MainThreadOperation GuiApplication::startupApplication()
{
	if(guiMode()) {
		// Set up graphical user interface.

		// Set the application icon.
		QIcon mainWindowIcon;
		mainWindowIcon.addFile(":/guibase/mainwin/window_icon_256.png");
		mainWindowIcon.addFile(":/guibase/mainwin/window_icon_128.png");
		mainWindowIcon.addFile(":/guibase/mainwin/window_icon_48.png");
		mainWindowIcon.addFile(":/guibase/mainwin/window_icon_32.png");
		mainWindowIcon.addFile(":/guibase/mainwin/window_icon_16.png");
		QGuiApplication::setWindowIcon(mainWindowIcon);

		// Create the main window.
		MainWindow* mainWin = new MainWindow();

		// Make the application shutdown as soon as the last main window has been closed.
		QGuiApplication::setQuitOnLastWindowClosed(true);

		// Show the main window.
		mainWin->setUpdatesEnabled(false);
#ifndef OVITO_DEBUG
		mainWin->showMaximized();
#else
		mainWin->show();
#endif
		mainWin->restoreLayout();
		mainWin->setUpdatesEnabled(true);

#ifdef OVITO_EXPIRATION_DATE
		QDate expirationDate = QDate::fromString(QStringLiteral(OVITO_EXPIRATION_DATE), Qt::ISODate);
		if(QDate::currentDate() > expirationDate) {
			QMessageBox msgbox(mainWin);
			msgbox.setWindowTitle(tr("Expiration - %1").arg(Application::applicationName()));
			msgbox.setStandardButtons(QMessageBox::Close);
			msgbox.setText(tr("<p>This is a preview version of %1 with a limited life span, which did expire on %2.</p>"
				"<p>Please obtain the final program release, which is now available on our website "
				"<a href=\"https://www.ovito.org/\">www.ovito.org</a>.</p>"
				"<p>This pre-release build of %1 can no longer be used and will quit now.</p>")
					.arg(Application::applicationName())
					.arg(expirationDate.toString(Qt::SystemLocaleLongDate)));
			msgbox.setTextInteractionFlags(Qt::TextBrowserInteraction);
			msgbox.setIcon(QMessageBox::Critical);
			msgbox.exec();
			return nullptr;
		}
#endif
		return MainThreadOperation::create(*mainWin);
	}
	else {
		// Use this application's command line user interface.
		return MainThreadOperation::create(*this);
	}
}

/******************************************************************************
* Is called at program startup once the event loop is running.
******************************************************************************/
void GuiApplication::postStartupInitialization(MainThreadOperation& operation)
{
	DataSetContainer& datasetContainer = operation.userInterface().datasetContainer();

	// This is to quit the application's event loop right after we are done executing the startup actions
	// (only when running in console mode). In GUI mode, the main window will keep the event loop going.
	QEventLoopLocker eventLoopLocker;

	// Load session state file specified on the command line.
	if(!cmdLineParser().positionalArguments().empty()) {
		QString startupFilename = cmdLineParser().positionalArguments().front();
		if(startupFilename.endsWith(".ovito", Qt::CaseInsensitive)) {
			try {
				datasetContainer.loadDataset(startupFilename, operation.createSubTask(true));
			}
			catch(const Exception& ex) {
				ex.reportError();
			}
		}
	}

	// If no .ovito state file was specified on the command line, load 
	// the user's default state from the standard location.
	if(datasetContainer.currentSet() == nullptr && guiMode()) {
		QString defaultsFilePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("defaults.ovito"));
		if(!defaultsFilePath.isEmpty()) {
			try {
				datasetContainer.loadDataset(defaultsFilePath, operation.createSubTask(true));
				datasetContainer.currentSet()->setFilePath({});
			}
			catch(Exception& ex) {
				ex.prependGeneralMessage(tr("An error occured while loading the user's default session state from the file: %1").arg(defaultsFilePath));
				ex.reportError();
			}
		}
	}

	// Create an empty dataset if nothing has been loaded.
	if(datasetContainer.currentSet() == nullptr) {
		datasetContainer.newDataset();
	}

	// Import data file(s) specified on the command line.
	if(!cmdLineParser().positionalArguments().empty()) {
		std::vector<QUrl> importUrls;
		int numSessionFiles = 0;
		for(const QString& importFilename : cmdLineParser().positionalArguments()) {
			if(importFilename.endsWith(".ovito", Qt::CaseInsensitive))
				numSessionFiles++;
			else
				importUrls.push_back(Application::instance()->fileManager().urlFromUserInput(importFilename));
		}
		try {
			if(!importUrls.empty()) {
				if(numSessionFiles)
					throw Exception(tr("Detected multiple command line arguments: Cannot open a session state file and a simulation data file at the same time."));
				if(GuiDataSetContainer* guiContainer = dynamic_object_cast<GuiDataSetContainer>(&datasetContainer))
					guiContainer->importFiles(std::move(importUrls), operation.createSubTask(true));
				else
					throw Exception(tr("Cannot import data files from the command line when running in console mode."));
			}
			if(numSessionFiles > 1)
				throw Exception(tr("Detected multiple command line arguments: Cannot open multiple session state files at the same time."));
		}
		catch(const Exception& ex) {
			ex.reportError();
		}
		if(datasetContainer.currentSet())
			datasetContainer.currentSet()->undoStack().setClean();
	}

	StandaloneApplication::postStartupInitialization(operation);
}

/******************************************************************************
* Handles events sent to the Qt application object.
******************************************************************************/
bool GuiApplication::eventFilter(QObject* watched, QEvent* event)
{
	if(event->type() == QEvent::FileOpen) {
		QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
		try {
			MainWindow* mainWindow = qobject_cast<MainWindow*>(QApplication::activeWindow());

			// If the main window is not the active window, look up it up among the list of all top-level windows
			// Only use it for opening the imported file if there currently is only a single MainWindow instance.
			if(!mainWindow) {
				for(QWidget* widget : QApplication::topLevelWidgets()) {
					if(MainWindow* mw = qobject_cast<MainWindow*>(widget)) {
						if(!mainWindow) mainWindow = mw;
						else {
							mainWindow = nullptr;
							break;
						}
					}
				}
			}

			if(mainWindow) {
				if(openEvent->file().endsWith(".ovito", Qt::CaseInsensitive)) {
					mainWindow->datasetContainer().loadDataset(openEvent->file(), MainThreadOperation::create(*mainWindow, true));
				}
				else {
					mainWindow->datasetContainer().importFiles({openEvent->url()}, MainThreadOperation::create(*mainWindow, true));
					mainWindow->datasetContainer().currentSet()->undoStack().setClean();
				}
			}
		}
		catch(const Exception& ex) {
			ex.reportError();
		}
	}
	return StandaloneApplication::eventFilter(watched, event);
}

/******************************************************************************
* Handler function for exceptions used in GUI mode.
******************************************************************************/
void GuiApplication::reportError(const Exception& ex, bool blocking)
{
	OVITO_ASSERT(QThread::currentThread() == this->thread());

	// Always display errors in the terminal window.
	Application::reportError(ex, blocking);

	if(guiMode()) {
		if(!blocking) {

			// Deferred display of the error.
			if(_errorList.empty())
				QMetaObject::invokeMethod(this, "showErrorMessages", Qt::QueuedConnection);

			// Queue error messages.
			_errorList.push_back(ex);
		}
		else {
			_errorList.push_back(ex);
			showErrorMessages();
		}
	}
}

/******************************************************************************
* Displays an error message box. This slot is called by reportError().
******************************************************************************/
void GuiApplication::showErrorMessages()
{
	while(!_errorList.empty()) {

		// Show next exception from queue.
		const Exception& exception = _errorList.front();

		// Prepare a message box dialog.
		QPointer<QMessageBox> msgbox = new QMessageBox();
		msgbox->setWindowTitle(tr("Error - %1").arg(Application::applicationName()));
		msgbox->setStandardButtons(QMessageBox::Ok);
		msgbox->setText(exception.message());
		msgbox->setIcon(QMessageBox::Critical);
		msgbox->setTextInteractionFlags(Qt::TextBrowserInteraction);

		// If the exception has been thrown within the context of a DataSet or a DataSetContainer,
		// show the message box under the corresponding window.
		QWidget* window = nullptr;
		if(DataSet* dataset = qobject_cast<DataSet*>(exception.context())) {
			if(GuiDataSetContainer* datasetContainer = qobject_cast<GuiDataSetContainer*>(dataset->container()))
				window = &datasetContainer->mainWindow();
		}
		else if(GuiDataSetContainer* datasetContainer = qobject_cast<GuiDataSetContainer*>(exception.context())) {
			window = &datasetContainer->mainWindow();
		}
		else {
			window = qobject_cast<QWidget*>(exception.context());
		}

		if(window) {

			// Stop animation playback when an error occurred.
			if(MainWindow* mainWindow = qobject_cast<MainWindow*>(window)) {
				QAction* playbackAction = mainWindow->actionManager()->getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK);
				if(playbackAction->isChecked())
					playbackAction->trigger();
			}

			// If there currently is a modal dialog box being shown,
			// make the error message dialog a child of this dialog to prevent a UI dead-lock.
			for(QDialog* dialog : window->findChildren<QDialog*>()) {
				if(dialog->isModal()) {
					window = dialog;
					dialog->show();
					break;
				}
			}

			msgbox->setParent(window);
			msgbox->setWindowModality(Qt::WindowModal);
		}

		// If the exception is associated with additional message strings,
		// show them in the Details section of the message box dialog.
		if(exception.messages().size() > 1) {
			QString detailText;
			for(int i = 1; i < exception.messages().size(); i++)
				detailText += exception.messages()[i] + QStringLiteral("\n");
			msgbox->setDetailedText(detailText);
		}

		// Show message box.
		msgbox->exec();
		if(!msgbox)
			return;
		else
			delete msgbox;
		_errorList.pop_front();
	}
}

}	// End of namespace
