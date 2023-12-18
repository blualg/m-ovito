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
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/units/UnitsManager.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(FloatParameterUI);

#if 0 // TODO
/******************************************************************************
* Constructor for a Qt property.
******************************************************************************/
FloatParameterUI::FloatParameterUI(PropertiesEditor* parentEditor, const char* propertyName, const QString& labelText, const QMetaObject* parameterUnitType) :
    NumericalParameterUI(parentEditor, propertyName, parameterUnitType ? parameterUnitType : &FloatParameterUnit::staticMetaObject, labelText)
{
}
#endif

/******************************************************************************
* Constructor for a PropertyField or ReferenceField property.
******************************************************************************/
FloatParameterUI::FloatParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField) :
    NumericalParameterUI(parentEditor, propField, &FloatParameterUnit::staticMetaObject)
{
}

/******************************************************************************
* Takes the value entered by the user and stores it in the property field
* this property UI is bound to.
******************************************************************************/
void FloatParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        if(isReferenceFieldUI()) {
            if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                ctrl->setFloatValue(currentAnimationTime().value_or(AnimationTime(0)), spinner()->floatValue());
            }
        }
#if 0 // TODO
        else if(isQtPropertyUI()) {
            if(!editObject()->setProperty(propertyName(), spinner()->floatValue())) {
                OVITO_ASSERT_MSG(false, "FloatParameterUI::updatePropertyValue()", qPrintable(QString("The value of property %1 of object class %2 could not be set.").arg(QString(propertyName()), editObject()->metaObject()->className())));
            }
        }
        else
#endif
        if(isPropertyFieldUI()) {
            editor()->changePropertyFieldValue(propertyField(), spinner()->floatValue());
        }
        Q_EMIT valueEntered();
    }
}

/******************************************************************************
* This method updates the displayed value of the parameter UI.
******************************************************************************/
void FloatParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        handleExceptions([&] {
            if(isReferenceFieldUI()) {
                if(Controller* ctrl = dynamic_object_cast<Controller>(parameterObject())) {
                    spinner()->setFloatValue(ctrl->getFloatValue(currentAnimationTime().value_or(AnimationTime(0))));
                }
            }
            else {
                QVariant val(0.0);
#if 0 // TODO
                if(isQtPropertyUI()) {
                    val = editObject()->property(propertyName());
                    OVITO_ASSERT_MSG(val.isValid() && val.canConvert<FloatType>(), "FloatParameterUI::updateUI()", qPrintable(QString("The object class %1 does not define a property with the name %2 that can be cast to float type.").arg(editObject()->metaObject()->className(), QString(propertyName()))));
                    if(!val.isValid() || !val.canConvert<FloatType>()) {
                        throw Exception(tr("The object class %1 does not define a property with the name %2 that can be cast to float type.").arg(editObject()->metaObject()->className(), QString(propertyName())));
                    }
                }
                else
#endif
                if(isPropertyFieldUI()) {
                    val = editObject()->getPropertyFieldValue(propertyField());
                    OVITO_ASSERT(val.isValid());
                }
                spinner()->setFloatValue(val.value<FloatType>());
            }
        });
    }
}

/******************************************************************************
* Gets the minimum value to be entered.
* This value is in native controller units.
******************************************************************************/
FloatType FloatParameterUI::minValue() const
{
    return (spinner() ? spinner()->minValue() : FLOATTYPE_MIN);
}

/******************************************************************************
* Sets the minimum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void FloatParameterUI::setMinValue(FloatType minValue)
{
    if(spinner()) spinner()->setMinValue(minValue);
}

/******************************************************************************
* Gets the maximum value to be entered.
* This value is in native controller units.
******************************************************************************/
FloatType FloatParameterUI::maxValue() const
{
    return (spinner() ? spinner()->maxValue() : FLOATTYPE_MAX);
}

/******************************************************************************
* Sets the maximum value to be entered.
* This value must be specified in native controller units.
******************************************************************************/
void FloatParameterUI::setMaxValue(FloatType maxValue)
{
    if(spinner()) spinner()->setMaxValue(maxValue);
}

}   // End of namespace
