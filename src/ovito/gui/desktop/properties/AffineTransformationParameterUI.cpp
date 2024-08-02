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
#include <ovito/gui/desktop/properties/AffineTransformationParameterUI.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(AffineTransformationParameterUI);

/******************************************************************************
* Constructor.
******************************************************************************/
void AffineTransformationParameterUI::initializeObject(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t row, size_t column)
{
    OVITO_ASSERT_MSG(row >= 0 && row < 3, "AffineTransformationParameterUI constructor", "The row must be in the range 0-2.");
    OVITO_ASSERT_MSG(column >= 0 && column < 4, "AffineTransformationParameterUI constructor", "The column must be in the range 0-3.");

    FloatParameterUI::initializeObject(parentEditor, propField);

    _row = row;
    _column = column;
}

/******************************************************************************
* Takes the value entered by the user and stores it in the parameter object
* this parameter UI is bound to.
******************************************************************************/
void AffineTransformationParameterUI::updatePropertyValue()
{
    if(editObject() && spinner()) {
        performTransaction(tr("Change parameter value"), [&]() {
            if(isPropertyFieldUI()) {
                QVariant currentValue = editObject()->getPropertyFieldValue(propertyField());
                if(currentValue.canConvert<AffineTransformation>()) {
                    AffineTransformation val = currentValue.value<AffineTransformation>();
                    val(_row, _column) = spinner()->floatValue();
                    currentValue.setValue(val);
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
void AffineTransformationParameterUI::updateUI()
{
    if(editObject() && spinner() && !spinner()->isDragging()) {
        QVariant val;
        if(isPropertyFieldUI()) {
            val = editObject()->getPropertyFieldValue(propertyField());
            OVITO_ASSERT(val.isValid() && (val.canConvert<AffineTransformation>()));
        }
        else return;

        if(val.canConvert<AffineTransformation>())
            spinner()->setFloatValue(val.value<AffineTransformation>()(_row, _column));
    }
}

}   // End of namespace
