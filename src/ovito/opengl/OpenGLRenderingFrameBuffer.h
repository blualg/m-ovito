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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/rendering/RenderingJob.h>
#include "OpenGLRenderingJob.h"

#include <QOpenGLFramebufferObject>

namespace Ovito {

/**
 * \brief An offscreen framebuffer the OpenGL renderer can render into.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLRenderingFrameBuffer : public AbstractRenderingFrameBuffer
{
    OVITO_CLASS(OpenGLRenderingFrameBuffer)

public:

    /// Constructor that allocates an offscreen OpenGL framebuffer.
    void initializeObject(ObjectInitializationFlags flags, OORef<OpenGLRenderingJob> renderingJob, const QRect& viewportRect, std::shared_ptr<FrameBuffer> outputFrameBuffer);

    /// Constructor that uses an existing OpenGL framebuffer.
    void initializeObject(ObjectInitializationFlags flags, OORef<OpenGLRenderingJob> renderingJob, const QRect& viewportRect, GLuint framebufferObjectId);

    /// Called when this frame buffer is being destroyed.
    virtual void aboutToBeDeleted() override;

	/// Returns the target area in the internal rendering framebuffer (e.g. OpenGL framebuffer).
	virtual QRect renderingViewportRect() const override { return QRect(QPoint(0, 0), _framebufferSize); }

    /// Returns the rendering job this frame buffer belongs to.
    const OORef<OpenGLRenderingJob>& renderingJob() const { return _renderingJob; }

    /// Returns the offscreen OpenGL framebuffer.
    std::optional<QOpenGLFramebufferObject>& framebufferObject() { return _framebufferObject; }

    /// The ID of the OpenGL framebuffer to render into.
    GLuint framebufferObjectId() const { return _framebufferObjectId; }

    /// Returns the physical resolution of the offscreen OpenGL framebuffer, which includes the multisampling factor.
    const QSize& framebufferSize() const { return _framebufferSize; }

    /// Keeps alive the OpenGL resources that got created during the last rendered frame.
    void storePreviousResourceFrame(RendererResourceCache::ResourceFrame&& previousResourceFrame) { _previousResourceFrame = std::move(previousResourceFrame); }

    /// Creates an offscreen OpenGL framebuffer for order-independent transparency rendering
    /// and sets up rendering to two framebuffers simultaneously.
    void beginOITRendering();

    /// Performs the compositing of framebuffer contents for order-independent transparency.
    void endOITRendering();

private:

    /// The rendering job this frame buffer belongs to.
    /// This reference keeps alive the job while the OpenGL framebuffer exists.
    OORef<OpenGLRenderingJob> _renderingJob;

    /// The offscreen OpenGL framebuffer.
    std::optional<QOpenGLFramebufferObject> _framebufferObject;

    /// An additional offscreen framebuffer used for the OIT transparency pass.
    std::optional<QOpenGLFramebufferObject> _oitFramebuffer;

    /// The ID of the OpenGL framebuffer to render into.
    GLuint _framebufferObjectId = 0;

    /// The physical resolution of the offscreen OpenGL framebuffer.
    /// This includes the multisampling factor.
    QSize _framebufferSize;

    /// Keeps alive the OpenGL resources that got created during the last rendered frame.
    /// Note: OpenGL objects must be released while an OpenGL context is current.
    /// The renderer needs to have control of when resources get released.
    RendererResourceCache::ResourceFrame _previousResourceFrame;
};

}   // End of namespace
