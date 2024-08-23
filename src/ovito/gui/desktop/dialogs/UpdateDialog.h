

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
    /// \brief Is called by the system during standalone application startup after the main window has been created.
    bool applicationStarting() override;

    /// \brief Is called when a new main window is created.
    void registerActions(ActionManager& actionManager, MainWindow& mainWindow) override;

protected:
    void onWebRequestFinished();

private:
    static QStringList extractVersion(const QString& input);

    void createUpdateDialog(const QStringList& versionMatch) const;

private:
    MainWindow* _mainWindow = nullptr;
};

}  // namespace Ovito