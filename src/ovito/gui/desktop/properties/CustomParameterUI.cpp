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
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/core/app/undo/UndoableOperation.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(CustomParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void CustomParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, QWidget* widget,
        std::function<void(const QVariant&)>&& updateWidgetFunction,
        std::function<QVariant()>&& updatePropertyFunction,
        std::function<void(RefTarget*)>&& resetUIFunction)
{
    PropertyParameterUI::initializeObject(parentEditor, propField);

    _widget = widget;
    _updateWidgetFunction = std::move(updateWidgetFunction);
    _updatePropertyFunction = std::move(updatePropertyFunction);
    _resetUIFunction = std::move(resetUIFunction);
}

/******************************************************************************
* Destructor.
******************************************************************************/
CustomParameterUI::~CustomParameterUI()
{
    // Release widget.
    delete widget();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void CustomParameterUI::resetUI()
{
    if(widget()) {
        widget()->setEnabled(editObject() != NULL && isEnabled());
        if(_resetUIFunction)
            _resetUIFunction(editObject());
    }

    PropertyParameterUI::resetUI();
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void CustomParameterUI::updateUI()
{
    PropertyParameterUI::updateUI();

    if(widget() && editObject()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid());
        }
        else if(isReferenceFieldUI() && !propertyField()->isVector()) {
            val = QVariant::fromValue(editObject()->getReferenceFieldTarget(propertyField()));
            OVITO_ASSERT(val.isValid());
        }
        else return;

        _updateWidgetFunction(val);
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void CustomParameterUI::setEnabled(bool enabled)
{
    if(enabled == isEnabled()) return;
    PropertyParameterUI::setEnabled(enabled);
    if(widget())
        widget()->setEnabled(editObject() != NULL && isEnabled());
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void CustomParameterUI::updatePropertyValue()
{
    if(widget() && editObject()) {
        performTransaction(tr("Change parameter"), [this]() {
            QVariant newValue = _updatePropertyFunction();
            if(isPropertyFieldUI()) {
                editObject()->setPropertyFieldValue(propertyField(), newValue);
            }
            else if(isReferenceFieldUI() && !propertyField()->isVector()) {
                OORef<RefTarget> target = newValue.value<RefTarget*>();
                editObject()->setReferenceFieldTarget(propertyField(), std::move(target));
            }

            Q_EMIT valueEntered();
        });
    }
}

}   // End of namespace
