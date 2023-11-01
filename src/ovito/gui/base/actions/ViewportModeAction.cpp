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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include "ViewportModeAction.h"

namespace Ovito {

/******************************************************************************
* Initializes the action object.
******************************************************************************/
ViewportModeAction::ViewportModeAction(UserInterface& userInterface, const QString& text, QObject* parent, ViewportInputMode* inputMode, const QColor& highlightColor)
    : QAction(text, parent), _inputMode(inputMode), _highlightColor(highlightColor), _viewportInputManager(*userInterface.viewportInputManager())
{
    OVITO_CHECK_POINTER(inputMode);
    OVITO_CHECK_POINTER(userInterface.viewportInputManager());

    setCheckable(true);
    setChecked(inputMode->isActive());

    connect(inputMode, &ViewportInputMode::statusChanged, this, &ViewportModeAction::setChecked);
    connect(this, &ViewportModeAction::toggled, this, &ViewportModeAction::onActionToggled);
    connect(this, &ViewportModeAction::triggered, this, &ViewportModeAction::onActionTriggered);
}

/******************************************************************************
* Is called when the user or the program have triggered the action's state.
******************************************************************************/
void ViewportModeAction::onActionToggled(bool checked)
{
    // Activate/deactivate the input mode.
    if(checked && !_inputMode->isActive()) {
        _viewportInputManager.pushInputMode(_inputMode);
        // Give viewport windows the input focus.
        _viewportInputManager.userInterface().setViewportInputFocus();
    }
    else if(!checked) {
        if(_viewportInputManager.activeMode() == _inputMode && _inputMode->modeType() == ViewportInputMode::ExclusiveMode) {
            // Make sure that an exclusive input mode cannot be deactivated by the user.
            setChecked(true);
        }
    }
}

/******************************************************************************
* Is called when the user has triggered the action's state.
******************************************************************************/
void ViewportModeAction::onActionTriggered(bool checked)
{
    if(!checked) {
        if(_inputMode->modeType() != ViewportInputMode::ExclusiveMode) {
            _viewportInputManager.removeInputMode(_inputMode);
        }
    }
}

}   // End of namespace
