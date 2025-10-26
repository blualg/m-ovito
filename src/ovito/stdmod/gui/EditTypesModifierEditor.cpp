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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/widgets/general/ActionsItemDelegate.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "EditTypesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(EditTypesModifierEditor);
DEFINE_VECTOR_REFERENCE_FIELD(EditTypesModifierEditor, elementTypes);

SET_OVITO_OBJECT_EDITOR(EditTypesModifier, EditTypesModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void EditTypesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Edit types"), rolloutParams, "manual:particles.modifiers.edit_types");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGridLayout* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0,0,0,0);
    gridLayout->setSpacing(2);
    gridLayout->setColumnStretch(1, 1);
    layout->addLayout(gridLayout);

    DataObjectReferenceParameterUI* sourcePropertyUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(EditTypesModifier::sourceProperty), Property::OOClass());
    gridLayout->addWidget(new QLabel(tr("Operate on:")), 0, 0);
    gridLayout->addWidget(sourcePropertyUI->comboBox(), 0, 1);
    sourcePropertyUI->setObjectFilter<Property>([](const Property* property) {
        return property->isTypedProperty();
    });

    class TableWidget : public QTableView {
    public:
        using QTableView::QTableView;
        virtual QSize sizeHint() const { return QSize(256, 200); }
    };
    _elementTypesTable = new TableWidget();
    _tableModel = new ViewModel(this);
    _elementTypesTable->setModel(_tableModel);
    _elementTypesTable->setShowGrid(false);
    _elementTypesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _elementTypesTable->setCornerButtonEnabled(false);
    _elementTypesTable->verticalHeader()->hide();
    _elementTypesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    _elementTypesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    _elementTypesTable->setWordWrap(false);
    _elementTypesTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _elementTypesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _elementTypesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _elementTypesTable->verticalHeader()->setDefaultSectionSize(_elementTypesTable->verticalHeader()->minimumSectionSize());
    _typeListCaption = new QLabel(tr("Types:"));
    layout->addWidget(_typeListCaption);
    layout->addWidget(_elementTypesTable);
    connect(_elementTypesTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &EditTypesModifierEditor::onElementTypeSelectionChanged);

    // Install custom item delegate for rendering element types in the table.
    ActionsItemDelegate* delegate = new ActionsItemDelegate(_elementTypesTable);
    _elementTypesTable->setItemDelegateForColumn(0, delegate);

    // Container widget for the element type sub-editor.
    _subEditorContainer = new QWidget(rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(_subEditorContainer);
    sublayout->setContentsMargins(0,0,0,0);
    layout->addWidget(_subEditorContainer);

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Update the element list whenever the modifier changes.
    connect(this, &PropertiesEditor::contentsChanged, this, &EditTypesModifierEditor::refreshElementTypeList);

    // Update the element list whenever the pipeline input changes.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &EditTypesModifierEditor::refreshElementTypeList);
}

/******************************************************************************
* Refreshes the displayed list of element types.
******************************************************************************/
void EditTypesModifierEditor::refreshElementTypeList()
{
    // Update the displayed list of element types.
    QModelIndexList selection = _elementTypesTable->selectionModel()->selectedRows();
    _tableModel->refresh();
    if(!selection.empty())
        _elementTypesTable->selectRow(selection.front().row());

    _typeListCaption->setText(_tableModel->listTitle().isEmpty() ? tr("Types:") : tr("%1:").arg(_tableModel->listTitle()));
}

/******************************************************************************
* Returns the data stored under the given role for the given RefTarget.
******************************************************************************/
QVariant EditTypesModifierEditor::ViewModel::data(const QModelIndex& index, int role) const
{
    if(index.isValid() && index.row() < elementTypes().size()) {
        if(role == Qt::DisplayRole) {
            if(index.column() == 0)
                return elementTypes()[index.row()]->nameOrNumericId();
            else if(index.column() == 1)
                return elementTypes()[index.row()]->numericId();
        }
        else if(role == Qt::DecorationRole) {
            if(index.column() == 0)
                return (QColor)elementTypes()[index.row()]->color();
        }
    }
    return {};
}

/******************************************************************************
* Returns the header data under the given role for the given RefTarget.
******************************************************************************/
QVariant EditTypesModifierEditor::ViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if(section == 0)
            return tr("Name");
        else if(section == 1)
            return tr("Id");
    }
    return {};
}

/******************************************************************************
* Returns the item flags for the given index.
******************************************************************************/
Qt::ItemFlags EditTypesModifierEditor::ViewModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index);
}

/******************************************************************************
* Updates the contents of the model.
******************************************************************************/
void EditTypesModifierEditor::ViewModel::refresh()
{
    beginResetModel();

    // Rebuild the list of element types.
    std::vector<DataOORef<const ElementType>> elementTypes;

    // Populate element types list with existing types from the selected input property.
    _listTitle.clear();
    if(modifier() && modifier()->sourceProperty()) {
        for(const PipelineFlowState& inputState : editor()->getPipelineInputs()) {
            if(const Property* inputProperty = inputState.getLeafObject(modifier()->sourceProperty())) {
                _listTitle = inputProperty->title();
                for(const ElementType* type : inputProperty->elementTypes()) {
                    if(!type)
                        continue;

                    // Make sure we don't add the same element type twice.
                    if(!boost::algorithm::any_of(elementTypes, [&](const auto& existingType) {
                        return (existingType->numericId() == type->numericId() && existingType->name() == type->name());
                    })) {
                        elementTypes.emplace_back(type);
                    }
                }
            }
        }
    }
    editor()->setElementTypes(std::move(elementTypes));

    endResetModel();
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool EditTypesModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TitleChanged || event.type() == ReferenceEvent::TargetChanged) {
        int index = elementTypes().indexOf(dynamic_object_cast<const ElementType>(source));
        if(index >= 0) {
            _tableModel->updateItem(index);
        }
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Is called whenever the selection of element types in the table has changed.
******************************************************************************/
void EditTypesModifierEditor::onElementTypeSelectionChanged()
{
    // Get the selected element type.
    ElementType* selectedType = nullptr;
    QModelIndexList selection = _elementTypesTable->selectionModel()->selectedRows();
    if(!selection.empty())
        selectedType = const_cast<ElementType*>(elementTypes().at(selection.front().row()).get());

    // Open sub-editor for the selected element type.
    handleExceptions([&] {
        bool updateLayout = false;
        if(_subEditor) {
            // Close old editor if it is no longer needed.
            if(!selectedType || _subEditor->editObject() == nullptr || _subEditor->editObject()->getOOClass() != selectedType->getOOClass()) {
                if(selectedType) {
                    _subEditor.reset();
                    updateLayout = true;
                }
            }
        }
        if(!_subEditor) {
            if(selectedType) {
                _subEditor = PropertiesEditor::create(ui(), selectedType);
            }
            else return;
            if(_subEditor) {
                _subEditor->initialize(container(), RolloutInsertionParameters().insertInto(_subEditorContainer), this);
                updateLayout = true;
            }
        }
        if(_subEditor) {
            _subEditor->setEditObject(selectedType, true);
        }
        if(updateLayout) {
            container()->updateRollouts();
        }
    });
}

}   // End of namespace
