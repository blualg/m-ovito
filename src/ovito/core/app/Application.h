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
#include <ovito/core/utilities/Exception.h>

namespace Ovito {

/**
 * \brief The main application.
 */
class OVITO_CORE_EXPORT Application : public QObject, public UserInterface
{
    Q_OBJECT

public:

    /// \brief Returns the one and only instance of this class.
    static Application* instance() { return _instance; }

    /// \brief Constructor.
    explicit Application(FileManager& fileManager);

    /// \brief Destructor.
    virtual ~Application();

    /// \brief Initializes the application.
    /// \param argc The number of command line arguments.
    /// \param argv The command line arguments.
    /// \return \c true if the application was initialized successfully;
    ///         \c false if an error occurred and the program should be terminated.
    bool initialize(int& argc, char** argv);

    /// \brief Handler method for Qt error messages.
    ///
    /// This can be used to set a debugger breakpoint for the OVITO_ASSERT macros.
    static void qtMessageOutput(QtMsgType type, const QMessageLogContext& context, const QString& msg);

    /// \brief Returns whether the application has been started in graphical mode.
    /// \return \c true if the application should use a graphical user interface;
    ///         \c false if the application has been started in the non-graphical console mode.
    bool guiMode() const { return !_consoleMode; }

    /// \brief Returns whether the application has been started in console mode.
    /// \return \c true if the application has been started in the non-graphical console mode;
    ///         \c false if the application should use a graphical user interface.
    bool consoleMode() const { return _consoleMode; }

    /// \brief Switches between graphical and console mode.
    void setGuiMode(bool enableGui) { _consoleMode = !enableGui; }

    /// \brief Returns whether printing of task status messages to the console is currently enabled.
    bool taskConsoleLoggingEnabled() const { return _taskConsoleLoggingEnabled; }

    /// \brief Enables or disables printing of task status messages to the console.
    void setTaskConsoleLoggingEnabled(bool enabled) { _taskConsoleLoggingEnabled = enabled; }

    /// Returns the global FileManager class instance.
    FileManager& fileManager() { return _fileManager; }

    /// Similar to QCoreApplication::applicationDirPath() but doesn't require a Qt application.
    QString applicationDirPath() const;

    /// Similar to QCoreApplication::applicationFilePath() but doesn't require a Qt application.
    QString applicationFilePath() const;

    /// Returns the major version number of the application.
    static int applicationVersionMajor();

    /// Returns the minor version number of the application.
    static int applicationVersionMinor();

    /// Returns the revision version number of the application.
    static int applicationVersionRevision();

    /// Returns the complete version string of the application release.
    static QString applicationVersionString();

    /// Returns the human-readable name of the application.
    static QString applicationName();

#ifndef Q_OS_WASM
    /// Returns the application-wide network access manager object.
    QNetworkAccessManager* networkAccessManager();
#endif

    /// Create the global Qt application object.
    void createQtApplication(bool supportGui);

protected:

    /// Creates the global instance of the right QCoreApplication derived class.
    virtual QCoreApplication* createQtApplicationImpl(bool supportGui, int& argc, char** argv) = 0;

    /// The number of original command line arguments.
    int* _argc;

    /// The original command line arguments.
    char** _argv;

    /// Indicates that the application is running in console mode.
    bool _consoleMode = true;

    /// Enables printing of task status messages to the console.
    bool _taskConsoleLoggingEnabled = false;

    /// The global file manager instance.
    FileManager& _fileManager;

#ifndef Q_OS_WASM
    /// The application-wide network manager object.
    QNetworkAccessManager* _networkAccessManager = nullptr;
#endif

    /// The default message handler method of Qt.
    static QtMessageHandler defaultQtMessageHandler;

    /// The one and only instance of this class.
    static Application* _instance;
};

}   // End of namespace
