////////////////////////////////////////////////////////////////////////////////////////
//
//  VisRTX renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/RenderingJob.h>
#include "OffscreenAnariRenderer.h"

namespace Ovito {

class OVITO_VISRTXRENDERER_EXPORT AnariRenderingJob : public RenderingJob
{
    OVITO_CLASS(AnariRenderingJob)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const OffscreenAnariRenderer> sceneRenderer);

    /// Creates a new abstract target frame buffer for rendering into.
    virtual OORef<RenderBuffer> createOffscreenRenderBuffer(const QSize& deviceIndependentSize) override;

    /// Renders an image of the given frame graph.
    [[nodiscard]] virtual SCFuture<void> renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress) override;

private:

    std::shared_ptr<RendererResourceCache> _visCache;
    OORef<const OffscreenAnariRenderer> _sceneRenderer;
};

}   // End of namespace
