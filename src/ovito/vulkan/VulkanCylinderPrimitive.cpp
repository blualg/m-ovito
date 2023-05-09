////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
* Creates the Vulkan pipelines for this rendering primitive.
******************************************************************************/
VkPipelineLayout VulkanSceneRenderer::createCylinderPrimitivePipeline(VulkanPipeline& pipeline)
{
    if(pipeline.isCreated())
        return pipeline.layout();

    std::array<VkVertexInputBindingDescription, 2> vertexBindingDesc;

    // Base + head + radius:
    vertexBindingDesc[0].binding = 0;
    vertexBindingDesc[0].stride = sizeof(Vector_3<float>) + sizeof(Vector_3<float>) + sizeof(float);
    vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Color + alpha:
    vertexBindingDesc[1].binding = 1;
    vertexBindingDesc[1].stride = 2 * sizeof(Vector_4<float>);
    vertexBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    TODO: Adopt new techniques from OpenGL renderer.

    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        VkVertexInputAttributeDescription{ // base:
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            0 // offset
        },
        VkVertexInputAttributeDescription{ // head:
            1, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            sizeof(Vector_3<float>) // offset
        },
        VkVertexInputAttributeDescription{ // radius:
            2, // location
            0, // binding
            VK_FORMAT_R32_SFLOAT,
            sizeof(Vector_3<float>) + sizeof(Vector_3<float>) // offset
        },
        VkVertexInputAttributeDescription{ // color1:
            3, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0 // offset
        },
        VkVertexInputAttributeDescription{ // color2:
            4, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            sizeof(Vector_4<float>) // offset
        }
    };

    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = { globalUniformsDescriptorSetLayout(), colorMapDescriptorSetLayout() };

    if(&pipeline == &_cylinderPrimitivePipelines.cylinder)
        pipeline.create(*context(),
            QStringLiteral("cylinder/cylinder"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            sizeof(Vector_2<float>), // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            5, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            2, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.cylinder_picking)
        pipeline.create(*context(),
            QStringLiteral("cylinder/cylinder_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.cylinder_flat)
        pipeline.create(*context(),
            QStringLiteral("cylinder/cylinder_flat"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Vector_4<float>), // vertexPushConstantSize
            sizeof(Vector_2<float>), // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            5, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            2, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.cylinder_flat_picking)
        pipeline.create(*context(),
            QStringLiteral("cylinder/cylinder_flat_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Vector_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_head)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_head"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            5, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_head_picking)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_head_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_tail)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_tail"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            5, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_tail_picking)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_tail_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_flat)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_flat"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Vector_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            5, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_cylinderPrimitivePipelines.arrow_flat_picking)
        pipeline.create(*context(),
            QStringLiteral("cylinder/arrow_flat_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Vector_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            1, // setLayoutCount
            descriptorSetLayouts.data()
        );

    OVITO_ASSERT(pipeline.isCreated());
    return pipeline.layout();
}

/******************************************************************************
* Destroys the Vulkan pipelines for this rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::releaseCylinderPrimitivePipelines()
{
    _cylinderPrimitivePipelines.cylinder.release(*context());
    _cylinderPrimitivePipelines.cylinder_picking.release(*context());
    _cylinderPrimitivePipelines.cylinder_flat.release(*context());
    _cylinderPrimitivePipelines.cylinder_flat_picking.release(*context());
    _cylinderPrimitivePipelines.arrow_head.release(*context());
    _cylinderPrimitivePipelines.arrow_head_picking.release(*context());
    _cylinderPrimitivePipelines.arrow_tail.release(*context());
    _cylinderPrimitivePipelines.arrow_tail_picking.release(*context());
    _cylinderPrimitivePipelines.arrow_flat.release(*context());
    _cylinderPrimitivePipelines.arrow_flat_picking.release(*context());
}

/******************************************************************************
* Renders a set of cylinders or arrow glyphs.
******************************************************************************/
void VulkanSceneRenderer::renderCylindersImplementation(const CylinderPrimitive& primitive)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
        return;

    // Compute full view-projection matrix including correction for OpenGL/Vulkan convention difference.
    QMatrix4x4 mvp = clipCorrection() * projParams().projectionMatrix * modelViewTM();

    // The effective number of primitives being rendered:
    uint32_t primitiveCount = primitive.basePositions()->size();
    uint32_t verticesPerPrimitive = 0;

    // Are we rendering semi-transparent cylinders?
    bool useBlending = !isPicking() && (primitive.transparencies() != nullptr);

    // Decide whether per-pixel pseudo-color mapping is used (instead of direct RGB coloring).
    bool renderWithPseudoColorMapping = false;
    if(primitive.pseudoColorMapping().isValid() && !isPicking() && primitive.colors() && primitive.colors()->componentCount() == 1) {
        OVITO_ASSERT(primitive.shape() == CylinderPrimitive::CylinderShape);
        renderWithPseudoColorMapping = true;
    }

    // Bind the right Vulkan pipeline.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    switch(primitive.shape()) {

        case CylinderPrimitive::CylinderShape:

            if(primitive.shadingMode() == CylinderPrimitive::NormalShading) {
                if(!isPicking()) {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.cylinder);
                    _cylinderPrimitivePipelines.cylinder.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.cylinder_picking);
                    _cylinderPrimitivePipelines.cylinder_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerPrimitive = 14; // Box rendered as triangle strip.
            }
            else {
                if(!isPicking()) {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.cylinder_flat);
                    _cylinderPrimitivePipelines.cylinder_flat.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.cylinder_flat_picking);
                    _cylinderPrimitivePipelines.cylinder_flat_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerPrimitive = 4; // Quad rendered as triangle strip.
            }

            break;

        case CylinderPrimitive::ArrowShape:

            if(primitive.shadingMode() == CylinderPrimitive::NormalShading) {
                if(!isPicking()) {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_head);
                    _cylinderPrimitivePipelines.arrow_head.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_head_picking);
                    _cylinderPrimitivePipelines.arrow_head_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerPrimitive = 14; // Box rendered as triangle strip.
            }
            else {
                if(!isPicking()) {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_flat);
                    _cylinderPrimitivePipelines.arrow_flat.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_flat_picking);
                    _cylinderPrimitivePipelines.arrow_flat_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerPrimitive = 7; // 2D arrow rendered as triangle fan.
            }

            break;

        default:
            return;
    }

    // Set up push constants.
    switch(primitive.shape()) {

        case CylinderPrimitive::CylinderShape:
        case CylinderPrimitive::ArrowShape:

            // Pass model-view-projection matrix to vertex shader as a push constant.
            deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());

            if(primitive.shadingMode() == CylinderPrimitive::NormalShading) {
                // Pass model-view transformation matrix to vertex shader as a push constant.
                // In order to match the 16-byte alignment requirements of shader interface blocks, we convert the 3x4 matrix from column-major
                // ordering to row-major ordering, with three rows or 4 floats. The shader uses "layout(row_major) mat4x3" to read the matrix.
                std::array<float, 3*4> transposed_modelview_matrix;
                {
                    auto transposed_modelview_matrix_iter = transposed_modelview_matrix.begin();
                    for(size_t row = 0; row < 3; row++)
                        for(size_t col = 0; col < 4; col++)
                            *transposed_modelview_matrix_iter++ = static_cast<float>(modelViewTM()(row,col));
                }
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(transposed_modelview_matrix), transposed_modelview_matrix.data());

                if(isPicking()) {
                    // Pass picking base ID to vertex shader as a push constant.
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.basePositions()->size());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(transposed_modelview_matrix), sizeof(pickingBaseId), &pickingBaseId);
                }
                else if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                    Vector_2<float> color_range(0,0);
                    if(renderWithPseudoColorMapping) {
                        // Rendering with pseudo-colors and a color mapping function.
                        // We pass the min/max range of the color map to the fragment shader in the push constants buffer.
                        color_range = Vector_2<float>(primitive.pseudoColorMapping().minValue(), primitive.pseudoColorMapping().maxValue());
                        // Avoid division by zero due to degenerate value interval.
                        if(color_range.y() == color_range.x()) color_range.y() = std::nextafter(color_range.y(), std::numeric_limits<float>::max());

                        // Create the descriptor set with the color map and bind it to the pipeline.
                        VkDescriptorSet colorMapSet = uploadColorMap(primitive.pseudoColorMapping().gradient());
                        deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &colorMapSet, 0, nullptr);
                    }
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Matrix_4<float>) + sizeof(transposed_modelview_matrix), sizeof(color_range), color_range.data());
                }
            }
            else {
                // Pass camera viewing direction (parallel) or camera position (perspective) in object space to vertex shader as a push constant.
                Vector_4<float> view_dir_eye_pos;
                if(projParams().isPerspective)
                    view_dir_eye_pos = Vector_4<float>(modelViewTM().inverse().column(3).toDataType<float>(), 0.0f); // Camera position in object space
                else
                    view_dir_eye_pos = Vector_4<float>(modelViewTM().inverse().column(2).toDataType<float>(), 0.0f); // Camera viewing direction in object space.
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(view_dir_eye_pos), view_dir_eye_pos.data());

                if(isPicking()) {
                    // Pass picking base ID to vertex shader as a push constant.
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.basePositions()->size());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(view_dir_eye_pos), sizeof(pickingBaseId), &pickingBaseId);
                }
                else if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                    Vector_2<float> color_range(0,0);
                    if(renderWithPseudoColorMapping) {
                        // Rendering with pseudo-colors and a color mapping function.
                        // We pass the min/max range of the color map to the fragment shader in the push constants buffer.
                        color_range = Vector_2<float>(primitive.pseudoColorMapping().minValue(), primitive.pseudoColorMapping().maxValue());
                        // Avoid division by zero due to degenerate value interval.
                        if(color_range.y() == color_range.x()) color_range.y() = std::nextafter(color_range.y(), std::numeric_limits<float>::max());

                        // Create the descriptor set with the color map and bind it to the pipeline.
                        VkDescriptorSet colorMapSet = uploadColorMap(primitive.pseudoColorMapping().gradient());
                        deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &colorMapSet, 0, nullptr);
                    }
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Matrix_4<float>) + sizeof(view_dir_eye_pos), sizeof(color_range), color_range.data());
                }
            }

            break;

        default:
            return;
    }

    // Bind the descriptor set to the pipeline.
    VkDescriptorSet globalUniformsSet = getGlobalUniformsDescriptorSet();
    deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalUniformsSet, 0, nullptr);

    // Put base/head positions and radii into one combined Vulkan buffer.
    // Radii are optional and may be substituted with a uniform radius value.
    RendererResourceKey<struct VulkanCylinderVertexCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> positionRadiusCacheKey{
        primitive.basePositions(),
        primitive.headPositions(),
        primitive.widths(),
        primitive.widths() ? FloatType(0) : primitive.uniformWidth()
    };

    // Upload vertex buffer with the base and head positions and radii.
    VkBuffer positionRadiusBuffer = context()->createCachedBuffer(positionRadiusCacheKey, primitiveCount * (sizeof(Vector_3<float>) + sizeof(Vector_3<float>) + sizeof(float)), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
        OVITO_ASSERT(!primitive.widths() || primitive.widths()->size() == primitive.basePositions()->size());
        ConstDataBufferAccess<Point3> basePositionArray(primitive.basePositions());
        ConstDataBufferAccess<Point3> headPositionArray(primitive.headPositions());
        ConstDataBufferAccess<FloatType> diameterArray(primitive.widths());
        float* dst = reinterpret_cast<float*>(buffer);
        const FloatType* diameter = diameterArray ? diameterArray.cbegin() : nullptr;
        const float uniformRadius = 0.5f * primitive.uniformWidth();
        const Point3* basePos = basePositionArray.cbegin();
        const Point3* headPos = headPositionArray.cbegin();
        for(; basePos != basePositionArray.cend(); ++basePos, ++headPos) {
            *dst++ = static_cast<float>(basePos->x());
            *dst++ = static_cast<float>(basePos->y());
            *dst++ = static_cast<float>(basePos->z());
            *dst++ = static_cast<float>(headPos->x());
            *dst++ = static_cast<float>(headPos->y());
            *dst++ = static_cast<float>(headPos->z());
            *dst++ = diameter ? (0.5f * static_cast<float>(*diameter++)) : uniformRadius;
        }
    });

    // The list of buffers that will be bound to vertex attributes.
    // We will bind the base/head positions and radii for sure. More buffers may be added to the list below.
    std::array<VkBuffer, 2> buffers = { positionRadiusBuffer };
    std::array<VkDeviceSize, 2> offsets = { 0, 0 };
    uint32_t buffersCount = 1;

    if(!isPicking()) {

        // Put colors and transparencies into one combined Vulkan buffer with 8 floats per primitive (two RGBA values).
        RendererResourceKey<struct VulkanCylinderColorCache, ConstDataBufferPtr, ConstDataBufferPtr, Color, uint32_t> colorCacheKey{
            primitive.colors(),
            primitive.transparencies(),
            primitive.colors() ? Color(0,0,0) : primitive.uniformColor(),
            primitiveCount // This is needed to NOT use the same cached buffer for rendering different number of cylinders which happen to use the same uniform color.
        };

        // Upload vertex buffer with the color data.
        VkBuffer colorBuffer = context()->createCachedBuffer(colorCacheKey, primitiveCount * 2 * sizeof(Vector_4<float>), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
            OVITO_ASSERT(!primitive.colors() || primitive.colors()->size() == primitive.basePositions()->size() || primitive.colors()->size() == 2 * primitive.basePositions()->size());
            OVITO_ASSERT(!primitive.colors() || (primitive.colors()->componentCount() == 1 && renderWithPseudoColorMapping) || (primitive.colors()->componentCount() == 3 && !renderWithPseudoColorMapping));
            OVITO_ASSERT(!primitive.transparencies() || primitive.transparencies()->size() == primitive.basePositions()->size() || primitive.transparencies()->size() == 2 * primitive.basePositions()->size());
            const ColorT<float> uniformColor = primitive.uniformColor().toDataType<float>();
            ConstDataBufferAccess<FloatType,true> colorArray(primitive.colors());
            ConstDataBufferAccess<FloatType> transparencyArray(primitive.transparencies());
            const FloatType* color = colorArray ? colorArray.cbegin() : nullptr;
            const FloatType* transparency = transparencyArray ? transparencyArray.cbegin() : nullptr;
            bool twoColorsPerPrimitive = (primitive.colors() && primitive.colors()->size() == 2 * primitive.basePositions()->size());
            bool twoTransparenciesPerPrimitive = (primitive.transparencies() && primitive.transparencies()->size() == 2 * primitive.basePositions()->size());
            for(float* dst = reinterpret_cast<float*>(buffer), *dst_end = dst + primitiveCount * 8; dst != dst_end; dst += 8) {
                // RGB/pseudocolor:
                if(renderWithPseudoColorMapping) {
                    OVITO_ASSERT(color);
                    dst[0] = static_cast<float>(*color++);
                    dst[1] = 0;
                    dst[2] = 0;
                }
                else if(color) {
                    dst[0] = static_cast<float>(*color++);
                    dst[1] = static_cast<float>(*color++);
                    dst[2] = static_cast<float>(*color++);
                }
                else {
                    dst[0] = uniformColor.r();
                    dst[1] = uniformColor.g();
                    dst[2] = uniformColor.b();
                }
                // Alpha:
                dst[3] = transparency ? qBound(0.0f, 1.0f - static_cast<float>(*transparency++), 1.0f) : 1.0f;
                // Second color and transparency.
                if(twoColorsPerPrimitive) {
                    if(renderWithPseudoColorMapping) {
                        dst[4] = static_cast<float>(*color++);
                        dst[5] = 0;
                        dst[6] = 0;
                    }
                    else {
                        dst[4] = static_cast<float>(*color++);
                        dst[5] = static_cast<float>(*color++);
                        dst[6] = static_cast<float>(*color++);
                    }
                }
                else {
                    dst[4] = dst[0];
                    dst[5] = dst[1];
                    dst[6] = dst[2];
                }
                if(twoTransparenciesPerPrimitive)
                    dst[7] = qBound(0.0f, 1.0f - static_cast<float>(*transparency++), 1.0f);
                else
                    dst[7] = dst[3];
            }
        });

        // Bind color vertex buffer.
        buffers[buffersCount++] = colorBuffer;
    }

    // Bind vertex buffers.
    deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, buffersCount, buffers.data(), offsets.data());

    // Draw triangle strip instances.
    deviceFunctions()->vkCmdDraw(currentCommandBuffer(), verticesPerPrimitive, primitiveCount, 0, 0);

    // Draw cylindric part of the arrows.
    if(primitive.shape() == CylinderPrimitive::ArrowShape && primitive.shadingMode() == CylinderPrimitive::NormalShading) {
        if(!isPicking()) {
            createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_tail);
            _cylinderPrimitivePipelines.arrow_tail.bind(*context(), currentCommandBuffer(), useBlending);
        }
        else {
            createCylinderPrimitivePipeline(_cylinderPrimitivePipelines.arrow_tail_picking);
            _cylinderPrimitivePipelines.arrow_tail_picking.bind(*context(), currentCommandBuffer());
        }
        deviceFunctions()->vkCmdDraw(currentCommandBuffer(), verticesPerPrimitive, primitiveCount, 0, 0);
    }
}

}   // End of namespace
