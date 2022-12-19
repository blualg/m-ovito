////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/vulkan/OffscreenVulkanSceneRenderer.h>

namespace Ovito {

/**
 * \brief An Vulkan-based offscreen renderer used for object picking in the viewports.
 */
class PickingVulkanSceneRenderer : public OffscreenVulkanSceneRenderer
{
	OVITO_CLASS(PickingVulkanSceneRenderer)

public:

	/// Constructor.
	explicit PickingVulkanSceneRenderer(ObjectCreationParams params, std::shared_ptr<VulkanContext> vulkanDevice, ViewportWindowInterface* window);

	/// This method is called just before renderFrame() is called.
	virtual void beginFrame(AnimationTime time, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

	/// Renders the current animation frame.
	virtual bool renderFrame(const QRect& viewportRect, MainThreadOperation& operation) override;

	/// This method is called after renderFrame() has been called.
	virtual void endFrame(bool renderingSuccessful, const QRect& viewportRect) override;

	/// Returns the object record and the sub-object ID for the object at the given pixel coordinates.
	std::tuple<const ObjectPickingRecord*, quint32> objectAtLocation(const QPoint& pos) const;

	/// Returns the world space position corresponding to the given screen position.
	Point3 worldPositionFromLocation(const QPoint& pos) const;

	/// Returns true if the picking buffer needs to be regenerated; returns false if the picking buffer still contains valid data.
	bool isRefreshRequired() const { return _frameBuffer.image().isNull(); }

	/// Resets the picking buffer and clears the stored object records.
	virtual void resetPickingBuffer() override;

private:

	/// The viewport window that owns this picking renderer.
	ViewportWindowInterface* _window;

	/// The frame buffer containing the object information.
	FrameBuffer _frameBuffer;
};

}	// End of namespace
