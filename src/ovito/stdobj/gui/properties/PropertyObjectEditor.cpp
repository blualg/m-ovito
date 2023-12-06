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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/gui/desktop/properties/RefTargetListParameterUI.h>
#include "PropertyObjectEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(PropertyObjectEditor);
SET_OVITO_OBJECT_EDITOR(Property, PropertyObjectEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void PropertyObjectEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(QString(), rolloutParams, "manual:scene_objects.particles");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(0);

    // Derive a custom class from the list parameter UI to display the element type colors, names and IDs.
    class CustomRefTargetListParameterUI : public RefTargetListParameterUI {
    public:
        using RefTargetListParameterUI::RefTargetListParameterUI;
    protected:

        /// Returns a data item from the list data model.
        virtual QVariant getItemData(RefTarget* target, const QModelIndex& index, int role) override {
            if(const ElementType* type = static_object_cast<ElementType>(target)) {
                if(role == Qt::DisplayRole) {
                    if(index.column() == 0) {
                        return type->nameOrNumericId();
                    }
                    else if(index.column() == 1) {
                        return type->numericId();
                    }
                }
                else if(role == Qt::DecorationRole) {
                    if(index.column() == 0)
                        return (QColor)type->color();
                }
            }
            return RefTargetListParameterUI::getItemData(target, index, role);
        }

        /// Returns the number of columns for the table view.
        virtual int tableColumnCount() override { return 2; }

        /// Returns the header data under the given role for the given RefTarget.
        virtual QVariant getHorizontalHeaderData(int index, int role) override {
            if(role == Qt::DisplayRole) {
                if(index == 0)
                    return QVariant::fromValue(tr("Name"));
                else if(index == 1)
                    return QVariant::fromValue(tr("Id"));
            }
            return RefTargetListParameterUI::getHorizontalHeaderData(index, role);
        }

        /// Opens a sub-editor for the object that is selected in the list view.
        virtual void openSubEditor() override {
            RefTargetListParameterUI::openSubEditor();
            editor()->container()->updateRollouts();
        }
    };

    QWidget* subEditorContainer = new QWidget(rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(subEditorContainer);
    sublayout->setContentsMargins(0,0,0,0);
    layout->addWidget(subEditorContainer);

    RefTargetListParameterUI* elementTypesListUI = new CustomRefTargetListParameterUI(this, PROPERTY_FIELD(Property::elementTypes), RolloutInsertionParameters().insertInto(subEditorContainer));
    QTableView* tableWidget = elementTypesListUI->tableWidget(250);
    layout->insertWidget(0, tableWidget);
    tableWidget->verticalHeader()->setDefaultSectionSize(tableWidget->verticalHeader()->minimumSectionSize());
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

}   // End of namespace
