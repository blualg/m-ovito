////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/opengl/OpenGLRenderer.h>
#include "OpenGLViewportWindow.h"
#include "WidgetOpenGLRenderingJob.h"
#include "PickingOpenGLRenderingJob.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpenGLViewportWindow);

/******************************************************************************
* Creates the Qt widget that is associated with this viewport window.
******************************************************************************/
QWidget* OpenGLViewportWindow::createQtWidget(QWidget* parent)
{
    /// A custom QOpenGLWidget subclass that forwards paint events to the viewport window.
    class OpenGLViewportWidget : public QOpenGLWidget
    {
    public:

        /// Constructor.
        OpenGLViewportWidget(QWidget* parent, OpenGLViewportWindow* owner) : QOpenGLWidget(parent), _owner(owner) {}

        /// Is called once before the first call to paintGL() or resizeGL().
        virtual void initializeGL() override {
            // Determine OpenGL vendor string so other parts of the code can decide
            // which OpenGL features are safe to use.
            OpenGLRenderer::determineOpenGLInfo();

            // Release any OpenGL resources before the widget's QOpenGLContext gets destroyed.
            connect(context(), &QOpenGLContext::aboutToBeDestroyed, _owner, &OpenGLViewportWindow::releaseResources);
        }

        /// Is called whenever the widget needs to be painted.
        virtual void paintGL() override {
            _owner->paint();
        }

        /// This override is here as a workaround for a Qt bug on the Windows platform (as of Qt 6.7.2).
        /// The viewport context menu does not close properly when the user clicks next to the menu into the same viewport window.
        virtual void mousePressEvent(QMouseEvent* event) override {}

    private:
        OpenGLViewportWindow* _owner;
    };

    // Create the QOpenGLWidget.
    return new OpenGLViewportWidget(parent, this);
}

/******************************************************************************
* Creates the rendering job that renders the contents of the viewport window.
******************************************************************************/
OORef<RenderingJob> OpenGLViewportWindow::createRenderingJob()
{
    // Obtain the renderer instance that provides the interactive rendering settings.
    OORef<OpenGLRenderer> renderer = dynamic_object_cast<OpenGLRenderer>(ViewportWindow::getInteractiveWindowRenderer("opengl"));
    if(!renderer)
        throw Exception(tr("Settings for OpenGL interactive viewport renderer could not be initialized."));

    // Create the window's viewport renderer implementation.
    return OORef<WidgetOpenGLRenderingJob>::create(
        glwin(),
        datasetContainer().visCache(), // Note: It's valid to use the global vis cache here, because the OpenGL renderer runs in the main thread.
        std::move(renderer));
}

/******************************************************************************
* Releases the renderer resources held by the viewport's surface and picking renderers.
******************************************************************************/
void OpenGLViewportWindow::releaseResources()
{
    // Release any OpenGL resources associated with the window's framebuffers.
    _visualRenderBuffer.reset();
    _pickingRenderBuffer.reset();

    // Release picking data.
    _objectPickingMap->reset();

    // Release frame graph.
    _frameGraph.reset();

    // Release resources of offscreen rendering job.
    _pickingRenderingJob.reset();

    // This also releases the rendering job and the OpenGL resources it holds.
    WidgetViewportWindow::releaseResources();
}

/******************************************************************************
* Renders the window contents after the frame graph has been regenerated.
******************************************************************************/
Future<void> OpenGLViewportWindow::renderFrameGraph(OORef<FrameGraph> frameGraph)
{
    // Hold on to the frame graph.
    _frameGraph = std::move(frameGraph);

    // Return immediately, because the OpenGL window performs all rendering in the paint() routine.
    return Future<void>::createImmediateEmpty();
}

/******************************************************************************
* Is called by Qt whenever the widget needs to be painted.
******************************************************************************/
void OpenGLViewportWindow::paint()
{
    // Do nothing if window has been detached from its viewport.
    if(!viewport())
        return;

    // Invalidate current picking information whenever the visible contents of the viewport change.
    _objectPickingMap->reset();

    if(!frameGraph())
        return;

    MainThreadOperation operation(ui(), MainThreadOperation::Isolated);
    try {
        // Recreate/resize render buffer for rendering into the widget if necessary.
        if(!_visualRenderBuffer || _visualRenderBuffer->size() != viewportWindowDeviceSize() || _visualRenderBuffer->framebufferObjectId() != glwin()->defaultFramebufferObject())
            _visualRenderBuffer = OORef<OpenGLRenderBuffer>::create(renderingJob(), viewportWindowDeviceSize(), glwin()->defaultFramebufferObject());
        OVITO_ASSERT(_visualRenderBuffer->size() == viewportWindowDeviceSize());

        // Render the viewport contents. This requires an active GL context.
        auto future = renderingJob()->renderFrame(frameGraph(), _visualRenderBuffer, nullptr, TaskProgress::Ignore);
        OVITO_ASSERT(future && future.isFinished() && !future.isCanceled());

        // Display issues that may have occurred during rendering to the user.
        displayRenderingIssues(QStringLiteral("OpenGL"), _visualRenderBuffer->renderingIssues());

        // Emit signal to inform listeners (e.g. SceneAnimationPlayback) that a full frame has been rendered and presented on screen.
        if(frameGraph()->isPreliminaryState() == false)
            Q_EMIT frameRenderComplete();
    }
    catch(Exception& ex) {
        QString openGLReport;
        QTextStream stream(&openGLReport, QIODevice::WriteOnly | QIODevice::Text);
        stream << "OpenGL version: " << OpenGLRenderer::openglSurfaceFormat().majorVersion() << QStringLiteral(".") << OpenGLRenderer::openglSurfaceFormat().minorVersion() << "\n";
        stream << "OpenGL profile: " << (OpenGLRenderer::openglSurfaceFormat().profile() == QSurfaceFormat::CoreProfile ? "core" : (OpenGLRenderer::openglSurfaceFormat().profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
        stream << "OpenGL vendor: " << QString::fromUtf8(OpenGLRenderer::openGLVendor()) << "\n";
        stream << "OpenGL renderer: " << QString::fromUtf8(OpenGLRenderer::openGLRenderer()) << "\n";
        stream << "OpenGL version string: " << QString::fromUtf8(OpenGLRenderer::openGLVersion()) << "\n";
        stream << "OpenGL shading language: " << QString::fromUtf8(OpenGLRenderer::openGLSLVersion()) << "\n";
        stream << "OpenGL shader programs: " << QOpenGLShaderProgram::hasOpenGLShaderPrograms() << "\n";
        ex.appendDetailMessage(openGLReport);
        releaseResources();
        Q_EMIT fatalError(ex);
    }
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> OpenGLViewportWindow::pick(const QPointF& pos)
{
    // Cannot perform picking while viewport is not visible or when updates are disabled.
    if(isVisible() && !ui().exitingDueToFatalError() && glwin()->isValid() && widget()->isEnabled()) {

        // Is the picking buffer still valid? If not, we need to render a new frame.
        if(!_objectPickingMap->isValid() && frameGraph()) {

            // Gracefully handle any exceptions that occur during rendering.
            handleExceptions([&]() {

                // Create the offscreen rendering job for object picking.
                if(!_pickingRenderingJob)
                    _pickingRenderingJob = PickingOpenGLRenderingJob::createSharedInstance(ui());

                // Recreate/resize offscreen OpenGL framebuffer.
                if(!_pickingRenderBuffer || _pickingRenderBuffer->size() != viewportWindowDeviceSize() || !_pickingRenderBuffer->framebufferObject()->isValid())
                    _pickingRenderBuffer = static_object_cast<OpenGLRenderBuffer>(pickingRenderingJob()->createOffscreenRenderBuffer(viewportWindowDeviceSize()));
                OVITO_ASSERT(_pickingRenderBuffer->size() == viewportWindowDeviceSize());
                OVITO_ASSERT(_pickingRenderBuffer->framebufferObject().has_value());

                // Render into the OpenGL framebuffer.
                _objectPickingMap->reset();
                pickingRenderingJob()->renderFrame(frameGraph(), _pickingRenderBuffer, nullptr, _objectPickingMap).waitForFinished();

                // Read out the contents of the OpenGL framebuffer.
                _objectPickingMap->acquireFramebufferContents(*_pickingRenderBuffer);
            });
        }

        // Query which object is at the given window location.
        if(_objectPickingMap->isValid() && frameGraph()) {
            const QPoint devicePixelPos = (pos * devicePixelRatio()).toPoint();
            return _objectPickingMap->pickAt(devicePixelPos, frameGraph()->projectionParams(), viewportWindowDeviceSize());
        }
    }

    return std::nullopt;
}

}   // End of namespace
