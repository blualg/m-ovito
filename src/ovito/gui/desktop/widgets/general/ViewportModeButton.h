/////////////////////////////////////////////////////////////////////////////////////////
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

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * An button widget that activates a ViewportInputMode.
 */
class OVITO_GUI_EXPORT ViewportModeButton : public QPushButton
{
    Q_OBJECT

public:

    /// Constructor.
    ViewportModeButton(ViewportModeAction* action, QWidget* parent = nullptr);

protected:

    virtual void hideEvent(QHideEvent* event) override {
        // When the button becomes hidden from the user, automatically deactivate the viewport input mode.
        // This is to prevent the viewport mode from remaining active when the user switches to another command panel tab.
        if(!event->spontaneous() && isChecked())
            click();

        QPushButton::hideEvent(event);
    }
};

}   // End of namespace
