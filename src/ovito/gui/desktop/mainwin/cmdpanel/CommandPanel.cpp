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
#include <ovito/gui/base/actions/ActionManager.h>
#include "CommandPanel.h"
#include "RenderCommandPage.h"
#include "ModifyCommandPage.h"
#include "OverlayCommandPage.h"
#include "UtilityCommandPage.h"

namespace Ovito {

/******************************************************************************
* The constructor of the command panel class.
******************************************************************************/
CommandPanel::CommandPanel(MainWindowUI& userInterface, QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    // Create tab widget
    _tabWidget = new QTabWidget(this);
    layout->addWidget(_tabWidget, 1);

    // Create the tabs.
    _tabWidget->setDocumentMode(true);
    _tabWidget->addTab(_modifyPage = new ModifyCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_modify"), QString());
    _tabWidget->addTab(_renderPage = new RenderCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_render"), QString());
    _tabWidget->addTab(_overlayPage = new OverlayCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_overlays"), QString());
    _tabWidget->addTab(_utilityPage = new UtilityCommandPage(userInterface, _tabWidget), QIcon::fromTheme("command_panel_tab_utilities"), QString());
    _tabWidget->setTabToolTip(0, tr("Pipelines"));
    _tabWidget->setTabToolTip(1, tr("Rendering"));
    _tabWidget->setTabToolTip(2, tr("Viewport layers"));
    _tabWidget->setTabToolTip(3, tr("Utilities"));
    setCurrentPage(MainWindow::MODIFY_PAGE);

    QAction* showModifyPageAction = userInterface.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_MODIFY, tr("Pipeline editor"), {}, tr("Switches to the pipeline editing tab."));
    connect(showModifyPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::MODIFY_PAGE); });

    QAction* showRenderPageAction = userInterface.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_RENDER, tr("Render settings"), {}, tr("Switches to the image & animation rendering tab."));
    connect(showRenderPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::RENDER_PAGE); });

    QAction* showOverlayPageAction = userInterface.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_OVERLAYS, tr("Viewport layers"), {}, tr("Switches to the viewport layers tab."));
    connect(showOverlayPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::OVERLAY_PAGE); });

    QAction* showUtilityPageAction = userInterface.actionManager()->createCommandAction(ACTION_COMMAND_PANEL_UTILITIES, tr("Utilities"), {}, tr("Switches to the utilities tab."));
    connect(showUtilityPageAction, &QAction::triggered, this, [this]() { setCurrentPage(MainWindow::UTILITY_PAGE); });
}

/******************************************************************************
* Loads the layout of the widgets from the settings store.
******************************************************************************/
void CommandPanel::restoreLayout()
{
    _modifyPage->restoreLayout();
    _renderPage->restoreLayout();
    _overlayPage->restoreLayout();
}

/******************************************************************************
* Saves the layout of the widgets to the settings store.
******************************************************************************/
void CommandPanel::saveLayout()
{
    _modifyPage->saveLayout();
    _renderPage->saveLayout();
    _overlayPage->saveLayout();
}

}   // End of namespace
