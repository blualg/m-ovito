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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include "OpenGLViewportWindow.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpenGLViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
OpenGLViewportWindow::OpenGLViewportWindow()
{
    // Create the window's viewport renderer.
    setRenderer(OORef<OpenGLSceneRenderer>::create());
}

/******************************************************************************
* Creates the UI widget that is associated with this viewport window.
******************************************************************************/
QWidget* OpenGLViewportWindow::createWidget(QWidget* parent)
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
            OpenGLSceneRenderer::determineOpenGLInfo();

            // Release OpenGL resources before the widget's QOpenGLContext gets destroyed.
            connect(context(), &QOpenGLContext::aboutToBeDestroyed, _owner, &OpenGLViewportWindow::releaseResources);
        }

        /// Is called whenever the widget needs to be painted.
        virtual void paintGL() override {
            _owner->paint();
        }

    private:
        OpenGLViewportWindow* _owner;
    };

    return new OpenGLViewportWidget(parent, this);
}

/******************************************************************************
* Releases the renderer resources held by the viewport's surface and picking renderers.
******************************************************************************/
void OpenGLViewportWindow::releaseResources()
{
    OVITO_ASSERT(openglRenderer());

    // Release any OpenGL resources held by the viewport renderer.
    if(openglRenderer() && openglRenderer()->hasCurrentResourceFrame() && widget()) {
        widget()->makeCurrent();
        openglRenderer()->setCurrentResourceFrame({});
        widget()->doneCurrent();
    }

    // Release picking buffer data.
    _pickingBuffer->reset();

    WidgetViewportWindow::releaseResources();
}

/******************************************************************************
* This is called after the frame graph has been updated to render the viewport contents on screen.
******************************************************************************/
void OpenGLViewportWindow::rerender()
{
    if(widget())
        widget()->update();
}

/******************************************************************************
* Is called whenever the widget needs to be painted.
******************************************************************************/
void OpenGLViewportWindow::paint()
{
    // Do nothing if window has been detached from its viewport or renderer.
    if(!viewport() || !openglRenderer())
        return;

    // OpenGL in a VirtualBox machine Windows guest reports "2.1 Chromium 1.9" as version string, which is
    // not correctly parsed by Qt. We have to work around this.
    QSurfaceFormat format = widget()->context()->format();
    if(OpenGLSceneRenderer::openGLVersion().startsWith("2.1 ")) {
        format.setMajorVersion(2);
        format.setMinorVersion(1);
    }

    // Invalidate current picking buffer whenever the visible contents of the viewport change.
    _pickingBuffer->reset();

    if(format.majorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MAJOR || (format.majorVersion() == OVITO_OPENGL_MINIMUM_VERSION_MAJOR && format.minorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MINOR)) {
        userInterface().exitWithFatalError(Exception(tr(
                "The OpenGL graphics driver installed on this system does not support OpenGL version %6.%7 or newer.\n\n"
                "Ovito requires modern graphics hardware and up-to-date graphics drivers to display 3D content. Your current system configuration is not compatible with Ovito and the application will quit now.\n\n"
                "To avoid this error, please install the newest graphics driver of the hardware vendor or, if necessary, consider replacing your graphics card with a newer model.\n\n"
                "The installed OpenGL graphics driver reports the following information:\n\n"
                "OpenGL vendor: %1\n"
                "OpenGL renderer: %2\n"
                "OpenGL version: %3.%4 (%5)\n\n"
                "Ovito requires at least OpenGL version %6.%7.")
                .arg(QString(OpenGLSceneRenderer::openGLVendor()))
                .arg(QString(OpenGLSceneRenderer::openGLRenderer()))
                .arg(format.majorVersion())
                .arg(format.minorVersion())
                .arg(QString(OpenGLSceneRenderer::openGLVersion()))
                .arg(OVITO_OPENGL_MINIMUM_VERSION_MAJOR)
                .arg(OVITO_OPENGL_MINIMUM_VERSION_MINOR)
            ));
        return;
    }

#ifdef Q_OS_WIN
    if(OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 2000" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 3000" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 4400") {
        userInterface().exitWithFatalError(Exception(tr(
                "The graphics chip installed in this system is not compatible with OVITO, unfortunately.\n\n"
                "Intel(R) HD Graphics, an integrated graphics chip released in the years 2010/2011/2012, does not support the specific OpenGL functions required by OVITO. "
                "There is no known workaround to make OVITO work on systems with this particular graphics unit. Please use OVITO on a computer with a more modern graphics processor.\n\n"
                "Detected graphics interface:\n\n"
                "OpenGL vendor: %1\n"
                "OpenGL renderer: %2\n"
                "OpenGL version: %3.%4 (%5)")
                .arg(QString(OpenGLSceneRenderer::openGLVendor()))
                .arg(QString(OpenGLSceneRenderer::openGLRenderer()))
                .arg(format.majorVersion())
                .arg(format.minorVersion())
                .arg(QString(OpenGLSceneRenderer::openGLVersion()))
            ));
        return;
    }
#endif

    if(!frameGraph())
        return;

    MainThreadOperation operation(ExecutionContext::Type::Interactive, userInterface());
    try {
        // Create a new OpenGL resource management frame. Keep the previous frame around until the new frame is rendered to re-use existing OpenGL resources.
        DataSetContainer& datasetContainer = userInterface().datasetContainer();
        RendererResourceCache::ResourceFrame previousResourceFrame = openglRenderer()->setCurrentResourceFrame(datasetContainer.visCache()->acquireResourceFrame());

        // Tell the OpenGL renderer to render into the widget's framebuffer.
        openglRenderer()->setPrimaryFramebuffer(widget()->defaultFramebufferObject());

        // Set up the renderer.
        openglRenderer()->startRender(viewportWindowDeviceSize());

        // Wrap the following in a try-catch block to ensure endRender() is called.
        try {
            openglRenderer()->renderFrame(frameGraph(), QRect(QPoint(0,0), viewportWindowDeviceSize()), nullptr);
            openglRenderer()->endRender();
        }
        catch(...) {
            openglRenderer()->endRender();
            throw;
        }
    }
    catch(Exception& ex) {
        ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewport contents. The program will quit."));

        QString openGLReport;
        QTextStream stream(&openGLReport, QIODevice::WriteOnly | QIODevice::Text);
        stream << "OpenGL version: " << format.majorVersion() << QStringLiteral(".") << format.minorVersion() << "\n";
        stream << "OpenGL profile: " << (format.profile() == QSurfaceFormat::CoreProfile ? "core" : (format.profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
        stream << "OpenGL vendor: " << QString(OpenGLSceneRenderer::openGLVendor()) << "\n";
        stream << "OpenGL renderer: " << QString(OpenGLSceneRenderer::openGLRenderer()) << "\n";
        stream << "OpenGL version string: " << QString(OpenGLSceneRenderer::openGLVersion()) << "\n";
        stream << "OpenGL shading language: " << QString(OpenGLSceneRenderer::openGLSLVersion()) << "\n";
        stream << "OpenGL shader programs: " << QOpenGLShaderProgram::hasOpenGLShaderPrograms() << "\n";
        ex.appendDetailMessage(openGLReport);
        setFrameGraph({});

        userInterface().exitWithFatalError(ex);
    }
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> OpenGLViewportWindow::pick(const QPointF& pos)
{
    // Cannot perform picking while viewport is not visible or when updates are disabled.
    if(isVisible() && !userInterface().exitingWithFatalError() && !userInterface().areViewportUpdatesSuspended() && openglRenderer() && openglRenderer()->hasCurrentResourceFrame() && widget()->isValid() && widget()->defaultFramebufferObject() != 0) {

        // Is the picking buffer still valid? If not, we need to render a new frame.
        if(!_pickingBuffer->isValid() && frameGraph()) {

            // We need an active OpenGL context to render the picking buffer.
            widget()->makeCurrent();

            // Gracefully handle any exceptions that occur during rendering.
            userInterface().handleExceptions([&]() {

                // Create an offscreen OpenGL framebuffer.
                QSize windowSize = viewportWindowDeviceSize();
                QOpenGLFramebufferObjectFormat framebufferFormat;
                framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
                QOpenGLFramebufferObject framebufferObject(windowSize, framebufferFormat);
                if(!framebufferObject.isValid())
                    throw Exception(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));

                // Bind OpenGL framebuffer.
                if(!framebufferObject.bind())
                    throw Exception(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

                // Tell the OpenGL renderer to render into the offscreen framebuffer.
                openglRenderer()->setPrimaryFramebuffer(framebufferObject.handle());

                // Set up the renderer.
                openglRenderer()->startRender(viewportWindowDeviceSize());

                // Wrap the following in a try-catch block to ensure endRender() is called.
                try {
                    openglRenderer()->renderFrame(frameGraph(), QRect(QPoint(0,0), windowSize), nullptr, _pickingBuffer);

                    // Read out the contents of the OpenGL framebuffer.
                    _pickingBuffer->acquire(windowSize, openglRenderer());

                    openglRenderer()->endRender();
                }
                catch(...) {
                    openglRenderer()->endRender();
                    throw;
                }
            });

            // Tell the OpenGL renderer to render into the widget's framebuffer again.
            openglRenderer()->setPrimaryFramebuffer(widget()->defaultFramebufferObject());

            // Done with the OpenGL context.
            widget()->doneCurrent();
        }

        // Query which object is at the given window location.
        if(_pickingBuffer->isValid() && frameGraph()) {
            const QPoint devicePixelPos = (pos * devicePixelRatio()).toPoint();
            return _pickingBuffer->pickAt(devicePixelPos, frameGraph()->projectionParams(), viewportWindowDeviceSize());
        }
    }

    return std::nullopt;
}

}   // End of namespace
