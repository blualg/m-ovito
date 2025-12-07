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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanActionParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanActionParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void BooleanActionParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, QAction* action)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _action = action;

    OVITO_ASSERT(isPropertyFieldUI());
    OVITO_ASSERT(action != nullptr);
    action->setCheckable(true);
    connect(action, &QAction::triggered, this, &BooleanActionParameterUI::updatePropertyValue);
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanActionParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(action())
        action()->setEnabled(editObject() && isEnabled() && !editor()->isReadOnly());
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanActionParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(action() && editObject()) {
        QVariant val(false);
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        action()->setChecked(val.toBool());
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanActionParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(action())
        action()->setEnabled(editObject() != nullptr && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanActionParameterUI::updatePropertyValue()
{
    if(action() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), action()->isChecked());
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
