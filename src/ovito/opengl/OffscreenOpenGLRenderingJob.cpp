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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include "OffscreenOpenGLRenderingJob.h"
#include "OpenGLRenderBuffer.h"
#include "OpenGLRenderer.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OffscreenOpenGLRenderingJob);

/******************************************************************************
* Constructor creating a new QOffscreenSurface.
******************************************************************************/
void OffscreenOpenGLRenderingJob::initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const OpenGLRenderer> sceneRenderer, int supersamplingLevel)
{
    OpenGLRenderingJob::initializeObject(flags, std::move(visCache), std::move(sceneRenderer), supersamplingLevel);

    // Create the offscreen surface.
    // This must happen in the main thread.
    createOffscreenSurface();

    // Initialize OpenGL in main thread if it hasn't already been initialized.
    // This call is a workaround for an access violation that otherwise occurs on Windows
    // when creating the first OpenGL context from a worker thread when running in headless mode.
    OpenGLRenderer::determineOpenGLInfo();
}

/******************************************************************************
* Creates the QOffscreenSurface in the main thread.
******************************************************************************/
void OffscreenOpenGLRenderingJob::createOffscreenSurface()
{
    // Surface creation can only be performed in the main thread.
    OVITO_ASSERT(this_task::isMainThread());

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
    _offscreenSurface = std::make_unique<QOffscreenSurface>();
    if(QOpenGLContext::globalShareContext())
        _offscreenSurface->setFormat(QOpenGLContext::globalShareContext()->format());
    else
        _offscreenSurface->setFormat(QSurfaceFormat::defaultFormat());
    _offscreenSurface->create();

    // Check offscreen surface (creation must have happened in createOffscreenSurface()).
    if(!_offscreenSurface->isValid())
        throw RendererException(tr("Failed to create offscreen surface for OpenGL rendering."));
}

/******************************************************************************
* Creates the QOpenGLContext for offscreen rendering (in any thread).
******************************************************************************/
QOpenGLContext& OffscreenOpenGLRenderingJob::createOffscreenContext()
{
#ifdef Q_OS_MACOS
    // On macOS, the global OpenGL context must be created in the main thread.
    // Also: Creating ad-hoc contexts seems to leak memory on macOS since Qt 6.7.
    // That's why we create only a single shared context and re-use it for all offscreen rendering tasks.
    OVITO_ASSERT(this_task::isMainThread());
    static QOpenGLContext sharedOffscreenContext;
    if(!sharedOffscreenContext.isValid()) {
        // The context should share its resources with interactive viewport renderers.
        sharedOffscreenContext.setShareContext(QOpenGLContext::globalShareContext());
        if(!sharedOffscreenContext.create())
            throw RendererException(tr("Failed to create OpenGL context for offscreen rendering."));
    }
    return sharedOffscreenContext;
#else
    // Can use the GL context only in the thread it was created in.
    if(!_offscreenContext.has_value() || _offscreenContext->thread() != QThread::currentThread()) {
        // Create an OpenGL context for rendering into the offscreen buffer.
        _offscreenContext.emplace();
        // The context should share its resources with the interactive viewport windows.
        _offscreenContext->setShareContext(QOpenGLContext::globalShareContext());
        if(!_offscreenContext->create())
            throw RendererException(tr("Failed to create OpenGL context for offscreen rendering."));
    }
    return _offscreenContext.value();
#endif
}

/******************************************************************************
* Requests the rendering job to make its OpenGL context current, e.g. for
* releasing OpenGL resources that require an active context.
******************************************************************************/
OpenGLContextRestore OffscreenOpenGLRenderingJob::activateContext()
{
    OpenGLContextRestore restore;
    OVITO_ASSERT(_offscreenSurface);
    Q_DECL_UNUSED bool success = createOffscreenContext().makeCurrent(_offscreenSurface.get());
    OVITO_ASSERT(success);
    return restore;
}

/******************************************************************************
* Called when this renderer is being destroyed.
******************************************************************************/
void OffscreenOpenGLRenderingJob::aboutToBeDeleted()
{
    OpenGLRenderingJob::aboutToBeDeleted();

    // Release the offscreen GL context.
    _offscreenContext.reset();

    // Release the offscreen surface.
    if(_offscreenSurface) {
        if(this_task::isMainThread())
            _offscreenSurface.reset();
        else
            _offscreenSurface.release()->deleteLater();
    }
}

}   // End of namespace
