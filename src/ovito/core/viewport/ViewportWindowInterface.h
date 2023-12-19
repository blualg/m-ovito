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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/dataset/scene/ScenePreparation.h>

namespace Ovito {

/**
 * \brief Abstract interface for viewport windows, which provide the connection between the
 *        non-visual Viewport class and the GUI layer.
 */
class OVITO_CORE_EXPORT ViewportWindowInterface
{
public:

    /// Constructor which associates this window with the given viewport instance.
    ViewportWindowInterface(UserInterface& userInterface, Viewport* vp);

    /// Destructor.
    ~ViewportWindowInterface();

    /// Associates this window with a different viewport.
    void setViewport(Viewport* vp);

    /// Returns the viewport associated with this window.
    Viewport* viewport() const { return _viewport; }

    /// Returns the abstract user interface hosting this viewport window.
    UserInterface& userInterface() const { return _userInterface; }

    /// Returns the object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    ScenePreparation& scenePreparation() { return *_scenePreparation; }

    /// Puts an update request for this window in the event loop.
    virtual void renderLater() = 0;

    /// If an update request is pending for this viewport window, immediately
    /// processes it and redraw the window contents.
    virtual void processViewportUpdate() = 0;

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() = 0;

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() = 0;

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() = 0;

    /// Makes the viewport window delete itself.
    /// This method is automatically called by the Viewport class destructor.
    virtual void destroyViewportWindow();

    /// Returns the interactive scene renderer used by the viewport window to render the graphics.
    virtual SceneRenderer* sceneRenderer() const { return nullptr; }

    /// Renders custom GUI elements in the viewport on top of the scene.
    virtual void renderGui(SceneRenderer* renderer) = 0;

    /// Determines the object that is located under the given mouse cursor position.
    virtual ViewportPickResult pick(const QPointF& pos) = 0;

    /// Makes the OpenGL context used by the viewport window for rendering the current context.
    virtual void makeOpenGLContextCurrent() {}

    /// Returns the list of gizmos to render in the viewport.
    virtual std::span<ViewportGizmo*> viewportGizmos() = 0;

    /// Returns whether the viewport window is currently visible on screen.
    virtual bool isVisible() const = 0;

    /// If enabled, shows the given text in a tooltip window.
    virtual void showToolTip(const QString& message, const QPointF& viewportLocation) {}

    /// Hides the tooltip window previously shown by showToolTip().
    virtual void hideToolTip() {}

    /// Sets the mouse cursor shape for the window.
    virtual void setCursor(const QCursor& cursor) {}

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() = 0;

public:

    /// Registry for viewport window implementations.
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    using Registry = QVarLengthArray<const QMetaObject*, 3>;
#else
    using Registry = QVarLengthArray<std::pair<const QMetaObject*, ViewportWindowInterface* (*)(Viewport*, UserInterface*, QWidget*)>, 3>;
#endif

    /// Returns the global registry, which allows enumerating all installed viewport window implementations.
    static Registry& registry();

protected:

    /// Render the axis tripod symbol in the corner of the viewport that indicates
    /// the coordinate system orientation.
    void renderOrientationIndicator(SceneRenderer* renderer);

    /// Renders the frame on top of the scene that indicates the visible rendering area.
    void renderRenderFrame(SceneRenderer* renderer);

    /// Renders the viewport caption text.
    QRectF renderViewportTitle(SceneRenderer* renderer, bool hoverState);

private:

    /// The abstract user interface hosting this viewport window.
    UserInterface& _userInterface;

    /// The viewport associated with this window.
    Viewport* _viewport;

#ifdef OVITO_DEBUG
    /// Counts how often this viewport has been rendered during the current program session.
    int _renderDebugCounter = 0;
#endif

    /// The primitive for rendering the viewport's orientation indicator.
    LinePrimitive _orientationTripodGeometry;

    /// The primitive for rendering the viewport's orientation indicator labels.
    TextPrimitive _orientationTripodLabels[3];

    /// Object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    OORef<ScenePreparation> _scenePreparation;
};

/// This macro registers a viewport window implementation in ViewportWindowInterface::registry() at compile time.
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    #define OVITO_REGISTER_VIEWPORT_WINDOW_IMPLEMENTATION(WindowClass) \
        static const int __registration##WindowClass = (Ovito::ViewportWindowInterface::registry().push_back(&WindowClass::staticMetaObject), 0);
#else
    #define OVITO_REGISTER_VIEWPORT_WINDOW_IMPLEMENTATION(WindowClass) \
        static Ovito::ViewportWindowInterface* __construct##WindowClass(Ovito::Viewport* vp, Ovito::UserInterface* ui, QWidget* parent) { return new WindowClass{vp, ui, parent}; } \
        static const int __registration##WindowClass = (Ovito::ViewportWindowInterface::registry().push_back( \
                std::make_pair(&WindowClass::staticMetaObject, &__construct##WindowClass)), 0);
#endif

}   // End of namespace
