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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/Application.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include "UpdateNotificationDialog.h"

#include <QtNetwork>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(UpdateNotificationService);

class UpdateDialog : public MessageDialog
{
public:
    UpdateDialog(int major, int minor, int patch, int dontRemind = 0, QWidget* parent = nullptr);

private:
    // Called when any button is pressed by the user
    void onButtonClicked(QAbstractButton* button);

    // Called when user presses "OK"
    static void onAccept();
    // Called when user presses "Skip this version"
    void onDontRemind() const;
    // Called when user presses "Download"
    static void onDownload();

private:
    // New version number displayed to the user
    int _major;
    int _minor;
    int _patch;
    // Version that no-remind will be set to if selected
    int _dontRemindVersion;
};

UpdateDialog::UpdateDialog(int major, int minor, int patch, int dontRemind, QWidget* parent)
    : MessageDialog(parent), _major(major), _minor(minor), _patch(patch), _dontRemindVersion(dontRemind)
{
    // Configure the message box
    setIcon(QMessageBox::Information);
    setDefaultButton(QMessageBox::Ok);
    setWindowTitle(UpdateNotificationService::tr("New version available"));
    setText(UpdateNotificationService::tr("%1 %2.%3.%4 is available for download")
                .arg(Application::applicationName()).arg(_major).arg(_minor).arg(_patch));
    setInformativeText(
        UpdateNotificationService::tr("<p><a href=\"https://docs.ovito.org/new_features.html\">New features and changes</a></p>"
                    "<p>Click 'Download' to open the download page in your browser, "
                    "'OK' to dismiss this message for now, "
                    "or 'Skip this version' to not be reminded again until the next major program update.</p>"));
    // Configure the buttons
    setStandardButtons(QMessageBox::Ok | QMessageBox::Help | QMessageBox::Cancel);
    button(QMessageBox::Cancel)->setText(UpdateNotificationService::tr("Skip this version"));
    button(QMessageBox::Help)->setText(UpdateNotificationService::tr("Download"));
    connect(this, &QMessageBox::buttonClicked, this, &UpdateDialog::onButtonClicked);
}

/******************************************************************************
 * Handle button clicks
 ******************************************************************************/
void UpdateDialog::onButtonClicked(QAbstractButton* button)
{
    OVITO_ASSERT(button);
    QMessageBox* messageBox = qobject_cast<QMessageBox*>(sender());
    OVITO_ASSERT(messageBox);
    // if no button or no messageBox close the dialog as fallback
    if(!messageBox || !button) {
        close();
    }

    QMessageBox::ButtonRole role = messageBox->buttonRole(button);
    if(role == QMessageBox::AcceptRole) {
        onAccept();
    }
    else if(role == QMessageBox::HelpRole) {
        onDownload();
    }
    else if(role == QMessageBox::RejectRole) {
        onDontRemind();
    }
    else {
        // unhandled button, should not happen
        OVITO_ASSERT(false);
        // fallback
        close();
    }
}

/******************************************************************************
 * Called when user presses "OK"
 ******************************************************************************/
void UpdateDialog::onAccept()
{
    QSettings settings;
    // Reset don't remind setting to be reminded again next time
    settings.remove("news/dontRemind");
}

/******************************************************************************
 * Called when user presses "Skip this version"
 ******************************************************************************/
void UpdateDialog::onDontRemind() const
{
    QSettings settings;
    // Update don't remind to the last "rejected" version
    settings.setValue("news/dontRemind", _dontRemindVersion);
}

/******************************************************************************
 * Called when user presses "Download"
 ******************************************************************************/
void UpdateDialog::onDownload()
{
    QDesktopServices::openUrl(QUrl("https://www.ovito.org/#download"));
}

/******************************************************************************
 * Is called by the system during standalone application startup after the main window has been created.
 * Downloads the new s segment from the web and sets the side panel and update dialog
 ******************************************************************************/
bool UpdateNotificationService::applicationStarting()
{
    // Do nothing when running in console mode.
    if(!Application::guiMode()) {
        return true;
    }

    // Get operating system
#if !defined(OVITO_BUILD_APPSTORE_VERSION)
    const QSettings settings;
    if(settings.value("updates/check_for_updates", true).toBool()) {
        QString operatingSystemString;
#if defined(Q_OS_MACOS)
        operatingSystemString = QStringLiteral("macosx");
#elif defined(Q_OS_WIN) || defined(Q_OS_CYGWIN)
        operatingSystemString = QStringLiteral("win");
#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
        operatingSystemString = QStringLiteral("linux");
#else
        operatingSystemString = QStringLiteral("other");
#endif

        // Get program edition
        QString programEdition;
#if defined(OVITO_BUILD_BASIC)
        programEdition = QStringLiteral("basic/");
#elif defined(OVITO_BUILD_PRO)
        programEdition = QStringLiteral("pro/");
#endif

        // Fetch newest web page from web server.
        const QString urlString = QStringLiteral("https://www.ovito.org/appnews/v%1.%2.%3/%4?ovito=000000000000000000&OS=%5%6")
                                      .arg(Application::applicationVersionMajor())
                                      .arg(Application::applicationVersionMinor())
                                      .arg(Application::applicationVersionRevision())
                                      .arg(programEdition)
                                      .arg(operatingSystemString)
                                      .arg(QT_POINTER_SIZE * 8);
        QNetworkAccessManager* networkAccessManager = Application::instance()->networkAccessManager();
        QNetworkReply* networkReply = networkAccessManager->get(QNetworkRequest(QUrl(urlString)));

        // Progress once the request finishes
        connect(networkReply, &QNetworkReply::finished, this, &UpdateNotificationService::onWebRequestFinished);
    }
#endif
    return true;
}

/******************************************************************************
 * Is called when a new main window is created.
 * Store the main window
 ******************************************************************************/
void UpdateNotificationService::registerActions(ActionManager& actionManager, MainWindow& mainWindow)
{
    _mainWindow = &mainWindow;
}

/******************************************************************************
 * Extracts two version strings from the first line of the news webpage
 * Version string pattern: <!--vX.Y.Z|vA.B.C-->
 * See createUpdateDialog for more details
 ******************************************************************************/
QStringList UpdateNotificationService::extractVersion(const QString& input)
{
    const static QRegularExpression regex(R"(<!--v(\d+)\.(\d+)\.(\d+)\|v(\d+)\.(\d+)\.(\d+)-->)");
    const QRegularExpressionMatch match = regex.match(input);
    return match.capturedTexts();
}

// Anonymous namespace instead of static for free function
namespace {

// Convert QString to int (with error checking in debug mode)
int str_to_int(const QString& str)
{
#ifdef OVITO_DEBUG
    bool flag;
    const int ret = str.toInt(&flag);
    OVITO_ASSERT(flag);
    return ret;
#else
    return str.toInt();
#endif
}

}  // namespace

/******************************************************************************
 * Creates the update popup window based on the version string read from the server:
 *
 * Version string pattern: <!--vX.Y.Z|vA.B.C-->
 *
 * X.Y.Z represents the last major update:
 *  - Will be stored in the application settings
 *  - Even users who clicked "Skip this version" before will get a notification when this number increases
 *
 * A.B.C represents the current minor update:
 *  - Users won't see reminders if they previously selected "Skip this version"
 *  - All other users will get a reminder when this number is greater than their version
 *
 * In any case, A.B.C should indicate the current version available for download (whether major or minor).
 * X.Y.Z should be bumped up to A.B.C during a major program release. Otherwise, X.Y.Z < A.B.C .
 ******************************************************************************/
void UpdateNotificationService::createUpdateDialog(const QStringList& versionMatch) const
{
    // something went wrong -> don't do anything anything
    if(!_mainWindow || versionMatch.size() != 2 * 3 + 1) {
        OVITO_ASSERT(false);
        return;
    }

    // If first match is ! we force the update window
    const QSettings settings;

    // Read don't remind from settings
    const int dontRemindVersion = settings.value("news/dontRemind", 0).toInt();

    // Read don't remind version
    const int dontRemindMajor = str_to_int(versionMatch[1]);  // X (from version string)
    const int dontRemindMinor = str_to_int(versionMatch[2]);  // Y (from version string)
    const int dontRemindPatch = str_to_int(versionMatch[3]);  // Z (from version string)
    const int dontRemindVersionUpdate = QT_VERSION_CHECK(dontRemindMajor, dontRemindMinor, dontRemindPatch);

    // Read always update version
    const int remindMajor = str_to_int(versionMatch[4]);  // A (from version string)
    const int remindMinor = str_to_int(versionMatch[5]);  // B (from version string)
    const int remindPatch = str_to_int(versionMatch[6]);  // C (from version string)

    // Compare versions. Remind if "normal" remind version > current version.
    bool show = QT_VERSION_CHECK(remindMajor, remindMinor, remindPatch) > QT_VERSION_CHECK(Application::applicationVersionMajor(),
                                                                                           Application::applicationVersionMinor(),
                                                                                           Application::applicationVersionRevision());
    // AND dont remind version > stored dont remind version
    // stored version will be 0 when it has never been set or the user is in "please remind" mode.
    show &= dontRemindVersionUpdate > dontRemindVersion;

    if(!show)
        return;

    // Show update information dialog (non-blocking).
    UpdateDialog* updateDialog = new UpdateDialog(remindMajor, remindMinor, remindPatch, dontRemindVersionUpdate, _mainWindow);
    updateDialog->setAttribute(Qt::WA_DeleteOnClose);
    updateDialog->show();
}

/******************************************************************************
 * Called when the web request finishes
 * Show the update dialog and set the "ProgramNotice"
 ******************************************************************************/
void UpdateNotificationService::onWebRequestFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    QByteArray page;
    if(reply->error() == QNetworkReply::NoError) {
        page = reply->readAll();
        reply->close();
    }

    if(_mainWindow && page.startsWith("<html><!--OVITO-->")) {
        const QString pageString = QString::fromUtf8(page.constData());

        // Display the downloaded page in the command panel on program startup.
        _mainWindow->commandPanel()->modifyPage()->showProgramNotice(pageString);

        // Extract the available update version from the first line of the HTML source code.
        createUpdateDialog(extractVersion(pageString.left(pageString.indexOf("\n"))));

        // Update the cache in the settings
        QSettings().setValue("news/cached_webpage", page);
    }

    // Cleanup
    reply->deleteLater();
}

};  // namespace Ovito