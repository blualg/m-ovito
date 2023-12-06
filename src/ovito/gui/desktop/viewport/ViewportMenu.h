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
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportLayout.h>

namespace Ovito {

/**
 * \brief The context menu of the viewports.
 */
class OVITO_GUI_EXPORT ViewportMenu : public QMenu
{
    Q_OBJECT

public:

    /// Initializes the menu.
    ViewportMenu(MainWindow& mainWindow, Viewport* viewport, QWidget* viewportWidget);

    /// Displays the menu.
    void show(const QPoint& pos);

private Q_SLOTS:

    void onRenderPreviewMode(bool checked);
    void onShowGrid(bool checked);
    void onConstrainRotation(bool checked);
    void onShowViewTypeMenu();
    void onViewType(QAction* action);
    void onAdjustView();
    void onViewNode(QAction* action);
    void onCreateCamera();
    void onDeleteViewport();
    void onSplitViewport(ViewportLayoutCell::SplitDirection direction);
    void onPipelineVisibility(bool checked);

private:

    /// The main window this context menu belongs to.
    MainWindow& _mainWindow;

    /// The viewport this menu belongs to.
    Viewport* _viewport;

    /// The viewport widget this menu is shown in.
    QWidget* _viewportWidget;

    /// The view type sub-menu.
    QMenu* _viewTypeMenu;

    /// The cell in the window layout the viewport is living in.
    ViewportLayoutCell* _layoutCell;
};

}   // End of namespace
