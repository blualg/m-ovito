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

#include <ovito/gui/qml/GUI.h>
#include <ovito/gui/qml/mainwin/MainWindow.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/app/Application.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include "QuickViewportWindow.h"

#include <QQuickOpenGLUtils>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
QuickViewportWindow::QuickViewportWindow() : ViewportWindowInterface(nullptr, nullptr)
{
    // Show the FBO contents upside down.
    setMirrorVertically(true);

    // Determine OpenGL vendor string so other parts of the code can decide
    // which OpenGL features are safe to use.
    OpenGLSceneRenderer::determineOpenGLInfo();

    // Receive mouse input events.
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
}

/******************************************************************************
* Destructor.
******************************************************************************/
QuickViewportWindow::~QuickViewportWindow()
{
    releaseRenderingResources();
}

/******************************************************************************
* Associates this window with a viewport.
******************************************************************************/
void QuickViewportWindow::setViewport(Viewport* vp)
{
    releaseRenderingResources();
    ViewportWindowInterface::setViewport(vp);

    // Create the viewport renderer.
    _viewportRenderer = new OpenGLSceneRenderer(viewport()->dataset());
    _viewportRenderer->setInteractive(true);

    // Create the object picking renderer.
    _pickingRenderer = new PickingOpenGLSceneRenderer(viewport()->dataset());
    _pickingRenderer->setInteractive(true);

    Q_EMIT viewportReplaced(viewport());
}

/******************************************************************************
* Releases the renderer resources held by the viewport's surface and picking renderers.
******************************************************************************/
void QuickViewportWindow::releaseRenderingResources()
{
    // Release any OpenGL resources held by the viewport renderers.
    if(_viewportRenderer && _viewportRenderer->currentResourceFrame()) {
        OpenGLResourceManager::instance()->releaseResourceFrame(_viewportRenderer->currentResourceFrame());
        _viewportRenderer->setCurrentResourceFrame(0);
    }
    if(_pickingRenderer && _pickingRenderer->currentResourceFrame()) {
        OpenGLResourceManager::instance()->releaseResourceFrame(_pickingRenderer->currentResourceFrame());
        _pickingRenderer->setCurrentResourceFrame(0);
    }
}

/******************************************************************************
* Returns the input manager handling mouse events of the viewport (if any).
******************************************************************************/
ViewportInputManager* QuickViewportWindow::inputManager() const
{
    return mainWindow() ? mainWindow()->viewportInputManager() : nullptr;
}

/******************************************************************************
* Create the renderer used to render into the FBO.
******************************************************************************/
QQuickFramebufferObject::Renderer* QuickViewportWindow::createRenderer() const
{
    return new Renderer(const_cast<QuickViewportWindow*>(this));
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void QuickViewportWindow::renderLater()
{
    _updateRequested = true;
    update();
}

/******************************************************************************
* If an update request is pending for this viewport window, immediately
* processes it and redraw the window contents.
******************************************************************************/
void QuickViewportWindow::processViewportUpdate()
{
    if(_updateRequested) {
        OVITO_ASSERT_MSG(!viewport()->isRendering(), "QuickViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");
        OVITO_ASSERT_MSG(!viewport()->dataset()->viewportConfig()->isRendering(), "QuickViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");
//      repaint();
    }
}

/******************************************************************************
* Handles double click events.
******************************************************************************/
void QuickViewportWindow::mouseDoubleClickEvent(QMouseEvent* event)
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
void QuickViewportWindow::mousePressEvent(QMouseEvent* event)
{
    viewport()->dataset()->viewportConfig()->setActiveViewport(viewport());

    if(event->buttons() != Qt::NoButton && !_mouseGrabWorkaround.isActive()) {
        if(!_mouseGrabWorkaround.container()) {
            QQuickItem* container = static_cast<MainWindow*>(mainWindow());
            while(QQuickItem* parent = container->parentItem())
                container = parent;
            _mouseGrabWorkaround.setContainer(container);
        }
        _mouseGrabWorkaround.setActive(true, this);
        setKeepMouseGrab(true);
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
void QuickViewportWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->buttons() == Qt::NoButton && _mouseGrabWorkaround.isActive()) {
        _mouseGrabWorkaround.setActive(false, this);
        setKeepMouseGrab(false);
    }

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
void QuickViewportWindow::mouseMoveEvent(QMouseEvent* event)
{
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
* Handles hover move events.
******************************************************************************/
void QuickViewportWindow::hoverMoveEvent(QHoverEvent* event)
{
    if(event->oldPosF() != event->position()) {
        QMouseEvent mouseEvent(QEvent::MouseMove, event->position(), Qt::NoButton, Qt::NoButton, event->modifiers());
        mouseMoveEvent(&mouseEvent);
    }
}

/******************************************************************************
* Handles mouse wheel events.
******************************************************************************/
void QuickViewportWindow::wheelEvent(QWheelEvent* event)
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
* Returns the list of gizmos to render in the viewport.
******************************************************************************/
const std::vector<ViewportGizmo*>& QuickViewportWindow::viewportGizmos()
{
    return inputManager()->viewportGizmos();
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
ViewportPickResult QuickViewportWindow::pick(const QPointF& pos)
{
    ViewportPickResult result;

    // Cannot perform picking while viewport is not visible or currently rendering or when updates are disabled.
    if(isVisible() && !viewport()->isRendering() && !viewport()->dataset()->viewportConfig()->isSuspended() && pickingRenderer()) {
        OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = 0;
        try {
            if(pickingRenderer()->isRefreshRequired()) {
                // Request a new frame from the resource manager for this render pass.
                previousResourceFrame = pickingRenderer()->currentResourceFrame();
                pickingRenderer()->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());

                // Let the viewport do the actual rendering work.
                viewport()->renderInteractive(pickingRenderer());
            }

            // Query which object is located at the given window position.
            const QPoint pixelPos = (pos * devicePixelRatio()).toPoint();
            const PickingOpenGLSceneRenderer::ObjectRecord* objInfo;
            quint32 subobjectId;
            std::tie(objInfo, subobjectId) = pickingRenderer()->objectAtLocation(pixelPos);
            if(objInfo) {
                result.setPipelineNode(objInfo->objectNode);
                result.setPickInfo(objInfo->pickInfo);
                result.setHitLocation(pickingRenderer()->worldPositionFromLocation(pixelPos));
                result.setSubobjectId(subobjectId);
            }
        }
        catch(const Exception& ex) {
            ex.reportError();
        }

        // Release the resources created by the OpenGL renderer during the last render pass before the current pass.
        if(previousResourceFrame)
            OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
    }

    return result;
}

/******************************************************************************
* Makes the OpenGL context used by the viewport window for rendering the current context.
******************************************************************************/
void QuickViewportWindow::makeOpenGLContextCurrent()
{
    OVITO_ASSERT(window()->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL);
}

/******************************************************************************
* Renders the contents of the viewport window.
******************************************************************************/
void QuickViewportWindow::renderViewport()
{
    _updateRequested = false;

    // Do not re-enter rendering function of the same viewport.
    if(!viewport() || viewport()->isRendering())
        return;

    // Invalidate picking buffer every time the visible contents of the viewport change.
    _pickingRenderer->reset();

    // Don't render anything if viewport updates are currently suspended.
    if(viewport()->dataset()->viewportConfig()->isSuspended())
        return;

#ifdef Q_OS_WASM
    // Verify that the EXT_frag_depth OpenGL ES 2.0 extension is available.
    static bool hasCheckedFragDepthExtension = false;
    if(!hasCheckedFragDepthExtension) {
        hasCheckedFragDepthExtension = true;
        if(QOpenGLContext::currentContext()->hasExtension("EXT_frag_depth") == false)
            Q_EMIT viewportError(tr("WARNING: WebGL extension 'EXT_frag_depth' is not supported by your browser.\nWithout this capability, visual artifacts are expected."));
    }
#endif

    // Request a new frame from the resource manager for this render pass.
    OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = _viewportRenderer->currentResourceFrame();
    _viewportRenderer->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());

    try {
        // Let the Viewport class do the actual rendering work.
        viewport()->renderInteractive(_viewportRenderer);
    }
    catch(Exception& ex) {
        if(ex.context() == nullptr) ex.setContext(viewport()->dataset());
        ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewport contents. The program will quit now."));
        viewport()->dataset()->viewportConfig()->suspendViewportUpdates();
        Q_EMIT viewportError(ex.messages().join(QChar('\n')));
        ex.reportError();
    }

    // Release the resources created by the OpenGL renderer during the last render pass before the current pass.
    if(previousResourceFrame) {
        OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
    }

    // Reset the OpenGL context back to its default state expected by Qt Quick.
    QQuickOpenGLUtils::resetOpenGLState();
}

/******************************************************************************
* Renders custom GUI elements in the viewport on top of the scene.
******************************************************************************/
void QuickViewportWindow::renderGui(SceneRenderer* renderer)
{
    if(viewport()->renderPreviewMode()) {
        // Render render frame.
        renderRenderFrame(renderer);
    }
    else {
        // Render orientation tripod.
        renderOrientationIndicator(renderer);
    }
}

}   // End of namespace
