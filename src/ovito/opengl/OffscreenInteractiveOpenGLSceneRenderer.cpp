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

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewportWindowInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/RenderSettings.h>
#include "OffscreenInteractiveOpenGLSceneRenderer.h"
#include "OpenGLDepthTextureBlitter.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(OffscreenInteractiveOpenGLSceneRenderer);

/******************************************************************************
* This method is called just before renderFrame() is called.
******************************************************************************/
void OffscreenInteractiveOpenGLSceneRenderer::beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    OVITO_ASSERT(vp);

    // Get the viewport's window.
    ViewportWindowInterface* vpWindow = vp->window();
    if(!vpWindow)
        throw RendererException(tr("Viewport window has not been created."));
    if(!vpWindow->isVisible())
        throw RendererException(tr("Viewport window is not visible."));

    // Before making our own GL context current, remember the old context that
    // is currently active so that we can restore it after we are done rendering.
    _oldContext = QOpenGLContext::currentContext();
    _oldSurface = _oldContext ? _oldContext->surface() : nullptr;

    // Get OpenGL context associated with the viewport window and make it active.
    vpWindow->makeOpenGLContextCurrent();
    QOpenGLContext* context = QOpenGLContext::currentContext();
    if(!context || !context->isValid())
        throw RendererException(tr("OpenGL context for viewport window has not been created."));

    // Prepare a functions table allowing us to call OpenGL functions in a platform-independent way.
    initializeOpenGLFunctions();
    
    // Size of the viewport window in physical pixels.
    QSize size = vpWindow->viewportWindowDeviceSize();

    if(!context->isOpenGLES() || !context->hasExtension("WEBGL_depth_texture")) {
        // Create offscreen OpenGL framebuffer.
        if(!_framebufferObject || _framebufferObject->size() != size || !_framebufferObject->isValid()) {
            QOpenGLFramebufferObjectFormat framebufferFormat;
            framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
            _framebufferObject = std::make_unique<QOpenGLFramebufferObject>(size, framebufferFormat);
        }

        // Clear OpenGL error state and verify validity of framebuffer.
        while(context->functions()->glGetError() != GL_NO_ERROR);
        if(!_framebufferObject->isValid())
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));

        // Bind OpenGL framebuffer.
        if(!_framebufferObject->bind())
            throw RendererException(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

        // Tell the base class about the FBO we are rendering into.
        setPrimaryFramebuffer(_framebufferObject->handle());
    }
    else {
        // When running in a web browser environment which supports the WEBGL_depth_texture extension,
        // create a custom framebuffer with attached color and and depth textures. 

        // Create a texture for storing the color buffer.
        glGenTextures(2, _framebufferTexturesGLES);
        glBindTexture(GL_TEXTURE_2D, _framebufferTexturesGLES[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create a texture for storing the depth buffer.
        glBindTexture(GL_TEXTURE_2D, _framebufferTexturesGLES[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, size.width(), size.height(), 0, GL_DEPTH_STENCIL, 0x84FA /*=GL_UNSIGNED_INT_24_8_WEBGL*/, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Cleanup
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create a framebuffer and associate the textures with it.
        glGenFramebuffers(1, &_framebufferObjectGLES);
        glBindFramebuffer(GL_FRAMEBUFFER, _framebufferObjectGLES);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _framebufferTexturesGLES[0], 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, _framebufferTexturesGLES[1], 0);

        // Check framebuffer status.
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw RendererException(tr("Failed to create OpenGL framebuffer for picking offscreen rendering."));

        // Tell the base class about the FBO we are rendering into.
        setPrimaryFramebuffer(_framebufferObjectGLES);
    }

    OpenGLSceneRenderer::beginFrame(time, scene, params, vp, viewportRect, frameBuffer);
}

/******************************************************************************
* Renders the current animation frame.
******************************************************************************/
bool OffscreenInteractiveOpenGLSceneRenderer::renderFrame(const QRect& viewportRect, MainThreadOperation& operation)
{
    // Let the base class do the main rendering work.
    if(!OpenGLSceneRenderer::renderFrame(viewportRect, operation))
        return false;

    // Clear OpenGL error state, so we start fresh for the glReadPixels() call below.
    while(this->glGetError() != GL_NO_ERROR);

    if(_framebufferObject) {
        // Fetch rendered image from the OpenGL framebuffer.
        QSize size = _framebufferObject->size();

#ifndef Q_OS_WASM
        // Read the color buffer contents.
        _image = QImage(size, QImage::Format_ARGB32);
        // Try GL_BGRA pixel format first. If not supported, use GL_RGBA instead and convert back to GL_BGRA.
        this->glReadPixels(0, 0, size.width(), size.height(), 0x80E1 /*GL_BGRA*/, GL_UNSIGNED_BYTE, _image.bits());
        if(this->glGetError() != GL_NO_ERROR) {
            OVITO_CHECK_OPENGL(this, this->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, _image.bits()));
            _image = std::move(_image).rgbSwapped();
        }
#else
        _image = _framebufferObject->toImage(false);
#endif
    }
    else {
        // Read the color buffer contents.
        glFlush();
        QSize size = viewport()->window()->viewportWindowDeviceSize();
        QImage image(size, QImage::Format_ARGB32);
        OVITO_CHECK_OPENGL(this, this->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits()));
        _image = std::move(image).rgbSwapped();

        // Detach textures from framebuffer.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        // Delete texture-backed framebuffer object.
        glDeleteFramebuffers(1, &_framebufferObjectGLES);
        _framebufferObjectGLES = 0;
    }

    return true;
}

/******************************************************************************
* This method is called after renderFrame() has been called.
******************************************************************************/
void OffscreenInteractiveOpenGLSceneRenderer::endFrame(bool renderingSuccessful, const QRect& viewportRect)
{
    if(_framebufferObject) {
        _framebufferObject.reset();
    }
    else {
        // Go back to using the default framebuffer.
        QOpenGLFramebufferObject::bindDefault();
        
        // Delete framebuffer object.
        glDeleteFramebuffers(1, &_framebufferObjectGLES);
        _framebufferObjectGLES = 0;

        // Delete color and depth textures used for offscreen rendering.
        glDeleteTextures(2, _framebufferTexturesGLES);
        _framebufferTexturesGLES[0] = _framebufferTexturesGLES[1] = 0;
    }
    setPrimaryFramebuffer(0);
    OpenGLSceneRenderer::endFrame(renderingSuccessful, viewportRect);

    // Reactivate old GL context.
    if(_oldSurface && _oldContext) {
        _oldContext->makeCurrent(_oldSurface);
    }
    else {
        QOpenGLContext* context = QOpenGLContext::currentContext();
        if(context) context->doneCurrent();
    }
    _oldContext = nullptr;
    _oldSurface = nullptr;
}

}   // End of namespace
