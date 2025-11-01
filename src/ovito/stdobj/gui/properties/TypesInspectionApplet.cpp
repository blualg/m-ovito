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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdmod/modifiers/EditTypesModifier.h>
#include <ovito/gui/desktop/widgets/general/CopyableTableView.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectorPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/properties/DefaultPropertiesEditor.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include "TypesInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TypesInspectionApplet);
OVITO_CLASSINFO(TypesInspectionApplet, "DisplayName", "Types")

/******************************************************************************
* Lets the applet create the UI widget that is to be placed into the data inspector panel.
******************************************************************************/
QWidget* TypesInspectionApplet::createWidget()
{
    _tableView = new CopyableTableView();
    _tableModel = new TypeTableModel(this, _tableView);
    _tableView->setModel(_tableModel);
    _tableView->horizontalHeader()->setResizeContentsPrecision(64); // Limit the number of rows taken into account when auto-resizing columns.
    _tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    _tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _tableView->verticalHeader()->setVisible(false);
    _cleanupHandler.add(_tableView);

    // Install custom item delegate for rendering element types in the table.
    ActionsItemDelegate* delegate = new ActionsItemDelegate(_tableView, TypeTableModel::InfoRole, TypeTableModel::ActionsRole);
    delegate->setSpanEntireRow(true);
    _tableView->setItemDelegateForColumn(1, delegate);

    // Update table whenever the user selects a different property in the list.
    connect(this, &DataInspectionApplet::currentObjectChanged, this, &TypesInspectionApplet::onCurrentPropertyChanged);

    QSplitter* splitter = new QSplitter();
    splitter->addWidget(objectSelectionWidget());
    splitter->addWidget(_tableView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    _cleanupHandler.add(splitter);

    // Create action for editing an element type.
    _editTypeAction = new ItemAction(QIcon::fromTheme("edit_rename_pipeline"), tr("Edit this type in pipeline"), this);
    connect(_editTypeAction, &ItemAction::triggeredForItem, this, &TypesInspectionApplet::onEditType);

    return splitter;
}

/******************************************************************************
* Determines the list of data objects that are displayed by the applet.
******************************************************************************/
std::vector<ConstDataObjectPath> TypesInspectionApplet::getDataObjectPaths()
{
    std::vector<ConstDataObjectPath> paths = currentState().getObjectsRecursive(Property::OOClass());

    // Filter out all properties that do not have element types.
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const ConstDataObjectPath& path) {
        return path.lastAs<Property>()->isTypedProperty() == false;
    }), paths.end());

    // Filter out all properties that belong to data tables, because their types often represent copies of
    // types associated with particle or bond properties. For example, a StructureAnalysisModifier adds the
    // structure types to both the "Structure Type" particle property and the "Structure Counts" data table.
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const ConstDataObjectPath& path) {
        return path.nextToLastAs<DataTable>() != nullptr;
    }), paths.end());

    // Make sure all particle properties appear first in the list.
    std::stable_partition(paths.begin(), paths.end(), [](const ConstDataObjectPath& path) {
        const PropertyContainer* container = path.nextToLastAs<PropertyContainer>();
        return container && container->getOOClass().name() == QStringLiteral("Particles");
    });

    return paths;
}

/******************************************************************************
* Initializes a list item representing the given data object path.
******************************************************************************/
void TypesInspectionApplet::configureDataObjectListItem(QListWidgetItem* item, const ConstDataObjectPath& objectPath)
{
    item->setText(objectPath.back()->objectTitle());
    item->setStatusTip(objectPath.toUIString());
    //item->setToolTip(objectPath.parentPath().toUIString());
}

/******************************************************************************
* Is called when the user selects a different property from the list.
******************************************************************************/
void TypesInspectionApplet::onCurrentPropertyChanged()
{
    handleExceptions([&]() {
        int oldColumnCount = _tableModel->columnCount();

        // Update the table model to display the element types of the newly selected property.
        _tableModel->setContents(selectedPropertyContainer(), selectedProperty());

        // Adjust the widths of the newly added columns.
        for(int i = oldColumnCount; i < _tableModel->columnCount(); i++) {
            if(i == 0)
                _tableView->setColumnWidth(i, 40); // Give the column "ID" a larger, fixed width.
            else if(i == 1)
                _tableView->setColumnWidth(i, 200); // Give the column "Type Name" a larger, fixed width.
            else
                _tableView->resizeColumnToContents(i);
        }
    });
}

/******************************************************************************
* Is called to edit the selected element type.
******************************************************************************/
void TypesInspectionApplet::onEditType(const QModelIndex& index)
{
    if(!selectedProperty() || !selectedPropertyContainer())
        return;
    if(index.row() < 0 || index.row() >= selectedProperty()->elementTypes().size())
        return;
    int typeId = selectedProperty()->elementTypes()[index.row()]->numericId();

    // To allow the user to edit the element type, we insert an EditTypesModifier into the pipeline at the right location.
    performTransaction(tr("Start editing type"), [&]() {
        // Walk up the pipeline to find the first node that created the selected property.
        PropertyDataObjectReference propertyRef = selectedDataObjectPath();
        PipelineNode* pipelineNode = currentPipeline()->head();
        PipelineNode* propertySourceNode = nullptr;
        ModificationNode* precedingModNode1 = nullptr;
        ModificationNode* precedingModNode2 = nullptr;
        OORef<ModificationNode> editTypesModNode;
        while(pipelineNode) {
            // Inspect the output data of the current pipeline node to see if it contains the selected property.
            bool propertyFound = false;
            PipelineFlowState state = pipelineNode->getCachedPipelineNodeOutput(currentAnimationTime());
            if(const Property* property = state.getLeafObject(propertyRef)) {
                if(property->elementType(typeId)) {
                    propertyFound = true;
                    propertySourceNode = pipelineNode;
                    precedingModNode2 = precedingModNode1;
                }
            }
            if(!propertyFound)
                break;

            // Walk up the pipeline.
            if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(pipelineNode)) {
                if(EditTypesModifier* modifier = dynamic_object_cast<EditTypesModifier>(modNode->modifier())) {
                    if(modifier->sourceProperty() == propertyRef) {
                        // Re-use existing EditTypesModifier.
                        editTypesModNode = modNode;
                        break;
                    }
                }
                precedingModNode1 = modNode;
                pipelineNode = modNode->input();
            }
            else
                pipelineNode = nullptr;
        }
        if(!editTypesModNode) {
            if(!propertySourceNode)
                throw Exception(tr("Could not find the source for the selected property in the current pipeline."));

            // Insert a new EditTypesModifier.
            OORef<EditTypesModifier> editTypesModifier = OORef<EditTypesModifier>::create();
            editTypesModifier->setSourceProperty(propertyRef);
            editTypesModNode = editTypesModifier->createModificationNode();
            editTypesModNode->setModifier(editTypesModifier);
            editTypesModNode->setInput(propertySourceNode);
            if(precedingModNode2)
                precedingModNode2->setInput(editTypesModNode);
            else
                currentPipeline()->setHead(editTypesModNode);
        }
        // Re-enable the modifier if it was disabled by the user.
        editTypesModNode->modifier()->setEnabled(true);

        // Activate the EditTypesModifier in the pipeline editor so that the user sees it.
        PropertiesEditor* currentEditor = ui().mainWindow()->commandPanel()->modifyPage()->startEditingPipelineNode(editTypesModNode);

        // Select the desired element type in the EditTypesModifier editor.
        if(DefaultPropertiesEditor* parentEditor = dynamic_object_cast<DefaultPropertiesEditor>(currentEditor)) {
            if(parentEditor->subEditors().size() == 1) {
                PropertiesEditor* modifierEditor = parentEditor->subEditors().front().get();
                OVITO_ASSERT(modifierEditor->getOOClass().name() == QStringLiteral("EditTypesModifierEditor"));
                QMetaObject::invokeMethod(modifierEditor, "selectElementTypeById", Q_ARG(int, typeId));
            }
            else OVITO_ASSERT(false);
        }
        else OVITO_ASSERT(false);
    });
}

/******************************************************************************
* Selects a specific data object in this applet.
******************************************************************************/
bool TypesInspectionApplet::selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint)
{
    // Check the property list in case the requested data object is a Property.
    if(DataInspectionApplet::selectDataObject(createdByNode, objectIdentifierHint, modeHint))
        return true;

    return false;
}

/******************************************************************************
* Replaces the contents of this data model.
******************************************************************************/
void TypesInspectionApplet::TypeTableModel::setContents(const PropertyContainer* container, const Property* property)
{
    OVITO_ASSERT(this_task::get());

    beginResetModel();
    _property = property;
    if(container && property) {
        // Query the list of table columns from the element type class.
        _elementTypeClass = container->getOOMetaClass().typedPropertyElementClass(property->typeId());
        if(!_elementTypeClass)
            _elementTypeClass = &ElementType::OOClass();
        _columnNames = _elementTypeClass->dataInspectorColumns();
    }
    else {
        _columnNames.clear();
        _elementTypeClass = nullptr;
    }
    endResetModel();
}

/******************************************************************************
* Returns the data stored under the given 'role' for the item referred to by
* the 'index'.
******************************************************************************/
QVariant TypesInspectionApplet::TypeTableModel::data(const QModelIndex& index, int role) const
{
    if(role == ActionsRole) {
        QList<QAction*> actions;
        actions.push_back(_applet->_editTypeAction);
        return QVariant::fromValue(actions);
    }
    if(_elementTypeClass) {
        return _elementTypeClass->dataInspectorModelData(index.column(), _columnNames[index.column()], _property->elementTypes()[index.row()], role);
    }
    return {};
}

/******************************************************************************
* Returns the data for the given role and section in the header with the specified orientation.
******************************************************************************/
QVariant TypesInspectionApplet::TypeTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < _columnNames.size()) {
        return _columnNames[section];
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

}   // End of namespace
