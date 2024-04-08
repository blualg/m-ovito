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

    /// Called when this renderer is being destroyed.
    virtual void aboutToBeDeleted() override;

	/// Prepares the renderer for rendering one or more frames.
	virtual void startRender(const QSize& frameBufferSize) override;

    /// Renders a single frame.
    virtual void renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

    /// Is called after rendering has finished.
    virtual void endRender() override;

    /// Returns the vis cache used for managing OpenGL resources.
    const std::shared_ptr<RendererResourceCache>& visCache() const { return _visCache; }

    /// Sets the vis cache used for managing OpenGL resources.
    void setVisCache(std::shared_ptr<RendererResourceCache> visCache) { _visCache = std::move(visCache); }

private:

    /// Creates the QOffscreenSurface in the main thread.
    void createOffscreenSurface();

private:

    /// The offscreen surface used to render into an image buffer using OpenGL.
    std::unique_ptr<QOffscreenSurface> _offscreenSurface;

    /// The temporary OpenGL rendering context.
    std::optional<QOpenGLContext> _offscreenContext;

    /// The OpenGL framebuffer.
    std::optional<QOpenGLFramebufferObject> _framebufferObject;

    /// The resolution of the offscreen framebuffer.
    QSize _framebufferSize;

    /// The vis cache used for managing OpenGL resources.
    std::shared_ptr<RendererResourceCache> _visCache;
};

}   // End of namespace
