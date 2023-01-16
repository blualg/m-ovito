////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/core/Core.h>
#include "VulkanSceneRenderer.h"

namespace Ovito {

/******************************************************************************
* Creates the Vulkan pipelines for the image rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::initImagePrimitivePipelines()
{
    // Are extended dynamic states supported by the Vulkan device? 
    // If yes, we can use them to switch depth testing on or off on demand.
    uint32_t extraDynamicStateCount = context()->supportsExtendedDynamicState() ? 1 : 0;
    VkDynamicState extraDynamicState = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT;
    
    // Specify the descriptor layout binding for the sampler.
    VkSampler sampler = context()->samplerNearest();
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = &sampler;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Create descriptor set layout.
    VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;
    VkResult err = deviceFunctions()->vkCreateDescriptorSetLayout(logicalDevice(), &layoutInfo, nullptr, &_imagePrimitivePipelines.descriptorSetLayout);
    if(err != VK_SUCCESS)
        throw Exception(QStringLiteral("Failed to create Vulkan descriptor set layout (error code %1).").arg(err));

    // Create pipeline.
    _imagePrimitivePipelines.imageQuad.create(*context(),
        QStringLiteral("image/image"),
        defaultRenderPass(),
        2 * sizeof(Point_2<float>), // vertexPushConstantSize
        0, // fragmentPushConstantSize
        0, // vertexBindingDescriptionCount
        nullptr, 
        0, // vertexAttributeDescriptionCount
        nullptr, 
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
        extraDynamicStateCount, // extraDynamicStateCount
        &extraDynamicState, // pExtraDynamicStates
        true, // enableAlphaBlending
        1, // setLayoutCount
        &_imagePrimitivePipelines.descriptorSetLayout
    );
}

/******************************************************************************
* Destroys the Vulkan pipelines for this rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::releaseImagePrimitivePipelines()
{
    _imagePrimitivePipelines.imageQuad.release(*context());

    if(_imagePrimitivePipelines.descriptorSetLayout != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyDescriptorSetLayout(logicalDevice(), _imagePrimitivePipelines.descriptorSetLayout, nullptr);
        _imagePrimitivePipelines.descriptorSetLayout = VK_NULL_HANDLE;
    }
}

/******************************************************************************
* Renders the geometry.
******************************************************************************/
void VulkanSceneRenderer::renderImageImplementation(const ImagePrimitive& primitive)
{
    if(primitive.image().isNull() || isPicking() || primitive.windowRect().isEmpty())
        return;

    // Upload the image to the GPU as a texture.
    VkImageView imageView = context()->uploadImage(primitive.image(), currentResourceFrame());

    // Bind the pipeline.
    _imagePrimitivePipelines.imageQuad.bind(*context(), currentCommandBuffer(), true);

    // Specify dynamic depth-test enabled state if Vulkan implementation supports it.
    if(context()->supportsExtendedDynamicState())
        context()->vkCmdSetDepthTestEnableEXT(currentCommandBuffer(), _depthTestEnabled);

    // Use the QImage cache key to look up descriptor set.
    RendererResourceKey<struct VulkanImagePrimitiveCache, qint64> cacheKey{ primitive.image().cacheKey() };

    // Create or look up the descriptor set.
    std::pair<VkDescriptorSet, bool> descriptorSet = context()->createDescriptorSet(_imagePrimitivePipelines.descriptorSetLayout, cacheKey, currentResourceFrame());

    // Initialize the descriptor set if it was newly created.
    if(descriptorSet.second) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = imageView;
        imageInfo.sampler = context()->samplerNearest();
        VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptorWrite.dstSet = descriptorSet.first;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        deviceFunctions()->vkUpdateDescriptorSets(logicalDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    // Bind the descriptor set to the pipeline.
    deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, _imagePrimitivePipelines.imageQuad.layout(), 0, 1, &descriptorSet.first, 0, nullptr);

    // Pass quad rectangle to vertex shader as a push constant.
    Point_2<float> quad[2];
    quad[0].x() = (float)(primitive.windowRect().minc.x() / (FloatType)frameBufferSize().width() * 2.0 - 1.0);
    quad[0].y() = (float)(primitive.windowRect().minc.y() / (FloatType)frameBufferSize().height() * 2.0 - 1.0);
    quad[1].x() = (float)(primitive.windowRect().maxc.x() / (FloatType)frameBufferSize().width() * 2.0 - 1.0);
    quad[1].y() = (float)(primitive.windowRect().maxc.y() / (FloatType)frameBufferSize().height() * 2.0 - 1.0);
    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _imagePrimitivePipelines.imageQuad.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(quad), quad);

    // Draw quad.
    deviceFunctions()->vkCmdDraw(currentCommandBuffer(), 4, 1, 0, 0);
}

}   // End of namespace
