////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/OverlayListModel.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "AvailableOverlaysSelectorWidget.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableOverlaysSelectorWidget::AvailableOverlaysSelectorWidget(QWidget* parent, MainWindowUI& ui, OverlayListModel* overlayListModel)
    : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui), _overlayListModel(overlayListModel)
{
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setModel(new AvailableOverlaysListModel(new AvailableOverlaysModel(this, ui, overlayListModel), this));
    setMaxVisibleItems(0xFFFF);
    connect(this, qOverload<int>(&QComboBox::activated), this, &AvailableOverlaysSelectorWidget::onOverlaySelected);

    // Update enabled state when the active viewport changes.
    connect(&datasetContainer(), &DataSetContainer::activeViewportChanged, this, &AvailableOverlaysSelectorWidget::onActiveViewportChanged);

    // Set initial enabled state.
    onActiveViewportChanged(datasetContainer().activeViewport());
}

/******************************************************************************
* Returns the tree model that organizes all available overlays by category.
******************************************************************************/
AvailableOverlaysModel* AvailableOverlaysSelectorWidget::availableOverlaysModel() const
{
    return availableOverlaysListModel()->sourceModel();
}

/******************************************************************************
* Returns the list model that presents the available overlays in flat list format.
******************************************************************************/
AvailableOverlaysListModel* AvailableOverlaysSelectorWidget::availableOverlaysListModel() const
{
    return static_cast<AvailableOverlaysListModel*>(model());
}

/******************************************************************************
* Handles selection of an overlay from the drop-down list.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onOverlaySelected(int index)
{
    if(index == availableOverlaysListModel()->getMoreExtensionsItemIndex()) {
        // Open the extensions gallery or website.
        if(QAction* action = actionManager()->getAction(ACTION_SCRIPTING_EXTENSIONS_GALLERY_OVERLAYS))
            action->trigger();
        else
            QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
    }
    else {
        // Insert the selected overlay into the viewport.
        availableOverlaysListModel()->insertOverlayByIndex(index);
    }
    // Reset the combo box to the default item.
    setCurrentIndex(0);
}

/******************************************************************************
* Updates the enabled state of this widget based on the current viewport.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onActiveViewportChanged(Viewport* activeViewport)
{
    setEnabled(activeViewport != nullptr && count() > 1);
}

}   // End of namespace
