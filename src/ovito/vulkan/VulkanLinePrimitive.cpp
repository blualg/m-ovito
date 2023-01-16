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
* Creates the Vulkan pipelines for the line rendering primitive.
******************************************************************************/
VulkanPipeline& VulkanSceneRenderer::createLinePrimitivePipeline(VulkanPipeline& pipeline)
{
    if(pipeline.isCreated())
        return pipeline;

    uint32_t extraDynamicStateCount = 0;
    std::array<VkDynamicState, 2> extraDynamicState;

    // Are wide lines supported by the Vulkan device?
    if(context()->supportsWideLines())
        extraDynamicState[extraDynamicStateCount++] = VK_DYNAMIC_STATE_LINE_WIDTH;

    // Are extended dynamic states supported by the Vulkan device?
    if(context()->supportsExtendedDynamicState())
        extraDynamicState[extraDynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT;

    // Create pipeline for shader "thin_with_colors":
    if(&pipeline == &_linePrimitivePipelines.thinWithColors)
    {
        VkVertexInputBindingDescription vertexBindingDesc[2];
        vertexBindingDesc[0].binding = 0;
        vertexBindingDesc[0].stride = sizeof(Point_3<float>);
        vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingDesc[1].binding = 1;
        vertexBindingDesc[1].stride = sizeof(ColorAT<float>);
        vertexBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertexAttrDesc[] = {
            { // position:
                0, // location
                0, // binding
                VK_FORMAT_R32G32B32_SFLOAT,
                0 // offset
            },
            { // color:
                1, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                0 // offset
            }
        };

        pipeline.create(*context(),
            QStringLiteral("lines/thin_with_colors"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data()
        );
    }

    // Create pipeline for shader "thin_uniform_color":
    if(&pipeline == &_linePrimitivePipelines.thinUniformColor)
    {
        VkVertexInputBindingDescription vertexBindingDesc[1];
        memset(vertexBindingDesc, 0, sizeof(vertexBindingDesc));
        vertexBindingDesc[0].binding = 0;
        vertexBindingDesc[0].stride = sizeof(Point_3<float>);
        vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertexAttrDesc[] = {
            { // position:
                0, // location
                0, // binding
                VK_FORMAT_R32G32B32_SFLOAT,
                0 // offset
            }
        };

        pipeline.create(*context(),
            QStringLiteral("lines/thin_uniform_color"), 
            defaultRenderPass(),
            sizeof(Matrix_4<float>), // vertexPushConstantSize
            sizeof(ColorAT<float>),  // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            1, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data()
        );
    }
    
    // Create pipeline for shader "thin_picking":
    if(&pipeline == &_linePrimitivePipelines.thinPicking)
    {
        VkVertexInputBindingDescription vertexBindingDesc[1];
        memset(vertexBindingDesc, 0, sizeof(vertexBindingDesc));
        vertexBindingDesc[0].binding = 0;
        vertexBindingDesc[0].stride = sizeof(Point_3<float>);
        vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertexAttrDesc[] = {
            { // position:
                0, // location
                0, // binding
                VK_FORMAT_R32G32B32_SFLOAT,
                0 // offset
            }
        };

        pipeline.create(*context(),
            QStringLiteral("lines/thin_picking"), 
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            1, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data()
        );    
    }

    OVITO_ASSERT(pipeline.isCreated());
    return pipeline; 
}

/******************************************************************************
* Destroys the Vulkan pipelines for this rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::releaseLinePrimitivePipelines()
{
    _linePrimitivePipelines.thinWithColors.release(*context());
    _linePrimitivePipelines.thinUniformColor.release(*context());
    _linePrimitivePipelines.thinPicking.release(*context());
}

/******************************************************************************
* Renders the geometry.
******************************************************************************/
void VulkanSceneRenderer::renderLinesImplementation(const LinePrimitive& primitive)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

#if 1
    // For now always rely on the line drawing capabilities of the Vulkan implementation,
    // even for lines wides than 1 pixel.
    renderThinLinesImplementation(primitive);
#else
    if(primitive.lineWidth() == 1 || (primitive.lineWidth() <= 0 && devicePixelRatio() <= 1))
        renderThinLinesImplementation(primitive);
    else
        renderThickLinesImplementation(primitive);
#endif
}

/******************************************************************************
* Renders the lines using Vulkan line primitives.
******************************************************************************/
void VulkanSceneRenderer::renderThinLinesImplementation(const LinePrimitive& primitive)
{
    // Bind the right pipeline.
    if(primitive.colors() && !isPicking()) {
        createLinePrimitivePipeline(_linePrimitivePipelines.thinWithColors).bind(*context(), currentCommandBuffer());
    }
    else {
        if(!isPicking()) {
            createLinePrimitivePipeline(_linePrimitivePipelines.thinUniformColor).bind(*context(), currentCommandBuffer());
        }
        else {
            createLinePrimitivePipeline(_linePrimitivePipelines.thinPicking).bind(*context(), currentCommandBuffer());
        }
    }

    // Specify line width if the Vulkan implementation supports it. 
    if(context()->supportsWideLines()) {
        FloatType effectiveLineWidth = (primitive.lineWidth() <= 0) ? devicePixelRatio() : primitive.lineWidth();
        deviceFunctions()->vkCmdSetLineWidth(currentCommandBuffer(), static_cast<float>(effectiveLineWidth));
    }

    // Specify dynamic depth-test enabled state if Vulkan implementation supports it.
    if(context()->supportsExtendedDynamicState()) {
        context()->vkCmdSetDepthTestEnableEXT(currentCommandBuffer(), _depthTestEnabled);
    }

    // Compute full view-projection matrix including correction for OpenGL/Vulkan convention difference.
    QMatrix4x4 mvp = clipCorrection() * projParams().projectionMatrix * modelViewTM();

    if(primitive.colors() && !isPicking()) {
        // Pass transformation matrix to vertex shader as a push constant.
        deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _linePrimitivePipelines.thinWithColors.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());

        // Bind vertex buffers for vertex positions and colors.
        std::array<VkBuffer, 2> buffers = {
            context()->uploadDataBuffer(primitive.positions(), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), 
            context()->uploadDataBuffer(primitive.colors(), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
        };
        std::array<VkDeviceSize, 2> offsets = { 0, 0 };
        deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, buffers.size(), buffers.data(), offsets.data());
    }
    else {
        if(!isPicking()) {
            // Pass transformation matrix to vertex shader as a push constant.
            deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _linePrimitivePipelines.thinUniformColor.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());
            // Pass uniform line color to fragment shader as a push constant.
            deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _linePrimitivePipelines.thinUniformColor.layout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Matrix_4<float>), sizeof(ColorAT<float>), ColorAT<float>(primitive.uniformColor()).data());
        }
        else {
            // Pass transformation matrix to vertex shader as a push constant.
            deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _linePrimitivePipelines.thinPicking.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());
            // Pass picking base ID to vertex shader as a push constant.
            uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size() / 2);
            deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), _linePrimitivePipelines.thinPicking.layout(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(pickingBaseId), &pickingBaseId);
        }

        // Bind vertex buffer for vertex positions.
        VkBuffer buffer = context()->uploadDataBuffer(primitive.positions(), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        VkDeviceSize offset = 0;
        deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, 1, &buffer, &offset);
    }

    // Draw lines.
    deviceFunctions()->vkCmdDraw(currentCommandBuffer(), primitive.positions()->size(), 1, 0, 0);
}

/******************************************************************************
* Renders the lines of arbitrary width using polygons.
******************************************************************************/
void VulkanSceneRenderer::renderThickLinesImplementation(const LinePrimitive& primitive)
{
    // Not implemented yet.
}

}   // End of namespace
