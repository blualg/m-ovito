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
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/viewport/ViewportGizmo.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include "ViewportInputMode.h"

namespace Ovito {

/**
 * \brief Base class for viewport navigation modes likes zoom, pan and orbit.
 */
class OVITO_GUIBASE_EXPORT NavigationMode : public ViewportInputMode
{
    OVITO_CLASS(NavigationMode)
    Q_OBJECT

public:

    /// Constructor.
    using ViewportInputMode::ViewportInputMode;

    /// Returns the activation behavior of this input mode.
    virtual InputModeType modeType() override { return TemporaryMode; }

    /// Handles the mouse down event for the given viewport.
    virtual void mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Handles the mouse up event for the given viewport.
    virtual void mouseReleaseEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Handles the mouse move event for the given viewport.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Is called when a viewport looses the input focus.
    virtual void focusOutEvent(ViewportWindow* vpwin, QFocusEvent* event) override;

    /// Applies a step-wise change of the view orientation.
    void discreteStep(ViewportWindow* vpwin, QPointF delta);

protected:

    /// Computes the new view based on the new mouse position.
    virtual void modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep) {}

    /// This is called by the system after the input handler has become the active handler.
    virtual void activated(bool temporaryActivation) override;

    /// This is called by the system after the input handler is no longer the active handler.
    virtual void deactivated(bool temporary) override;

    /// Returns the camera object associates with the given viewport.
    static PipelineNode* getViewportCamera(Viewport* vp);

protected:

    /// Mouse position at first click.
    QPointF _startPoint;

    /// The saved camera position.
    Point3 _oldCameraPosition;

    /// The saved camera direction.
    Vector3 _oldCameraDirection;

    /// The saved camera transformation.
    AffineTransformation _oldCameraTM;

    /// The saved zoom factor.
    FloatType _oldFieldOfView;

    /// The saved world to camera transformation matrix.
    AffineTransformation _oldViewMatrix;

    /// The saved camera to world transformation matrix.
    AffineTransformation _oldInverseViewMatrix;

    /// The current viewport we are working in.
    OORef<Viewport> _viewport;

    /// Indicates whether this navigation mode is only temporarily activated.
    bool _temporaryActivation;

    /// The cached orbit center as determined when the navigation mode was activated.
    Point3 _currentOrbitCenter;

    /// Used to make changes to the viewport camera reversible.
    UndoableTransaction _undoTransaction;
};

/******************************************************************************
* The orbit viewport input mode.
******************************************************************************/
class OVITO_GUIBASE_EXPORT OrbitMode : public NavigationMode
{
    OVITO_CLASS(OrbitMode)
    Q_OBJECT

public:

    /// Constructor.
    OrbitMode() {
#ifndef Q_OS_WASM
        setCursor(QCursor(QPixmap(":/guibase/cursor/viewport/cursor_orbit.png")));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        setCursor(Qt::PointingHandCursor);
#endif
    }

protected:

    /// Computes the new view based on the new mouse position.
    virtual void modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep) override;
};

/******************************************************************************
* The pan viewport input mode.
******************************************************************************/
class OVITO_GUIBASE_EXPORT PanMode : public NavigationMode
{
    OVITO_CLASS(PanMode)
    Q_OBJECT

public:

    /// Constructor.
    PanMode() {
#ifndef Q_OS_WASM
        setCursor(QCursor(QPixmap(":/guibase/cursor/viewport/cursor_pan.png")));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        setCursor(Qt::PointingHandCursor);
#endif
    }

protected:

    /// Computes the new view based on the new mouse position.
    virtual void modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep) override;
};


/******************************************************************************
* The zoom viewport input mode.
******************************************************************************/
class OVITO_GUIBASE_EXPORT ZoomMode : public NavigationMode
{
    OVITO_CLASS(ZoomMode)
    Q_OBJECT

public:

    /// Constructor.
    ZoomMode() {
#ifndef Q_OS_WASM
        setCursor(QCursor(QPixmap(":/guibase/cursor/viewport/cursor_zoom.png")));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        setCursor(Qt::PointingHandCursor);
#endif
    }

    /// Zooms the given viewport in or out.
    void zoom(Viewport* vp, FloatType steps, UserInterface& ui);

protected:

    /// Computes the new view based on the new mouse position.
    virtual void modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep) override;

    /// Computes a scaling factor that depends on the total size of the scene which is used to
    /// control the zoom sensitivity in perspective mode.
    FloatType sceneSizeFactor(Viewport* vp);
};

/******************************************************************************
* The field of view input mode.
******************************************************************************/
class OVITO_GUIBASE_EXPORT FOVMode : public NavigationMode
{
    OVITO_CLASS(FOVMode)
    Q_OBJECT

public:

    /// Constructor.
    FOVMode() {
#ifndef Q_OS_WASM
        setCursor(QCursor(QPixmap(":/guibase/cursor/viewport/cursor_fov.png")));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        setCursor(Qt::PointingHandCursor);
#endif
    }

protected:

    /// Computes the new view based on the new mouse position.
    virtual void modifyView(ViewportWindow* vpwin, Viewport* vp, QPointF delta, bool discreteStep) override;
};

/******************************************************************************
* This input mode lets the user pick the center of rotation for the orbit mode.
******************************************************************************/
class OVITO_GUIBASE_EXPORT PickOrbitCenterMode : public ViewportInputMode, public ViewportGizmo
{
    OVITO_CLASS(PickOrbitCenterMode)
    Q_OBJECT

public:

    /// Constructor.
    PickOrbitCenterMode() {
#ifndef Q_OS_WASM
        _hoverCursor = QCursor(QPixmap(":/guibase/cursor/editing/cursor_mode_select.png"));
#else
        // WebAssembly platform does not support custom cursor shapes. Have to use one of the built-in shapes.
        setCursor(Qt::PointingHandCursor);
#endif
    }

    /// Handles the mouse click event for a Viewport.
    virtual void mousePressEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Is called when the user moves the mouse.
    virtual void mouseMoveEvent(ViewportWindow* vpwin, QMouseEvent* event) override;

    /// Sets the orbit rotation center to the space location under given mouse coordinates.
    bool pickOrbitCenter(ViewportWindow* vpwin, const QPointF& pos);

    /// Lets the input mode render its overlay content in a viewport.
    virtual void renderOverlay(Viewport* vp, ViewportWindow* vpWin, FrameGraph& frameGraph, DataSet* dataset) override;

private:

    /// Finds the intersection point between a ray originating from the current mouse cursor position and the scene.
    bool findIntersection(ViewportWindow* vpwin, const QPointF& mousePos, Point3& intersectionPoint);

    /// The mouse cursor that is shown when over an object.
    QCursor _hoverCursor;

    /// Indicates that the mouse cursor is over an object.
    bool _showCursor = false;
};

}   // End of namespace
