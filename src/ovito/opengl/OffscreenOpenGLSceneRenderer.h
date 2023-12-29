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

#pragma once


#include <ovito/core/Core.h>
#include "OpenGLSceneRenderer.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

namespace Ovito {

/**
 * \brief OpenGL renderer that renders into an offscreen framebuffer instead of the interactive viewports.
 */
class OVITO_OPENGLRENDERER_EXPORT OffscreenOpenGLSceneRenderer : public OpenGLSceneRenderer
{
    OVITO_CLASS(OffscreenOpenGLSceneRenderer)

public:

    /// Constructor.
    explicit OffscreenOpenGLSceneRenderer(ObjectInitializationFlags flags);

    /// Prepares the renderer for rendering one or more frames.
    virtual bool startRender(const RenderSettings* settings, const QSize& frameBufferSize, MixedKeyCache& visCache) override;

    /// This method is called just before renderFrame() is called.
    virtual void beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

    /// Renders the current animation frame.
    virtual bool renderFrame(const QRect& viewportRect) override;

    /// Renders the overlays/underlays of the viewport into the framebuffer.
    virtual bool renderOverlays(bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect) override;

    /// This method is called after renderFrame() has been called.
    virtual void endFrame(bool renderingSuccessful, const QRect& viewportRect) override;

    /// Is called after rendering has finished.
    virtual void endRender() override;

private:

    /// Creates the QOffscreenSurface in the main thread.
    void createOffscreenSurface();

private:

    /// The offscreen surface used to render into an image buffer using OpenGL.
    std::optional<QOffscreenSurface> _offscreenSurface;

    /// The temporary OpenGL rendering context.
    std::unique_ptr<QOpenGLContext> _offscreenContext;

    /// The OpenGL framebuffer.
    std::unique_ptr<QOpenGLFramebufferObject> _framebufferObject;

    /// The resolution of the offscreen framebuffer.
    QSize _framebufferSize;

    /// The monotonically increasing identifier of the last frame that was rendered.
    OpenGLResourceManager::ResourceFrameHandle _previousResourceFrame = 0;
};

}   // End of namespace
