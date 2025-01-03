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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/undo/UndoableOperation.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include "ViewportInputManager.h"
#include "SelectionMode.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SelectionMode);

/******************************************************************************
* Handles the mouse down event for the given viewport.
******************************************************************************/
void SelectionMode::mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        _viewport = vpwin->viewport();
        _clickPoint = getMousePosition(event);
    }
    else if(event->button() == Qt::RightButton) {
        _viewport = nullptr;
    }
    ViewportInputMode::mousePressEvent(vpwin, event);
}

/******************************************************************************
* Handles the mouse up event for the given viewport.
******************************************************************************/
void SelectionMode::mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    if(_viewport != nullptr) {
        // Select object under mouse cursor.
        if(std::optional<ViewportWindow::PickResult> pickResult = vpwin->pick(_clickPoint)) {
            if(_viewport->scene() && pickResult->sceneNode()->scene() == _viewport->scene()) {
                inputManager()->userInterface().performTransaction(tr("Select"), [&] {
                    _viewport->scene()->selection()->setNode(pickResult->sceneNode());
                });
            }
        }
        _viewport = nullptr;
    }
    ViewportInputMode::mouseReleaseEvent(vpwin, event);
}

/******************************************************************************
* This is called by the system after the input handler is
* no longer the active handler.
******************************************************************************/
void SelectionMode::deactivated(bool temporary)
{
    inputManager()->userInterface().clearStatusBarMessage();
    _viewport = nullptr;
    ViewportInputMode::deactivated(temporary);
}

/******************************************************************************
* Handles the mouse move event for the given viewport.
******************************************************************************/
void SelectionMode::mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event)
{
    // Perform object picking under the mouse cursor.
    // Suppress object picking while animation playback is active, because the offscreen rendering slows down the playback.
    std::optional<ViewportWindow::PickResult> pickResult;
    if(!vpwin->userInterface().datasetContainer().isPlaybackActive())
        pickResult = vpwin->pick(getMousePosition(event));

    // Change mouse cursor while hovering over an object.
    setCursor(pickResult ? selectionCursor() : QCursor{});

    // Display a description of the object under the mouse cursor in the status bar and/or in a tooltip window.
    if(pickResult && pickResult->pickInfo()) {
        QString infoText = pickResult->pickInfo()->infoString(pickResult->sceneNode()->pipeline(), pickResult->subobjectId());
        inputManager()->userInterface().showStatusBarMessage(infoText);
    }
    else {
        inputManager()->userInterface().clearStatusBarMessage();
    }

    ViewportInputMode::mouseMoveEvent(vpwin, event);
}

}   // End of namespace
