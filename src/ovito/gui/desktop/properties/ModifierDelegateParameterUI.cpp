////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/DelegatingModifier.h>
#include <ovito/core/app/PluginManager.h>
#include "ModifierDelegateParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ModifierDelegateParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void ModifierDelegateParameterUI::initializeObject(PropertiesEditor* parentEditor, const OvitoClass& delegateType)
{
    ParameterUI::initializeObject(parentEditor);

    _delegateType = &delegateType;
    _comboBox = new StableComboBox();

    connect(comboBox(), &QComboBox::textActivated, this, &ModifierDelegateParameterUI::updatePropertyValue);

    // Update the list whenever the pipeline input changes.
    connect(parentEditor, &PropertiesEditor::pipelineInputChanged, this, &ModifierDelegateParameterUI::updateUI);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ModifierDelegateParameterUI::~ModifierDelegateParameterUI()
{
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void ModifierDelegateParameterUI::resetUI()
{
    ParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ModifierDelegateParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::ReferenceChanged &&
            static_cast<const ReferenceFieldEvent&>(event).field() == PROPERTY_FIELD(DelegatingModifier::delegate)) {
        // The modifier has been assigned a new delegate -> update list of delegates
        updateUI();
    }
    return ParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void ModifierDelegateParameterUI::updateUI()
{
    ParameterUI::updateUI();

    if(DelegatingModifier* modifier = dynamic_object_cast<DelegatingModifier>(editObject())) {
        populateComboBox(comboBox(), editor(), modifier, modifier->delegate(), modifier->delegate() ? modifier->delegate()->inputDataObject() : DataObjectReference(), *_delegateType);
    }
}

/******************************************************************************
* This method populates the combobox widget.
******************************************************************************/
void ModifierDelegateParameterUI::populateComboBox(StableComboBox* comboBox, PropertiesEditor* editor, Modifier* modifier, RefTarget* delegate, const DataObjectReference& inputDataObject, const OvitoClass& delegateType)
{
    OVITO_ASSERT(!delegate || delegateType.isMember(delegate));

    if(!comboBox)
        return;

    if(modifier) {
        // The new list of combo-box items.
        std::vector<std::unique_ptr<QStandardItem>> items;

        // Obtain pipeline inputs.
        std::vector<PipelineFlowState> pipelineInputs = editor->getPipelineInputs();

        // Add list items for the registered delegate classes.
        int indexToBeSelected = -1;
        for(const OvitoClassPtr& clazz : PluginManager::instance().listClasses(delegateType)) {

            // Collect the set of data objects in the modifier's pipeline input this delegate can handle.
            QVector<DataObjectReference> applicableObjects;
            for(const PipelineFlowState& state : pipelineInputs) {
                if(!state) continue;

                // Query the delegate for the list of input data objects it can handle.
                QVector<DataObjectReference> objList;
                if(clazz->isDerivedFrom(ModifierDelegate::OOClass()))
                    objList = static_cast<const ModifierDelegate::OOMetaClass*>(clazz)->getApplicableObjects(*state.data());

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

            if(!applicableObjects.empty()) {
                // Add an extra item to the list box for every data object that the delegate can handle.
                for(const DataObjectReference& ref : applicableObjects) {
                    items.push_back(std::make_unique<QStandardItem>(ref.dataTitle().isEmpty() ? clazz->displayName() : ref.dataTitle()));
                    items.back()->setData(QVariant::fromValue(clazz), Qt::UserRole);
                    items.back()->setData(QVariant::fromValue(ref), Qt::UserRole + 1);
                    if(delegate && &delegate->getOOClass() == clazz && (inputDataObject == ref || !inputDataObject)) {
                        indexToBeSelected = (int)items.size() - 1;
                    }
                }
            }
            else {
                // Even if this delegate cannot handle the input data, still show it in the list box as a disabled item.
                items.push_back(std::make_unique<QStandardItem>(clazz->displayName()));
                items.back()->setData(QVariant::fromValue(clazz), Qt::UserRole);
                items.back()->setEnabled(false);
                if(delegate && &delegate->getOOClass() == clazz)
                    indexToBeSelected = (int)items.size() - 1;
            }
        }

        // Select the right item in the list box.
        if(delegate) {
            if(indexToBeSelected < 0) {
                if(delegate && inputDataObject) {
                    // Add a place-holder item if the selected data object does not exist anymore.
                    QString title = inputDataObject.dataTitle();
                    if(title.isEmpty() && inputDataObject.dataClass())
                        title = inputDataObject.dataClass()->displayName();
                    title += tr(" (not available)");
                    items.push_back(std::make_unique<QStandardItem>(title));
                    items.back()->setData(QVariant::fromValue(&delegate->getOOClass()), Qt::UserRole);
                    items.back()->setIcon(StableComboBox::warningIcon());
                }
                else if(comboBox->count() != 0) {
                    items.push_back(std::make_unique<QStandardItem>(tr("‹Please select a data object›")));
                }
                indexToBeSelected = (int)items.size() - 1;
            }
            if(items.empty()) {
                items.push_back(std::make_unique<QStandardItem>(tr("‹No inputs available›")));
                items.back()->setIcon(StableComboBox::warningIcon());
                indexToBeSelected = 0;
            }
        }
        else {
            if(!items.empty())
                items.push_back(std::make_unique<QStandardItem>(tr("‹Please select a data object›")));
            else
                items.push_back(std::make_unique<QStandardItem>(tr("‹None›")));
            indexToBeSelected = (int)items.size() - 1;
            items.back()->setIcon(StableComboBox::warningIcon());
        }
        comboBox->setItems(std::move(items));
        comboBox->setCurrentIndex(indexToBeSelected);
    }
    else {
        comboBox->clear();
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void ModifierDelegateParameterUI::updatePropertyValue()
{
    Modifier* mod = dynamic_object_cast<Modifier>(editObject());
    if(comboBox() && mod) {
        performTransaction(tr("Change input type"), [this,mod]() {
            if(OvitoClassPtr delegateType = comboBox()->currentData().value<OvitoClassPtr>()) {
                DataObjectReference ref = comboBox()->currentData(Qt::UserRole + 1).value<DataObjectReference>();
                if(DelegatingModifier* delegatingMod = dynamic_object_cast<DelegatingModifier>(mod)) {
                    if(delegatingMod->delegate() == nullptr || &delegatingMod->delegate()->getOOClass() != delegateType || delegatingMod->delegate()->inputDataObject() != ref) {
                        // Create the new delegate object.
                        OORef<ModifierDelegate> delegate = static_object_cast<ModifierDelegate>(delegateType->createInstance());
                        // Set which input data object the delegate should operate on.
                        delegate->setInputDataObject(ref);
                        // Activate the new delegate.
                        delegatingMod->setDelegate(std::move(delegate));
                    }
                }
            }
            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ModifierDelegateParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    ParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

}   // End of namespace
