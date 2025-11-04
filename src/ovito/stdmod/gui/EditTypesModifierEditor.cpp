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
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdmod/modifiers/SelectTypeModifier.h>
#include <ovito/stdmod/modifiers/DeleteSelectedModifier.h>
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

    // Filter the list of selectable properties to only show typed properties.
    // These are the only properties that have element types.
    sourcePropertyUI->setObjectFilter([](const ConstDataObjectPath& path) {
        if(const Property* property = path.lastAs<Property>()) {
            if(property->isTypedProperty() == false)
                return false;

            // Filter out all properties that belong to data tables, because their types often represent copies of
            // types associated with particle or bond properties. For example, a StructureAnalysisModifier adds the
            // structure types to both the "Structure Type" particle property and the "Structure Counts" data table.
            if(path.nextToLastAs<DataTable>() != nullptr)
                return false;

            return true; // Accept property.
        }
        return false;
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

    _addNewTypeButton = new QPushButton(tr("Add new type..."));
    _addNewTypeButton->setEnabled(false);
    layout->addWidget(_addNewTypeButton, 0, Qt::AlignRight | Qt::AlignTop);
    connect(_addNewTypeButton, &QPushButton::clicked, this, &EditTypesModifierEditor::addNewType);

    // Install custom item delegate for rendering element types in the table.
    ActionsItemDelegate* delegate = new ActionsItemDelegate(_elementTypesTable, ViewModel::InfoRole, ViewModel::ActionsRole);
    delegate->setSpanEntireRow(true);
    _elementTypesTable->setItemDelegateForColumn(0, delegate);

    // Create actions for deleting and restoring element types.
    _deleteAction = new ItemAction(QIcon::fromTheme("edit_delete_pipeline"), tr("Delete"), this);
    _restoreAction = new ItemAction(QIcon::fromTheme("restore_object"), tr("Restore"), this);
    connect(_deleteAction, &ItemAction::triggeredForItem, this, &EditTypesModifierEditor::deleteType);
    connect(_restoreAction, &ItemAction::triggeredForItem, this, &EditTypesModifierEditor::restoreType);

    // Container widget for the element type sub-editor.
    _subEditorContainer = new QWidget(rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(_subEditorContainer);
    sublayout->setContentsMargins(0,0,0,0);
    layout->addWidget(_subEditorContainer);

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // Update the type list whenever the modifier changes.
    connect(this, &PropertiesEditor::contentsChanged, this, &EditTypesModifierEditor::refreshElementTypeList);

    // Update the type list whenever the pipeline input changes.
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
    else if(_elementTypesTable->model()->rowCount() > 0)
        _elementTypesTable->selectRow(0);

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
        else if(role == Qt::FontRole) {
            if(modifier()->deletedTypeIDs().contains(elementTypes()[index.row()]->numericId())) {
                QFont font = editor()->_elementTypesTable->font();
                font.setStrikeOut(true);
                return font;
            }
        }
        else if(role == InfoRole) {
            if(index.column() == 0) {
                if(modifier()->deletedTypeIDs().contains(elementTypes()[index.row()]->numericId())) {
                    return QVariant::fromValue(tr("(deleted)"));
                }
                else if(modifier()->editedTypes().contains(elementTypes()[index.row()])) {
                    return QVariant::fromValue(!isNewlyAddedType(index) ? tr("(edited)") : tr("(new)"));
                }
            }
        }
        else if(role == ActionsRole) {
            if(index.column() == 0) {
                QList<QAction*> actions;
                if((modifier()->deletedTypeIDs().contains(elementTypes()[index.row()]->numericId()) || modifier()->editedTypes().contains(elementTypes()[index.row()])) && !isNewlyAddedType(index)) {
                    actions.push_back(editor()->_restoreAction);
                }
                else if(!modifier()->deletedTypeIDs().contains(elementTypes()[index.row()]->numericId())) {
                    actions.push_back(editor()->_deleteAction);
                }
                return QVariant::fromValue(actions);
            }
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
    if(index.row() < elementTypes().size())
        return QAbstractTableModel::flags(index);
    else
        return QAbstractTableModel::flags(index) & ~Qt::ItemIsSelectable;
}

/******************************************************************************
* Updates the contents of the model.
******************************************************************************/
void EditTypesModifierEditor::ViewModel::refresh()
{
    beginResetModel();

    // Rebuild the list of element types.
    std::vector<OORef<ElementType>> elementTypes;

    // Populate element types list with existing types from the selected input property.
    _listTitle.clear();
    _upstreamElementTypeCount = 0;
    bool foundSourceProperty = false;
    if(modifier() && modifier()->sourceProperty()) {
        CloneHelper cloneHelper;
        for(const PipelineFlowState& inputState : editor()->getPipelineInputs()) {
            if(const Property* inputProperty = inputState.getLeafObject(modifier()->sourceProperty())) {
                foundSourceProperty = true;
                _listTitle = inputProperty->title();
                for(const ElementType* type : inputProperty->elementTypes()) {
                    if(!type)
                        continue;

                    // Make sure we don't add the same element type twice.
                    if(!boost::algorithm::any_of(elementTypes, [&](const auto& existingType) {
                        return (existingType->numericId() == type->numericId() && existingType->name() == type->name());
                    })) {
                        // Check if the element type has been edited.
                        auto iter = std::find_if(modifier()->editedTypes().begin(), modifier()->editedTypes().end(),
                            [&](const auto& editedType) {
                                return (editedType->numericId() == type->numericId() && &editedType->getOOClass() == &type->getOOClass());
                            });
                        if(iter != modifier()->editedTypes().end()) {
                            // Add the edited element type to the list.
                            elementTypes.push_back(*iter);
                        }
                        else {
                            // Add an editable copy of the element type to the list.
                            OORef<ElementType> clonedType = cloneHelper.cloneObject(type, false);

                            // Freeze all property fields of the type to make it easier to spot which fields have been modified by the user later.
                            clonedType->freezeInitialParameterValues();

                            elementTypes.push_back(std::move(clonedType));
                        }
                    }
                }
            }
        }
        _upstreamElementTypeCount = elementTypes.size();

        // Append the edited element types that are not already in the list.
        for(ElementType* editedType : modifier()->editedTypes()) {
            if(!boost::algorithm::any_of(elementTypes, [&](const auto& existingType) {
                return (existingType->numericId() == editedType->numericId() && &existingType->getOOClass() == &editedType->getOOClass());
            })) {
                elementTypes.push_back(editedType);
            }
        }
    }
    editor()->setElementTypes(std::move(elementTypes));

    endResetModel();

    editor()->_addNewTypeButton->setEnabled(foundSourceProperty);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool EditTypesModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(ElementType* editedType = dynamic_object_cast<ElementType>(source)) {
            int index = elementTypes().indexOf(editedType);
            if(index >= 0 && modifier()) {
                if(!isUndoingOrRedoing()) {
                    // Record that this element type has been edited.
                    modifier()->addEditedType(editedType);
                }
                _tableModel->updateItem(index);
            }
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
            bool isTypeDeleted = selectedType && modifier()->deletedTypeIDs().contains(selectedType->numericId());
            _subEditor->setEditObject(selectedType, isTypeDeleted);
        }
        if(updateLayout) {
            container()->updateRollouts();
        }
    });
}

/******************************************************************************
* Selects the element type with the given numeric ID in the table.
* This is used to communicate the selection from the TypesInspectionApplet to the modifier editor.
******************************************************************************/
void EditTypesModifierEditor::selectElementTypeById(int typeId)
{
    // Look up the ElementType with the given numeric ID.
    auto elementTypeIt = std::find_if(elementTypes().begin(), elementTypes().end(), [&](const OORef<ElementType>& type) {
        return type->numericId() == typeId;
    });
    // Select the corresponding row in the table.
    if(elementTypeIt != elementTypes().end()) {
        _elementTypesTable->selectRow(std::distance(elementTypes().begin(), elementTypeIt));
        _subEditorContainer->setFocus();
    }
    else OVITO_ASSERT(false); // This should never happen.
}

/******************************************************************************
* Deletes the selected element type.
******************************************************************************/
void EditTypesModifierEditor::deleteType(const QModelIndex& index)
{
    if(modifier() && index.isValid() && index.row() < elementTypes().size()) {
        auto typeId = elementTypes()[index.row()]->numericId();
        performTransaction(tr("Delete type"), [&]() {

            if(_tableModel->isNewlyAddedType(index)) {
                // Just remove the newly added type without further checks.
                modifier()->restoreType(typeId);
                return;
            }


            // Before deleting the type, check if elements of that type exist in the pipeline data.
            size_t numElementsOfType = 0;
            QString elementDescriptionName;
            PropertyReference propertyRef;
            PropertyContainerReference containerRef;
            const DeleteSelectedModifierDelegate::OOMetaClass* deleteSelectedDelegate = nullptr;
            for(const PipelineFlowState& inputState : getPipelineInputs()) {
                ConstDataObjectPath path = inputState.getObject(modifier()->sourceProperty());
                if(const Property* inputProperty = path.lastAs<Property>()) {
                    if(inputProperty->isTypedProperty() && inputProperty->elementType(typeId)) {
                        numElementsOfType += inputProperty->count(typeId);
                        if(numElementsOfType) {
                            if(const PropertyContainer* container = path.nextToLastAs<PropertyContainer>()) {
                                elementDescriptionName = container->getOOMetaClass().elementDescriptionName();
                                propertyRef = inputProperty;
                                containerRef = path.parentPath();
                                deleteSelectedDelegate = getDeleteSelectedModifierDelegateMetaClassForContainer(inputState, containerRef);
                            }
                        }
                    }
                }
            }

            // Ask user for confirmation if elements of the type to be deleted exist.
            if(numElementsOfType != 0) {
                const QString& typeName = elementTypes()[index.row()]->name();
                MessageDialog msgBox(QMessageBox::Warning, tr("Delete type"),
                        tr("Do you really want to delete type %1 even though there are %2 %3 of this type?").arg(
                        typeName.isEmpty() ? tr("%1").arg(typeId) : tr("'%1' (numeric id %2)").arg(typeName).arg(typeId)).arg(numElementsOfType).arg(elementDescriptionName),
                        QMessageBox::Yes | QMessageBox::Cancel, parentWindow());
                if(deleteSelectedDelegate) {
                    msgBox.setInformativeText(tr(
                        "<p>Deleting a type definition while some %1 of that type still exist may result in an invalid state and should be avoided.</p>"
                        "<p>Choose <b><i>Yes, also delete %1</i></b> to insert modifiers into the current pipeline that will also remove those existing %1.</p>"
                        "<p>Choose <b><i>Yes</i></b> to ignore this warning and delete the type anyway.</p>")
                        .arg(elementDescriptionName));

                    QPushButton* deleteElementsButton = msgBox.addButton(tr("Yes, also delete %1").arg(elementDescriptionName), QMessageBox::YesRole);
                    msgBox.setEscapeButton(QMessageBox::Cancel);
                    msgBox.exec();
                    if(msgBox.clickedButton() == msgBox.button(QMessageBox::Cancel)) {
                        this_task::cancelAndThrow(); // Operation canceled by user.
                    }
                    else if(msgBox.clickedButton() == deleteElementsButton) {
                        // Insert a SelectTypeModifier and a DeleteSelectedModifier into the pipeline to remove existing elements of the type.
                        for(ModificationNode* node : modificationNodes()) {
                            insertModifiersToDeleteElementsOfType(node, typeId, elementDescriptionName, propertyRef, containerRef, deleteSelectedDelegate);
                        }
                    }
                }
                else {
                    // It's not possible to first delete the elements from this type of PropertyContainer. Just show a warning.
                    msgBox.setInformativeText(tr(
                        "<p>Deleting a type definition while some %1 of that type still exist may result in an invalid state and should be avoided.</p>"
                        "<p>Choose <b><i>Yes</i></b> to ignore this warning and delete the type anyway.</p>")
                        .arg(elementDescriptionName));
                    msgBox.setEscapeButton(QMessageBox::Cancel);
                    if(msgBox.exec() != QMessageBox::Yes)
                        this_task::cancelAndThrow(); // Operation canceled by user.
                }
            }

            // Delete the type.
            modifier()->deleteType(typeId);
        });
    }
}

/******************************************************************************
* Restores the selected element type.
******************************************************************************/
void EditTypesModifierEditor::restoreType(const QModelIndex& index)
{
    if(modifier() && index.isValid() && index.row() < elementTypes().size()) {
        auto typeId = elementTypes()[index.row()]->numericId();
        performTransaction(tr("Restore type"), [&]() {
            modifier()->restoreType(typeId);
        });
    }
}

/******************************************************************************
* Finds a suitable DeleteSelectedModifierDelegate for the given PropertyContainer type.
******************************************************************************/
const DeleteSelectedModifierDelegate::OOMetaClass* EditTypesModifierEditor::getDeleteSelectedModifierDelegateMetaClassForContainer(const PipelineFlowState& state, const PropertyContainerReference& containerRef)
{
    // PropertyContainer must support selections.
    if(!containerRef || !containerRef.dataClass()->isValidStandardPropertyId(Property::GenericSelectionProperty))
        return nullptr;

    // Enumerate all registered DeleteSelectedModifierDelegate metaclasses and check if they support the given container type.
    for(const DeleteSelectedModifierDelegate::OOMetaClass* clazz : PluginManager::instance().metaclassMembers<DeleteSelectedModifierDelegate>()) {
        QVector<DataObjectReference> applicableObjects = clazz->getApplicableObjects(state);
        if(applicableObjects.contains(containerRef))
            return clazz;
    }

    // No suitable delegate found.
    return nullptr;
}

/******************************************************************************
* Inserts modifiers into the pipeline to delete existing elements of the given type ID.
******************************************************************************/
void EditTypesModifierEditor::insertModifiersToDeleteElementsOfType(ModificationNode* modNode, int typeId, const QString& elementDescriptionName, const PropertyReference& propertyRef, const PropertyContainerReference& containerRef, const DeleteSelectedModifierDelegate::OOMetaClass* deleteSelectedDelegateType)
{
    // First, check if the pipeline already contains a SelectTypeModifier and a DeleteSelectedModifier.
    if(ModificationNode* deleteSelectedModNode = dynamic_object_cast<ModificationNode>(modNode->input())) {
        if(DeleteSelectedModifier* deleteSelectedModifier = dynamic_object_cast<DeleteSelectedModifier>(deleteSelectedModNode->modifier())) {
            if(ModificationNode* selectTypeModNode = dynamic_object_cast<ModificationNode>(deleteSelectedModNode->input())) {
                if(SelectTypeModifier* selectTypeModifier = dynamic_object_cast<SelectTypeModifier>(selectTypeModNode->modifier())) {
                    if(deleteSelectedModifier->isEnabled() && deleteSelectedModifier->isEnabled() && selectTypeModifier->sourceProperty() == propertyRef && selectTypeModifier->subject() == containerRef) {
                        // Found existing SelectTypeModifier and DeleteSelectedModifier. Just add the type ID to be deleted.
                        auto selectedTypeIDs = selectTypeModifier->selectedTypeIDs();
                        selectedTypeIDs.insert(typeId);
                        selectTypeModifier->setSelectedTypeIDs(std::move(selectedTypeIDs));
                        return;
                    }
                }
            }
        }
    }

    // Insert SelectTypeModifier.
    OORef<SelectTypeModifier> selectTypeModifier = OORef<SelectTypeModifier>::create();
    selectTypeModifier->setSubject(containerRef);
    selectTypeModifier->setSourceProperty(propertyRef);
    selectTypeModifier->setSelectedTypeIDs({typeId});
    OORef<ModificationNode> selectTypeModNode = selectTypeModifier->createModificationNode();
    selectTypeModNode->setModifier(selectTypeModifier);
    selectTypeModNode->setInput(modNode->input());

    // Insert DeleteSelectedModifier.
    OORef<DeleteSelectedModifier> deleteSelectedModifier = OORef<DeleteSelectedModifier>::create();
    for(ModifierDelegate* delegate : deleteSelectedModifier->delegates()) {
        delegate->setEnabled(&delegate->getOOMetaClass() == deleteSelectedDelegateType);
    }
    OORef<ModificationNode> deleteSelectedModNode = deleteSelectedModifier->createModificationNode();
    deleteSelectedModNode->setModifier(deleteSelectedModifier);
    deleteSelectedModNode->setInput(selectTypeModNode);

    // Reconnect the original modifier node to the new DeleteSelectedModifier.
    modNode->setInput(deleteSelectedModNode);
}

/******************************************************************************
* Adds a new element type.
******************************************************************************/
void EditTypesModifierEditor::addNewType()
{
    if(!modifier())
        return;

    performTransaction(tr("Add new type"), [&]() {

        // To create a new element type, we first need to determine the container class and the name of the typed property.
        OwnerPropertyRef ownerPropertyRef;
        for(const PipelineFlowState& inputState : getPipelineInputs()) {
            ConstDataObjectPath path = inputState.getObject(modifier()->sourceProperty());
            if(const Property* inputProperty = path.lastAs<Property>()) {
                if(inputProperty->isTypedProperty()) {
                    if(const PropertyContainer* container = path.nextToLastAs<PropertyContainer>()) {
                        ownerPropertyRef = OwnerPropertyRef(&container->getOOMetaClass(), inputProperty);
                        break;
                    }
                }
            }
        }
        if(!ownerPropertyRef)
            throw Exception(tr("Cannot add new element type because the property container could not be determined."));

        // Let the PropertyContainer class determine the right element type class needed for the typed property.
        ElementTypeClassPtr elementTypeClass = ownerPropertyRef.containerClass()->typedPropertyElementClass(ownerPropertyRef.typeId());
        if(elementTypeClass == nullptr)
            elementTypeClass = &ElementType::OOClass();

        // Determine a unique numeric ID for the element type
        // and allow user to override the suggested numeric ID.
        std::optional<int> id = askForUniqueTypeId();
        if(!id)
            this_task::cancelAndThrow(); // Operation canceled by user.

        // Create the new element type.
        OORef<ElementType> elementType = static_object_cast<ElementType>(elementTypeClass->createInstance());
        elementType->setNumericId(id.value());
        elementType->initializeType(ownerPropertyRef);

        // Register the new element type.
        modifier()->addEditedType(elementType);

        // Select the newly added type in the table.
        _elementTypesTable->selectRow(elementTypes().indexOf(elementType));
    });
}

/******************************************************************************
* Suggests a unique numeric ID for a new element type.
* Gives the user the opportunity to change it if desired.
******************************************************************************/
std::optional<int> EditTypesModifierEditor::askForUniqueTypeId() const
{
    // Suggest a unique numeric ID.
    int uniqueId = 1;
    for(const ElementType* type : elementTypes())
        uniqueId = std::max(uniqueId, type->numericId() + 1);

    // Show dialog to allow user to change the suggested numeric ID.
    QDialog dlg(parentWindow());
    dlg.setWindowTitle(tr("Add new type"));
    QGridLayout* mainLayout = new QGridLayout(&dlg);
    mainLayout->setHorizontalSpacing(0);
    mainLayout->setVerticalSpacing(6);
    mainLayout->setColumnStretch(0, 1);

    mainLayout->addWidget(new QLabel(tr("Please specify a unique numeric ID for the new type:")), 0, 0, 1, 2);

    QLineEdit* idBox = new QLineEdit();
    mainLayout->addWidget(idBox, 1, 0);
    SpinnerWidget* idSpinner = new SpinnerWidget();
    idSpinner->setUnit(unitsManager().integerIdentityUnit());
    idSpinner->setTextBox(idBox);
    idSpinner->setIntValue(uniqueId);
    mainLayout->addWidget(idSpinner, 1, 1);

    QAction* errorAction = idBox->addAction(QIcon(":/guibase/mainwin/status/status_error.png"), QLineEdit::TrailingPosition);
    errorAction->setVisible(false);
    errorAction->setToolTip(tr("A type with this numeric ID already exists. Please choose a different ID."));

    mainLayout->setRowMinimumHeight(2, 12);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLayout->addWidget(buttonBox, 3, 0, 1, 2);
    QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setEnabled(true);

    // Validate user input. Display an error indicator if the entered ID is already in use.
    connect(idSpinner, &SpinnerWidget::valueChanged, &dlg, [&]() {
        uniqueId = idSpinner->intValue();
        okButton->setEnabled(std::ranges::none_of(elementTypes(), [&](const ElementType* type) {
            return type->numericId() == uniqueId;
        }));
        errorAction->setVisible(!okButton->isEnabled());
    });

    if(dlg.exec() == QDialog::Accepted)
        return uniqueId;
    else
        return std::nullopt;
}


}   // End of namespace
