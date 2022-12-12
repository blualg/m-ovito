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
#include "VulkanSceneRenderer.h"

namespace Ovito {

/**
 * \brief An Vulkan-based scene renderer used for offscreen rendering in OVITO.
 */
class OVITO_VULKANRENDERER_EXPORT OffscreenVulkanSceneRenderer : public VulkanSceneRenderer
{
	OVITO_CLASS(OffscreenVulkanSceneRenderer)

public:

	/// Constructor.
	Q_INVOKABLE OffscreenVulkanSceneRenderer(ObjectCreationParams params, std::shared_ptr<VulkanContext> vulkanDevice = {}, bool grabDepthBuffer = false);

	/// Prepares the renderer for rendering one or more frames.
	virtual bool startRender(const RenderSettings* settings, const QSize& frameBufferSize, MixedKeyCache& visCache) override;

	/// This method is called just before renderFrame() is called.
	virtual void beginFrame(TimePoint time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer) override;

	/// Renders the current animation frame.
	virtual bool renderFrame(const QRect& viewportRect, MainThreadOperation& operation) override;

	/// This method is called after renderFrame() has been called.
	virtual void endFrame(bool renderingSuccessful, const QRect& viewportRect) override;

protected:

	/// Releases all Vulkan resources held by the renderer class.
    virtual void releaseVulkanDeviceResources() override {
		VulkanSceneRenderer::releaseVulkanDeviceResources();
		releaseVulkanFramebuffers();
	}

	/// Returns the Z-value at the given position in the depth buffer.
	/// This method is only used by the PickingVulkanSceneRenderer subclass.
	FloatType depthAtPixel(const QPoint& pos) const;

private:

	/// Release the Vulkan offscreen framebuffers managed by this renderer.
	void releaseVulkanFramebuffers();

private:
	
	/// The resolution of the rendered output image.
	QSize _outputSize;

	/// Flag indicating whether we are interested in reading back the depth buffer contents. 
	/// This is used by the PickingVulkanSceneRenderer subclass.
	bool _grabDepthBuffer = false;

    VkDeviceMemory _colorMem = VK_NULL_HANDLE;
    VkImage _colorImage = VK_NULL_HANDLE;
    VkImageView _colorView = VK_NULL_HANDLE;

    VkDeviceMemory _dsMem = VK_NULL_HANDLE;
    VkImage _dsImage = VK_NULL_HANDLE;
    VkImageView _dsView = VK_NULL_HANDLE;

	VkRenderPass _renderPass = VK_NULL_HANDLE;
	VkFramebuffer _framebuffer = VK_NULL_HANDLE;
	VkCommandBuffer _cmdBuf = VK_NULL_HANDLE;

	VkDeviceMemory _frameGrabImageMem = VK_NULL_HANDLE;
	VkImage _frameGrabImage = VK_NULL_HANDLE;

	VkBuffer _depthGrabBuffer = VK_NULL_HANDLE;
	VmaAllocation _depthGrabBufferAllocation = VK_NULL_HANDLE;
	int _depthBufferBits = 0;
};

}	// End of namespace
