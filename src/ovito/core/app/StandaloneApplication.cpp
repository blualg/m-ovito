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
    if(!Application::initialize(argc, argv))
        return false;

    // Set the application name.
    QCoreApplication::setApplicationName(QStringLiteral("Ovito"));
    QCoreApplication::setOrganizationName(tr("Ovito"));
    QCoreApplication::setOrganizationDomain("ovito.org");
    QCoreApplication::setApplicationVersion(QStringLiteral(OVITO_VERSION_STRING));

    // OVITO prefers the "C" locale over the system's default locale.
    QLocale::setDefault(QLocale::c());

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
        // We are not interested in this parameter, so exclude it from our internal list.
        if(arg.startsWith(QStringLiteral("-psn")))
            continue;
#endif
        arguments << std::move(arg);
    }

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
        setGuiMode(false);
        return true;
    }

    // Help command line option implicitly activates console mode.
    if(_cmdLineParser.isSet("help")) {
        setGuiMode(false);
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
            setGuiMode(false);
            return false;
        }

        // Handle --help command line option. Print list of command line options and quit.
        if(_cmdLineParser.isSet("help")) {
            std::cout << qPrintable(_cmdLineParser.helpText()) << std::endl;
            return true;
        }

        // Establish a temporary task context in which application initialization takes place.
        MainThreadOperation initializationTask(*this, MainThreadOperation::Kind::Isolated, false);

        // Notify registered application services that the application is initializing.
        for(const auto& service : applicationServices()) {
            service->applicationInitializing();
        }

        // Prepare the application to start running.
        MainThreadOperation startupOperation = startupApplication();

        // Notify registered application services that the application is starting up.
        for(const auto& service : applicationServices()) {
            service->applicationStarting();
        }

        // Complete the startup process by calling postStartupInitialization() once the main event loop is running.
        DeferredObjectExecutor(this).execute([startupOperation = PromiseBase(std::move(startupOperation))]() noexcept {
            Task::Scope taskScope(startupOperation.task());
            try {
                try {
                    // Let the application perform further initialization steps.
                    StandaloneApplication::instance()->postStartupInitialization();

                    if(startupOperation.isCanceled()) {
                        // If something has canceled the startup process, close the window and quit the app.
                        this_task::ui()->shutdown();
                        QCoreApplication::exit(1);
                    }
                    else {
                        // Startup phase is complete.
                        startupOperation.setFinished();
                    }
                }
                catch(const Exception&) {
                    throw;
                }
                catch(const std::bad_alloc&) {
                    throw Exception(tr("Not enough memory."));
                }
                catch(const std::exception& ex) {
                    qWarning() << "WARNING: non-standard exception thrown during application startup:" << ex.what();
                    throw Exception(tr("Exception: %1").arg(QString::fromLatin1(ex.what())));
                }
            }
            catch(const Exception& ex) {
                // Shutdown with error exit code when running in scripting mode.
                if(Application::instance()->guiMode())
                    this_task::ui()->reportError(ex);
                else
                    this_task::ui()->exitWithFatalError(ex);
            }
        });

        // Make sure the main event loop is not running yet at this point.
        OVITO_ASSERT(QThread::currentThread()->loopLevel() == 0);
    }
    catch(OperationCanceled) {
        return false;
    }
    catch(const Exception& ex) {
        reportError(ex);
        return false;
    }

    return true;
}

/******************************************************************************
* Destructor is called just before program exit.
******************************************************************************/
StandaloneApplication::~StandaloneApplication()
{
    // Destroy Qt application object (if there is one).
    delete QCoreApplication::instance();

    // Release application services.
    _applicationServices.clear();

    // Unload plugins.
    PluginManager::shutdown();
}

/******************************************************************************
* Is called at program startup once the event loop is running.
******************************************************************************/
void StandaloneApplication::postStartupInitialization()
{
    // Notify registered application services that the application is fully running now.
    for(const auto& service : applicationServices()) {
        service->applicationStarted();
        if(this_task::isCanceled())
            break;
    }
}

/******************************************************************************
* Create the global instance of the right QCoreApplication derived class.
******************************************************************************/
QCoreApplication* StandaloneApplication::createQtApplicationImpl(bool supportGui, int& argc, char** argv)
{
    if(supportGui) {
        return new QGuiApplication(argc, argv);
    }
    else {
#ifdef Q_OS_LINUX
        // On Linux, use the 'minimal' QPA platform plugin instead of the standard XCB plugin when no X server is available.
        // Still create a Qt GUI application object, because otherwise we cannot use (offscreen) font rendering functions.
        if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            qputenv("QT_QPA_PLATFORM", "minimal");
            // Enable rudimentary font rendering support, which is implemented by the 'minimal' platform plugin:
            if(!qEnvironmentVariableIsSet("QT_DEBUG_BACKINGSTORE")) qputenv("QT_DEBUG_BACKINGSTORE", "1");
        }

        // Set the font directory path.
        if(!qEnvironmentVariableIsSet("QT_QPA_FONTDIR")) {
            // Determine font directory path.
            std::string applicationPath = argv[0];
            auto sepIndex = applicationPath.rfind('/');
            if(sepIndex != std::string::npos)
                applicationPath.resize(sepIndex + 1);
            std::string fontPath = applicationPath + "../share/ovito/fonts";
            if(!QDir(QString::fromStdString(fontPath)).exists())
                fontPath = "/usr/share/fonts";
            qputenv("QT_QPA_FONTDIR", fontPath.c_str());
        }

        // Disable OpenGL context sharing, because we cannot create GL contexts when using the 'minimal' QPA plugin.
        // If AA_ShareOpenGLContexts is set, the QGuiApplication constructor tries to create an OpenGL context, which fails with a warning message:
        // "This plugin does not support createPlatformOpenGLContext!".
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#endif

        // Create a QGuiApplication because we need at least the font rendering capability of Qt.
        return new QGuiApplication(argc, argv);
    }
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
        taskManager().setMaxThreadCount(nthreads);
    }

    return true;
}

}   // End of namespace
