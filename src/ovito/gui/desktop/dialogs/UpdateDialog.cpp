
#include "UpdateDialog.h"
#include <ovito/core/app/Application.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>

#include <QtNetwork>

#if 0
#include <QWebEngineView>
#endif

namespace Ovito {
#if 0
class ExternalLinksWebPage : public QWebEnginePage
{
public:
    ExternalLinksWebPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
};

bool ExternalLinksWebPage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame)
{
    if(type == NavigationTypeLinkClicked) {
        QDesktopServices::openUrl(url);
        return false;
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

class UpdateBrowserDialog : public QDialog
{
public:
    UpdateBrowserDialog(QWidget* parent = nullptr);

    void setDontRemindVersion(int version) { _dontRemindVersion = version; }

protected:
    void onAccept();

    void onReject();

    static void onDownload();

private:
    int _dontRemindVersion = 0;
};

UpdateBrowserDialog::UpdateBrowserDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("A new %1 version is available").arg(Application::applicationName()));
    // Width 768 prevents the navigation sidebar in the manual from showing up
    resize(768, 576);
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Webview
    QWebEngineView* view = new QWebEngineView(this);
    // Use custom QWebEnginePage that opens links in a new window instead of navigating
    ExternalLinksWebPage* page = new ExternalLinksWebPage(view);
    view->setPage(page);
    // Load our changelog
    view->load(QUrl("https://docs.ovito.org/new_features.html#new-features"));

    layout->addWidget(view);

    // Buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);

    // Ok button -> closes the change log
    QPushButton* okButton = buttonBox->addButton(QDialogButtonBox::Ok);
    // Cancel / dont remind again button -> (also closes the dialog)
    QPushButton* cancelButton = buttonBox->addButton(QDialogButtonBox::Cancel);
    cancelButton->setText(tr("Remind me later"));
    // Download button -> opens an external browser window
    QPushButton* downloadButton = buttonBox->addButton(tr("Download"), QDialogButtonBox::ActionRole);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &UpdateBrowserDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &UpdateBrowserDialog::onReject);
    connect(downloadButton, &QPushButton::clicked, this, &UpdateBrowserDialog::onDownload);

    layout->addWidget(buttonBox);
}

void UpdateBrowserDialog::onAccept()
{
    QSettings settings;
    // Reset dont remind setting to be reminded again next time
    settings.setValue("news/dontRemind", 0);
    accept();
}

void UpdateBrowserDialog::onReject()
{
    QSettings settings;
    // Update dont remind to the last "rejected" version
    settings.setValue("news/dontRemind", _dontRemindVersion);
    reject();
}

void UpdateBrowserDialog::onDownload() { QDesktopServices::openUrl(QUrl("https://www.ovito.org/#download")); }
#else
class UpdateDialog : public MessageDialog
{
public:
    UpdateDialog(int major, int minor, int patch, int dontRemind = 0, QWidget* parent = nullptr);

private:
    // Called when any button is pressed by the user
    void onButtonClicked(QAbstractButton* button);

    // Called when user presses "OK"
    static void onAccept();
    // Called when user presses "Skip thisa version"
    void onDontRemind() const;
    // Called when user presses "Download"
    static void onDownload();

private:
    // New version number displayed to the user
    int _major;
    int _minor;
    int _patch;
    // Version that no remind will be set to if pressed
    int _dontRemindVersion;
};

UpdateDialog::UpdateDialog(int major, int minor, int patch, int dontRemind, QWidget* parent)
    : MessageDialog(parent), _major(major), _minor(minor), _patch(patch), _dontRemindVersion(dontRemind)
{
    // Cofigure the message box
    setIcon(QMessageBox::Information);
    setDefaultButton(QMessageBox::Ok);
    setText(QObject::tr("%1 version %2.%3.%4 is available for download.")
                .arg(Application::applicationName(), QString::number(_major), QString::number(_minor), QString::number(_patch)));
    setInformativeText(
        QObject::tr("Click 'Download' open the download page in your browser, "
                    "'OK' to close this dialog for now, "
                    "or 'Skip this version' to not be reminded again until the next significant update."));
    // Configure the buttons
    setStandardButtons(QMessageBox::Ok | QMessageBox::Help | QMessageBox::Cancel);
    button(QMessageBox::Cancel)->setText(QObject::tr("Skip this version"));
    button(QMessageBox::Help)->setText(QObject::tr("Download"));
    // Connect the button press
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
    // if no button or no messageBox close the dialog fallback
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
    // Reset dont remind setting to be reminded again next time
    settings.setValue("news/dontRemind", 0);
}

/******************************************************************************
 * Called when user presses "Skip this version"
 ******************************************************************************/
void UpdateDialog::onDontRemind() const
{
    QSettings settings;
    // Update dont remind to the last "rejected" version
    settings.setValue("news/dontRemind", _dontRemindVersion);
}

/******************************************************************************
 * Called when user presses "Download"
 ******************************************************************************/
void UpdateDialog::onDownload() { QDesktopServices::openUrl(QUrl("https://www.ovito.org/#download")); }

#endif

IMPLEMENT_CREATABLE_OVITO_CLASS(GuiUpdateInfoService);

/******************************************************************************
 * Is called by the system during standalone application startup after the main window has been created.
 * Downloads the new s segment from the web and sets the side panel and update dialog
 ******************************************************************************/
bool GuiUpdateInfoService::applicationStarting()
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
        const QString urlString = QString("https://www.ovito.org/appnews/v%1.%2.%3/%4?ovito=000000000000000000&OS=%5%6")
                                      .arg(Application::applicationVersionMajor())
                                      .arg(Application::applicationVersionMinor())
                                      .arg(Application::applicationVersionRevision())
                                      .arg(programEdition)
                                      .arg(operatingSystemString)
                                      .arg(QT_POINTER_SIZE * 8);
        QNetworkAccessManager* networkAccessManager = Application::instance()->networkAccessManager();
        QNetworkReply* networkReply = networkAccessManager->get(QNetworkRequest(QUrl(urlString)));

        // Progress once the request finishes
        connect(networkReply, &QNetworkReply::finished, this, &GuiUpdateInfoService::onWebRequestFinished);
    }
#endif
    return true;
}

/******************************************************************************
 * Is called when a new main window is created.
 * Store the main window
 ******************************************************************************/
void GuiUpdateInfoService::registerActions(ActionManager& actionManager, MainWindow& mainWindow) { _mainWindow = &mainWindow; }

/******************************************************************************
 * Extracts the two version strings from the first line of the news webpage
 * Pattern of the version strings: <!--vX+.Y+.Z+|vA+.B+.C+-->
 * where X+.Y+.Z+ are the significant version even shown when "Skip this version" is pressed
 * and A+.B+.C+ are all programm versions shown to every user
 ******************************************************************************/
QStringList GuiUpdateInfoService::extractVersion(const QString& input)
{
    const static QRegularExpression regex(R"(<!--v(\d+)\.(\d+)\.(\d+)\|v(\d+)\.(\d+)\.(\d+)-->)");
    const QRegularExpressionMatch match = regex.match(input);
    return match.capturedTexts();
}

// Anonymous namespace instead of static for free function
namespace {
// Convert str to int with error checking (in debug mode)
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
 * Creates the update popup window
 * Checks the newly available version and compares it against the current version
 * and validates against the "Skip this version" choice by the user
 ******************************************************************************/
void GuiUpdateInfoService::createUpdateDialog(const QStringList& versionMatch) const
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
    const int dontRemindMajor = str_to_int(versionMatch[1]);
    const int dontRemindMinor = str_to_int(versionMatch[2]);
    const int dontRemindPatch = str_to_int(versionMatch[3]);
    const int dontRemindVersionUpdate = QT_VERSION_CHECK(dontRemindMajor, dontRemindMinor, dontRemindPatch);

    // Read always update version
    const int remindMajor = str_to_int(versionMatch[4]);
    const int remindMinor = str_to_int(versionMatch[5]);
    const int remindPatch = str_to_int(versionMatch[6]);

    // Compare versions
    // Remind if "normal" remind version > current version
    bool show = QT_VERSION_CHECK(remindMajor, remindMinor, remindPatch) > QT_VERSION_CHECK(Application::applicationVersionMajor(),
                                                                                           Application::applicationVersionMinor(),
                                                                                           Application::applicationVersionRevision());
    // AND dont remind version > stored dont remind version
    // stored version will be 0 when it has never been set or the user is in "please remind" mode
    show &= dontRemindVersionUpdate > dontRemindVersion;

    if(!show) {
        return;
    }
#if 0
    UpdateBrowserDialog* updateBrowser = new UpdateBrowserDialog(_mainWindow);
    updateBrowser->setDontRemindVersion(dontRemindVersionUpdate);
    updateBrowser->show();
#else
    // Show update information dialog (non-blocking)
    UpdateDialog* updateDialog = new UpdateDialog(remindMajor, remindMinor, remindPatch, dontRemindVersionUpdate, _mainWindow);
    updateDialog->show();
#endif
}

/******************************************************************************
 * Called when the web request finishes
 * Show the update dialog and set the "ProgramNotice"
 ******************************************************************************/
void GuiUpdateInfoService::onWebRequestFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
    QByteArray page;
    if(reply->error() == QNetworkReply::NoError) {
        page = reply->readAll();
        reply->close();
    }

    if(_mainWindow && page.startsWith("<html><!--OVITO-->")) {
        const QString pageString = QString::fromUtf8(page.constData());

        // Update the command panel with the downloaded page
        _mainWindow->commandPanel()->modifyPage()->showProgramNotice(pageString);

        // Get the version from the first line of the pageString
        createUpdateDialog(extractVersion(pageString.left(pageString.indexOf("\n"))));

        // Update the cache in the settings
        QSettings settings;
        settings.setValue("news/cached_webpage", page);
    }

    // Cleanup
    reply->deleteLater();
}

};  // namespace Ovito