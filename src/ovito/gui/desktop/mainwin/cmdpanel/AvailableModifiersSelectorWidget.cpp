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

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AvailableModifiersSelectorWidget::AvailableModifiersSelectorWidget(QWidget* parent, MainWindowUI& ui, PipelineListModel* pipelineListModel)
    : QComboBox(parent), UserInterfaceComponent<MainWindowUI>(ui), _pipelineListModel(pipelineListModel)
{
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setModel(new AvailableModifiersListModel(new AvailableModifiersModel(this, ui, pipelineListModel), this));
    setMaxVisibleItems(0xFFFF);
    connect(this, qOverload<int>(&QComboBox::activated), this, &AvailableModifiersSelectorWidget::onModifierSelected);

    // Update enabled state when the pipeline selection changes.
    connect(pipelineListModel, &PipelineListModel::selectedItemChanged, this, &AvailableModifiersSelectorWidget::onPipelineSelectionChanged);

    // Set initial enabled state.
    onPipelineSelectionChanged();
}

/******************************************************************************
* Returns the tree model that organizes all available modifiers by category.
******************************************************************************/
AvailableModifiersModel* AvailableModifiersSelectorWidget::availableModifiersModel() const
{
    return availableModifiersListModel()->sourceModel();
}

/******************************************************************************
* Returns the list model that presents the available modifiers in flat list format.
******************************************************************************/
AvailableModifiersListModel* AvailableModifiersSelectorWidget::availableModifiersListModel() const
{
    return static_cast<AvailableModifiersListModel*>(model());
}

/******************************************************************************
* Called when the popup menu is about to be shown.
******************************************************************************/
void AvailableModifiersSelectorWidget::showPopup()
{
    availableModifiersListModel()->updateActionState();
    QComboBox::showPopup();
}

/******************************************************************************
* Handles selection of a modifier from the drop-down list.
******************************************************************************/
void AvailableModifiersSelectorWidget::onModifierSelected(int index)
{
    if(index == availableModifiersListModel()->getMoreExtensionsItemIndex()) {
        // Open the extensions gallery or website.
        if(QAction* action = actionManager()->getAction(ACTION_SCRIPTING_EXTENSIONS_GALLERY_MODIFIERS))
            action->trigger();
        else
            QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
    }
    else {
        // Insert the selected modifier into the pipeline.
        availableModifiersListModel()->insertModifierByIndex(index);
    }
    // Reset the combo box to the default item.
    setCurrentIndex(0);
}

/******************************************************************************
* Updates the enabled state of this widget based on the current pipeline selection.
******************************************************************************/
void AvailableModifiersSelectorWidget::onPipelineSelectionChanged()
{
    setEnabled(_pipelineListModel->selectedPipeline() != nullptr);
}

}   // End of namespace
