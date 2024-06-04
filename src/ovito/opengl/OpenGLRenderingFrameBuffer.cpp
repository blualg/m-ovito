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
#include "OpenGLRenderingFrameBuffer.h"
#include "OpenGLHelpers.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OpenGLRenderingFrameBuffer);

/******************************************************************************
* Constructor that allocates an offscreen OpenGL framebuffer.
******************************************************************************/
void OpenGLRenderingFrameBuffer::initializeObject(ObjectInitializationFlags flags, OORef<OpenGLRenderingJob> renderingJob, const QRect& viewportRect, std::shared_ptr<FrameBuffer> outputFrameBuffer)
{
    AbstractRenderingFrameBuffer::initializeObject(flags, viewportRect, std::move(outputFrameBuffer));

    _renderingJob = std::move(renderingJob);

    // Creating an OpenGL framebuffer requires an active OpenGL context.
    OVITO_ASSERT(QOpenGLContext::currentContext());

    // Determine internal framebuffer size including supersampling.
    _framebufferSize = QSize(viewportRect.width() * _renderingJob->multisamplingLevel(), viewportRect.height() * _renderingJob->multisamplingLevel());

    // Create the OpenGL framebuffer.
    QOpenGLFramebufferObjectFormat framebufferFormat;
    framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    _framebufferObject.emplace(_framebufferSize, framebufferFormat);
    if(!_framebufferObject->isValid()) {
        if(_framebufferSize.width() > 16000 || _framebufferSize.height() > 16000)
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering. The selected combination of large image rendering size and/or antialiasing (supersampling) level may exceed what is supported by the OpenGL graphics driver."));
        else
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));
    }

    _framebufferObjectId = _framebufferObject->handle();
}

/******************************************************************************
* Constructor that uses an existing OpenGL framebuffer.
******************************************************************************/
void OpenGLRenderingFrameBuffer::initializeObject(ObjectInitializationFlags flags, OORef<OpenGLRenderingJob> renderingJob, const QRect& viewportRect, GLuint framebufferObjectId)
{
    AbstractRenderingFrameBuffer::initializeObject(flags, viewportRect, {});

    _renderingJob = std::move(renderingJob);
    _framebufferObjectId = framebufferObjectId;
    _framebufferSize = viewportRect.size();

    OVITO_ASSERT(_renderingJob->multisamplingLevel() == 1);
}

/******************************************************************************
* Called when this renderer is being destroyed.
******************************************************************************/
void OpenGLRenderingFrameBuffer::aboutToBeDeleted()
{
    // Release OpenGL frame buffer and other resources. This requires an active GL context.
    if(renderingJob() && (_framebufferObject || _oitFramebuffer || _previousResourceFrame)) {
        OpenGLContextRestore contextRestore = renderingJob()->activateContext();
        QOpenGLFramebufferObject::bindDefault();
        _previousResourceFrame = {};
        _framebufferObject.reset();
        _oitFramebuffer.reset();
    }

    AbstractRenderingFrameBuffer::aboutToBeDeleted();
}

/******************************************************************************
* Creates an offscreen OpenGL framebuffer for order-independent transparency rendering
* and sets up rendering to two framebuffers simultaneously.
******************************************************************************/
void OpenGLRenderingFrameBuffer::beginOITRendering()
{
    // Verify OpenGL capabilities.
    if(!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit())
        throw RendererException(tr("Your OpenGL graphics driver does not support framebuffer blit operations needed for order-independent transparency."));
    if(!renderingJob()->openGLFeatures().testFlag(QOpenGLFunctions::MultipleRenderTargets))
        throw RendererException(tr("Your OpenGL graphics driver does not support multiple render targets, which are required for order-independent transparency."));

    // Create additional offscreen OpenGL framebuffer.
    if(!_oitFramebuffer) {
        // Set up format.
        QOpenGLFramebufferObjectFormat framebufferFormat;
        framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        framebufferFormat.setInternalTextureFormat(GL_RGBA16F);

        // Create framebuffer.
        _oitFramebuffer.emplace(_framebufferSize, framebufferFormat);
        _oitFramebuffer->addColorAttachment(_framebufferSize, GL_R16F);
    }

    // Verify validity of framebuffer.
    if(!_oitFramebuffer->isValid())
        throw RendererException(tr("Failed to create offscreen OpenGL framebuffer object for order-independent transparency."));

    // Clear OpenGL error state and verify validity of framebuffer.
    while(renderingJob()->glGetError() != GL_NO_ERROR);
    if(!_oitFramebuffer->isValid())
        throw RendererException(tr("Failed to create offscreen OpenGL framebuffer object for order-independent transparency."));

    // Bind OpenGL framebuffer.
    if(!_oitFramebuffer->bind())
        throw RendererException(tr("Failed to bind OpenGL framebuffer object for order-independent transparency."));

    // Render to the two output textures simultaneously.
    constexpr GLenum drawBuffersList[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glDrawBuffers(2, drawBuffersList));

    // Clear the contents of the OIT buffer.
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glClearColor(0, 0, 0, 1));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glClear(GL_COLOR_BUFFER_BIT));

    // Blit depth buffer from primary FBO to transparency FBO.
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBindFramebuffer(GL_READ_FRAMEBUFFER, _framebufferObjectId));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBlitFramebuffer(0, 0, _framebufferSize.width(), _framebufferSize.height(), 0, 0, _framebufferSize.width(), _framebufferSize.height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBindFramebuffer(GL_READ_FRAMEBUFFER, 0));

    // Disable writing to the depth buffer.
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glDepthMask(GL_FALSE));

    // Enable blending.
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glEnable(GL_BLEND));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBlendEquation(GL_FUNC_ADD));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA));
}

/******************************************************************************
* Performs the compositing of framebuffer contents for order-independent transparency.
******************************************************************************/
void OpenGLRenderingFrameBuffer::endOITRendering()
{
    // Switch back to the primary rendering buffer.
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBindFramebuffer(GL_FRAMEBUFFER, _framebufferObjectId));
    constexpr GLenum drawBuffersList[] = { GL_COLOR_ATTACHMENT0 };
    renderingJob()->glDrawBuffers(1, drawBuffersList);

    OVITO_ASSERT(renderingJob()->glIsEnabled(GL_BLEND));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE));

    // Perform 2D compositing step.
    renderingJob()->glDisable(GL_DEPTH_TEST);

    // Activate the OpenGL shader program for drawing a screen-filling quad.
    OpenGLShaderHelper shader(renderingJob());
    shader.load("oit_compose", "image/oit_compose.vert", "image/oit_compose.frag");
    shader.setVerticesPerInstance(4);
    shader.setInstanceCount(1);

    // Bind the OIT framebuffer as textures.
    QVector<GLuint> textureIds = _oitFramebuffer->textures();
    OVITO_ASSERT(textureIds.size() == 2);
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glActiveTexture(GL_TEXTURE0));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBindTexture(GL_TEXTURE_2D, textureIds[0]));
    shader.setUniformValue("accumulationTex", 0);
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glActiveTexture(GL_TEXTURE1));
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBindTexture(GL_TEXTURE_2D, textureIds[1]));
    shader.setUniformValue("revealageTex", 1);
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glActiveTexture(GL_TEXTURE0));

    // Draw a quad with 4 vertices.
    shader.draw(GL_TRIANGLE_STRIP);

    renderingJob()->glBindTexture(GL_TEXTURE_2D, 0);
    renderingJob()->glDepthMask(GL_TRUE);
    renderingJob()->glDisable(GL_BLEND);
    renderingJob()->glEnable(GL_DEPTH_TEST);
}

}   // End of namespace
