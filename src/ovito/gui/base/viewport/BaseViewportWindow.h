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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief Generic base class for viewport windows that implements mouse input handling.
 */
class OVITO_GUIBASE_EXPORT BaseViewportWindow : public ViewportWindow
{
    Q_OBJECT
	OVITO_CLASS(BaseViewportWindow)

public:

    /// Returns the input manager handling mouse events of the viewport (if any).
    ViewportInputManager* inputManager() const;

    /// Returns the list of gizmos to render in the viewport.
    virtual std::vector<ViewportGizmo*> viewportGizmos() override;

public Q_SLOTS:

    /// Releases the renderer resources held by the viewport window and the renderer.
    virtual void releaseResources();

protected:

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    virtual void aboutToBeDeleted() override;

    /// Is called when the viewport becomes visible.
    void showEvent(QShowEvent* event);

    /// Is called when the viewport becomes hidden.
    void hideEvent(QHideEvent* event);

    /// Is called when the mouse cursor leaves the widget.
    void leaveEvent(QEvent* event);

    /// Handles double click events.
    void mouseDoubleClickEvent(QMouseEvent* event);

    /// Handles mouse press events.
    void mousePressEvent(QMouseEvent* event);

    /// Handles mouse release events.
    void mouseReleaseEvent(QMouseEvent* event);

    /// Handles mouse move events.
    void mouseMoveEvent(QMouseEvent* event);

    /// Handles mouse wheel events.
    void wheelEvent(QWheelEvent* event);

    /// Is called when the widget looses the input focus.
    void focusOutEvent(QFocusEvent* event);

    /// Is called when the widget is resized.
    void resizeEvent(QResizeEvent* event);

    /// Handles key-press events.
    void keyPressEvent(QKeyEvent* event);
};

}   // End of namespace
