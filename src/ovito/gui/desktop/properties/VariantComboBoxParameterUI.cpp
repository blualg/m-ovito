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
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(VariantComboBoxParameterUI);

/******************************************************************************
* Constructor for a PropertyField property.
******************************************************************************/
VariantComboBoxParameterUI::VariantComboBoxParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField) :
    PropertyParameterUI(parentEditor, propField), _comboBox(new QComboBox())
{
    connect(comboBox(), qOverload<int>(&QComboBox::activated), this, &VariantComboBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
VariantComboBoxParameterUI::~VariantComboBoxParameterUI()
{
    // Release GUI controls.
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void VariantComboBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void VariantComboBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox() && editObject()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        else return;
        comboBox()->setCurrentIndex(comboBox()->findData(val));
        if(comboBox()->isEditable())
            comboBox()->setEditText(val.toString());
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void VariantComboBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox()) comboBox()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void VariantComboBoxParameterUI::updatePropertyValue()
{
    if(comboBox() && editObject() && comboBox()->currentIndex() >= 0) {
        performTransaction(tr("Change parameter"), [this]() {
            QVariant newValue;
            if(comboBox()->isEditable())
                newValue = comboBox()->currentText();
            else
                newValue = comboBox()->itemData(comboBox()->currentIndex());

            if(isPropertyFieldUI()) {
                editor()->changePropertyFieldValue(propertyField(), newValue);
            }

            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
