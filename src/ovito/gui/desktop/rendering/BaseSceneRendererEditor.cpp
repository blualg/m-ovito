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
#include <ovito/core/viewport/ViewportWindow.h>
#include "BaseSceneRendererEditor.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BaseSceneRendererEditor);

/******************************************************************************
* Constructor.
******************************************************************************/
BaseSceneRendererEditor::BaseSceneRendererEditor()
{
    connect(this, &PropertiesEditor::contentsReplaced, this, [this](RefTarget* editObject) {
        if(!editObject)
            return;
        handleExceptions([&]() {
            for(const auto& [id, label, windowClass, rendererClass] : ViewportWindow::listInteractiveWindowImplementations()) {
                if(ViewportWindow::getInteractiveWindowRenderer(id) == editObject) {
                    Q_EMIT editingInteractiveRenderer();
                    return;
                }
            }
            Q_EMIT editingFinalFrameRenderer();
        });
    });
}

/******************************************************************************
* Creates an action widget that lets the user copy the settings of the
* interactive renderer to/from the final-frame renderer.
******************************************************************************/
QWidget* BaseSceneRendererEditor::createCopySettingsBetweenRenderersWidget(QWidget* parent)
{
    QLabel* label = new QLabel(tr("<html><a href=\"action\">Copy settings...</a></html>"), parent);
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    label->setContextMenuPolicy(Qt::ActionsContextMenu);
    QAction* i2fAction = label->addAction(tr("Interactive view → Final image"));
    QAction* f2iAction = label->addAction(tr("Final image → Interactive view"));
    connect(label, &QLabel::linkActivated, label, [this, label, f2iAction]() {

        // Enable the action that copies settings from the final image to the interactive viewports
        // only if the selected final-image renderer is of the same type as the interactive viewport renderer.
        SceneRenderer* finalFrameRenderer = mainWindow().datasetContainer().currentSet()->renderSettings()->renderer();
        SceneRenderer* interactiveRenderer = dynamic_object_cast<SceneRenderer>(editObject());
        f2iAction->setEnabled(
            finalFrameRenderer &&
            interactiveRenderer &&
            canTransferSettingsBetweenRenderers(finalFrameRenderer, interactiveRenderer));

        // Show the context menu containing the actions.
        QMenu::exec(label->actions(), QCursor::pos(label->screen()), nullptr, label);
    });
    connect(i2fAction, &QAction::triggered, this, &BaseSceneRendererEditor::copySettingsInteractiveToFinalFrame);
    connect(f2iAction, &QAction::triggered, this, &BaseSceneRendererEditor::copySettingsFinalFrameToInteractive);
    // Show the widget only in the editor of the interactive renderer.
    connect(this, &BaseSceneRendererEditor::editingFinalFrameRenderer, label, &QWidget::hide);
    return label;
}

/******************************************************************************
* Copies the settings of the interactive renderer to the final-frame renderer.
******************************************************************************/
void BaseSceneRendererEditor::copySettingsInteractiveToFinalFrame()
{
    mainWindow().performTransaction(tr("Copy render settings"), [this]() {
        if(OORef<RenderSettings> renderSettings = mainWindow().datasetContainer().currentSet()->renderSettings()) {
            OORef<SceneRenderer> interactiveRenderer = dynamic_object_cast<SceneRenderer>(editObject());
            OORef<SceneRenderer> finalFrameRenderer = renderSettings->renderer();
            if(interactiveRenderer) {
                if(!finalFrameRenderer || !canTransferSettingsBetweenRenderers(interactiveRenderer, finalFrameRenderer)) {
                    // Create a new final-frame renderer of the same type as the interactive renderer.
                    finalFrameRenderer = static_object_cast<SceneRenderer>(interactiveRenderer->getOOClass().createInstance());
                    renderSettings->setRenderer(finalFrameRenderer);
                }
                if(finalFrameRenderer && canTransferSettingsBetweenRenderers(interactiveRenderer, finalFrameRenderer)) {
                    transferSettingsBetweenRenderers(interactiveRenderer, finalFrameRenderer, true);
                    mainWindow().setCurrentCommandPanelPage(MainWindow::CommandPanelPage::RENDER_PAGE);
                }
            }
        }
    });
}

/******************************************************************************
* Copies the settings of the final-frame renderer to the interactive renderer.
******************************************************************************/
void BaseSceneRendererEditor::copySettingsFinalFrameToInteractive()
{
    mainWindow().performTransaction(tr("Copy render settings"), [this]() {
        if(RenderSettings* renderSettings = mainWindow().datasetContainer().currentSet()->renderSettings()) {
            SceneRenderer* interactiveRenderer = dynamic_object_cast<SceneRenderer>(editObject());
            SceneRenderer* finalFrameRenderer = renderSettings->renderer();
            if(interactiveRenderer && finalFrameRenderer && canTransferSettingsBetweenRenderers(finalFrameRenderer, interactiveRenderer)) {
                transferSettingsBetweenRenderers(finalFrameRenderer, interactiveRenderer, false);
            }
        }
    });
}

}   // End of namespace
