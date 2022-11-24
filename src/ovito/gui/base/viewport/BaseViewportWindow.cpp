////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/DataSet.h>
#include "BaseViewportWindow.h"

namespace Ovito {

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
const std::vector<ViewportGizmo*>& BaseViewportWindow::viewportGizmos()
{
	return inputManager()->viewportGizmos();
}

/******************************************************************************
* Handles double click events.
******************************************************************************/
void BaseViewportWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				mode->mouseDoubleClickEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport mouse event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void BaseViewportWindow::mousePressEvent(QMouseEvent* event)
{
	dataset()->viewportConfig()->setActiveViewport(viewport());

	// Intercept mouse clicks on the viewport caption.
	if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event))) {
		Q_EMIT viewport()->contextMenuRequested(event->pos());
		return;
	}

	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				mode->mousePressEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport mouse event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Handles mouse release events.
******************************************************************************/
void BaseViewportWindow::mouseReleaseEvent(QMouseEvent* event)
{
	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				mode->mouseReleaseEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport mouse event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void BaseViewportWindow::mouseMoveEvent(QMouseEvent* event)
{
	if(contextMenuArea().contains(ViewportInputMode::getMousePosition(event)) && !_cursorInContextMenuArea && event->buttons() == Qt::NoButton) {
		_cursorInContextMenuArea = true;
		viewport()->updateViewport();
	}
	else if(!contextMenuArea().contains(ViewportInputMode::getMousePosition(event)) && _cursorInContextMenuArea) {
		_cursorInContextMenuArea = false;
		viewport()->updateViewport();
	}

	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				mode->mouseMoveEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport mouse event handler:";
				ex.logError();
			}
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
			try {
				mode->wheelEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport mouse event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Is called when the mouse cursor leaves the widget.
******************************************************************************/
void BaseViewportWindow::leaveEvent(QEvent* event)
{
	if(_cursorInContextMenuArea) {
		_cursorInContextMenuArea = false;
		viewport()->updateViewport();
	}
	userInterface().clearStatusBarMessage();
}

/******************************************************************************
* Is called when the widgets looses the input focus.
******************************************************************************/
void BaseViewportWindow::focusOutEvent(QFocusEvent* event)
{
	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				mode->focusOutEvent(this, event);
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void BaseViewportWindow::keyPressEvent(QKeyEvent* event)
{
	if(inputManager()) {
		if(ViewportInputMode* mode = inputManager()->activeMode()) {
			try {
				if(mode->keyPressEvent(this, event))
					return; // Do not propagate handled key events to base class.
			}
			catch(const Exception& ex) {
				qWarning() << "Uncaught exception in viewport key-press event handler:";
				ex.logError();
			}
		}
	}
}

/******************************************************************************
* Renders custom GUI elements in the viewport on top of the scene.
******************************************************************************/
void BaseViewportWindow::renderGui(SceneRenderer* renderer)
{
	if(viewport()->renderPreviewMode()) {
		// Render render frame.
		renderRenderFrame(renderer);
	}
	else {
		// Render orientation tripod.
		renderOrientationIndicator(renderer);
	}

	// Render viewport caption.
	if(isViewportTitleVisible())
		_contextMenuArea = renderViewportTitle(renderer, _cursorInContextMenuArea);
	else
		_contextMenuArea = QRectF();
}

}	// End of namespace
