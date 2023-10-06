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


#include <ovito/gui/qml/GUI.h>
#include <ovito/gui/qml/mainwin/MouseGrabWorkaround.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>
#include <ovito/core/viewport/ViewportWindowInterface.h>

namespace Ovito {

/**
 * \brief The internal render window asosciated with the Viewport class.
 */
class OVITO_GUI_EXPORT QuickViewportWindow : public QQuickFramebufferObject, public ViewportWindowInterface
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Ovito::Viewport* viewport READ viewport NOTIFY viewportReplaced)

public:

    /// Constructor.
    QuickViewportWindow();

    /// Destructor.
    virtual ~QuickViewportWindow();

    /// Associates this window with a viewport.
    void setViewport(Viewport* vp);

    /// Returns the input manager handling mouse events of the viewport (if any).
    ViewportInputManager* inputManager() const;

    /// Returns the interactive scene renderer used by the viewport window to render the graphics.
    virtual SceneRenderer* sceneRenderer() const override { return _viewportRenderer; }

    /// Creates the renderer used to render into the FBO.
    virtual QQuickFramebufferObject::Renderer* createRenderer() const override;

    /// \brief Puts an update request for this window in the event loop.
    virtual void renderLater() override;

    /// If an update request is pending for this viewport window, immediately
    /// processes it and redraw the window contents.
    virtual void processViewportUpdate() override;

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() override {
        return QSize(width(), height()) * window()->effectiveDevicePixelRatio();
    }

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() override {
        return QSize(width(), height());
    }

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() override {
        return window()->effectiveDevicePixelRatio();
    }

    /// Lets the viewport window delete itself.
    /// This is called by the Viewport class destructor.
    virtual void destroyViewportWindow() override {
        deleteLater();
        ViewportWindowInterface::destroyViewportWindow();
    }

    /// Renders custom GUI elements in the viewport on top of the scene.
    virtual void renderGui(SceneRenderer* renderer) override;

    /// Makes the OpenGL context used by the viewport window for rendering the current context.
    virtual void makeOpenGLContextCurrent() override;

    /// \brief Determines the object that is visible under the given mouse cursor position.
    virtual ViewportPickResult pick(const QPointF& pos) override;

    /// Returns the renderer generating an offscreen image of the scene used for object picking.
    PickingOpenGLSceneRenderer* pickingRenderer() const { return _pickingRenderer; }

    /// \brief Displays the context menu for the viewport.
    /// \param pos The position in where the context menu should be displayed.
    void showViewportMenu(const QPoint& pos = QPoint(0,0));

    /// Returns the list of gizmos to render in the viewport.
    virtual const std::vector<ViewportGizmo*>& viewportGizmos() override;

    /// Returns whether the viewport window is currently visible on screen.
    virtual bool isVisible() const override { return QQuickFramebufferObject::isVisible(); }

Q_SIGNALS:

    /// This signal is emitted whenever a new Viewport is associated with this window.
    void viewportReplaced(Viewport* viewport);

    /// This signal is emitted when an error state is detected in the viewport window.
    void viewportError(const QString& message);

private:

    class Renderer : public QQuickFramebufferObject::Renderer
    {
    public:

        /// Constructor.
        explicit Renderer(QuickViewportWindow* vpwin) : _vpwin(vpwin) {}

        /// Destructor.
        ~Renderer() {
            _vpwin->releaseRenderingResources();
        }

        virtual QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
            QOpenGLFramebufferObjectFormat format;
            format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            return new QOpenGLFramebufferObject(size, format);
        }

        /// Renders into the FBO.
        virtual void render() override {
            _vpwin->renderViewport();
        }

    private:
        /// Pointer to the viewport window to which this renderer belongs.
        QuickViewportWindow* _vpwin;
    };

    /// Releases the renderer resources held by the viewport's surface and picking renderers.
    void releaseRenderingResources();

    /// Renders the contents of the viewport window.
    void renderViewport();

protected:

    /// Handles double click events.
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override;

    /// Handles mouse press events.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Handles mouse release events.
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    /// Handles mouse move events.
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    /// Handles mouse wheel events.
    virtual void wheelEvent(QWheelEvent* event) override;

    /// Handles hover move events.
    virtual void hoverMoveEvent(QHoverEvent* event) override;

private:

    /// A flag that indicates that a viewport update has been requested.
    bool _updateRequested = false;

    /// This is the renderer of the interactive viewport.
    OORef<OpenGLSceneRenderer> _viewportRenderer;

    /// This renderer generates an offscreen rendering of the scene that allows picking of objects.
    OORef<PickingOpenGLSceneRenderer> _pickingRenderer;

    /// A helper object used on the Wasm platform to work around a mouse grabbing issue.
    MouseGrabWorkaround _mouseGrabWorkaround;
};

}   // End of namespace

QML_DECLARE_TYPE(Ovito::QuickViewportWindow);
QML_DECLARE_TYPE(Ovito::Viewport);
