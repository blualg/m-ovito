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
#include <ovito/core/rendering/SceneRenderer.h>
#include "VulkanPipeline.h"
#include "VulkanContext.h"

namespace Ovito {

/******************************************************************************
* Creates the Vulkan pipeline.
******************************************************************************/
void VulkanPipeline::create(VulkanContext& context, 
    const QString& shaderName, 
    VkRenderPass renderpass,
    uint32_t vertexPushConstantSize,
    uint32_t fragmentPushConstantSize,
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription* pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription* pVertexAttributeDescriptions,
    VkPrimitiveTopology topology,
    uint32_t extraDynamicStateCount,
    const VkDynamicState* pExtraDynamicStates,
    bool supportAlphaBlending,
    uint32_t setLayoutCount,
    const VkDescriptorSetLayout* pSetLayouts,
    bool enableDepthOffset)
{
    OVITO_ASSERT(_layout == VK_NULL_HANDLE);
    OVITO_ASSERT(_pipeline == VK_NULL_HANDLE);

    // This method may only be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == context.thread());

    // Set up push constants used by the shader.
    VkPushConstantRange pushConstantRanges[2];
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].size = vertexPushConstantSize;
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRanges[1].offset = vertexPushConstantSize;
    pushConstantRanges[1].size = fragmentPushConstantSize;
    pushConstantRanges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Create the pipeline layout.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.pushConstantRangeCount = (vertexPushConstantSize != 0 ? 1 : 0) + (fragmentPushConstantSize != 0 ? 1 : 0);
    pipelineLayoutInfo.pPushConstantRanges = (vertexPushConstantSize != 0) ? &pushConstantRanges[0] : &pushConstantRanges[1];
    pipelineLayoutInfo.setLayoutCount = setLayoutCount;
    pipelineLayoutInfo.pSetLayouts = pSetLayouts;
    VkResult err = context.deviceFunctions()->vkCreatePipelineLayout(context.logicalDevice(), &pipelineLayoutInfo, nullptr, &_layout);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(VulkanContext::tr("Failed to create Vulkan pipeline layout (error code %1) for shader '%2'.").arg(err).arg(shaderName));

    // Shaders
    VkShaderModule vertShaderModule = context.createShader(QStringLiteral(":/vulkanrenderer/%1.vert.spv").arg(shaderName));
    VkShaderModule fragShaderModule = context.createShader(QStringLiteral(":/vulkanrenderer/%1.frag.spv").arg(shaderName));

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertShaderModule,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragShaderModule,
            "main",
            nullptr
        }
    };
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
    vertexInputInfo.pVertexBindingDescriptions = pVertexBindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptionCount;
    vertexInputInfo.pVertexAttributeDescriptions = pVertexAttributeDescriptions;
    pipelineInfo.pVertexInputState = &vertexInputInfo;

    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = topology;
    pipelineInfo.pInputAssemblyState = &ia;

    // The viewport and scissor will be set dynamically via vkCmdSetViewport/Scissor.
    // This way the pipeline does not need to be touched when resizing the window.
    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    pipelineInfo.pViewportState = &vp;

    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT; 
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    if(enableDepthOffset) {
        rs.depthBiasEnable = true;
        rs.depthBiasConstantFactor = 1.0f;
        rs.depthBiasSlopeFactor = 1.0f;
    }
    pipelineInfo.pRasterizationState = &rs;

    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipelineInfo.pMultisampleState = &ms;

    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineInfo.pDepthStencilState = &ds;

    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    // No blend, write out all of RGBA.
    VkPipelineColorBlendAttachmentState att;
    memset(&att, 0, sizeof(att));
    att.colorWriteMask = 0xF;
    cb.attachmentCount = 1;
    cb.pAttachments = &att;
    pipelineInfo.pColorBlendState = &cb;

    // Compile of dynamic pipeline states.
    VkPipelineDynamicStateCreateInfo dyn = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    QVarLengthArray<VkDynamicState, 4> dynEnableVector;
    dynEnableVector.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynEnableVector.push_back(VK_DYNAMIC_STATE_SCISSOR);
    // Add user-supplied dynamic state types.
    if(extraDynamicStateCount != 0)
        dynEnableVector.append(pExtraDynamicStates, extraDynamicStateCount);
    dyn.dynamicStateCount = (uint32_t)dynEnableVector.size();
    dyn.pDynamicStates = dynEnableVector.data();
    pipelineInfo.pDynamicState = &dyn;

    pipelineInfo.layout = _layout;
    pipelineInfo.renderPass = renderpass;

    err = context.deviceFunctions()->vkCreateGraphicsPipelines(context.logicalDevice(), context.pipelineCache(), 1, &pipelineInfo, nullptr, &_pipeline);
    if(err != VK_SUCCESS)
        throw Exception(VulkanContext::tr("Failed to create Vulkan graphics pipeline (error code %1) for shader '%2'").arg(err).arg(shaderName));

    // If requested, build another copy of the pipeline with alpha blending enabled.
    if(supportAlphaBlending) {
        // Enable standard alpha blending.
        VkPipelineColorBlendAttachmentState att_blend = {
            VK_TRUE,                              // VkBool32                 blendEnable
            VK_BLEND_FACTOR_SRC_ALPHA,            // VkBlendFactor            srcColorBlendFactor
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // VkBlendFactor            dstColorBlendFactor
            VK_BLEND_OP_ADD,                      // VkBlendOp                colorBlendOp
            VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,            // VkBlendFactor            srcAlphaBlendFactor
            VK_BLEND_FACTOR_ONE,  // VkBlendFactor            dstAlphaBlendFactor
            VK_BLEND_OP_ADD,                      // VkBlendOp                alphaBlendOp
            VK_COLOR_COMPONENT_R_BIT |            // VkColorComponentFlags    colorWriteMask
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT
        };
        cb.pAttachments = &att_blend;

        err = context.deviceFunctions()->vkCreateGraphicsPipelines(context.logicalDevice(), context.pipelineCache(), 1, &pipelineInfo, nullptr, &_pipelineWithBlending);
        if(err != VK_SUCCESS)
            throw Exception(VulkanContext::tr("Failed to create Vulkan graphics pipeline (error code %1) for shader '%2'").arg(err).arg(shaderName));
    }

    if(vertShaderModule)
        context.deviceFunctions()->vkDestroyShaderModule(context.logicalDevice(), vertShaderModule, nullptr);
    if(fragShaderModule)
        context.deviceFunctions()->vkDestroyShaderModule(context.logicalDevice(), fragShaderModule, nullptr);
}

/******************************************************************************
* Destroys the Vulkan pipeline.
******************************************************************************/
void VulkanPipeline::release(VulkanContext& context)
{
    if(_pipeline != VK_NULL_HANDLE) {
        context.deviceFunctions()->vkDestroyPipeline(context.logicalDevice(), _pipeline, nullptr);
        _pipeline = VK_NULL_HANDLE;
    }
    if(_pipelineWithBlending != VK_NULL_HANDLE) {
        context.deviceFunctions()->vkDestroyPipeline(context.logicalDevice(), _pipelineWithBlending, nullptr);
        _pipelineWithBlending = VK_NULL_HANDLE;
    }
    if(_layout != VK_NULL_HANDLE) {
        context.deviceFunctions()->vkDestroyPipelineLayout(context.logicalDevice(), _layout, nullptr);
        _layout = VK_NULL_HANDLE;
    }
}

/******************************************************************************
* Binds the pipeline.
******************************************************************************/
void VulkanPipeline::bind(VulkanContext& context, VkCommandBuffer cmdBuf, bool enableBlending) const
{
    // Check that blending was enabled at the time the pipeline was created when blending is requested now at draw time.
    OVITO_ASSERT(!enableBlending || _pipelineWithBlending);
    OVITO_ASSERT(_pipeline != VK_NULL_HANDLE);

    context.deviceFunctions()->vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, enableBlending ? _pipelineWithBlending : _pipeline);
}

}   // End of namespace
