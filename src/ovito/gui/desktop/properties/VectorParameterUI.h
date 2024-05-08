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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include "FloatParameterUI.h"

namespace Ovito {

/******************************************************************************
* A parameter UI for Vector3 properties.
* This ParameterUI lets the user edit one of the X, Y and Z components of the vector.
******************************************************************************/
class OVITO_GUI_EXPORT VectorParameterUI : public FloatParameterUI
{
    OVITO_CLASS(VectorParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    VectorParameterUI(PropertiesEditor* parentEditor, const PropertyFieldDescriptor* propField, size_t vectorComponentIndex, size_t vectorComponentCount);

    /// This method updates the displayed value of the parameter UI.
    virtual void updateUI() override;

    /// Takes the value entered by the user and stores it in the property field
    /// this property UI is bound to.
    virtual void updatePropertyValue() override;

private:

    /// The index of the vector component to control (0 - 2).
    size_t _componentIndex;

    /// The vector component count (2 or 3)
    size_t _componentCount;
};

}   // End of namespace
