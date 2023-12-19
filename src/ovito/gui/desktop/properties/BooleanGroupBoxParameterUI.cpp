////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanGroupBoxParameterUI);

/******************************************************************************
* Constructor for a PropertyField property.
******************************************************************************/
BooleanGroupBoxParameterUI::BooleanGroupBoxParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField) :
    PropertyParameterUI(parentEditor, propField)
{
    // Create UI widget.
    _groupBox = new QGroupBox(propField->displayName());
    _groupBox->setCheckable(true);
    _childContainer = new QWidget(_groupBox);
    QVBoxLayout* layout = new QVBoxLayout(_groupBox);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    layout->addWidget(_childContainer, 1);
    connect(_groupBox.data(), &QGroupBox::clicked, this, &BooleanGroupBoxParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanGroupBoxParameterUI::~BooleanGroupBoxParameterUI()
{
    // Release GUI controls.
    delete groupBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanGroupBoxParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(groupBox()) {
        if(isReferenceFieldUI())
            groupBox()->setEnabled(parameterObject() != NULL && isEnabled());
        else
            groupBox()->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanGroupBoxParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(groupBox() && editObject()) {
        if(!isReferenceFieldUI()) {
            QVariant val(false);
            if(isPropertyFieldUI()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid());
            }
            groupBox()->setChecked(val.toBool());
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanGroupBoxParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(groupBox()) {
        if(isReferenceFieldUI())
            groupBox()->setEnabled(parameterObject() != NULL && isEnabled());
        else
            groupBox()->setEnabled(editObject() != NULL && isEnabled());
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void BooleanGroupBoxParameterUI::updatePropertyValue()
{
    if(groupBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                editor()->changePropertyFieldValue(propertyField(), groupBox()->isChecked());
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
