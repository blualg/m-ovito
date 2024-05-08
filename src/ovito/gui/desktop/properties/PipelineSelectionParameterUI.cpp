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
#include <ovito/gui/desktop/widgets/general/PopupUpdateComboBox.h>
#include "PipelineSelectionParameterUI.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PipelineSelectionParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
PipelineSelectionParameterUI::PipelineSelectionParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField) :
    PropertyParameterUI(parentEditor, propField)
{
    OVITO_ASSERT(isReferenceFieldUI() && !propertyField()->isVector());

    PopupUpdateComboBox* comboBox = new PopupUpdateComboBox();
    connect(comboBox, qOverload<int>(&QComboBox::activated), this, &PipelineSelectionParameterUI::updatePropertyValue);
    connect(comboBox, &PopupUpdateComboBox::dropDownActivated, this, &PipelineSelectionParameterUI::updateUI);
    _comboBox = comboBox;
}

/******************************************************************************
* Destructor.
******************************************************************************/
PipelineSelectionParameterUI::~PipelineSelectionParameterUI()
{
    delete comboBox();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void PipelineSelectionParameterUI::resetUI()
{
    PropertyParameterUI::resetUI();

    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool PipelineSelectionParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == parameterObject() && event.type() == ReferenceEvent::TitleChanged) {
        updateUI();
    }
    return PropertyParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the
* properties owner this parameter UI belongs to.
******************************************************************************/
void PipelineSelectionParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(comboBox()) {
        comboBox()->clear();
        if(editObject()) {
            editor()->visitScenePipelines([&](Pipeline* pipeline) {
                comboBox()->addItem(pipeline->objectTitle(), QVariant::fromValue(pipeline));
                return true;
            });
        }
        if(comboBox()->count() == 0 || !parameterObject())
            comboBox()->addItem(tr("‹none›"), QVariant::fromValue<Pipeline*>(nullptr));
        comboBox()->setCurrentIndex(comboBox()->findData(QVariant::fromValue(dynamic_object_cast<Pipeline>(parameterObject()))));
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void PipelineSelectionParameterUI::updatePropertyValue()
{
    if(editObject()) {
        OVITO_ASSERT(isReferenceFieldUI() && !propertyField()->isVector());
        performTransaction(tr("Select pipeline"), [&]() {
            editObject()->setReferenceFieldTarget(propertyField(), comboBox()->currentData().value<Pipeline*>());
            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void PipelineSelectionParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    PropertyParameterUI::setEnabled(enabled);
    if(comboBox())
        comboBox()->setEnabled(editObject() && isEnabled());
}

}   // End of namespace
