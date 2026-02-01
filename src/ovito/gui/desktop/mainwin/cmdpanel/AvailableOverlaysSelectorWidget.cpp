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
#include "ActionCardsPopup.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableOverlaysSelectorWidget::AvailableOverlaysSelectorWidget(QWidget* parent, MainWindowUI& ui, OverlayListModel* overlayListModel)
    : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui), _overlayListModel(overlayListModel)
{
    // Fill combo box with a dummy item.
    addItem(tr("Add layer..."));

    // Create the available overlays model.
    _availableOverlaysModel = new AvailableOverlaysModel(this, ui, overlayListModel);

    // Update enabled state when the active viewport changes.
    connect(&datasetContainer(), &DataSetContainer::activeViewportChanged, this, &AvailableOverlaysSelectorWidget::onActiveViewportChanged);

    // Set initial enabled state.
    onActiveViewportChanged(datasetContainer().activeViewport());
}

/******************************************************************************
* Updates the enabled state of this widget based on the current viewport.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onActiveViewportChanged(Viewport* activeViewport)
{
    setEnabled(activeViewport != nullptr);
}

/******************************************************************************
* Called when the popup menu is about to be shown.
******************************************************************************/
void AvailableOverlaysSelectorWidget::showPopup()
{
    // Lazy create the card popup
    if(!_cardPopup) {
        _cardPopup = new ActionCardsPopup(availableOverlaysModel(), tr("Get more layers..."), this);
        connect(_cardPopup, &ActionCardsPopup::getMoreActionsClicked, this, &AvailableOverlaysSelectorWidget::onGetMoreLayersFromPopup);
    }
    _cardPopup->updateContent();
    _cardPopup->showBelow(this);
}

/******************************************************************************
* Handles click on "Get more layers..." button.
******************************************************************************/
void AvailableOverlaysSelectorWidget::onGetMoreLayersFromPopup()
{
    // Open the extensions gallery or website.
    if(QAction* action = actionManager()->getAction(ACTION_SCRIPTING_EXTENSIONS_GALLERY_OVERLAYS))
        action->trigger();
    else
        QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
}

}   // End of namespace
