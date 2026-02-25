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
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include "ModifierDelegateFixedListParameterUI.h"
#include "ModifierDelegateParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ModifierDelegateFixedListParameterUI);
DEFINE_VECTOR_REFERENCE_FIELD(ModifierDelegateFixedListParameterUI, delegates);

/******************************************************************************
* Constructor.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::initializeObject(PropertiesEditor* parentEditor)
{
    ParameterUI::initializeObject(parentEditor);

    _containerWidget = new QWidget();

    QFormLayout* layout = new QFormLayout(_containerWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setVerticalSpacing(2);

    // Update UI components when the pipeline input to the modifier changes.
    connect(editor(), &PropertiesEditor::pipelineInputChanged, this, &ParameterUI::updateUI);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ModifierDelegateFixedListParameterUI::~ModifierDelegateFixedListParameterUI()
{
    // Release widget.
    delete containerWidget();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to. The parameter UI should react to this change appropriately and
* show the properties value for the new edit object in the UI.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::resetUI()
{
    // Create our own copy of the list of delegates of the modifier.
    if(editObject())
        _delegates.setTargets(this, PROPERTY_FIELD(delegates), static_object_cast<MultiDelegatingModifier>(editObject())->delegates());
    else
        _delegates.clear(this, PROPERTY_FIELD(delegates));

    // Update enabled state of UI widget.
    if(containerWidget())
        containerWidget()->setEnabled(editObject() && isEnabled());

    ParameterUI::resetUI();
}

/******************************************************************************
* Fills the combo box with the list of data objects the given delegate can handle.
******************************************************************************/
bool ModifierDelegateFixedListParameterUI::populateObjectList(ModifierDelegate* delegate, StableComboBox* comboBox, const DataObjectReference& selectedDataObject, const std::vector<PipelineFlowState>& pipelineInputs)
{
    // The new list of combo-box items.
    std::vector<std::unique_ptr<QStandardItem>> items;

    // Add list items for the registered delegate classes.
    int indexToBeSelected = -1;

    // Collect the set of data objects in the modifier's pipeline input this delegate can handle.
    QVector<DataObjectReference> applicableObjects;
    for(const PipelineFlowState& state : pipelineInputs) {
        if(!state) continue;

        // Query the delegate for the list of input data objects it can handle.
        QVector<DataObjectReference> objList = delegate->getOOMetaClass().getApplicableObjects(*state.data());

        // Combine the delegate's list with the existing list.
        // Make sure no data object appears more than once.
        if(applicableObjects.empty()) {
            applicableObjects = std::move(objList);
        }
        else {
            for(const DataObjectReference& ref : objList) {
                if(!applicableObjects.contains(ref))
                    applicableObjects.push_back(ref);
            }
        }
    }

    // Add a special list item that represents the application of the delegate to all applicable data objects.
    if(!applicableObjects.empty() || selectedDataObject) {
        items.push_back(std::make_unique<QStandardItem>(tr("‹all›")));
    }
    else {
        items.push_back(std::make_unique<QStandardItem>(tr("‹not present›")));
    }

    // Add a list item for every data object that the delegate can handle.
    if(applicableObjects.size() > 1 || selectedDataObject) {
        for(const DataObjectReference& ref : applicableObjects) {
            items.push_back(std::make_unique<QStandardItem>(ref.dataTitle().isEmpty() ? delegate->objectTitle() : ref.dataTitle()));
            items.back()->setData(QVariant::fromValue(ref), Qt::UserRole);
            if(selectedDataObject == ref)
                indexToBeSelected = (int)items.size() - 1;
        }
    }

    // Select the right item in the list box.
    if(indexToBeSelected < 0) {
        if(selectedDataObject) {
            // Add a placeholder item if the selected data object does not exist anymore.
            QString title = selectedDataObject.dataTitle();
            if(title.isEmpty() && selectedDataObject.dataClass())
                title = selectedDataObject.dataClass()->displayName();
            title += tr(" (not available)");
            items.push_back(std::make_unique<QStandardItem>(title));
            items.back()->setData(QVariant::fromValue(selectedDataObject), Qt::UserRole);
            items.back()->setIcon(StableComboBox::warningIcon());
            indexToBeSelected = (int)items.size() - 1;
        }
        else indexToBeSelected = 0;
    }
    comboBox->setItems(std::move(items));
    comboBox->setCurrentIndex(indexToBeSelected);
    comboBox->setEnabled(delegate->isEnabled() && (comboBox->count() > 2 || selectedDataObject));

    return !applicableObjects.empty();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::updateUI()
{
    ParameterUI::updateUI();

    MultiDelegatingModifier* modifier = dynamic_object_cast<MultiDelegatingModifier>(editObject());
    OVITO_ASSERT(!modifier || boost::range::equal(modifier->delegates(), delegates()));
    OVITO_ASSERT(modifier || delegates().empty());
    OVITO_ASSERT(_delegateCheckBoxes.size() == delegates().size());
    OVITO_ASSERT(_delegateObjectLists.size() == delegates().size());

    // Query the pipeline for its input to the modifier.
    std::vector<PipelineFlowState> pipelineInputs = editor()->getPipelineInputs();

    // Update the contents of the UI widgets.
    for(const auto& [checkBox, objectList, delegate] : std::views::zip(_delegateCheckBoxes, _delegateObjectLists, delegates())) {
        if(delegate) {
            bool hasApplicableObjects = false;
            if(objectList) {
                hasApplicableObjects = populateObjectList(delegate, objectList, delegate->inputDataObject(), pipelineInputs);
            }
            if(checkBox) {
                checkBox->setEnabled(hasApplicableObjects);
                checkBox->setChecked(delegate && delegate->isEnabled());
            }
        }
        else {
            if(checkBox) {
                checkBox->setChecked(false);
                checkBox->setEnabled(false);
            }
            if(objectList) {
                objectList->clear();
                objectList->setEnabled(false);
            }
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    ParameterUI::setEnabled(enabled);
    if(containerWidget())
        containerWidget()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ModifierDelegateFixedListParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject()) {
        // Synchronize our internal list of delegates with the modifier's list of delegates.
        if(event.type() == ReferenceEvent::ReferenceAdded) {
            const ReferenceFieldEvent& refevent = static_cast<const ReferenceFieldEvent&>(event);
            if(refevent.field() == PROPERTY_FIELD(MultiDelegatingModifier::delegates)) {
                _delegates.insert(this, PROPERTY_FIELD(delegates), refevent.index(), static_object_cast<ModifierDelegate>(refevent.newTarget()));
            }
        }
        else if(event.type() == ReferenceEvent::ReferenceRemoved) {
            const ReferenceFieldEvent& refevent = static_cast<const ReferenceFieldEvent&>(event);
            if(refevent.field() == PROPERTY_FIELD(MultiDelegatingModifier::delegates)) {
                OVITO_ASSERT(refevent.oldTarget() == delegates()[refevent.index()]);
                _delegates.remove(this, PROPERTY_FIELD(delegates), refevent.index());
            }
        }
        else if(event.type() == ReferenceEvent::ReferenceChanged) {
            const ReferenceFieldEvent& refevent = static_cast<const ReferenceFieldEvent&>(event);
            if(refevent.field() == PROPERTY_FIELD(MultiDelegatingModifier::delegates)) {
                _delegates.set(this, PROPERTY_FIELD(delegates), refevent.index(), static_object_cast<ModifierDelegate>(refevent.newTarget()));
            }
        }
    }
    else if(delegates().contains(source)) {
        // When a delegate changes, update the UI to reflect the new state of the delegate.
        updateUI();
    }
    return ParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(delegates) && containerWidget()) {
        // When a new delegate is added to the modifier, create the corresponding UI elements for it.
        QFormLayout* layout = static_cast<QFormLayout*>(containerWidget()->layout());
        QCheckBox* checkBox = new QCheckBox();
        StableComboBox* comboBox = new StableComboBox();
        if(ModifierDelegate* delegate = dynamic_object_cast<ModifierDelegate>(newTarget)) {
            // Hide mnemonics in the checkbox text.
            QString title = delegate->objectTitle();
            title.replace('&', "&&");
            checkBox->setText(std::move(title));
            checkBox->setChecked(delegate->isEnabled());
        }
        else {
            checkBox->hide();
            comboBox->hide();
        }
        // Vertically center checkbox in the form cell.
        comboBox->setMinimumContentsLength(6);
        comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        checkBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(checkBox, &QCheckBox::clicked, this, &ModifierDelegateFixedListParameterUI::onDelegateToggled);
        connect(comboBox, qOverload<int>(&QComboBox::activated), this, &ModifierDelegateFixedListParameterUI::onDataObjectSelected);
        _delegateCheckBoxes.insert(listIndex, checkBox);
        _delegateObjectLists.insert(listIndex, comboBox);
        layout->insertRow(listIndex, checkBox, comboBox);
        OVITO_ASSERT(layout->rowCount() == delegates().size());
        editor()->container()->updateRolloutsLater();
    }
    ParameterUI::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(delegates) && containerWidget()) {
        // When a delegate is removed from the modifier, remove the corresponding UI elements.
        QFormLayout* layout = static_cast<QFormLayout*>(containerWidget()->layout());
        layout->removeRow(listIndex);
        OVITO_ASSERT(layout->rowCount() == delegates().size());
        _delegateCheckBoxes.remove(listIndex);
        _delegateObjectLists.remove(listIndex);
        editor()->container()->updateRolloutsLater();
    }
    ParameterUI::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been replaced in a VectorReferenceField of this RefMaker.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(delegates)) {
        // TODO: When a delegate is replaced in the modifier, update the corresponding UI elements to reflect the new delegate's state.
    }
    ParameterUI::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when the user toggles the enabled state of a delegate.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::onDelegateToggled(bool checked)
{
    // Find the delegate corresponding to the sender check box and toggle its enabled state.
    int index = _delegateCheckBoxes.indexOf(qobject_cast<QCheckBox*>(sender()));
    if(index < 0 || index >= delegates().size())
        return;
    if(ModifierDelegate* delegate = delegates()[index]) {
        performTransaction(tr("Enable/disable data element"), [delegate, checked]() {
            delegate->setEnabled(checked);
        });
    }
}

/******************************************************************************
* Is called when the user selects a different data object for a delegate.
******************************************************************************/
void ModifierDelegateFixedListParameterUI::onDataObjectSelected(int index)
{
    // Find the delegate corresponding to the sender combo box and update its input data object.
    int delegateIndex = _delegateObjectLists.indexOf(qobject_cast<StableComboBox*>(sender()));
    if(delegateIndex < 0 || delegateIndex >= delegates().size())
        return;
    if(ModifierDelegate* delegate = delegates()[delegateIndex]) {
        QVariant data = _delegateObjectLists[delegateIndex]->currentData(Qt::UserRole);
        DataObjectReference ref = data.value<DataObjectReference>();
        performTransaction(tr("Change object to operate on"), [delegate, ref]() {
            delegate->setInputDataObject(ref);
        });
    }
}

}   // End of namespace
