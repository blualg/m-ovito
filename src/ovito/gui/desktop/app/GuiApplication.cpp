////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/mainwin/OvitoStyle.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "GuiApplication.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    #ifdef Q_OS_LINUX
        #include <QDBusConnection>
        #include <QDBusMessage>
        #include <QDBusVariant>
        #include <QDBusReply>
    #endif
#endif

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
GuiApplication::GuiApplication()
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
    if(_cmdLineParser.isSet("nogui")) {
        // Activate console mode.
        setRunMode(Application::TerminalMode);
#if defined(Q_OS_MACOS)
        // Don't let Qt move the app to the foreground when running in console mode.
        ::setenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1", 1);
#endif
    }

    return true;
}

/******************************************************************************
* Create the global instance of the right QCoreApplication derived class.
******************************************************************************/
QCoreApplication* GuiApplication::createQtApplicationImpl(bool supportGui, int& argc, char** argv)
{
    // Verify that the OpenGLRenderer class has registered the right default surface format.
    OVITO_ASSERT(QSurfaceFormat::defaultFormat().depthBufferSize() == 24 && QSurfaceFormat::defaultFormat().stencilBufferSize() == 1);

    QCoreApplication* qtApp = nullptr;
    if(!supportGui) {
        qtApp = StandaloneApplication::createQtApplicationImpl(supportGui, argc, argv);
    }
    else {
#ifndef Q_OS_MACOS
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        // Use Fusion UI style on Windows to enable dark mode support (can be changed by the user in the application settings).
        // If dark mode is not enabled in the application settings (default), use the classic Windows Vista UI style.
        if(automaticallyEnableDarkMode()) {
            qputenv("QT_QPA_PLATFORM", "windows:darkmode=2");
            QApplication::setStyle("Fusion");
        }
        else {
            qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");
            QApplication::setStyle("windowsvista");
        }
#elif defined(Q_OS_LINUX)
        // Prefer XCB platform plugin over Wayland.
        if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "xcb");

        // Always enforce Fusion UI style on Linux.
        qunsetenv("QT_STYLE_OVERRIDE");
        QApplication::setStyle("Fusion");
#endif

        // Create the global application object.
        qtApp = new QApplication(argc, argv);

#if !defined(Q_OS_WIN) || QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        // Install our customized UI style.
        QApplication::setStyle(new OvitoStyle());
#endif

        // Verify that a global sharing OpenGL context has been created by the Qt application as requested.
        OVITO_ASSERT(QOpenGLContext::globalShareContext() != nullptr);

#ifdef OVITO_SSH_CLIENT
        if(Application::guiEnabled()) {
            fileManager().registerAskUserForPasswordImpl(&GuiApplication::askUserForPassword);
            fileManager().registerAskUserForKbiResponseImpl(&GuiApplication::askUserForKbiResponse);
            fileManager().registerAskUserForKeyPassphraseImpl(&GuiApplication::askUserForKeyPassphrase);
            fileManager().registerAskUserForPKCS11CredentialsImpl(&GuiApplication::askUserForPKCS11Credentials);
            fileManager().registerDetectedUnknownSshServerImpl(&GuiApplication::detectedUnknownSshServer);
        }
#endif
    }

    // Process events sent to the Qt application by the OS.
    qtApp->installEventFilter(this);

    return qtApp;
}

#if defined(Q_OS_LINUX) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
static void activateThemeColors(bool dark)
{
    static const QPalette lightPalette = qApp->palette();
    static const QPalette darkPalette = []() {
        QColor darkGray(53, 53, 53);
        QColor gray(128, 128, 128);
        QColor black(25, 25, 25);
        QColor blue(42, 130, 218);

        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, darkGray);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, black);
        darkPalette.setColor(QPalette::AlternateBase, darkGray);
        darkPalette.setColor(QPalette::ToolTipBase, blue);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, darkGray);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Link, blue);
        darkPalette.setColor(QPalette::Highlight, blue);
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::PlaceholderText, QColor(83,83,83));
        darkPalette.setColor(QPalette::Active, QPalette::Button, gray.darker());
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, gray);
        darkPalette.setColor(QPalette::Disabled, QPalette::Light, darkGray);
        return darkPalette;
    }();
    qApp->setPalette(dark ? darkPalette : lightPalette);
}
#endif

/******************************************************************************
* Prepares application to start running.
******************************************************************************/
MainThreadOperation GuiApplication::startupApplication()
{
    OVITO_ASSERT(this_task::get());

    if(Application::guiEnabled()) {
        // Set up Qt event loop.
        createQtApplication(true);

        // Activate icon theme that matches the current UI theme.
        bool darkTheme = usingDarkTheme();
        QIcon::setThemeName(darkTheme ? QStringLiteral("ovito-dark") : QStringLiteral("ovito-light"));

        if(automaticallyEnableDarkMode()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            // Install handler to get notified whenever the system color scheme is changed by the user.
            connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, [](Qt::ColorScheme scheme) {
                // Adjust OVITO's icon theme to match the UI color scheme.
                QIcon::setThemeName(scheme == Qt::ColorScheme::Dark ? QStringLiteral("ovito-dark") : QStringLiteral("ovito-light"));
            });
#endif

#if defined(Q_OS_LINUX) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
            if(darkTheme)
                activateThemeColors(true);

            if(qgetenv("XDG_CURRENT_DESKTOP") != "KDE") {
                // Install handler to get notified whenever the system color theme is changed by the user.
                // This involves permanently running the external process 'gsettings monitor org.gnome.desktop.interface color-scheme' and 'gtk-theme'.
                auto installThemeNotificationHandler = [&](const QString& settingsKey) {
                    QProcess* gsettingsProcess = new QProcess(this);
                    connect(gsettingsProcess, &QProcess::readyReadStandardOutput, this, [this]() {
                        QByteArray output = static_cast<QProcess*>(sender())->readAllStandardOutput().toLower();
                        if(output.contains("-dark")) {
                            _usingDarkTheme = true;
                            activateThemeColors(true);
                        }
                        else if(!output.isEmpty()) {
                            _usingDarkTheme = false;
                            activateThemeColors(false);
                        }
                    });

                    // Terminate gsettings process when the window closes.
                    connect(qApp, &QGuiApplication::lastWindowClosed, gsettingsProcess, [gsettingsProcess]() {
                        gsettingsProcess->terminate();
                        gsettingsProcess->waitForFinished(1000);
                    });

                    // Launch gsettings monitoring process.
                    gsettingsProcess->start("gsettings", QStringList() << "monitor" << "org.gnome.desktop.interface" << settingsKey);
                };
                if(qgetenv("XDG_CURRENT_DESKTOP") != "XFCE")
                    installThemeNotificationHandler(QStringLiteral("color-scheme"));
                else
                    installThemeNotificationHandler(QStringLiteral("gtk-theme"));
            }
#endif
        }

        // Set the application icon.
        QIcon mainWindowIcon;
        mainWindowIcon.addFile(":/guibase/mainwin/window_icon_256.png");
        mainWindowIcon.addFile(":/guibase/mainwin/window_icon_128.png");
        mainWindowIcon.addFile(":/guibase/mainwin/window_icon_48.png");
        mainWindowIcon.addFile(":/guibase/mainwin/window_icon_32.png");
        mainWindowIcon.addFile(":/guibase/mainwin/window_icon_16.png");
        QGuiApplication::setWindowIcon(mainWindowIcon);

        if(Application::runMode() == Application::AppMode) {
            // Create the main window widget and user interface object.
            OORef<MainWindowUI> mainWinUI = OORef<MainWindowUI>::create();

            // Show the main window.
            mainWinUI->mainWindow()->setUpdatesEnabled(false);
            mainWinUI->mainWindow()->restoreMainWindowGeometry();
            mainWinUI->mainWindow()->restoreLayout();
            mainWinUI->mainWindow()->setUpdatesEnabled(true);

            return MainThreadOperation(*mainWinUI, MainThreadOperation::Kind::Isolated);
        }
    }

    // Run application in non-gui mode.
    return MainThreadOperation(*this, MainThreadOperation::Kind::Isolated);
}

/******************************************************************************
* Is called at program startup once the event loop is running.
******************************************************************************/
void GuiApplication::postStartupInitialization()
{
    GuiApplication::initializeUserInterface(*this_task::ui(), cmdLineParser().positionalArguments());
    StandaloneApplication::postStartupInitialization();
}

/******************************************************************************
* Initializes a new abstract user interface object (e.g. a MainWindow in GUI mode or this application object in console mode).
******************************************************************************/
void GuiApplication::initializeUserInterface(UserInterface& ui, const QStringList& arguments)
{
    OVITO_ASSERT(this_task::isInteractive());

    DataSetContainer& datasetContainer = ui.datasetContainer();

    // Load session state file specified on the command line.
    if(!arguments.empty()) {
        QString startupFilename = arguments.front();
        if(startupFilename.endsWith(".ovito", Qt::CaseInsensitive)) {
            try {
                OORef<DataSet> dataset = DataSet::createFromFile(startupFilename);
                if(ui.checkLoadedDataset(dataset))
                    datasetContainer.setCurrentSet(std::move(dataset));
            }
            catch(const Exception& ex) {
                ui.reportError(ex);
            }
        }
    }

    // If no .ovito state file was specified on the command line, load
    // the user's default state from the standard location.
    if(datasetContainer.currentSet() == nullptr && Application::runMode() == Application::AppMode) {
        QString defaultsFilePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("defaults.ovito"));
        if(!defaultsFilePath.isEmpty()) {
            try {
                OORef<DataSet> dataset = DataSet::createFromFile(defaultsFilePath);
                if(ui.checkLoadedDataset(dataset)) {
                    dataset->setFilePath({});
                    datasetContainer.setCurrentSet(std::move(dataset));
                }
            }
            catch(Exception& ex) {
                ui.reportError(ex.prependGeneralMessage(tr("An error occurred while loading the user's default session state from the file: %1").arg(defaultsFilePath)));
            }
        }
    }

    // Create an empty dataset if nothing has been loaded.
    if(datasetContainer.currentSet() == nullptr) {
        datasetContainer.setCurrentSet(OORef<DataSet>::create());
    }

    // Import data file(s) specified on the command line.
    if(!arguments.empty()) {
        std::vector<QUrl> importUrls;
        int numSessionFiles = 0;
        for(const QString& importFilename : arguments) {
            if(importFilename.endsWith(".ovito", Qt::CaseInsensitive))
                numSessionFiles++;
            else
                importUrls.push_back(Application::instance()->fileManager().urlFromUserInput(importFilename));
        }
        ui.handleExceptions([&](){
            if(!importUrls.empty()) {
                if(numSessionFiles)
                    throw Exception(tr("Detected incompatible arguments: Cannot open a session state file and a simulation data file at the same time."));
                if(MainWindowUI* mainWindowUI = dynamic_object_cast<MainWindowUI>(&ui))
                    mainWindowUI->importFiles(std::move(importUrls));
                else
                    throw Exception(tr("Cannot import data files from the command line when running in console mode."));
            }
            if(numSessionFiles > 1)
                throw Exception(tr("Detected incompatible arguments: Cannot open multiple session state files at the same time."));
        });

        // Make sure we start with a clean undo stack at application startup.
        if(ui.undoStack())
            ui.undoStack()->clear();
    }
}

/******************************************************************************
* Handles events sent to the Qt application object.
******************************************************************************/
bool GuiApplication::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::FileOpen) {
        QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);

        // Only use an existing main window to load the imported file if it has no existing scene content. Otherwise, open a new window.
        MainWindow* mainWindow = nullptr;
        MainWindow::visitMainWindows([&](MainWindow* mw) {
            if(!mw->datasetContainer().activeScene() || mw->datasetContainer().activeScene()->children().empty()) {
                mainWindow = mw;
            }
        });

        if(mainWindow) {
            MainWindowUI& ui = mainWindow->ui();
            ui.handleExceptions([&] {
                if(openEvent->file().endsWith(".ovito", Qt::CaseInsensitive)) {
                    ui.askForSaveChanges();
                    OORef<DataSet> dataset = DataSet::createFromFile(openEvent->file());
                    if(ui.checkLoadedDataset(dataset))
                        ui.datasetContainer().setCurrentSet(std::move(dataset));
                }
                else {
                    ui.importFiles({openEvent->url()});
                }
            });
        }
        else {
            handleExceptions([&] {
                MainWindow::openNewWindow({openEvent->file()});
            });
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

    // In app mode, display a message box (application modal).
    if(Application::runMode() == Application::AppMode) {
        // Require a Qt event loop to show message box.
        createQtApplication(true);

        MessageDialog msgbox;
        msgbox.setWindowTitle(tr("Error - %1").arg(applicationName()));
        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setText(ex.message());
        msgbox.setIcon(QMessageBox::Critical);
        msgbox.setTextInteractionFlags(Qt::TextBrowserInteraction);

        // If the exception is associated with additional message strings,
        // show them in the Details section of the message box dialog.
        QString detailText;
        if(ex.messages().size() > 1) {
            for(int i = 1; i < ex.messages().size(); i++)
                detailText += ex.messages()[i] + QStringLiteral("\n");
        }
        // Also show traceback information.
        if(!ex.traceback().isEmpty()) {
            if(!detailText.isEmpty())
                detailText += QChar('\n');
            detailText += ex.traceback();
        }
        msgbox.setDetailedText(std::move(detailText));

        // Show message box.
        msgbox.exec();
    }
}

/******************************************************************************
* Returns whether app's UI should automatically follow the system color scheme.
******************************************************************************/
bool GuiApplication::automaticallyEnableDarkMode()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    return true;
#else
    #if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return QSettings().value("ui/automatic_dark_mode", false).toBool();
    #else
        return false;
    #endif
#endif
}

/******************************************************************************
* Returns whether the application currently uses a dark UI theme.
******************************************************************************/
bool GuiApplication::usingDarkTheme() const
{
    if(!automaticallyEnableDarkMode())
        return false;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    #ifndef Q_OS_LINUX
        return detectDarkTheme();
    #else
        if(!_usingDarkTheme.has_value())
            _usingDarkTheme = detectDarkTheme();
        return *_usingDarkTheme;
    #endif
#endif
}

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
/******************************************************************************
* Queries the system to determine whether the desktop currently uses a dark desktop theme.
******************************************************************************/
bool GuiApplication::detectDarkTheme() const
{
#ifndef Q_OS_LINUX
    // It's likely a dark theme if the window background is darker than the foreground text color.
    const QPalette& palette = qApp->palette();
    auto bg = palette.color(QPalette::Active, QPalette::Window);
    auto txt = palette.color(QPalette::Active, QPalette::Text);
    return bg.lightness() < txt.lightness();
#else // Q_OS_LINUX
    if(qgetenv("XDG_CURRENT_DESKTOP") == "KDE") {
        // On KDE, query system theme preference via D-Bus protocol.
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                 QStringLiteral("/org/freedesktop/portal/desktop"),
                                                 QStringLiteral("org.freedesktop.portal.Settings"),
                                                 QStringLiteral("Read"));
        message << QStringLiteral("org.freedesktop.appearance") << QStringLiteral("color-scheme");

        QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(message);
        if(reply.isValid()) {
            const QDBusVariant dbusVariant = qvariant_cast<QDBusVariant>(reply.value());
            return dbusVariant.variant().toUInt() == 1;  // 0=none, 1=prefer dark, 2=prefer light
        }
    }

    QProcess process;
    QByteArray gsettingsOutput;
    if(qgetenv("XDG_CURRENT_DESKTOP") != "XFCE") {
        process.start("gsettings", QStringList() << "get" << "org.gnome.desktop.interface" << "color-scheme", QIODevice::ReadOnly);
        process.waitForFinished();
        gsettingsOutput = process.readAllStandardOutput();
    }
    if(gsettingsOutput.isEmpty()) {
        process.start("gsettings", QStringList() << "get" << "org.gnome.desktop.interface" << "gtk-theme", QIODevice::ReadOnly);
        process.waitForFinished();
        gsettingsOutput = process.readAllStandardOutput();
    }
    if(gsettingsOutput.toLower().contains("-dark"))
        return true;
    return false;
#endif
}
#endif

#ifdef OVITO_SSH_CLIENT

/******************************************************************************
* Asks the user for the login password for a SSH server.
******************************************************************************/
bool GuiApplication::askUserForPassword(const QString& hostname, const QString& username, QString& password)
{
    bool ok;
    password = QInputDialog::getText(nullptr, tr("SSH Password Authentication"),
        tr("<p>OVITO is connecting to remote host <b>%1</b> via SSH.</p></p>Please enter the password for user <b>%2</b>:</p>").arg(hostname.toHtmlEscaped()).arg(username.toHtmlEscaped()),
        QLineEdit::Password, password, &ok);
    return ok;
}

/******************************************************************************
* Asks the user for the passphrase for a private SSH key.
******************************************************************************/
bool GuiApplication::askUserForKeyPassphrase(const QString& hostname, const QString& prompt, QString& passphrase)
{
    bool ok;
    passphrase = QInputDialog::getText(nullptr, tr("SSH Remote Connection"),
        tr("<p>OVITO is connecting to remote host <b>%1</b> via SSH.</p><p>%2</p>").arg(hostname.toHtmlEscaped()).arg(prompt.toHtmlEscaped()),
        QLineEdit::Password, passphrase, &ok);
    return ok;
}

/******************************************************************************
* Asks the user for the answer to a keyboard-interactive question sent by the SSH server.
******************************************************************************/
bool GuiApplication::askUserForKbiResponse(const QString& hostname, const QString& username, const QString& instruction, const QString& question, bool showAnswer, QString& answer)
{
    bool ok;
    answer = QInputDialog::getText(nullptr, tr("SSH Keyboard-Interactive Authentication"),
        tr("<p>OVITO is connecting to remote host <b>%1</b> via SSH.</p></p>Please enter your response to the following question sent by the SSH server:</p><p>%2 <b>%3</b></p>").arg(hostname.toHtmlEscaped()).arg(instruction.toHtmlEscaped()).arg(question.toHtmlEscaped()),
        showAnswer ? QLineEdit::Normal : QLineEdit::Password, QString(), &ok);
    return ok;
}

/******************************************************************************
* Asks the user for PKCS#11 smartcard credentials.
******************************************************************************/
bool GuiApplication::askUserForPKCS11Credentials(const QString& hostname, const QString& username, QString& pkcs11Uri, QString& pin)
{
    // If URI is already set (e.g., from SSH config), only ask for PIN
    bool uriAlreadySet = !pkcs11Uri.isEmpty();

    // Create a custom dialog for collecting URI and/or PIN
    QDialog dialog(nullptr);
    dialog.setWindowTitle(tr("SSH PKCS#11 Smartcard Authentication"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Info label
    QString infoText;
    if(uriAlreadySet) {
        infoText = tr("<p>OVITO is connecting to remote host <b>%1</b> via SSH using PKCS#11 smartcard authentication.</p>"
                     "<p>Please enter the PIN for user <b>%2</b>:</p>")
                   .arg(hostname.toHtmlEscaped()).arg(username.toHtmlEscaped());
    } else {
        infoText = tr("<p>OVITO is connecting to remote host <b>%1</b> via SSH.</p>"
                     "<p>Please provide your PKCS#11 smartcard credentials for user <b>%2</b>:</p>")
                   .arg(hostname.toHtmlEscaped()).arg(username.toHtmlEscaped());
    }
    QLabel* infoLabel = new QLabel(infoText);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QLineEdit* uriEdit = nullptr;
    if(uriAlreadySet) {
        // Show the configured URI as read-only info
        QLabel* uriInfoLabel = new QLabel(tr("<b>PKCS#11 URI:</b> %1").arg(pkcs11Uri.toHtmlEscaped()));
        uriInfoLabel->setWordWrap(true);
        uriInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(uriInfoLabel);
        layout->addSpacing(10);
    } else {
        // URI input (only if not already set)
        QLabel* uriLabel = new QLabel(tr("PKCS#11 URI:"));
        layout->addWidget(uriLabel);

        uriEdit = new QLineEdit(&dialog);
        uriEdit->setPlaceholderText(tr("pkcs11:token=MyToken;object=MyKey;type=private"));
        uriEdit->setText(pkcs11Uri);
        layout->addWidget(uriEdit);

        QLabel* uriHintLabel = new QLabel(tr("<small>Example: pkcs11:token=MySmartCard;object=MyPrivateKey;type=private</small>"));
        uriHintLabel->setWordWrap(true);
        layout->addWidget(uriHintLabel);

        layout->addSpacing(10);
    }

    // PIN input (always shown)
    QLabel* pinLabel = new QLabel(uriAlreadySet ? tr("PIN:") : tr("PIN (optional if included in URI):"));
    layout->addWidget(pinLabel);

    QLineEdit* pinEdit = new QLineEdit(&dialog);
    pinEdit->setEchoMode(QLineEdit::Password);
    pinEdit->setPlaceholderText(tr("Enter PIN"));
    pinEdit->setText(pin);
    layout->addWidget(pinEdit);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    // Enable/disable OK button based on URI validity (only if URI needs to be entered)
    QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok);
    if(!uriAlreadySet && uriEdit) {
        // Update OK button state when URI changes
        auto updateOkButton = [okButton, uriEdit]() {
            okButton->setEnabled(!uriEdit->text().trimmed().isEmpty());
        };
        connect(uriEdit, &QLineEdit::textChanged, updateOkButton);
        updateOkButton(); // Set initial state
    }

    if(dialog.exec() == QDialog::Accepted) {
        if(!uriAlreadySet && uriEdit) {
            pkcs11Uri = uriEdit->text().trimmed();
        }
        // If URI was already set, keep it unchanged
        pin = pinEdit->text();
        return !pkcs11Uri.isEmpty();
    }

    return false;
}

/******************************************************************************
* Informs the user about an unknown SSH host.
******************************************************************************/
bool GuiApplication::detectedUnknownSshServer(const QString& hostname, const QString& unknownHostMessage, const QString& hostPublicKeyHash)
{
    return MessageDialog::question(nullptr, tr("SSH Unknown Remote Host"),
        tr("<p>OVITO is connecting to unknown remote host <b>%1</b> via SSH.</p><p>%2</p><p>Host key fingerprint is %3</p><p>Are you sure you want to continue connecting?</p>")
        .arg(hostname.toHtmlEscaped()).arg(unknownHostMessage.toHtmlEscaped()).arg(hostPublicKeyHash),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

#endif

}   // End of namespace
