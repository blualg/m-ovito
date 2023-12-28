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
#include <ovito/core/app/ApplicationService.h>
#include "Application.h"

namespace Ovito {

/**
 * \brief The application object used when running as a standalone application.
 */
class OVITO_CORE_EXPORT StandaloneApplication : public Application
{
    Q_OBJECT

public:

    /// \brief Returns the one and only instance of this class.
    static StandaloneApplication* instance() {
        return qobject_cast<StandaloneApplication*>(Application::instance());
    }

    /// Inherit constructor from base class.
    using Application::Application;

    /// Destructor is called just before program exit.
    virtual ~StandaloneApplication();

    /// \brief Initializes the application.
    /// \param argc The number of command line arguments.
    /// \param argv The command line arguments.
    /// \return \c true if the application was initialized successfully;
    ///         \c false if an error occurred and the program should be terminated.
    ///
    /// This is called on program startup.
    bool initialize(int& argc, char** argv);

    /// This is called from main() just before program exit.
    void cleanupBeforeExit();

    /// \brief Returns the command line options passed to the program.
    const QCommandLineParser& cmdLineParser() const { return _cmdLineParser; }

    /// Returns the list of application services created at application startup.
    const std::vector<OORef<ApplicationService>>& applicationServices() const { return _applicationServices; }

    /// Tells the UI to process any pending events in the event queue and return immediately.
    /// The function can return true to indicate that the running operation should be canceled.
    virtual bool processUIEvents() override;

protected Q_SLOTS:

    /// Is called at program startup once the event loop is running.
    virtual void postStartupInitialization();

protected:

    /// Create the global instance of the right QCoreApplication derived class.
    virtual QCoreApplication* createQtApplicationImpl(bool supportGui, int& argc, char** argv) override;

    /// Defines the program's command line parameters.
    virtual void registerCommandLineParameters(QCommandLineParser& parser);

    /// Interprets the command line parameters provided to the application.
    virtual bool processCommandLineParameters();

    /// Prepares application at startup.
    virtual MainThreadOperation startupApplication() = 0;

protected:

    /// The parser for the command line options passed to the program.
    QCommandLineParser _cmdLineParser;

    /// The service objects created at application startup.
    std::vector<OORef<ApplicationService>> _applicationServices;
};

}   // End of namespace
