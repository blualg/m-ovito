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
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include "AvailableModifiersSelectorWidget.h"
#include "ActionCardsPopup.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableModifiersSelectorWidget::AvailableModifiersSelectorWidget(QWidget* parent, MainWindowUI& ui, PipelineListModel* pipelineListModel)
    : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui), _pipelineListModel(pipelineListModel)
{
    // Fill combo box with a dummy item.
    addItem(tr("Add modification..."));

    // Create the available modifiers model.
    _availableModifiersModel = new AvailableModifiersModel(this, ui, pipelineListModel);

    // Update enabled state when the pipeline selection changes.
    connect(pipelineListModel, &PipelineListModel::selectedItemChanged, this, &AvailableModifiersSelectorWidget::onPipelineSelectionChanged);

    // Set initial enabled state.
    onPipelineSelectionChanged();
}

/******************************************************************************
* Called when the popup menu is about to be shown.
******************************************************************************/
void AvailableModifiersSelectorWidget::showPopup()
{
    availableModifiersModel()->updateActionState();

    // Lazy create the card popup
    if(!_cardPopup) {
        _cardPopup = new ActionCardsPopup(availableModifiersModel(), tr("Get more modifiers..."), this);
        connect(_cardPopup, &ActionCardsPopup::getMoreActionsClicked, this, &AvailableModifiersSelectorWidget::onGetMoreModifiersFromPopup);
    }
    _cardPopup->updateContent();
    _cardPopup->showBelow(this);
}

/******************************************************************************
* Updates the enabled state of this widget based on the current pipeline selection.
******************************************************************************/
void AvailableModifiersSelectorWidget::onPipelineSelectionChanged()
{
    setEnabled(_pipelineListModel->selectedPipeline() != nullptr);
}

/******************************************************************************
* Handles click on "Get more modifiers..." button.
******************************************************************************/
void AvailableModifiersSelectorWidget::onGetMoreModifiersFromPopup()
{
    // Open the extensions gallery or website.
    if(QAction* action = actionManager()->getAction(ACTION_SCRIPTING_EXTENSIONS_GALLERY_MODIFIERS))
        action->trigger();
    else
        QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
}

}   // End of namespace
