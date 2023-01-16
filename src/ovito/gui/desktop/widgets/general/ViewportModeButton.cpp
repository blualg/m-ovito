////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include "ViewportModeButton.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ViewportModeButton::ViewportModeButton(ViewportModeAction* action, QWidget* parent) : QPushButton(action->icon(), action->text(), parent)
{
    setCheckable(true);
    setChecked(action->isChecked());
    setToolTip(action->toolTip());

#ifndef Q_OS_MACOS
    if(action->highlightColor().isValid())
        setStyleSheet("QPushButton:checked { background-color: " + action->highlightColor().name() + " }");
    else
        setStyleSheet("QPushButton:checked { background-color: moccasin; }");
#endif

    connect(action, &ViewportModeAction::toggled, this, &QPushButton::setChecked);
    connect(this, &QPushButton::clicked, action, &ViewportModeAction::trigger);
}

}   // End of namespace
