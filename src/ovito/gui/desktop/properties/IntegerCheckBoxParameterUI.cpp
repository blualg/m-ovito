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
#include <ovito/gui/desktop/properties/IntegerCheckBoxParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(IntegerCheckBoxParameterUI);

/******************************************************************************
* Constructor for a PropertyField property.
******************************************************************************/
IntegerCheckBoxParameterUI::IntegerCheckBoxParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, int uncheckedValue, int checkedValue) :
    PropertyParameterUI(parentEditor, propField),
    _uncheckedValue(uncheckedValue),
    _checkedValue(checkedValue)
{
    OVITO_ASSERT(uncheckedValue != checkedValue);
    // Create UI widget.
    _checkBox = new QCheckBox(propField->displayName());
    connect(_checkBox.data(), &QCheckBox::clicked, this, &IntegerCheckBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
IntegerCheckBoxParameterUI::~IntegerCheckBoxParameterUI()
{
    // Release widget.
    delete checkBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerCheckBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled());
        else
            checkBox()->setEnabled(editObject() && isEnabled());
    }

    if(isReferenceFieldUI() && editObject()) {
        // Update the displayed value when the animation time has changed.
        connect(&mainWindow().datasetContainer(), &DataSetContainer::currentFrameChanged, this, &IntegerCheckBoxParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void IntegerCheckBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(checkBox() && editObject()) {
        int value = _uncheckedValue;
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                value = ctrl->getIntValue(currentAnimationTime().value_or(AnimationTime(0)));
            }
        }
        else if(isPropertyFieldUI()) {
            QVariant val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
            value = val.toInt();
        }
        checkBox()->setChecked(value == _checkedValue);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void IntegerCheckBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(checkBox()) {
        if(isReferenceFieldUI())
            checkBox()->setEnabled(parameterObject() && isEnabled());
        else
            checkBox()->setEnabled(editObject() && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void IntegerCheckBoxParameterUI::updatePropertyValue()
{
    if(checkBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            int value = checkBox()->isChecked() ? _checkedValue : _uncheckedValue;
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    ctrl->setIntValue(currentAnimationTime().value_or(AnimationTime(0)), value);
                    updateUI();
                }
            }
            else if(isPropertyFieldUI()) {
                editor()->changePropertyFieldValue(propertyField(), value);
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
