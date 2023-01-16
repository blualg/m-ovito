////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/viewport/ViewportWindowInterface.h>

namespace Ovito {

/**
 * \brief Generic base class for viewport windows.
 */
class OVITO_GUIBASE_EXPORT BaseViewportWindow : public ViewportWindowInterface
{
public:

    /// Constructor.
    BaseViewportWindow(UserInterface& userInterface, Viewport* vp) : ViewportWindowInterface(userInterface, vp) {}

    /// Returns the input manager handling mouse events of the viewport (if any).
    ViewportInputManager* inputManager() const;

    /// Returns the list of gizmos to render in the viewport.
    virtual span<ViewportGizmo*> viewportGizmos() override;

    /// Renders custom GUI elements in the viewport on top of the scene.
    virtual void renderGui(SceneRenderer* renderer) override;

    /// Returns the QWidget that is associated with this viewport window.
    virtual QWidget* widget() { return nullptr; }

    /// Returns the zone in the upper left corner of the viewport where the context menu can be activated by the user.
    const QRectF& contextMenuArea() const { return _contextMenuArea; }

    /// Returns whether the viewport title is shown in the user interface.
    bool isViewportTitleVisible() const { return _showViewportTitle; }

    /// Sets whether the viewport title is shown in the user interface.
    void setViewportTitleVisible(bool visible) { _showViewportTitle = visible; }

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

    /// Is called when the widgets looses the input focus.
    void focusOutEvent(QFocusEvent* event);

    /// Handles key-press events.
    void keyPressEvent(QKeyEvent* event);

private:

    /// The zone in the upper left corner of the viewport where
    /// the context menu can be activated by the user.
    QRectF _contextMenuArea;

    /// Indicates that the mouse cursor is currently positioned inside the
    /// viewport area that activates the viewport context menu.
    bool _cursorInContextMenuArea = false;

    /// Controls the visibility of the viewport title in the user interface.
    bool _showViewportTitle = true;
};

}   // End of namespace
