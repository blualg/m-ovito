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
#include <ovito/gui/base/viewport/BaseViewportWindow.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>

namespace Ovito {

/**
 * \brief The internal render window/widget used by the Viewport class.
 */
class OVITO_OPENGLRENDERERGUI_EXPORT OpenGLOffscreenViewportWindow : public QObject, public BaseViewportWindow, public UserInterface
{
    Q_OBJECT

public:

    /// Constructor.
    OpenGLOffscreenViewportWindow(Viewport* vp, const QSize& initialSize, std::function<void(QImage)> imageCallback);

    /// Destructor.
    virtual ~OpenGLOffscreenViewportWindow();

    /// Returns the interactive scene renderer used by the viewport window to render the graphics.
    virtual SceneRenderer* sceneRenderer() const override { return _viewportRenderer; }

    /// Puts an update request for this window in the event loop.
    virtual void renderLater() override;

    /// If an update request is pending for this viewport window, immediately
    /// processes it and redraw the window contents.
    virtual void processViewportUpdate() override;

    /// Returns the size of the window in device pixels.
    QSize size() const { return _framebufferObject->size(); }

    /// Changes the size of the offscreen window.
    void setSize(const QSize& size);

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() override { return size(); }

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() override {
        return size() / devicePixelRatio();
    }

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() override { return _devicePixelRatio; }

    /// Changes the device pixel ratio of the viewport window's canvas.
    void setDevicePixelRatio(qreal ratio) { _devicePixelRatio = ratio; }

    /// Sets the mouse cursor shape for the window. 
    virtual void setCursor(const QCursor& cursor) override {}

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() override { return {}; }

    /// Makes the OpenGL context used by the viewport window for rendering the current context.
    virtual void makeOpenGLContextCurrent() override { _offscreenContext.makeCurrent(_offscreenSurface); }

    /// Returns whether the viewport window is currently visible on screen.
    virtual bool isVisible() const override { return true; }

    /// Returns the renderer generating an offscreen image of the scene used for object picking.
    PickingOpenGLSceneRenderer* pickingRenderer() const { return _pickingRenderer; }

    /// Determines the object that is located under the given mouse cursor position.
    virtual ViewportPickResult pick(const QPointF& pos) override;

    /// Controls whether processViewportUpdate() causes an immediate repaint or not.
    void setImmediateViewportUpdatesEnabled(bool enabled) { _immediateViewportUpdatesEnabled = enabled; }

    /// Returns a reference to this window's input mode manager.
    ViewportInputManager& inputManager() { return _inputManager; }

protected:

    /// Handles timer events of the object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// Releases the renderer resources held by the viewport's surface and picking renderers. 
    void releaseResources();

    /// Renders the contents of the viewport window.
    void renderViewport();

private:

    /// This is the renderer of the interactive viewport.
    OORef<OpenGLSceneRenderer> _viewportRenderer;

    /// This renderer generates an offscreen rendering of the scene that allows picking of objects.
    OORef<PickingOpenGLSceneRenderer> _pickingRenderer;

    /// The offscreen surface used to render into an image buffer using OpenGL.
    QOffscreenSurface* _offscreenSurface = nullptr;

    /// The OpenGL rendering context.
    QOpenGLContext _offscreenContext;

    /// The OpenGL offscreen framebuffer.
    std::unique_ptr<QOpenGLFramebufferObject> _framebufferObject;

    /// Timer used for scheduling windows refreshs.
    QBasicTimer _repaintTimer;

    /// The callback function registered by the client which is called each time the windows renders a new image.
    std::function<void(QImage)> _imageCallback;

    /// The device pixel ratio of the rendering buffer.
    qreal _devicePixelRatio = 1.0;

    /// Controls whether processViewportUpdate() causes an immediate repaint or not.
    bool _immediateViewportUpdatesEnabled = true;

    /// Handles mouse input for the window.
    ViewportInputManager _inputManager;
};

}   // End of namespace
