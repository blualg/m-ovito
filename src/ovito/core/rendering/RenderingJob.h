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
#include <ovito/core/oo/RefTarget.h>
#include "FrameGraph.h"
#include "FrameBuffer.h"
#include "RenderBuffer.h"

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
 * Abstract base class for scene rendering implementations, which produce a picture of the three-dimensional scene.
 */
class OVITO_CORE_EXPORT RenderingJob : public RefTarget
{
	OVITO_CLASS(RenderingJob)

public:

	/// A callback function that can decide which commands from the frame graph should be executed by the renderer.
	/// The function is called for each rendering command in the frame graph along with its command group.
	/// A return value of true means that the command should be skipped during rendering.
	/// The filter function allows a parent renderer to control which commands should be handled by a sub-renderer.
    using RenderingCommandFilter = fu2::unique_function<bool(RenderingJob&, const FrameGraph::RenderingCommandGroup&, const FrameGraph::RenderingCommand&)>;

	/// Creates a new abstract target buffer to render into.
	virtual OORef<RenderBuffer> createOffscreenRenderBuffer(const QSize& deviceIndependentSize) = 0;

	/// Renders an image of the given frame graph. The image is first rendered into the given device-specific render buffer,
	/// and then optionally copied into the given output frame buffer (may be null).
	[[nodiscard]] virtual SCFuture<void> renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress) = 0;

    /// Perform post-processing of a newly generated frame graph, which is to be rendered by this rendering job.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) {}

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const { return QImage::Format_ARGB32_Premultiplied; }

	/// Queries the optional filter function that decides which commands from the frame graph should be executed by this renderer.
	/// If not set, it returns false and all commands are executed.
	bool renderingCommandFilter(const FrameGraph::RenderingCommandGroup& group, const FrameGraph::RenderingCommand& command) { return _renderingCommandFilter && _renderingCommandFilter(*this, group, command); }

	/// Sets an optional filter function that decides which commands from the frame graph should be executed by this renderer.
	/// If not set, all commands are executed.
	void setRenderingCommandFilter(RenderingCommandFilter filter) { _renderingCommandFilter = std::move(filter); }

#ifdef OVITO_BUILD_BASIC
	/// Creates an image serving as watermark for demo versions of scene renderers.
    static QImage createWatermark(const QSize& size);
#endif

private:

	/// Optional filter function that decides which commands from the frame graph should be executed by this renderer.
	/// If not set, all commands are executed.
	RenderingCommandFilter _renderingCommandFilter;
};

}	// End of namespace
