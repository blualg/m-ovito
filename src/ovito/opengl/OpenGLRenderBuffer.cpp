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
#include "OpenGLRenderBuffer.h"
#include "OpenGLHelpers.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OpenGLRenderBuffer);

/******************************************************************************
* Constructor that allocates an offscreen OpenGL framebuffer.
******************************************************************************/
void OpenGLRenderBuffer::initializeObject(OORef<OpenGLRenderingJob> renderingJob, const QSize& deviceIndependentSize)
{
    RenderBuffer::initializeObject(deviceIndependentSize * renderingJob->supersamplingLevel());

    _renderingJob = std::move(renderingJob);

    // Creating an OpenGL framebuffer requires an active OpenGL context.
    OVITO_ASSERT(QOpenGLContext::currentContext());

    // Create the OpenGL framebuffer.
    QOpenGLFramebufferObjectFormat framebufferFormat;
    framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    _framebufferObject.emplace(size(), framebufferFormat);
    if(!_framebufferObject->isValid()) {
        if(size().width() > 16000 || size().height() > 16000)
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering. The selected combination of large image rendering size and/or antialiasing (supersampling) level may exceed what is supported by the OpenGL graphics driver."));
        else
            throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));
    }

    _framebufferObjectId = _framebufferObject->handle();
}

/******************************************************************************
* Constructor that uses an existing OpenGL framebuffer.
******************************************************************************/
void OpenGLRenderBuffer::initializeObject(OORef<OpenGLRenderingJob> renderingJob, const QSize& deviceIndependentSize, GLuint framebufferObjectId)
{
    RenderBuffer::initializeObject(deviceIndependentSize);

    _renderingJob = std::move(renderingJob);
    _framebufferObjectId = framebufferObjectId;
    OVITO_ASSERT(_renderingJob->supersamplingLevel() == 1);
}

/******************************************************************************
* Called when this renderer is being destroyed.
******************************************************************************/
void OpenGLRenderBuffer::aboutToBeDeleted()
{
    // Release OpenGL frame buffer and other resources. This requires an active GL context.
    if(renderingJob() && (_framebufferObject || _oitFramebuffer || _previousResourceFrame)) {
        OpenGLContextRestore contextRestore = renderingJob()->activateContext();
        QOpenGLFramebufferObject::bindDefault();
        _previousResourceFrame = {};
        _framebufferObject.reset();
        _oitFramebuffer.reset();
    }

    RenderBuffer::aboutToBeDeleted();
}

/******************************************************************************
* Creates an offscreen OpenGL framebuffer for order-independent transparency rendering
* and sets up rendering to two framebuffers simultaneously.
******************************************************************************/
void OpenGLRenderBuffer::beginOITRendering()
{
    // Verify OpenGL capabilities.
    if(!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit())
        throw RendererException(tr("Your OpenGL graphics driver does not support framebuffer blit operations needed for order-independent transparency."));
    if(!renderingJob()->openGLFeatures().testFlag(QOpenGLFunctions::MultipleRenderTargets))
        throw RendererException(tr("Your OpenGL graphics driver does not support multiple render targets, which are required for order-independent transparency."));

    // Create additional offscreen OpenGL framebuffer.
    if(!_oitFramebuffer || !_oitFramebuffer->isValid()) {
        // Set up format.
        QOpenGLFramebufferObjectFormat framebufferFormat;
        framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        framebufferFormat.setInternalTextureFormat(GL_RGBA16F);

        // Create framebuffer.
        _oitFramebuffer.emplace(size(), framebufferFormat);
        _oitFramebuffer->addColorAttachment(size(), GL_R16F);
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
    OVITO_CHECK_OPENGL(renderingJob(), renderingJob()->glBlitFramebuffer(0, 0, size().width(), size().height(), 0, 0, size().width(), size().height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST));
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
void OpenGLRenderBuffer::endOITRendering()
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
