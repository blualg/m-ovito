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
#include <ovito/gui/desktop/properties/Vector3ParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(Vector3ParameterUI);

/******************************************************************************
* Constructor for a PropertyField property.
******************************************************************************/
Vector3ParameterUI::Vector3ParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t vectorComponent)
    : FloatParameterUI(parentEditor, propField), _component(vectorComponent)
{
    OVITO_ASSERT_MSG(vectorComponent >= 0 && vectorComponent < 3, "Vector3ParameterUI constructor", "The vector component must be in the range 0-2.");

    switch(_component) {
        case 0: label()->setText(propField->displayName() + " (X):"); break;
        case 1: label()->setText(propField->displayName() + " (Y):"); break;
        case 2: label()->setText(propField->displayName() + " (Z):"); break;
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the parameter object
* this parameter UI is bound to.
******************************************************************************/
void Vector3ParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        handleExceptions([&] {
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    Vector3 val = ctrl->getVector3Value(currentAnimationTime().value_or(AnimationTime(0)));
                    val[_component] = spinner()->floatValue();
                    ctrl->setVector3Value(currentAnimationTime().value_or(AnimationTime(0)), val);
                }
            }
            else if(isPropertyFieldUI()) {
                QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
                if(currentValue.canConvert<Vector3>()) {
                    Vector3 val = currentValue.value<Vector3>();
                    val[_component] = spinner()->floatValue();
                    currentValue.setValue(val);
                }
                else if(currentValue.canConvert<Point3>()) {
                    Point3 val = currentValue.value<Point3>();
                    val[_component] = spinner()->floatValue();
                    currentValue.setValue(val);
                }
                editor()->changePropertyFieldValue(propertyField(), currentValue);
            }

            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void Vector3ParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                spinner()->setFloatValue(ctrl->getVector3Value(currentAnimationTime().value_or(AnimationTime(0)))[_component]);
            }
        }
        else {
            QVariant val;
            if(isPropertyFieldUI()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid() && (val.canConvert<Vector3>() || val.canConvert<Point3>()));
            }
            else return;

            if(val.canConvert<Vector3>())
                spinner()->setFloatValue(val.value<Vector3>()[_component]);
            else if(val.canConvert<Point3>())
                spinner()->setFloatValue(val.value<Point3>()[_component]);
        }
    }
}

}   // End of namespace
