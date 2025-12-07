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
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include "ModifierTemplatesPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModifierTemplatesPage);

/******************************************************************************
* When the user is creating a new template, this method populates the list of available objects,
* which the the user can select to be included in the template.
******************************************************************************/
QVector<QTreeWidgetItem*> ModifierTemplatesPage::populateAvailableObjectsList(QTreeWidget* objectListWidget, QComboBox* nameBox)
{
    PipelineListModel* pipelineModel = ui().mainWindow()->commandPanel()->modifyPage()->pipelineListModel();
    QVector<RefTarget*> selectedPipelineObjects = pipelineModel->selectedObjects();
    QVector<QTreeWidgetItem*> itemList;
    int rowCount = 0;

    // Iterate over the modifiers in the selected pipeline.
    if(Pipeline* pipeline = pipelineModel->selectedPipeline()) {
        ModifierGroup* currentGroup = nullptr;
        QTreeWidgetItem* currentGroupItem = nullptr;
        ModificationNode* modNode = dynamic_object_cast<ModificationNode>(pipeline->head());
        while(modNode) {
            if(modNode->modifierGroup() != currentGroup) {
                if(modNode->modifierGroup()) {
                    currentGroupItem = new QTreeWidgetItem(objectListWidget, { modNode->modifierGroup()->objectTitle() });
                    currentGroupItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate);
                    currentGroupItem->setExpanded(true);
                    rowCount++;
                }
                else currentGroupItem = nullptr;
                currentGroup = modNode->modifierGroup();
            }
            if(modNode->modifier()) {
                QTreeWidgetItem* listItem = currentGroupItem
                    ? new QTreeWidgetItem(currentGroupItem, {modNode->modifier()->objectTitle()})
                    : new QTreeWidgetItem(objectListWidget, {modNode->modifier()->objectTitle()});
                listItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                if(selectedPipelineObjects.contains(modNode) || selectedPipelineObjects.contains(modNode->modifierGroup())) {
                    listItem->setCheckState(0, Qt::Checked);
                }
                else {
                    listItem->setCheckState(0, Qt::Unchecked);
                }
                listItem->setData(0, Qt::UserRole, QVariant::fromValue(OORef<OvitoObject>(modNode->modifier())));
                itemList.push_back(listItem);
                rowCount++;
            }
            modNode = dynamic_object_cast<ModificationNode>(modNode->input());
        }
    }
    if(itemList.empty())
        throw Exception(tr("A modifier template must always be created on the basis of existing modifiers, but the current data pipeline does not contain any modifiers. "
                            "Please close this dialog, insert some modifier into the pipeline first, configure its settings and then come back here to create a template from it."));
    objectListWidget->setMaximumHeight(objectListWidget->sizeHintForRow(0) * qBound(3, rowCount, 10) + 2 * objectListWidget->frameWidth());

    ModificationNode* selectedModNode = (selectedPipelineObjects.size() == 1) ? dynamic_object_cast<ModificationNode>(selectedPipelineObjects.front()) : nullptr;
    if(selectedModNode && selectedModNode->modifier()) {
        if(selectedModNode->modifier()->title().isEmpty())
            nameBox->setCurrentText(tr("Custom %1").arg(selectedModNode->modifier()->objectTitle()));
        else
            nameBox->setCurrentText(selectedModNode->modifier()->title());
    }
    else if(ModifierGroup* selectedModGroup = (selectedPipelineObjects.size() == 1) ? dynamic_object_cast<ModifierGroup>(selectedPipelineObjects.front()) : nullptr) {
        if(selectedModGroup->title().isEmpty())
            nameBox->setCurrentText(tr("My %1").arg(selectedModGroup->objectTitle()));
        else
            nameBox->setCurrentText(selectedModGroup->title());
    }
    else {
        nameBox->setCurrentText(tr("Custom modifier template 1"));
    }

    return itemList;
}

}   // End of namespace
