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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/app/Application.h>
#include "OffscreenOpenGLSceneRenderer.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OffscreenOpenGLSceneRenderer);

/******************************************************************************
* Constructor.
******************************************************************************/
OffscreenOpenGLSceneRenderer::OffscreenOpenGLSceneRenderer(ObjectInitializationFlags flags) : OpenGLSceneRenderer(flags)
{
    // Create the offscreen surface.
    // This must happen in the main thread.
    createOffscreenSurface();

    // Initialize OpenGL in main thread if it hasn't already been initialized.
    // This call is a workaround for an access vialotion that otherwise occurs on Windows
    // when creating the first OpenGL context from a worker thread when running in headless mode.
    OpenGLSceneRenderer::determineOpenGLInfo();
}

/******************************************************************************
* Creates the QOffscreenSurface in the main thread.
******************************************************************************/
void OffscreenOpenGLSceneRenderer::createOffscreenSurface()
{
    // Surface creation can only be performed in the main thread.
    OVITO_ASSERT(ExecutionContext::isMainThread());

    // OpenGL rendering and surface creation requires a Qt GUI application object.
    try {
        Application::instance()->createQtApplication(true);
    }
    catch(const Exception& ex) {
        RendererException rendererEx(ex.messages());
        throw rendererEx.prependGeneralMessage(tr(
                "OVITO's OpenGLRenderer cannot be used in headless mode, that is if the application is running without access to a graphics environment. "
                "Please use a different rendering backend or see https://docs.ovito.org/python/modules/ovito_vis.html#ovito.vis.OpenGLRenderer for instructions "
                "on how to enable OpenGL rendering in Python scripts."));
    }

    // Create the QOffscreenSurface.
    if(!_offscreenSurface) {
        _offscreenSurface.emplace();
        if(QOpenGLContext::globalShareContext())
            _offscreenSurface->setFormat(QOpenGLContext::globalShareContext()->format());
        else
            _offscreenSurface->setFormat(QSurfaceFormat::defaultFormat());
        _offscreenSurface->create();
    }
}

/******************************************************************************
* Prepares the renderer for rendering one or more frames.
******************************************************************************/
void OffscreenOpenGLSceneRenderer::startRender(const QSize& frameBufferSize)
{
    OpenGLSceneRenderer::startRender(frameBufferSize);

    // Use the global vis cache from the DataSetContainer by default.
    if(!visCache())
        setVisCache(ExecutionContext::current().ui().datasetContainer().visCache());

    if(!_offscreenContext) {
        // Create an OpenGL context for rendering to an offscreen buffer.
        _offscreenContext.emplace();
        // The context should share its resources with interactive viewport renderers (only when operating in the same thread).
        if(QOpenGLContext::globalShareContext() && QThread::currentThread() == QOpenGLContext::globalShareContext()->thread())
            _offscreenContext->setShareContext(QOpenGLContext::globalShareContext());
        if(!_offscreenContext->create())
            throw RendererException(tr("Failed to create OpenGL context for rendering."));
    }

    // Check offscreen surface (creation must have happened in createOffscreenSurface()).
    if(!_offscreenSurface->isValid())
        throw RendererException(tr("Failed to create offscreen rendering surface."));

    // Make the context current.
    if(!_offscreenContext->makeCurrent(&_offscreenSurface.value()))
        throw RendererException(tr("Failed to make OpenGL context current."));

    QSurfaceFormat format = _offscreenContext->format();
    // OpenGL in a VirtualBox machine Windows guest reports "2.1 Chromium 1.9" as version string, which is
    // not correctly parsed by Qt. We have to workaround this.
    if(OpenGLSceneRenderer::openGLVersion().startsWith("2.1 ")) {
        format.setMajorVersion(2);
        format.setMinorVersion(1);
    }
    if(format.majorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MAJOR || (format.majorVersion() == OVITO_OPENGL_MINIMUM_VERSION_MAJOR && format.minorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MINOR)) {
        throw RendererException(tr(
                    "The OpenGL graphics driver installed on this system does not support OpenGL version %6.%7 or newer.\n\n"
                    "Ovito requires modern graphics hardware and up-to-date graphics drivers to display 3D content. Your current system configuration is not compatible with Ovito.\n\n"
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
                );
    }

    // Determine internal framebuffer size including supersampling.
    _framebufferSize = QSize(frameBufferSize.width() * multisamplingLevel(), frameBufferSize.height() * multisamplingLevel());

    // Create OpenGL framebuffer.
    QOpenGLFramebufferObjectFormat framebufferFormat;
    framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    _framebufferObject.emplace(_framebufferSize, framebufferFormat);
    if(!_framebufferObject->isValid()) {
        if(_framebufferSize.width() > 16000 || _framebufferSize.height() > 16000)
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering. The selected combination of large image rendering size and/or antialiasing (supersampling) level may exceed what is supported by the OpenGL graphics driver."));
        else
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));
    }

    // Bind OpenGL buffer.
    if(!_framebufferObject->bind())
        throw RendererException(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

    // Tell the base class about the FBO we are rendering into.
    setPrimaryFramebuffer(_framebufferObject->handle());
}

/******************************************************************************
* Renders a single frame.
******************************************************************************/
void OffscreenOpenGLSceneRenderer::renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    OVITO_ASSERT(visCache());

    // Make GL context current.
    if(!_offscreenContext || !_offscreenContext->makeCurrent(&_offscreenSurface.value()))
        throw RendererException(tr("Failed to make OpenGL context current."));

    // Open a new cache frame for the OpenGL resource managament.
    // Keep the previous cache frame alive long enough so we can re-use the cached OpenGL resources.
    RendererResourceCache::ResourceFrame previousResourceFrame = setCurrentResourceFrame(visCache()->acquireResourceFrame());

    // Always render into the upper left corner of the OpenGL framebuffer.
    // That's because the OpenGL framebuffer may be smaller than the target OVITO framebuffer.
    QRect shiftedViewportRect = viewportRect;
    shiftedViewportRect.moveTo(0,0);

    // Let the base class do the main rendering work.
    OpenGLSceneRenderer::renderFrame(frameGraph, shiftedViewportRect, frameBuffer);

    // Transfer the rendered image from the OpenGL framebuffer to the output frame buffer.
    if(frameBuffer) {
        // Flush the contents to the FBO before extracting image.
        _offscreenContext->swapBuffers(&_offscreenSurface.value());

        // Clear destination area in the framebuffer (only necessary if OpenGL image is not fully opaque).
        if(frameGraph.clearColor().a() != 1 && !frameBuffer->image().isNull())
            frameBuffer->clear(frameGraph.clearColor(), viewportRect);

        // Fetch rendered image from OpenGL framebuffer.
        QImage renderedImage = _framebufferObject->toImage();
        // We need it in ARGB32 format for best results.
        renderedImage.reinterpretAsFormat(QImage::Format_ARGB32);
        // Rescale supersampled image.
        QSize originalSize(renderedImage.width() / multisamplingLevel(), renderedImage.height() / multisamplingLevel());
        QImage scaledImage = renderedImage.scaled(originalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // Transfer OpenGL image to the output frame buffer.
        if(!frameBuffer->image().isNull()) {
            QPainter painter(&frameBuffer->image());
            painter.drawImage(viewportRect, scaledImage, QRect(0, scaledImage.height() - viewportRect.height(), viewportRect.width(), viewportRect.height()));
        }
        else {
            frameBuffer->image() = scaledImage;
        }
        frameBuffer->update(viewportRect);
    }
}

/******************************************************************************
* Is called after rendering has finished.
******************************************************************************/
void OffscreenOpenGLSceneRenderer::endRender()
{
    OpenGLSceneRenderer::endRender();

    // Release OpenGL resources. This requires an active GL context.
    if(_offscreenContext) {
        bool success = _offscreenContext->makeCurrent(&_offscreenSurface.value());
        OVITO_ASSERT(success);
    }
    setCurrentResourceFrame({});
    QOpenGLFramebufferObject::bindDefault();
    _framebufferObject.reset();
    if(_offscreenContext)
        _offscreenContext->doneCurrent();

    setPrimaryFramebuffer(0);

    // Note: Keeping offscreen surface and GL context alive and re-use them in subsequent render passes until the renderer is deleted.
}

}   // End of namespace
