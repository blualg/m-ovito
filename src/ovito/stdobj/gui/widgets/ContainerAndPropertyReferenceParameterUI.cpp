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
#include <ovito/stdobj/properties/ContainerAndPropertyReference.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include "ContainerAndPropertyReferenceParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ContainerAndPropertyReferenceParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
ContainerAndPropertyReferenceParameterUI::ContainerAndPropertyReferenceParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, PropertyComponentsMode componentsMode) :
    PropertyParameterUI(parentEditor, propField),
    _containerComboBox(new QComboBox()),
    _propertyComboBox(new QComboBox()),
    _componentsMode(componentsMode)
{
    connect(containerComboBox(), &QComboBox::textActivated, this, &ContainerAndPropertyReferenceParameterUI::containerSelected);
    connect(propertyComboBox(), &QComboBox::textActivated, this, &ContainerAndPropertyReferenceParameterUI::propertySelected);

    // Update the lists whenever the pipeline input changes.
    connect(parentEditor, &PropertiesEditor::pipelineInputChanged, this, &ContainerAndPropertyReferenceParameterUI::updateUI);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ContainerAndPropertyReferenceParameterUI::~ContainerAndPropertyReferenceParameterUI()
{
    delete containerComboBox();
    delete propertyComboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void ContainerAndPropertyReferenceParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(containerComboBox())
        containerComboBox()->setEnabled(editObject() && isEnabled());
    if(propertyComboBox())
        propertyComboBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void ContainerAndPropertyReferenceParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(containerComboBox()) {
        containerComboBox()->clear();
        if(editObject()) {
            // Get the current object reference.
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid() && val.canConvert<ContainerAndPropertyReference>());
            const ContainerAndPropertyReference selectedReference = val.value<ContainerAndPropertyReference>();
            const PropertyContainerReference& selectedObjectReference = selectedReference.container();

            // Update list of property containers available in the pipeline's output.
            int selectedIndex = -1;
            bool currentObjectFilteredOut = false;
            for(const PipelineFlowState& state : editor()->getPipelineInputs()) {
                std::vector<ConstDataObjectPath> dataObjectPaths = state.getObjectsRecursive(PropertyContainer::OOClass());
                for(const ConstDataObjectPath& path : dataObjectPaths) {
                    const PropertyContainer* container = static_object_cast<PropertyContainer>(path.back());
                    PropertyContainerReference ref(path);

                    // The client can apply a custom filter function to the property container list.
                    if(_propertyFilter) {
                        // List the container only if at least of its properties passes the filter.
                        if(boost::algorithm::none_of(container->properties(), std::bind_front(_propertyFilter, container))) {
                            if(selectedObjectReference == ref)
                                currentObjectFilteredOut = true;
                            continue;
                        }
                    }

                    // Do not add the same container to the list more than once.
                    if(containerComboBox()->findData(QVariant::fromValue(ref)) != -1)
                        continue;

                    if(ref == selectedObjectReference)
                        selectedIndex = containerComboBox()->count();

                    containerComboBox()->addItem(ref.dataTitle(), QVariant::fromValue(ref));
                }
            }

            static QIcon warningIcon(QStringLiteral(":/guibase/mainwin/status/status_warning.png"));
            if(selectedIndex < 0) {
                if(selectedObjectReference) {
                    // Add a place-holder item if the selected object does not exist anymore.
                    QString title = selectedObjectReference.dataTitle();
                    if(title.isEmpty() && selectedObjectReference.dataClass())
                        title = selectedObjectReference.dataClass()->displayName();
                    if(!currentObjectFilteredOut)
                        title += tr(" (not available)");
                    containerComboBox()->addItem(title, QVariant::fromValue(selectedObjectReference));
                    QStandardItem* item = static_cast<QStandardItemModel*>(containerComboBox()->model())->item(containerComboBox()->count()-1);
                    item->setIcon(warningIcon);
                }
                else if(containerComboBox()->count() != 0) {
                    containerComboBox()->addItem(tr("‹Please select›"));
                    QStandardItem* item = static_cast<QStandardItemModel*>(containerComboBox()->model())->item(containerComboBox()->count()-1);
                    item->setIcon(warningIcon);
                }
                selectedIndex = containerComboBox()->count() - 1;
            }

            if(containerComboBox()->count() == 0) {
                containerComboBox()->addItem(tr("‹No available objects›"));
                QStandardItem* item = static_cast<QStandardItemModel*>(containerComboBox()->model())->item(0);
                item->setIcon(warningIcon);
                selectedIndex = 0;
            }

            containerComboBox()->setCurrentIndex(selectedIndex);

            // Sort list entries alphabetically.
            static_cast<QStandardItemModel*>(containerComboBox()->model())->sort(0);
        }
    }

    if(propertyComboBox()) {
        propertyComboBox()->clear();
        if(editObject()) {
            // Get the current object reference.
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid() && val.canConvert<ContainerAndPropertyReference>());
            const ContainerAndPropertyReference selectedReference = val.value<ContainerAndPropertyReference>();

            // Update list of properties available in the pipeline's output.
            if(selectedReference.container()) {
                for(const PipelineFlowState& state : editor()->getPipelineInputs()) {
                    if(const PropertyContainer* container = state.getLeafObject(selectedReference.container())) {
                        for(const Property* property : container->properties()) {
                            if(!property)
                                continue;

                            // The client can apply a filter to the displayed property list.
                            if(_propertyFilter && !_propertyFilter(container, property))
                                continue;

                            if(_componentsMode != ShowOnlyComponents || (property->componentCount() <= 1 && property->componentNames().empty())) {
                                // Property without component:
                                auto ref = QVariant::fromValue(ContainerAndPropertyReference(selectedReference.container(), property->name()));
                                if(propertyComboBox()->findData(QVariant::fromValue(ref)) == -1) {
                                    propertyComboBox()->addItem(
                                        property->name(),
                                        std::move(ref));
                                }
                            }
                            if(_componentsMode != ShowNoComponents && property->componentCount() > 1) {
                                // Components of a vector property:
                                for(size_t vectorComponent = 0; vectorComponent < property->componentCount(); vectorComponent++) {
                                    auto ref = QVariant::fromValue(ContainerAndPropertyReference(selectedReference.container(), property->nameWithComponent(vectorComponent)));
                                    if(propertyComboBox()->findData(QVariant::fromValue(ref)) == -1) {
                                        propertyComboBox()->addItem(
                                            (_componentsMode == ShowComponentsAndVectorProperties ? QStringLiteral("  ") : QString()) + property->nameWithComponent(vectorComponent),
                                            std::move(ref));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ContainerAndPropertyReferenceParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(containerComboBox())
        containerComboBox()->setEnabled(editObject() != nullptr && isEnabled());
    if(propertyComboBox())
        propertyComboBox()->setEnabled(editObject() != nullptr && isEnabled());
}

/******************************************************************************
* Is called when the user selects a different property container from the list.
******************************************************************************/
void ContainerAndPropertyReferenceParameterUI::containerSelected()
{
    if(containerComboBox() && editObject()) {
        performTransaction(tr("Select property container"), [this]() {

            PropertyContainerReference newContainerRef = containerComboBox()->currentData().value<PropertyContainerReference>();
            ContainerAndPropertyReference ref = editObject()->getPropertyFieldValue(propertyField()).value<ContainerAndPropertyReference>();
            if(newContainerRef == ref.container())
                return;

            ref.setContainer(newContainerRef);
            editObject()->setPropertyFieldValue(propertyField(), QVariant::fromValue(std::move(ref)));

            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* Is called when the user selects a different property from the list.
******************************************************************************/
void ContainerAndPropertyReferenceParameterUI::propertySelected()
{
}

}   // End of namespace
