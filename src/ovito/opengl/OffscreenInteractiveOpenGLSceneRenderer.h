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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>

namespace Ovito {

/**
 * \brief A viewport renderer used by interactive viewport windows.
 */
class OVITO_OPENGLRENDERER_EXPORT OffscreenInteractiveOpenGLSceneRenderer : public OpenGLSceneRenderer
{
    OVITO_CLASS(OffscreenInteractiveOpenGLSceneRenderer)

public:

    /// Constructor.
    using OpenGLSceneRenderer::OpenGLSceneRenderer;

    /// This method is called just before renderFrame() is called.
    virtual void beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

    /// Renders the current animation frame.
    virtual bool renderFrame(const QRect& viewportRect) override;

    /// This method is called after renderFrame() has been called.
    virtual void endFrame(bool renderingSuccessful, const QRect& viewportRect) override;

protected:

    /// Returns the image that was read from the OpenGL framebuffer after rendering.
    const QImage& framebufferImage() const { return _image; }

    /// Throws away the stored framebuffer snapshot.
    void discardFramebufferImage() { _image = QImage(); }

    /// Returns the OpenGL offscreen framebuffer used on desktop OpenGL platforms.
    QOpenGLFramebufferObject* framebufferObject() const { return _framebufferObject.get(); }

    /// Returns the OpenGL texture used as depth buffer (only on OpenGGL ES1 platform).
    GLuint depthTextureId() const { return _framebufferTexturesGLES[1]; }

private:

    /// The OpenGL offscreen framebuffer used on desktop OpenGL platform.
    std::unique_ptr<QOpenGLFramebufferObject> _framebufferObject;

    /// The color and depth texture used for the offscreen framebuffer on GLES platforms.
    GLuint _framebufferTexturesGLES[2] = { 0, 0 };

    /// The OpenGL framebuffer object used for offscreen rendering on GLES platforms.
    GLuint _framebufferObjectGLES = 0;

    /// The image read from the OpenGL framebuffer.
    QImage _image;

    /// Used to restore previous OpenGL context that was active before rendering.
    QPointer<QOpenGLContext> _oldContext;

    /// Used to restore previous OpenGL context that was active before rendering.
    QSurface* _oldSurface;
};

}   // End of namespace
