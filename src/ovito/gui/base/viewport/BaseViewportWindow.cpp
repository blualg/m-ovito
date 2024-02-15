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
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "BaseViewportWindow.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(BaseViewportWindow);

/******************************************************************************
* Returns the input manager handling mouse events of the viewport (if any).
******************************************************************************/
ViewportInputManager* BaseViewportWindow::inputManager() const
{
    return userInterface().viewportInputManager();
}

/******************************************************************************
* Returns the list of gizmos to render in the viewport.
******************************************************************************/
std::span<ViewportGizmo*> BaseViewportWindow::viewportGizmos()
{
    if(ViewportInputManager* man = inputManager())
        return std::span<ViewportGizmo*>{const_cast<ViewportGizmo**>(man->viewportGizmos().data()), man->viewportGizmos().size()};
    else
        return {};
}

/******************************************************************************
* Handles show events.
******************************************************************************/
void BaseViewportWindow::showEvent(QShowEvent* event)
{
    // Schedule a rendering pass if the window becomes visible and an update request has been scheduled while it was hidden.
    if(!event->spontaneous() && !userInterface().areViewportUpdatesSuspended())
        resumeViewportUpdates();
}

/******************************************************************************
* Handles hide events.
******************************************************************************/
void BaseViewportWindow::hideEvent(QHideEvent* event)
{
    // Release all renderer resources when the window becomes hidden.
    releaseResources();
}

/******************************************************************************
* Handles double click events.
******************************************************************************/
void BaseViewportWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                mode->mouseDoubleClickEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void BaseViewportWindow::mousePressEvent(QMouseEvent* event)
{
    if(!inputManager())
        return;

    // Make this viewport the active one.
    if(DataSet* dataset = userInterface().datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig()) {
            inputManager()->userInterface().handleExceptions([&] {
                viewportConfig->setActiveViewport(viewport());
            });
        }
    }

    // Intercept mouse clicks on the viewport caption.
    if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event))) {
        inputManager()->requestContextMenu(this, event->pos());
        return;
    }

    if(ViewportInputMode* mode = inputManager()->activeMode()) {
        inputManager()->userInterface().handleExceptions([&] {
            mode->mousePressEvent(this, event);
        });
    }
}

/******************************************************************************
* Handles mouse release events.
******************************************************************************/
void BaseViewportWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                mode->mouseReleaseEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void BaseViewportWindow::mouseMoveEvent(QMouseEvent* event)
{
    if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event)) && event->buttons() == Qt::NoButton) {
        setCursorInContextMenuArea(true);
    }
    else if(!contextMenuArea().contains(ViewportInputMode::getMousePosition(event))) {
        setCursorInContextMenuArea(false);
    }

    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                mode->mouseMoveEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Handles mouse wheel events.
******************************************************************************/
void BaseViewportWindow::wheelEvent(QWheelEvent* event)
{
    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                mode->wheelEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Is called when the mouse cursor leaves the widget.
******************************************************************************/
void BaseViewportWindow::leaveEvent(QEvent* event)
{
    setCursorInContextMenuArea(false);
    userInterface().clearStatusBarMessage();
}

/******************************************************************************
* Is called when the widget looses the input focus.
******************************************************************************/
void BaseViewportWindow::focusOutEvent(QFocusEvent* event)
{
    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                mode->focusOutEvent(this, event);
            });
        }
    }
}

/******************************************************************************
* Is called when the widget is resized.
******************************************************************************/
void BaseViewportWindow::resizeEvent(QResizeEvent* event)
{
    requestUpdate();
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void BaseViewportWindow::keyPressEvent(QKeyEvent* event)
{
    if(inputManager()) {
        if(ViewportInputMode* mode = inputManager()->activeMode()) {
            inputManager()->userInterface().handleExceptions([&] {
                if(mode->keyPressEvent(this, event))
                    return; // Do not propagate handled key events to base class.
            });
        }
    }
}

}   // End of namespace
