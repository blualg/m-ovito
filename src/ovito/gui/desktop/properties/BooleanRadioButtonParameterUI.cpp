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
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanRadioButtonParameterUI);

/******************************************************************************
* Constructor for a PropertyField property.
******************************************************************************/
BooleanRadioButtonParameterUI::BooleanRadioButtonParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField) :
    PropertyParameterUI(parentEditor, propField)
{
    _buttonGroup = new QButtonGroup(this);
    connect(_buttonGroup.data(), &QButtonGroup::idClicked, this, &BooleanRadioButtonParameterUI::updatePropertyValue);

    QRadioButton* buttonNo = new QRadioButton();
    QRadioButton* buttonYes = new QRadioButton();
    _buttonGroup->addButton(buttonNo, 0);
    _buttonGroup->addButton(buttonYes, 1);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanRadioButtonParameterUI::~BooleanRadioButtonParameterUI()
{
    // Release GUI controls.
    delete buttonTrue();
    delete buttonFalse();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanRadioButtonParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons())
            button->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanRadioButtonParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(buttonGroup() && editObject()) {
        QVariant val;
        if(propertyField()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        bool state = val.toBool();
        if(state && buttonTrue())
            buttonTrue()->setChecked(true);
        else if(!state && buttonFalse())
            buttonFalse()->setChecked(true);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanRadioButtonParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(buttonGroup()) {
        for(QAbstractButton* button : buttonGroup()->buttons())
            button->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanRadioButtonParameterUI::updatePropertyValue()
{
    if(buttonGroup() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            int id = buttonGroup()->checkedId();
            if(id != -1) {
                QVariant oldval;
                if(propertyField()) {
                    oldval = editObject()->getPropertyFieldValue(propertyField());
                }
                if((bool)id != oldval.toBool()) {
                    if(propertyField()) {
                        editor()->changePropertyFieldValue(propertyField(), (bool)id);
                    }
                    Q_EMIT valueEntered();
                }
            }
        });
    }
}

}   // End of namespace
