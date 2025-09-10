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
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/animation/controller/Controller.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BooleanParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void BooleanParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    // Create UI widget.
    _checkBox = new QCheckBox(propField->displayName());
    connect(_checkBox.data(), &QCheckBox::clicked, this, &BooleanParameterUI::updatePropertyValue);
}

/******************************************************************************
* Destructor.
******************************************************************************/
BooleanParameterUI::~BooleanParameterUI()
{
    // Release widget.
    delete checkBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanParameterUI::resetUI()
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
        connect(&datasetContainer(), &DataSetContainer::currentFrameChanged, this, &BooleanParameterUI::updateUI, Qt::UniqueConnection);
    }
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void BooleanParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(checkBox() && editObject()) {
        if(!isReferenceFieldUI()) {
            QVariant val(false);
            if(propertyField()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid());
            }
            checkBox()->setChecked(val.toBool());
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void BooleanParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
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
void BooleanParameterUI::updatePropertyValue()
{
    if(checkBox() && editObject()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), checkBox()->isChecked());
            }
            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
