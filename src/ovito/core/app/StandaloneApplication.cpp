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
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/app/UserInterface.h>
#include "StandaloneApplication.h"

namespace Ovito {

/******************************************************************************
* This is called on program startup.
******************************************************************************/
bool StandaloneApplication::initialize(int& argc, char** argv)
{
	if(!Application::initialize())
		return false;

	// Set the application name.
	QCoreApplication::setApplicationName(QStringLiteral("Ovito"));
	QCoreApplication::setOrganizationName(tr("Ovito"));
	QCoreApplication::setOrganizationDomain("ovito.org");
	QCoreApplication::setApplicationVersion(QStringLiteral(OVITO_VERSION_STRING));

	// Register command line arguments.
	_cmdLineParser.setApplicationDescription(tr("OVITO - Open Visualization Tool"));
	registerCommandLineParameters(_cmdLineParser);

	// Parse command line arguments.
	// Ignore unknown command line options for now.
	QStringList arguments;
	for(int i = 0; i < argc; i++) {
		QString arg = QString::fromLocal8Bit(argv[i]);
#ifdef Q_OS_MACOS
		// When started from the macOS Finder, the OS may pass the 'process serial number' to the application.
		// We are not interested in this command line parameter, so filter it out from the list.
		if(arg.startsWith(QStringLiteral("-psn")))
			continue;
#endif
		arguments << std::move(arg);
	}
#ifdef OVITO_DEBUG
//	arguments << QStringLiteral("C:/Users/astuk/temp/test.ovito");
#endif

	// Because they may collide with our own options, we should ignore script arguments though.
	QStringList filteredArguments;
	for(int i = 0; i < arguments.size(); i++) {
		if(arguments[i] == QStringLiteral("--scriptarg")) {
			i += 1;
			continue;
		}
		filteredArguments.push_back(arguments[i]);
	}
	_cmdLineParser.parse(filteredArguments);

	// Output program version if requested.
	if(cmdLineParser().isSet("version")) {
		std::cout << qPrintable(Application::applicationName()) << " " << qPrintable(Application::applicationVersionString()) << std::endl;
		_consoleMode = true;
		return true;
	}

	// Help command line option implicitly activates console mode.
	if(_cmdLineParser.isSet("help")) {
		_consoleMode = true;
	}

	try {
		// Interpret the command line arguments.
		if(!processCommandLineParameters()) {
			return true;
		}
	}
	catch(const Exception& ex) {
		reportError(ex);
		return false;
	}

	// Create Qt application object.
	createQtApplication(argc, argv);

	// Reactivate default "C" locale, which, in the meantime, may have been changed by QCoreApplication.
	std::setlocale(LC_NUMERIC, "C");

	try {
		// Load plugins.
		PluginManager::initialize();
		PluginManager::instance().loadAllPlugins();

		// Load application service classes and let them register their custom command line options.
		for(OvitoClassPtr clazz : PluginManager::instance().listClasses(ApplicationService::OOClass())) {
			OORef<ApplicationService> service = static_object_cast<ApplicationService>(clazz->createInstance());
			service->registerCommandLineOptions(_cmdLineParser);
			_applicationServices.push_back(std::move(service));
		}

		// Parse the command line parameters again after the plugins have registered their options.
		if(!_cmdLineParser.parse(arguments)) {
	        qInfo().noquote() << "Error:" << _cmdLineParser.errorText();
			_consoleMode = true;
			shutdown();
			return false;
		}

		// Handle --help command line option. Print list of command line options and quit.
		if(_cmdLineParser.isSet("help")) {
			std::cout << qPrintable(_cmdLineParser.helpText()) << std::endl;
			shutdown();
			return true;
		}

		// Prepares the application to start running. Creates the global Qt application object.
		MainThreadOperation startupOperation = startupApplication();
		if(!startupOperation.isValid()) {
			shutdown();
			return false;
		}

		// Notify registered application services that application is starting up.
		for(const auto& service : applicationServices()) {
			// If any of the service callbacks returns false, abort the application startup process.
			if(!service->applicationStarting()) {
				shutdown();
				return false;
			}
		}

		// Complete the startup process by calling postStartupInitialization() once the main event loop is running.
		ObjectExecutor(this, true).execute([this, promise=Promise<>(std::move(startupOperation))]() {
			try {
				// Let the application perform late initialization steps.
				postStartupInitialization();

				// If someone has canceled the startup process, quit the application.
				if(promise.isCanceled()) 
					QCoreApplication::exit(1);
				else
					promise.setFinished();
			}
			catch(const Exception& ex) {
				// Shutdown with error exit code when running in scripting mode.
				if(consoleMode())
					ExecutionContext::current().ui().exitWithFatalError(ex);
				else
					reportError(ex);
			}
		});
	}
	catch(const Exception& ex) {
		reportError(ex);
		shutdown();
		return false;
	}

	return true;
}

/******************************************************************************
* Is called at program startup once the event loop is running.
******************************************************************************/
void StandaloneApplication::postStartupInitialization()
{
	// Notify registered application services that the application is fully running now.
	for(const auto& service : applicationServices())
		service->applicationStarted();
}

/******************************************************************************
* Defines the program's command line parameters.
******************************************************************************/
void StandaloneApplication::registerCommandLineParameters(QCommandLineParser& parser)
{
	parser.addOption(QCommandLineOption(QStringList{{"h", "help"}}, tr("Shows this list of program options and exits.")));
	parser.addOption(QCommandLineOption(QStringList{{"v", "version"}}, tr("Prints the program version and exits.")));
	parser.addOption(QCommandLineOption(QStringList{{"nthreads"}}, tr("Sets the number of parallel threads to use for computations."), QStringLiteral("N")));
}

/******************************************************************************
* Interprets the command line parameters provided to the application.
******************************************************************************/
bool StandaloneApplication::processCommandLineParameters()
{
	// Output program version if requested.
	if(cmdLineParser().isSet("version")) {
		std::cout << qPrintable(Application::applicationName()) << " " << qPrintable(Application::applicationVersionString()) << std::endl;
		return false;
	}

	// User can overwrite the number of parallel threads to use.
	if(cmdLineParser().isSet("nthreads")) {
		bool ok;
		int nthreads = cmdLineParser().value("nthreads").toInt(&ok);
		if(!ok || nthreads <= 0)
			throw Exception(tr("Invalid thread count specified on command line."));
		setIdealThreadCount(nthreads);
	}

	return true;
}

/******************************************************************************
* Starts the main event loop.
******************************************************************************/
int StandaloneApplication::runApplication()
{
	// Enter the main event loop.
	return QCoreApplication::exec();
}

/******************************************************************************
* This is called on program shutdown.
******************************************************************************/
void StandaloneApplication::shutdown()
{
	// Destroy Qt application object.
	delete QCoreApplication::instance();

	// Release application services.
	_applicationServices.clear();

	// Unload plugins.
	PluginManager::shutdown();
}

}	// End of namespace
