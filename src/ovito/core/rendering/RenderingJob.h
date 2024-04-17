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
#include <ovito/core/oo/RefTarget.h>
#include "FrameGraph.h"

namespace Ovito {

/**
 * A special exception type thrown by a renderer to indicate that something went wrong.
 * The error will interrupt the rendering process and will be shown to the user.
 */
class OVITO_CORE_EXPORT RendererException : public Exception
{
public:
	// Inherit constructors from base class.
	using Exception::Exception;
};

/**
 * Abstract target frame buffer a RenderingJob renders into (e.g. an OpenGL frame buffer or an ANARI frame object).
 */
class OVITO_CORE_EXPORT AbstractRenderingFrameBuffer : public RefTarget
{
	OVITO_CLASS(AbstractRenderingFrameBuffer)

public:

    /// Constructor.
    explicit AbstractRenderingFrameBuffer(ObjectInitializationFlags flags, const QRect& viewportRect, std::shared_ptr<FrameBuffer> outputFrameBuffer) : RefTarget(flags), _viewportRect(viewportRect), _outputFrameBuffer(std::move(outputFrameBuffer)) {}

	/// Returns the output area in the final FrameBuffer (in device pixels).
	const QRect& viewportRect() const { return _viewportRect; }

	/// Returns the device pixel ratio to be used when building the frame graph.
	virtual qreal devicePixelRatio() const { return 1.0; }

	/// Returns the output FrameBuffer where the rendered pixels will be copied to (may be null).
	const std::shared_ptr<FrameBuffer>& outputFrameBuffer() const { return _outputFrameBuffer; }

private:

	/// The output area in the FrameBuffer (in device pixels).
	const QRect _viewportRect;

	/// The output FrameBuffer where the rendered pixels will be copied to (may be null).
	const std::shared_ptr<FrameBuffer> _outputFrameBuffer;
};

/**
 * Abstract base class for scene rendering implementations, which produce a picture of the three-dimensional scene.
 */
class OVITO_CORE_EXPORT RenderingJob : public RefTarget
{
	OVITO_CLASS(RenderingJob)

public:

    /// Constructor.
    explicit RenderingJob(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache = std::make_shared<RendererResourceCache>()) : RefTarget(flags), _visCache(std::move(visCache)) {}

    /// Returns the cache managing rendering resources.
    const std::shared_ptr<RendererResourceCache>& visCache() const { return _visCache; }

	/// Creates a new abstract target frame buffer for rendering into.
	virtual OORef<AbstractRenderingFrameBuffer> createOffscreenFrameBuffer(const QRect& viewportRect, const std::shared_ptr<FrameBuffer>& frameBuffer) = 0;

	/// Renders an image of the given frame graph into the given target frame buffer.
	virtual Future<> renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<AbstractRenderingFrameBuffer> frameBuffer, std::shared_ptr<ObjectPickingIdentifierMap> pickingMap = {}) = 0;

    /// Perform post-processing of a newly generated frame graph, which is to be rendered by this rendering job.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) {}

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const { return QImage::Format_ARGB32_Premultiplied; }

protected:

	/// Renders the 2d graphics of a frame graph render layer into the frame buffer.
	static void render2DPrimitives(FrameGraph::RenderLayer renderLayer, const FrameGraph& frameGraph, const QRect& viewportRect, const std::shared_ptr<FrameBuffer>& frameBuffer);

#ifdef OVITO_BUILD_BASIC
	/// Creates an image serving as watermark for demo versions of scene renderers.
    static QImage createWatermark(const QSize& size);
#endif

private:

    /// The cache managing rendering resources.
    const std::shared_ptr<RendererResourceCache> _visCache;
};

}	// End of namespace
