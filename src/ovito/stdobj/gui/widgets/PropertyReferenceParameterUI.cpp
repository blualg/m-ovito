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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "PropertyReferenceParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PropertyReferenceParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
PropertyReferenceParameterUI::PropertyReferenceParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, PropertyContainerClassPtr containerClass, PropertyComponentsMode componentsMode, bool inputProperty) :
    PropertyParameterUI(parentEditor, propField),
    _comboBox(new PropertySelectionComboBox(containerClass)),
    _componentsMode(componentsMode),
    _isInputProperty(inputProperty)
{
    connect(comboBox(), &QComboBox::textActivated, this, &PropertyReferenceParameterUI::updatePropertyValue);

    if(!inputProperty)
        comboBox()->setEditable(true);

    // Specify the type of property container to look for in the pipeline input.
    setContainerRef(containerClass);
}

/******************************************************************************
* Destructor.
******************************************************************************/
PropertyReferenceParameterUI::~PropertyReferenceParameterUI()
{
    delete comboBox();
}

/******************************************************************************
* Sets the reference to the property container from which the user can select a property.
******************************************************************************/
void PropertyReferenceParameterUI::setContainerRef(const PropertyContainerReference& containerRef)
{
    if(_containerRef != containerRef) {
        OVITO_ASSERT(!container());

        _containerRef = containerRef;
        _comboBox->setContainerClass(_containerRef.dataClass());

        // Refresh list of available properies.
        updateUI();

        // Update the list whenever the pipeline input changes.
        if(_containerRef)
            connect(editor(), &PropertiesEditor::pipelineInputChanged, this, &PropertyReferenceParameterUI::updateUI);
        else
            disconnect(editor(), &PropertiesEditor::pipelineInputChanged, this, &PropertyReferenceParameterUI::updateUI);
    }
}

/******************************************************************************
* Sets the concrete property container from which properties can be selected.
******************************************************************************/
void PropertyReferenceParameterUI::setContainer(const PropertyContainer* container)
{
    if(_container != container) {
        OVITO_ASSERT(!containerRef());

        _container = container;
        _comboBox->setContainerClass(container ? &container->getOOMetaClass() : nullptr);
        updateUI();
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void PropertyReferenceParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* Returns the value currently set for the property field.
******************************************************************************/
PropertyReference PropertyReferenceParameterUI::getPropertyReference()
{
    if(editObject()) {
        if(isPropertyFieldUI()) {
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid() && val.canConvert<PropertyReference>());
            return val.value<PropertyReference>();
        }
    }
    return {};
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void PropertyReferenceParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox() && editObject() && (containerRef() || container())) {
        PropertyReference pref = getPropertyReference();

        if(_isInputProperty) {
            _comboBox->clear();

            // Build the list of available input properties.
            if(container()) {
                // Populate combo box with items from the input container.
                addItemsToComboBox(container());
            }
            else {
                // Populate combo box with items from the upstream pipeline.
                for(const PipelineFlowState& state : editor()->getPipelineInputs()) {
                    addItemsToComboBox(state);
                }
            }

            // Select the right item in the list box.
            int selIndex = _comboBox->propertyIndex(pref);
            static QIcon warningIcon(QStringLiteral(":/guibase/mainwin/status/status_warning.png"));
            if(selIndex < 0) {
                if(!pref.isNull() && pref.containerClass() == containerClass()) {
                    // Add a place-holder item if the selected property does not exist anymore.
                    _comboBox->addItem(pref, tr("%1 (not available)").arg(pref.name()));
                    QStandardItem* item = static_cast<QStandardItemModel*>(_comboBox->model())->item(_comboBox->count()-1);
                    item->setIcon(warningIcon);
                }
                else if(_comboBox->count() != 0) {
                    _comboBox->addItem({}, tr("<Please select a property>"));
                }
                selIndex = _comboBox->count() - 1;
            }
            if(_comboBox->count() == 0) {
                _comboBox->addItem(PropertyReference(), tr("<No available properties>"));
                QStandardItem* item = static_cast<QStandardItemModel*>(_comboBox->model())->item(0);
                item->setIcon(warningIcon);
                selIndex = 0;
            }
            _comboBox->setCurrentIndex(selIndex);
        }
        else {
            if(_comboBox->count() == 0) {
                for(int typeId : containerClass()->standardPropertyIds())
                    _comboBox->addItem(PropertyReference(containerClass(), typeId));
            }
            _comboBox->setCurrentProperty(pref);
        }
    }
    else if(comboBox()) {
        comboBox()->clear();
    }
}

/******************************************************************************
* Populates the combox box with items.
******************************************************************************/
void PropertyReferenceParameterUI::addItemsToComboBox(const PipelineFlowState& state)
{
    OVITO_ASSERT(containerRef());
    if(const PropertyContainer* container = state ? state.getLeafObject(containerRef()) : nullptr) {
        addItemsToComboBox(container);
    }
}

/******************************************************************************
* Populates the combox box with items.
******************************************************************************/
void PropertyReferenceParameterUI::addItemsToComboBox(const PropertyContainer* container)
{
    if(!_nullPropertyItem.isEmpty())
        _comboBox->addItem(PropertyReference{}, _nullPropertyItem);

    for(const Property* property : container->properties()) {

        // The client can apply a filter to the displayed property list.
        if(_propertyFilter && !_propertyFilter(property))
            continue;

        // Properties with a non-numeric data type cannot be used as source properties.
        if(property->dataType() != Property::Int8 && property->dataType() != Property::Int32 && property->dataType() != Property::Int64 && property->dataType() != Property::Float32 && property->dataType() != Property::Float64)
            continue;

        if(_componentsMode != ShowOnlyComponents || (property->componentCount() <= 1 && property->componentNames().empty())) {
            // Property without component:
            _comboBox->addItem(property);
        }
        if(_componentsMode != ShowNoComponents && property->componentCount() > 1) {
            // Components of vector property:
            bool isChildItem = (_componentsMode == ShowComponentsAndVectorProperties);
            for(int vectorComponent = 0; vectorComponent < (int)property->componentCount(); vectorComponent++) {
                _comboBox->addItem(property, vectorComponent, isChildItem);
            }
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void PropertyReferenceParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void PropertyReferenceParameterUI::updatePropertyValue()
{
    if(comboBox() && editObject() && comboBox()->currentText().isEmpty() == false) {
        performTransaction(tr("Change parameter"), [this]() {
            PropertyReference pref = _comboBox->currentProperty();
            if(isPropertyFieldUI()) {

                // Check if new value differs from old value.
                QVariant oldval = editObject()->getPropertyFieldValue(propertyField());
                if(pref == oldval.value<PropertyReference>())
                    return;

                editObject()->setPropertyFieldValue(propertyField(), QVariant::fromValue(pref));
            }
            else return;

            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
