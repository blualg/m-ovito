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
#include <ovito/gui/desktop/properties/VectorParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(VectorParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void VectorParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t vectorComponentIndex, size_t vectorComponentCount)
{
    OVITO_ASSERT_MSG(vectorComponentCount >= 2 && vectorComponentCount <= 3, "VectorParameterUI constructor", "The vector component count must be in the range 3-4.");
    OVITO_ASSERT_MSG(vectorComponentIndex >= 0 && vectorComponentIndex < vectorComponentCount, "VectorParameterUI constructor", "The vector component index is out of range.");

    FloatParameterUI::initializeObject(parentEditor, propField);

    _componentIndex = vectorComponentIndex;
    _componentCount = vectorComponentCount;

    switch(vectorComponentIndex) {
        case 0: label()->setText(propField->displayName() + " (X):"); break;
        case 1: label()->setText(propField->displayName() + " (Y):"); break;
        case 2: label()->setText(propField->displayName() + " (Z):"); break;
    }
}

/******************************************************************************
* Takes the value entered by the user and stores it in the parameter object
* this parameter UI is bound to.
******************************************************************************/
void VectorParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        handleExceptions([&] {
            if(isReferenceFieldUI() && _componentCount == 3) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    Vector3 val = ctrl->getVector3Value(currentAnimationTime().value_or(AnimationTime(0)));
                    val[_componentIndex] = spinner()->floatValue();
                    ctrl->setVector3Value(currentAnimationTime().value_or(AnimationTime(0)), val);
                }
            }
            else if(isPropertyFieldUI()) {
                QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
                if(_componentCount == 3) {
                    if(currentValue.canConvert<Vector3>()) {
                        Vector3 val = currentValue.value<Vector3>();
                        val[_componentIndex] = spinner()->floatValue();
                        currentValue.setValue(val);
                    }
                    else if(currentValue.canConvert<Point3>()) {
                        Point3 val = currentValue.value<Point3>();
                        val[_componentIndex] = spinner()->floatValue();
                        currentValue.setValue(val);
                    }
                }
                else if(_componentCount == 2) {
                    if(currentValue.canConvert<Vector2>()) {
                        Vector2 val = currentValue.value<Vector2>();
                        val[_componentIndex] = spinner()->floatValue();
                        currentValue.setValue(val);
                    }
                    else if(currentValue.canConvert<Point2>()) {
                        Point2 val = currentValue.value<Point2>();
                        val[_componentIndex] = spinner()->floatValue();
                        currentValue.setValue(val);
                    }
                }
                editObject()->setPropertyFieldValue(propertyField(), currentValue);
            }

            Q_EMIT valueEntered();
        });
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void VectorParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        if(isReferenceFieldUI() && _componentCount == 3) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                spinner()->setFloatValue(ctrl->getVector3Value(currentAnimationTime().value_or(AnimationTime(0)))[_componentIndex]);
            }
        }
        else {
            QVariant val;
            if(isPropertyFieldUI()) {
                val = editObject()->getPropertyFieldValue(propertyField());
                OVITO_ASSERT(val.isValid());
            }
            else return;

            if(_componentCount == 3) {
                if(val.canConvert<Vector3>())
                    spinner()->setFloatValue(val.value<Vector3>()[_componentIndex]);
                else if(val.canConvert<Point3>())
                    spinner()->setFloatValue(val.value<Point3>()[_componentIndex]);
                else
                    OVITO_ASSERT(false);
            }
            else if(_componentCount == 2) {
                if(val.canConvert<Vector2>())
                    spinner()->setFloatValue(val.value<Vector2>()[_componentIndex]);
                else if(val.canConvert<Point2>())
                    spinner()->setFloatValue(val.value<Point2>()[_componentIndex]);
                else
                    OVITO_ASSERT(false);
            }
        }
    }
}

}   // End of namespace
