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
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * \brief This is the default scene renderer used for high-quality image output.
 */
class OVITO_CORE_EXPORT StandardSceneRenderer : public SceneRenderer
{
    OVITO_CLASS(StandardSceneRenderer)
    OVITO_CLASSINFO("DisplayName", "OpenGL");
    OVITO_CLASSINFO("Description", "Hardware-accelerated rendering engine, also used by OVITO's interactive viewports. "
                               "The OpenGL renderer is fast and has the smallest memory footprint.");

public:

    /// Constructor.
    explicit StandardSceneRenderer(ObjectInitializationFlags flags);

    /// Prepares the renderer for rendering one or more frames.
    virtual void startRender(const QSize& frameBufferSize) override;

    /// Renders a single frame into the frame buffer.
    virtual void renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

    /// Is called after rendering of one or more frames has finished.
    virtual void endRender() override;

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const override { OVITO_ASSERT(_internalRenderer); return _internalRenderer->preferredImageFormat(); }

	/// Returns the multisampling level currently used by the internal renderer.
	virtual int multisamplingLevel() const override { OVITO_ASSERT(_internalRenderer); return _internalRenderer->multisamplingLevel(); }

    /// Lets the renderer perform post-processing of a newly generated frame graph.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) override { OVITO_ASSERT(_internalRenderer); _internalRenderer->postprocessFrameGraph(frameGraph); }

private:

    /// Controls the number of sub-pixels to render.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, antialiasingLevel, setAntialiasingLevel, PROPERTY_FIELD_RESETTABLE);

    /// The active renderer implementation.
    OORef<SceneRenderer> _internalRenderer;
};

}   // End of namespace
