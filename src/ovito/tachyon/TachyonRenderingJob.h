////////////////////////////////////////////////////////////////////////////////////////
//
//  Tachyon renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RenderingJob.h>
#include "TachyonRenderer.h"

namespace Ovito {

class OVITO_TACHYONRENDERER_EXPORT TachyonRenderingJob : public RenderingJob
{
    OVITO_CLASS(TachyonRenderingJob)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const TachyonRenderer> sceneRenderer);

    /// Creates a new abstract target frame buffer for rendering into.
    virtual OORef<RenderBuffer> createOffscreenRenderBuffer(const QSize& deviceIndependentSize) override;

    /// Renders an image of the given frame graph.
    [[nodiscard]] virtual SCFuture<void> renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress) override;

    /// Returns the best format for QImage to be used when creating an ImagePrimitive.
    virtual QImage::Format preferredImageFormat() const override { return QImage::Format_ARGB32_Premultiplied; }

    /// Performs post-processing of a newly generated frame graph to be rendered by this implementation.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) override {
        frameGraph.renderTextAsImagePrimitives();
        frameGraph.adjustWireframeLineWidths();
    }

private:

    std::shared_ptr<RendererResourceCache> _visCache;
    OORef<const TachyonRenderer> _sceneRenderer;
};

}   // End of namespace
