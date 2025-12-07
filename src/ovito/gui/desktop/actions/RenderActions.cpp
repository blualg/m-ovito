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
#include <ovito/gui/desktop/actions/WidgetActionManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/rendering/FrameBufferWindow.h>
#include <ovito/gui/desktop/dialogs/ConfigureViewportGraphicsDialog.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>

namespace Ovito {

/******************************************************************************
* Handles the ACTION_RENDER_ACTIVE_VIEWPORT command.
******************************************************************************/
void WidgetActionManager::on_RenderActiveViewport_triggered()
{
    if(!dataset())
        return;

    handleExceptions([&] {

        // Set focus to main window to process any pending user inputs in QLineEdit widgets.
        mainWindow()->setFocus();

        // Stop animation playback in the interactive viewports before rendering an image.
        datasetContainer().stopAnimationPlayback();

        // Get the current render settings.
        RenderSettings* renderSettings = dataset()->renderSettings();
        if(!renderSettings)
            throw Exception(tr("Cannot render without an active RenderSettings object."));

        // Validate save-to-file option if an animation is being rendered.
        if((renderSettings->renderingRangeType() == RenderSettings::ANIMATION_INTERVAL ||
            renderSettings->renderingRangeType() == RenderSettings::CUSTOM_INTERVAL) &&
           (!renderSettings->saveToFile())) {
            // Focus the relevant panel
            mainWindow()->setCurrentCommandPanelPage(MainWindow::CommandPanelPage::RENDER_PAGE);

            // Show a message box.
            MessageDialog msgBox(QMessageBox::Warning, tr("Output will not be saved"),
                                 tr("You are about to render an animation without saving it to disk."), QMessageBox::Yes | QMessageBox::No,
                                 mainWindow());
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setEscapeButton(QMessageBox::No);
            msgBox.setInformativeText(
                tr("You did not check the \"Save to file\" option while rendering an animation. "
                   "This means the rendered frames will be lost.\n\n"
                   "Do you want to continue rendering anyway?"));
            int result = msgBox.exec();
            if(result != QMessageBox::Yes) {
                // Early exit
                this_task::cancelAndThrow();
            }
        }

        // Get the current viewport configuration.
        OORef<const ViewportConfiguration> viewportConfig = dataset()->viewportConfig();
        if(!viewportConfig) throw Exception(tr("Cannot render without an active ViewportConfiguration object."));

        // Allocate new frame buffer (or resize existing one) and display it in a window.
        std::shared_ptr<FrameBuffer> frameBuffer =
            ui().createAndShowFrameBuffer(renderSettings->outputImageWidth(), renderSettings->outputImageHeight());

        // Call high-level rendering function, which will take care of the rest.
        Future<void> future = renderSettings->render(*viewportConfig, frameBuffer);

        // Display a progress indicator in the UI while the rendering operation is in progress.
        ui().showRenderingProgress(frameBuffer, std::move(future));
    });
}

/******************************************************************************
* Handles the ACTION_CONFIGURE_VIEWPORT_GRAPHICS command.
******************************************************************************/
void WidgetActionManager::on_ConfigureViewportGraphics_triggered()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    if(ConfigureViewportGraphicsDialog* dialog = mainWindow()->findChild<ConfigureViewportGraphicsDialog*>(Qt::FindDirectChildrenOnly)) {
#else
    if(ConfigureViewportGraphicsDialog* dialog = mainWindow()->findChild<ConfigureViewportGraphicsDialog*>(QString(), Qt::FindDirectChildrenOnly)) {
#endif
        dialog->raise();
        dialog->activateWindow();
    }
    else {
        dialog = new ConfigureViewportGraphicsDialog(ui(), mainWindow());
        dialog->show();
    }
}

}   // End of namespace
