

// ähnlich license check
// gui application service
//  -> web request
//  -> registerActions to connect main window and web request result processor
#pragma once

#include <ovito/gui/desktop/app/GuiApplicationService.h>
#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

class OVITO_GUI_EXPORT GuiUpdateInfoService : public QObject, public GuiApplicationService
{
    Q_OBJECT

    OVITO_CLASS(GuiUpdateInfoService)

public:
    /// Is called by the system during standalone application startup after the main window has been created.
    /// Downloads the new s segment from the web and sets the side panel and update dialog
    bool applicationStarting() override;

    /// Is called when a new main window is created.
    /// Used to store the main window
    void registerActions(ActionManager& actionManager, MainWindow& mainWindow) override;

private:
    /// Extracts the two version strings from the first line of the news webpage
    /// Pattern of the version strings: <!--vX+.Y+.Z+|vA+.B+.C+-->
    /// where X+.Y+.Z+ are the significant version even shown when "Skip this version" is pressed
    /// and A+.B+.C+ are all programm versions shown to every user
    static QStringList extractVersion(const QString& input);

    /// Called when the web request finishes
    /// Show the update dialog and set the "ProgramNotice"
    void onWebRequestFinished();

    /// Creates the update popup window
    ///  Checks the newly available version and compares it against the current version
    ///  and validates against the "Skip this version" choice by the user
    void createUpdateDialog(const QStringList& versionMatch) const;

private:
    /// Pointer to the current main window
    MainWindow* _mainWindow = nullptr;
};

}  // namespace Ovito