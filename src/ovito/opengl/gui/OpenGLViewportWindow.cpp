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
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>
#include "OpenGLViewportWindow.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpenGLViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
OpenGLViewportWindow::OpenGLViewportWindow()
{
    // Create the viewport renderer.
    _viewportRenderer = OORef<OpenGLSceneRenderer>::create();

    // Create the object picking renderer.
    _pickingRenderer = OORef<PickingOpenGLSceneRenderer>::create();
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
    // Release any OpenGL resources held by the viewport renderers.
    if(_viewportRenderer && _viewportRenderer->hasCurrentResourceFrame()) {
        widget()->makeCurrent();
        _viewportRenderer->setCurrentResourceFrame({});
        widget()->doneCurrent();
    }
    if(_pickingRenderer && _pickingRenderer->hasCurrentResourceFrame()) {
        widget()->makeCurrent();
        _pickingRenderer->setCurrentResourceFrame({});
        widget()->doneCurrent();
    }
}

/******************************************************************************
* This is called after the frame graph has been updated to render the viewport contents on screen.
******************************************************************************/
void OpenGLViewportWindow::refreshDisplay()
{
    widget()->update();
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
ViewportPickResult OpenGLViewportWindow::pick(const QPointF& pos)
{
    ViewportPickResult result;

#if 0 // TODO
    // Cannot perform picking while viewport is not visible or currently rendering or when updates are disabled.
    if(isVisible() && !userInterface().isRenderingInteractiveViewports() && !userInterface().areViewportUpdatesSuspended() && pickingRenderer()) {
        OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = 0;
        try {
            if(pickingRenderer()->isRefreshRequired()) {
                // A dataset is required for rendering.
                if(DataSet* dataset = userInterface().datasetContainer().currentSet()) {
                    // Request a new frame from the resource manager for this render pass.
                    previousResourceFrame = pickingRenderer()->currentResourceFrame();
                    pickingRenderer()->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());
                    pickingRenderer()->setPrimaryFramebuffer(defaultFramebufferObject());

                    // Let the viewport do the actual rendering work.
                    viewport()->renderInteractive(userInterface(), dataset, pickingRenderer());
                }
                else {
                    return result; // Return null result if no dataset is available.
                }
            }

            // Query which object is located at the given window position.
            const QPoint pixelPos = (pos * devicePixelRatio()).toPoint();
            const PickingOpenGLSceneRenderer::ObjectPickingRecord* objInfo;
            quint32 subobjectId;
            std::tie(objInfo, subobjectId) = pickingRenderer()->objectAtLocation(pixelPos);
            if(objInfo) {
                result.setPipeline(objInfo->pipeline);
                result.setPickInfo(objInfo->pickInfo);
                result.setHitLocation(pickingRenderer()->worldPositionFromLocation(pixelPos));
                result.setSubobjectId(subobjectId);
            }
        }
        catch(const Exception& ex) {
            userInterface().reportError(ex);
        }

        // Release the resources created by the OpenGL renderer during the last render pass before the current pass.
        if(previousResourceFrame)
            OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
    }
#endif
    return result;
}

/******************************************************************************
* Is called whenever the widget needs to be painted.
******************************************************************************/
void OpenGLViewportWindow::paint()
{
    // Do nothing if window has been detached from its viewport.
    if(!viewport())
        return;

    // OpenGL in a VirtualBox machine Windows guest reports "2.1 Chromium 1.9" as version string, which is
    // not correctly parsed by Qt. We have to work around this.
    QSurfaceFormat format = widget()->context()->format();
    if(OpenGLSceneRenderer::openGLVersion().startsWith("2.1 ")) {
        format.setMajorVersion(2);
        format.setMinorVersion(1);
    }

#if 0 // TODO
    // Invalidate picking buffer every time the visible contents of the viewport change.
    _pickingRenderer->resetPickingBuffer();
#endif

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

    // Take the current frame graph from the window (we'll give it back later).
    std::unique_ptr<FrameGraph> frameGraph = setFrameGraph({});
    if(!frameGraph)
        return;

    MainThreadOperation operation(ExecutionContext::Type::Interactive, userInterface());
    try {
        // Create a new OpenGL resource management frame. Keep the previous frame around until the new frame is rendered to re-use existing OpenGL resources.
        DataSetContainer& datasetContainer = userInterface().datasetContainer();
        RendererResourceCache::ResourceFrame previousResourceFrame = _viewportRenderer->setCurrentResourceFrame(datasetContainer.visCache()->acquireResourceFrame());

        // Tell the OpenGL renderer to render into the widget's framebuffer.
        _viewportRenderer->setPrimaryFramebuffer(widget()->defaultFramebufferObject());

        // Set up the renderer.
        _viewportRenderer->startRender(viewportWindowDeviceSize());

        // Wrap the following in a try-catch block to ensure endRender() is called.
        try {
            _viewportRenderer->renderFrame(*frameGraph, QRect(QPoint(0,0), viewportWindowDeviceSize()), nullptr);
            _viewportRenderer->endRender();
        }
        catch(...) {
            _viewportRenderer->endRender();
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

        userInterface().exitWithFatalError(ex);
    }

    // Return the frame graph back to the window.
    setFrameGraph(std::move(frameGraph));
}

}   // End of namespace
