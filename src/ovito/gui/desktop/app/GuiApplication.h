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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/app/StandaloneApplication.h>

namespace Ovito {

/**
 * \brief The main application with a graphical user interface.
 */
class OVITO_GUI_EXPORT GuiApplication : public StandaloneApplication
{
    Q_OBJECT

public:

    /// Returns the one and only instance of this class.
    static GuiApplication* instance() { return static_cast<GuiApplication*>(Application::instance()); }

    /// Constructor.
    GuiApplication();

    /// Create the global instance of the right QCoreApplication derived class.
    virtual void createQtApplication(int& argc, char** argv) override;

    /// Handler function for exceptions.
    virtual void reportError(const Exception& exception, bool blocking = false) override;

    /// Returns whether the application currently uses a dark UI theme.
    bool usingDarkTheme() const;

    /// Initializes an abstract user interface (e.g. a MainWindow).
    static void initializeUserInterface(UserInterface& userInterface, const QStringList& arguments);

    /// Returns whether app's UI should automatically follow the system color scheme.
    static bool automaticallyEnableDarkMode();

protected:

    /// Defines the program's command line parameters.
    virtual void registerCommandLineParameters(QCommandLineParser& parser) override;

    /// Interprets the command line parameters provided to the application.
    virtual bool processCommandLineParameters() override;

    /// Prepares application to start running.
    virtual MainThreadOperation startupApplication() override;

    /// Is called at program startup once the event loop is running.
    virtual void postStartupInitialization() override;

    /// Handles events sent to the Qt application object.
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

#ifdef OVITO_SSH_CLIENT
    /// \brief Asks the user for the login password for a SSH server.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForPassword(const QString& hostname, const QString& username, QString& password);

    /// \brief Asks the user for the answer to a keyboard-interactive question sent by the SSH server.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForKbiResponse(const QString& hostname, const QString& username, const QString& instruction, const QString& question, bool showAnswer, QString& answer);

    /// \brief Asks the user for the passphrase for a private SSH key.
    /// \return True on success, false if user has canceled the operation.
    static bool askUserForKeyPassphrase(const QString& hostname, const QString& prompt, QString& passphrase);

    /// \brief Informs the user about an unknown SSH host.
    static bool detectedUnknownSshServer(const QString& hostname, const QString& unknownHostMessage, const QString& hostPublicKeyHash);
#endif

private:

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    /// Queries the system to determine whether the desktop currently uses a dark desktop theme.
    bool detectDarkTheme() const;
#endif

#if defined(Q_OS_LINUX) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    /// Cached results of detectDarkTheme().
    mutable std::optional<bool> _usingDarkTheme;
#endif
};

}   // End of namespace
