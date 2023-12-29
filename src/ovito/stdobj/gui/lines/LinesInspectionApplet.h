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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {

/**
 * \brief Data inspector page for lines.
 */
class OVITO_STDOBJGUI_EXPORT LinesInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(LinesInspectionApplet)
    OVITO_CLASSINFO("DisplayName", "Lines");

public:

    /// Constructor.
    explicit LinesInspectionApplet() : PropertyInspectionApplet(Lines::OOClass()) {}

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 250; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;
};

}  // namespace Ovito
