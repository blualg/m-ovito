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
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/MixedKeyCache.h>
#include <ovito/core/dataset/DataSetContainer.h>

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
    /// \return \c true if the application was initialized successfully;
    ///         \c false if an error occurred and the program should be terminated.
    bool initialize();

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

    /// \brief Returns whether the application runs in headless mode (without an X server on Linux and no OpenGL support).
    bool headlessMode() const { return _headlessMode; }

    /// \brief Switches between graphical and console mode.
    void setGuiMode(bool enableGui) { _consoleMode = !enableGui; }

    /// Returns the global FileManager class instance.
    FileManager& fileManager() { return _fileManager; }

    /// Returns the global data cache used by visualzation elements to store rendering primitives.
    MixedKeyCache& visCache() { return _visCache; }

    /// Returns the number of parallel threads to be used by the application when doing computations.
    int idealThreadCount() const { return _idealThreadCount; }

    /// Sets the number of parallel threads to be used by the application when doing computations.
    void setIdealThreadCount(int count) { _idealThreadCount = std::max(1, count); }

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

    /// Create the global instance of the right QCoreApplication derived class.
    virtual void createQtApplication(int& argc, char** argv);

    /// Handler function for exceptions.
    virtual void reportError(const Exception& exception);

    /// Creates a signal/slot connection which is fired on shutdown.
    virtual QMetaObject::Connection whenAboutToQuit(const QObject* receiver, const char* method, Qt::ConnectionType type = Qt::AutoConnection) override;

#ifndef Q_OS_WASM
    /// Returns the application-wide network access manager object.
    QNetworkAccessManager* networkAccessManager();
#endif

Q_SIGNALS:

    /// This signal is emitted when UserInterface::shutdown() is called.
    void aboutToQuit();

protected:

    /// This virtual method is called from UserInterface::shutdown().
    virtual void signalAboutToQuit() override { Q_EMIT aboutToQuit(); }

protected:

    /// Indicates that the application is running in console mode.
    bool _consoleMode = true;

    /// Indicates that the application is running in headless mode (without OpenGL support).
    bool _headlessMode = true;

    /// The number of parallel threads to be used by the application when doing computations.
    int _idealThreadCount = 1;

    /// The global file manager instance.
    FileManager& _fileManager;

    /// The global dataset container (only used in non-GUI mode).
    DataSetContainer _globalDatasetContainer;

#ifndef Q_OS_WASM
    /// The application-wide network manager object.
    QNetworkAccessManager* _networkAccessManager = nullptr;
#endif

    /// Data cache used by visualization elements to store rendering primitives.
    MixedKeyCache _visCache;

    /// The default message handler method of Qt.
    static QtMessageHandler defaultQtMessageHandler;

    /// The one and only instance of this class.
    static Application* _instance;
};

}   // End of namespace
