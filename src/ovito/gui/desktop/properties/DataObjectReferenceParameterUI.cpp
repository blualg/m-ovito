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
#include <ovito/core/dataset/data/DataObjectReference.h>
#include "DataObjectReferenceParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DataObjectReferenceParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void DataObjectReferenceParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, const DataObject::OOMetaClass& dataObjectClass)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _comboBox = new StableComboBox();
    _dataObjectClass = &dataObjectClass;

    connect(comboBox(), &QComboBox::textActivated, this, &DataObjectReferenceParameterUI::updatePropertyValue);

    // Update the list whenever the pipeline input changes.
    connect(parentEditor, &PropertiesEditor::pipelineInputChanged, this, &DataObjectReferenceParameterUI::updateUI);
}

/******************************************************************************
* Destructor.
******************************************************************************/
DataObjectReferenceParameterUI::~DataObjectReferenceParameterUI()
{
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void DataObjectReferenceParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void DataObjectReferenceParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox() && editObject()) {

        // Get the current object reference.
        QVariant val = editObject()->getPropertyFieldValue(propertyField());
        OVITO_ASSERT(val.isValid() && val.canConvert<DataObjectReference>());
        DataObjectReference selectedObjectReference = val.value<DataObjectReference>();

        // Update list of data objects available in the pipeline's output.
        std::vector<std::unique_ptr<QStandardItem>> items;

        int selectedIndex = -1;
        bool currentObjectFilteredOut = false;
        for(const PipelineFlowState& state : editor()->getPipelineInputs()) {
            std::vector<ConstDataObjectPath> dataObjectPaths = state.getObjectsRecursive(*dataObjectClass());
            for(const ConstDataObjectPath& path : dataObjectPaths) {
                const DataObject* dataObject = path.back();

                DataObjectReference ref(path);

                // The client can apply a custom filter function to the object list.
                if(_objectFilter && !_objectFilter(dataObject)) {
                    if(selectedObjectReference == ref)
                        currentObjectFilteredOut = true;
                    continue;
                }

                // Do not add the same container to the list more than once.
                if(boost::algorithm::any_of(items, [&ref](const std::unique_ptr<QStandardItem>& item) {
                    return item->data(Qt::UserRole).value<DataObjectReference>() == ref;
                }))
                    continue;

                if(ref == selectedObjectReference)
                    selectedIndex = items.size();

                items.push_back(std::make_unique<QStandardItem>(ref.dataTitle()));
                items.back()->setData(QVariant::fromValue(ref), Qt::UserRole);
            }
        }

        if(selectedIndex < 0) {
            if(selectedObjectReference) {
                // Add a place-holder item if the selected object does not exist anymore.
                QString title = selectedObjectReference.dataTitle();
                if(title.isEmpty() && selectedObjectReference.dataClass())
                    title = selectedObjectReference.dataClass()->displayName();
                if(!currentObjectFilteredOut)
                    title += tr(" (not available)");
                items.push_back(std::make_unique<QStandardItem>(std::move(title)));
                items.back()->setData(QVariant::fromValue(selectedObjectReference), Qt::UserRole);
                items.back()->setIcon(StableComboBox::warningIcon());
            }
            else if(!items.empty()) {
                items.push_back(std::make_unique<QStandardItem>(tr("‹Please select›")));
                items.back()->setIcon(StableComboBox::warningIcon());
            }
            selectedIndex = (int)items.size() - 1;
        }

        if(items.empty()) {
            items.push_back(std::make_unique<QStandardItem>(tr("‹No available objects›")));
            items.back()->setIcon(StableComboBox::warningIcon());
            selectedIndex = 0;
        }

        comboBox()->setItems(std::move(items));
        comboBox()->setCurrentIndex(selectedIndex);

        // Sort list entries alphabetically.
        static_cast<QStandardItemModel*>(comboBox()->model())->sort(0);
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void DataObjectReferenceParameterUI::updatePropertyValue()
{
    if(comboBox() && editObject()) {
        performTransaction(tr("Select data object"), [this]() {
            OOWeakRef<DataObjectReferenceParameterUI> self(this);

            DataObjectReference ref = comboBox()->currentData().value<DataObjectReference>();

            // Check if new value differs from old value.
            QVariant oldval = editObject()->getPropertyFieldValue(propertyField());
            if(ref == oldval.value<DataObjectReference>())
                return;

            editObject()->setPropertyFieldValue(propertyField(), QVariant::fromValue(ref));

            if(self.lock())
                Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void DataObjectReferenceParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

}   // End of namespace
