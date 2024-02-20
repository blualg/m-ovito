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

/**
 * \file SceneRenderer.h
 * \brief Contains the definition of the Ovito::SceneRenderer class.
 */

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include "RendererResourceCache.h"
#include "FrameGraph.h"

namespace Ovito {

/**
 * Abstract base class for scene renderers, which produce a picture of the three-dimensional scene.
 */
class OVITO_CORE_EXPORT SceneRenderer : public RefTarget
{
	OVITO_CLASS(SceneRenderer)

public:

	/// A special exception type thrown by a scene renderer from one of its renderXXX() methods
	/// to indicate that something went wrong. The error will interrupt the rendering process and
	/// will be shown to the user.
	class OVITO_CORE_EXPORT RendererException : public Exception {
	public:
		using Exception::Exception;
	};

	/// Constructor.
	using RefTarget::RefTarget;

    /// Lets the renderer perform post-processing of a newly generated frame graph.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) {}

	/// Prepares the renderer for rendering one or more frames.
	virtual void startRender(const QSize& frameBufferSize) {}

	/// Renders a single frame into the frame buffer.
	virtual void renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) = 0;

	/// Is called after rendering of one or more frames has finished.
	virtual void endRender() {}

	/// This may be called on a renderer before startRender() to control its supersampling level.
	virtual void setMultisamplingLevel(int multisamplingLevel) {}

	/// Returns the multisampling level currently used by the renderer.
	virtual int multisamplingLevel() const { return 1; }

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const { return QImage::Format_ARGB32_Premultiplied; }

#ifdef OVITO_BUILD_BASIC
	/// Creates an image serving as watermark for demo versions of scene renderers.
    QImage createWatermark(const QSize& size);
#endif

protected:

	/// Renders the 2d graphics of a render layer into the frame buffer.
	void render2DPrimitives(FrameGraph::RenderLayer renderLayer, FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) const;
};

}	// End of namespace
