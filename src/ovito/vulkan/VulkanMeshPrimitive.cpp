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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include "VulkanSceneRenderer.h"

#include <boost/range/irange.hpp>

namespace Ovito {

/******************************************************************************
* Creates the Vulkan pipelines for the mesh rendering primitive.
******************************************************************************/
VulkanPipeline& VulkanSceneRenderer::createMeshPrimitivePipeline(VulkanPipeline& pipeline)
{
    if(pipeline.isCreated())
        return pipeline;

    // Are extended dynamic states supported by the Vulkan device?
    // If yes, we use the feature to dynamically turn back-face culling on and off.
    uint32_t extraDynamicStateCount = 0;
    std::array<VkDynamicState, 2> extraDynamicState;
    extraDynamicState[extraDynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    if(context()->supportsExtendedDynamicState()) {
        extraDynamicState[extraDynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE_EXT;
    }

    std::array<VkDescriptorSetLayout, 1> descriptorSetLayouts = { globalUniformsDescriptorSetLayout() };

    VkVertexInputBindingDescription vertexBindingDesc[3];
    vertexBindingDesc[0].binding = 0;
    vertexBindingDesc[0].stride = sizeof(MeshPrimitive::RenderVertex);
    vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexBindingDesc[1].binding = 1;
    vertexBindingDesc[1].stride = 3 * sizeof(Vector_4<float>);
    vertexBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    vertexBindingDesc[2].binding = 2;
    vertexBindingDesc[2].stride = sizeof(ColorAT<float>);
    vertexBindingDesc[2].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        { // position:
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(MeshPrimitive::RenderVertex, position) // offset
        },
        { // normal:
            1, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            offsetof(MeshPrimitive::RenderVertex, normal) // offset
        },
        { // color:
            2, // location
            0, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            offsetof(MeshPrimitive::RenderVertex, color) // offset
        },
        { // instance transformation (row 1):
            3, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0 * sizeof(Vector_4<float>) // offset
        },
        { // instance transformation (row 2):
            4, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            1 * sizeof(Vector_4<float>) // offset
        },
        { // instance transformation (row 3):
            5, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            2 * sizeof(Vector_4<float>) // offset
        },
        { // instance color:
            6, // location
            2, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0 // offset
        },
    };

    if(&pipeline == &_meshPrimitivePipelines.mesh)
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data(), // pSetLayouts
            true // enableDepthOffset
        );

    if(&pipeline == &_meshPrimitivePipelines.mesh_color_mapping) {
        std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts2 = { globalUniformsDescriptorSetLayout(), colorMapDescriptorSetLayout() };
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_color_mapping"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            true, // supportAlphaBlending
            descriptorSetLayouts2.size(), // setLayoutCount
            descriptorSetLayouts2.data(), // pSetLayouts
            true // enableDepthOffset
        );
    }

    if(&pipeline == &_meshPrimitivePipelines.mesh_picking)
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            1, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data(), // pSetLayouts
            true // enableDepthOffset
        );

    if(&pipeline == &_meshPrimitivePipelines.mesh_instanced)
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_instanced"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            6, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data(), // pSetLayouts
            true // enableDepthOffset
        );

    if(&pipeline == &_meshPrimitivePipelines.mesh_instanced_picking) {
        VkVertexInputAttributeDescription vertexAttrDesc[] = {
            { // position:
                0, // location
                0, // binding
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(MeshPrimitive::RenderVertex, position) // offset
            },
            { // instance transformation (row 1):
                1, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                0 * sizeof(Vector_4<float>) // offset
            },
            { // instance transformation (row 2):
                2, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                1 * sizeof(Vector_4<float>) // offset
            },
            { // instance transformation (row 3):
                3, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                2 * sizeof(Vector_4<float>) // offset
            }
        };
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_instanced_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            4, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data(), // pSetLayouts
            true // enableDepthOffset
        );
    }

    if(&pipeline == &_meshPrimitivePipelines.mesh_instanced_with_colors)
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_instanced_with_colors"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            3, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            7, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // topology
            extraDynamicStateCount,
            extraDynamicState.data(),
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data(), // pSetLayouts
            true // enableDepthOffset
        );

    if(&pipeline == &_meshPrimitivePipelines.mesh_wireframe) {
        VkVertexInputBindingDescription vertexBindingDesc[1];
        vertexBindingDesc[0].binding = 0;
        vertexBindingDesc[0].stride = sizeof(Point_3<float>);
        vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription vertexAttrDesc = { // position:
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            0 // offset
        };
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_wireframe"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>), // vertexPushConstantSize
            sizeof(ColorAT<float>),  // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            1, // vertexAttributeDescriptionCount
            &vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true // supportAlphaBlending
        );
    }

    if(&pipeline == &_meshPrimitivePipelines.mesh_wireframe_instanced) {
        VkVertexInputBindingDescription vertexBindingDesc[2];
        vertexBindingDesc[0].binding = 0;
        vertexBindingDesc[0].stride = sizeof(Point_3<float>);
        vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingDesc[1].binding = 1;
        vertexBindingDesc[1].stride = 3 * sizeof(Vector_4<float>);
        vertexBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        VkVertexInputAttributeDescription vertexAttrDesc[] = {
            { // position:
                0, // location
                0, // binding
                VK_FORMAT_R32G32B32_SFLOAT,
                offsetof(MeshPrimitive::RenderVertex, position) // offset
            },
            { // instance transformation (row 1):
                1, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                0 * sizeof(Vector_4<float>) // offset
            },
            { // instance transformation (row 2):
                2, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                1 * sizeof(Vector_4<float>) // offset
            },
            { // instance transformation (row 3):
                3, // location
                1, // binding
                VK_FORMAT_R32G32B32A32_SFLOAT,
                2 * sizeof(Vector_4<float>) // offset
            }
        };
        pipeline.create(*context(),
            QStringLiteral("mesh/mesh_wireframe_instanced"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>), // vertexPushConstantSize
            sizeof(ColorAT<float>),  // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc, 
            4, // vertexAttributeDescriptionCount
            vertexAttrDesc, 
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true // supportAlphaBlending
        );
    }

    OVITO_ASSERT(pipeline.isCreated());
    return pipeline; 
}

/******************************************************************************
* Destroys the Vulkan pipelines for this rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::releaseMeshPrimitivePipelines()
{
	_meshPrimitivePipelines.mesh.release(*context());
	_meshPrimitivePipelines.mesh_picking.release(*context());
	_meshPrimitivePipelines.mesh_wireframe.release(*context());
	_meshPrimitivePipelines.mesh_wireframe_instanced.release(*context());
	_meshPrimitivePipelines.mesh_instanced.release(*context());
	_meshPrimitivePipelines.mesh_instanced_picking.release(*context());
	_meshPrimitivePipelines.mesh_instanced_with_colors.release(*context());
	_meshPrimitivePipelines.mesh_color_mapping.release(*context());
}

/******************************************************************************
* Renders the mesh geometry.
******************************************************************************/
void VulkanSceneRenderer::renderMeshImplementation(const MeshPrimitive& primitive)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
	if(!primitive.mesh() || primitive.mesh()->faceCount() == 0)
		return;
    if(primitive.useInstancedRendering() && primitive.perInstanceTMs()->size() == 0)
        return;

    const TriMeshObject& mesh = *primitive.mesh();

    // Check size limits of the mesh.
    if(mesh.faceCount() * 3 > std::numeric_limits<VkDeviceSize>::max() / sizeof(MeshPrimitive::RenderVertex)) {
        qWarning() << "WARNING: Vulkan renderer - mesh to be rendered has too many faces, exceeding Vulkan device limits.";
        return;
    }

    // Compute full view-projection matrix including correction for OpenGL/Vulkan convention difference.
    QMatrix4x4 mvp = clipCorrection() * projParams().projectionMatrix * modelViewTM();

    // Render wireframe lines.
    if(primitive.emphasizeEdges() && !isPicking())
        renderMeshWireframeImplementation(primitive, mvp);

    // Apply optional positive depth-offset to mesh faces to make the wireframe lines fully visible.
    deviceFunctions()->vkCmdSetDepthBias(currentCommandBuffer(), primitive.emphasizeEdges() ? 1.0f : 0.0f, 0.0f, primitive.emphasizeEdges() ? 1.0f : 0.0f);

    // Are we rendering a semi-transparent mesh?
    bool useBlending = !isPicking() && !primitive.isFullyOpaque();

    // Decide whether per-pixel pseudo-color mapping is used.
    bool renderWithPseudoColorMapping = false;
    if(primitive.pseudoColorMapping().isValid() && !isPicking() && !primitive.useInstancedRendering()) {
        if(!mesh.hasVertexColors() && mesh.hasVertexPseudoColors())
            renderWithPseudoColorMapping = true;
        else if(!mesh.hasFaceColors() && mesh.hasFacePseudoColors())
            renderWithPseudoColorMapping = true;
    }

    // Bind the right pipeline.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if(!primitive.useInstancedRendering()) {
        if(isPicking()) {
            createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_picking).bind(*context(), currentCommandBuffer());
            pipelineLayout = _meshPrimitivePipelines.mesh_picking.layout();
        }
        else if(renderWithPseudoColorMapping) {
            createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_color_mapping).bind(*context(), currentCommandBuffer(), useBlending);
            pipelineLayout = _meshPrimitivePipelines.mesh_color_mapping.layout();
        }
        else {
            createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh).bind(*context(), currentCommandBuffer(), useBlending);
            pipelineLayout = _meshPrimitivePipelines.mesh.layout();
        }
    }
    else {
        OVITO_ASSERT(!renderWithPseudoColorMapping); // Note: Color mapping has not been implemented yet for instanced mesh primtives.
        if(!isPicking()) {
            if(!primitive.perInstanceColors()) {
                createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_instanced).bind(*context(), currentCommandBuffer(), useBlending);
                pipelineLayout = _meshPrimitivePipelines.mesh_instanced.layout();
            }
            else {
                createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_instanced_with_colors).bind(*context(), currentCommandBuffer(), useBlending);
                pipelineLayout = _meshPrimitivePipelines.mesh_instanced_with_colors.layout();
            }
        }
        else {
            createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_instanced_picking).bind(*context(), currentCommandBuffer());
            pipelineLayout = _meshPrimitivePipelines.mesh_instanced_picking.layout();
        }
    }

    // Turn back-face culling on/off if Vulkan implementation supports it.
    if(context()->supportsExtendedDynamicState()) {
        context()->vkCmdSetCullModeEXT(currentCommandBuffer(), primitive.cullFaces() ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    }

    // Pass model-view-projection matrix to vertex shader as a push constant.
    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());

    if(!isPicking()) {
        // Pass normal transformation matrix to vertex shader as a push constant.
        Matrix3 normal_matrix;
        if(modelViewTM().linear().inverse(normal_matrix)) {
            normal_matrix.column(0).normalize();
            normal_matrix.column(1).normalize();
            normal_matrix.column(2).normalize();
        }
        else normal_matrix.setIdentity();
        // It's almost impossible to pass a mat3 to the shader with the correct memory layout. 
        // Better use a mat4 to be safe:
        Matrix_4<float> normal_matrix4(normal_matrix.toDataType<float>().transposed());
        deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(normal_matrix4), normal_matrix4.data());
    }
    else {
        // Pass picking base ID to vertex shader as a push constant.
        uint32_t pickingBaseId = registerSubObjectIDs(primitive.useInstancedRendering() ? primitive.perInstanceTMs()->size() : mesh.faceCount());
        deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(pickingBaseId), &pickingBaseId);
    }

    // Bind the descriptor set to the pipeline.
    VkDescriptorSet globalUniformsSet = getGlobalUniformsDescriptorSet();
    deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalUniformsSet, 0, nullptr);

    // The lookup key for the Vulkan buffer cache.
    RendererResourceKey<struct VulkanMeshPrimitiveCache, DataOORef<const TriMeshObject>, std::vector<ColorA>, ColorA, Color> meshCacheKey{
        primitive.mesh(),
        primitive.materialColors(),
        primitive.uniformColor(),
        primitive.faceSelectionColor()
    };

    // Upload vertex buffer to GPU memory.
    VkBuffer meshBuffer = context()->createCachedBuffer(meshCacheKey, mesh.faceCount() * 3 * sizeof(MeshPrimitive::RenderVertex), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
        bool highlightSelectedFaces = isInteractive() && !isPicking();
    	primitive.generateRenderableVertices(reinterpret_cast<MeshPrimitive::RenderVertex*>(buffer), highlightSelectedFaces, renderWithPseudoColorMapping);
    });

    // Bind vertex buffer.
    VkDeviceSize offset = 0;
    deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, 1, &meshBuffer, &offset);

    // Are we rendering with pseudo-colors and a color mapping function.
	if(renderWithPseudoColorMapping) {
        // We pass the min/max range of the color map to the vertex shader in the push constants buffer.
        // But since the push constants buffer is already occupied with two mat4 matrices (128 bytes), we 
        // have to squeeze the values into unused elements of the normal transformation matrix.
        Vector_2<float> color_range(primitive.pseudoColorMapping().minValue(), primitive.pseudoColorMapping().maxValue());
        // Avoid division by zero due to degenerate value interval.
        if(color_range.y() == color_range.x()) color_range.y() = std::nextafter(color_range.y(), std::numeric_limits<float>::max());
        deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(float) * 4 * 3, sizeof(color_range), color_range.data());

        // Create the descriptor set with the color map and bind it to the pipeline.
        VkDescriptorSet colorMapSet = uploadColorMap(primitive.pseudoColorMapping().gradient());
        deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &colorMapSet, 0, nullptr);
    }

    // The number of instances the Vulkan draw command should draw.
    uint32_t renderInstanceCount = 1;

    if(primitive.useInstancedRendering()) {
        renderInstanceCount = primitive.perInstanceTMs()->size();

        // Upload the per-instance TMs to GPU memory.
        VkBuffer instanceTMBuffer = getMeshInstanceTMBuffer(primitive);
        if(!instanceTMBuffer)
            return;

        // Bind buffer with the instance matrices to the second binding of the shader.
        VkDeviceSize offset = 0;
        deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 1, 1, &instanceTMBuffer, &offset);

        if(primitive.perInstanceColors() && !isPicking()) {
            // Upload the per-instance colors to GPU memory.
            VkBuffer instanceColorBuffer = context()->uploadDataBuffer(primitive.perInstanceColors(), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

            // Bind buffer with the instance colors to the third binding of the shader.
            VkDeviceSize offset = 0;
            deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 2, 1, &instanceColorBuffer, &offset);
        }
    }

    if(isPicking() || primitive.isFullyOpaque()) {
        // Draw triangles in regular storage order (not sorted).
        deviceFunctions()->vkCmdDraw(currentCommandBuffer(), mesh.faceCount() * 3, renderInstanceCount, 0, 0);
    }
    else if(primitive.depthSortingMode() == MeshPrimitive::ConvexShapeMode) {
        // Assuming that the input mesh is convex, render semi-transparent triangles in two passes: 
        // First, render triangles facing away from the viewer, then render triangles facing toward the viewer.
        // Each time we pass the entire triangle list to Vulkan and use Vulkan's backface/frontfrace culling
        // option to render the right subset of triangles.
        if(!primitive.cullFaces() && context()->supportsExtendedDynamicState()) {
            // First pass is only needed if backface culling is not active.
            context()->vkCmdSetCullModeEXT(currentCommandBuffer(), VK_CULL_MODE_FRONT_BIT);
            deviceFunctions()->vkCmdDraw(currentCommandBuffer(), mesh.faceCount() * 3, renderInstanceCount, 0, 0);
        }
        // Now render front-facing triangles only.
        if(context()->supportsExtendedDynamicState())
            context()->vkCmdSetCullModeEXT(currentCommandBuffer(), VK_CULL_MODE_BACK_BIT);
        deviceFunctions()->vkCmdDraw(currentCommandBuffer(), mesh.faceCount() * 3, renderInstanceCount, 0, 0);
    }
    else if(!primitive.useInstancedRendering()) {
        // Create a buffer for an indexed drawing command to render the triangles in back-to-front order. 

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the index buffer.
        RendererResourceKey<struct VulkanMeshPrimitiveOrderCache, VkBuffer, Vector3> indexBufferCacheKey{
            meshBuffer,
            direction
        };

        // Create index buffer with three entries per triangle face.
        VkBuffer indexBuffer = context()->createCachedBuffer(indexBufferCacheKey, mesh.faceCount() * 3 * sizeof(uint32_t), currentResourceFrame(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, [&](void* buffer) {

            // Compute each face's center point.
            std::vector<Vector_3<float>> faceCenters(mesh.faceCount());
            auto tc = faceCenters.begin();
            for(auto face = mesh.faces().cbegin(); face != mesh.faces().cend(); ++face, ++tc) {
                // Compute centroid of triangle.
                const auto& v1 = mesh.vertex(face->vertex(0));
                const auto& v2 = mesh.vertex(face->vertex(1));
                const auto& v3 = mesh.vertex(face->vertex(2));
                tc->x() = (float)(v1.x() + v2.x() + v3.x()) / 3.0f;
                tc->y() = (float)(v1.y() + v2.y() + v3.y()) / 3.0f;
                tc->z() = (float)(v1.z() + v2.z() + v3.z()) / 3.0f;
            }

            // Next, compute distance of each face from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(mesh.faceCount());
            boost::transform(faceCenters, distances.begin(), [direction = direction.toDataType<float>()](const Vector_3<float>& v) {
                return direction.dot(v);
            });

            // Create index array with all face indices.
            std::vector<uint32_t> sortedIndices(mesh.faceCount());
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort face indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            // Fill the index buffer with vertex indices to render.
            uint32_t* dst = reinterpret_cast<uint32_t*>(buffer);
            for(uint32_t index : sortedIndices) {
                *dst++ = index * 3;
                *dst++ = index * 3 + 1;
                *dst++ = index * 3 + 2;
            }
        });

        // Bind index buffer.
        deviceFunctions()->vkCmdBindIndexBuffer(currentCommandBuffer(), indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw triangles in sorted order.
        deviceFunctions()->vkCmdDrawIndexed(currentCommandBuffer(), mesh.faceCount() * 3, renderInstanceCount, 0, 0, 0);
    }
    else {
        // Create a buffer for an indirect drawing command to render the particles in back-to-front order. 

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the indirect drawing command buffer.
        RendererResourceKey<struct VulkanMeshPrimitiveInstanceOrderCache, ConstDataBufferPtr, Vector3> indirectBufferCacheKey{
            primitive.perInstanceTMs(),
            direction
        };

        // Create indirect drawing buffer.
        VkBuffer indirectBuffer = context()->createCachedBuffer(indirectBufferCacheKey, renderInstanceCount * sizeof(VkDrawIndirectCommand), currentResourceFrame(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, [&](void* buffer) {

            // First, compute distance of each instance from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(renderInstanceCount);
			boost::transform(boost::irange<size_t>(0, renderInstanceCount), distances.begin(), [direction, tmArray = ConstDataBufferAccess<AffineTransformation>(primitive.perInstanceTMs())](size_t i) {
				return direction.dot(tmArray[i].translation());
			});

            // Create index array with all indices.
            std::vector<uint32_t> sortedIndices(renderInstanceCount);
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            // Fill the buffer with VkDrawIndirectCommand records.
            VkDrawIndirectCommand* dst = reinterpret_cast<VkDrawIndirectCommand*>(buffer);
            for(uint32_t index : sortedIndices) {
                dst->vertexCount = mesh.faceCount() * 3;
                dst->instanceCount = 1;
                dst->firstVertex = 0;
                dst->firstInstance = index;
                ++dst;
            }
        });

        // Draw instances in sorted order.
        deviceFunctions()->vkCmdDrawIndirect(currentCommandBuffer(), indirectBuffer, 0, renderInstanceCount, sizeof(VkDrawIndirectCommand));
    }
}

/******************************************************************************
* Prepares the Vulkan buffer with the per-instance transformation matrices.
******************************************************************************/
VkBuffer VulkanSceneRenderer::getMeshInstanceTMBuffer(const MeshPrimitive& primitive)
{
    OVITO_ASSERT(primitive.useInstancedRendering() && primitive.perInstanceTMs());

    // Check size limit.
    if(primitive.perInstanceTMs()->size() > std::numeric_limits<VkDeviceSize>::max() / (3 * sizeof(Vector_4<float>))) {
        qWarning() << "WARNING: Vulkan renderer - Number of mesh instances to be rendered exceeds device limits";
        return {};
    }

    // The lookup key for storing the per-instance TMs in the Vulkan buffer cache.
    RendererResourceKey<struct VulkanMeshPrimitiveInstanceTMCache, ConstDataBufferPtr> instanceTMsKey{ primitive.perInstanceTMs() };

    // Upload the per-instance TMs to GPU memory.
    VkBuffer instanceTMBuffer = context()->createCachedBuffer(instanceTMsKey, primitive.perInstanceTMs()->size() * 3 * sizeof(Vector_4<float>), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
        Vector_4<float>* row = reinterpret_cast<Vector_4<float>*>(buffer);
        for(const AffineTransformation& tm : ConstDataBufferAccess<AffineTransformation>(primitive.perInstanceTMs())) {
            *row++ = tm.row(0).toDataType<float>();
            *row++ = tm.row(1).toDataType<float>();
            *row++ = tm.row(2).toDataType<float>();
        }
    });

    return instanceTMBuffer;
}

/******************************************************************************
* Generates the list of wireframe line elements.
******************************************************************************/
ConstDataBufferPtr VulkanSceneRenderer::generateMeshWireframeLines(const MeshPrimitive& primitive)
{
    OVITO_ASSERT(primitive.emphasizeEdges());

    // Cache the wireframe geometry generated for the current mesh.
    RendererResourceKey<struct WireframeCache, DataOORef<const TriMeshObject>> cacheKey(primitive.mesh());
    ConstDataBufferPtr& wireframeLines = context()->lookup<ConstDataBufferPtr>(std::move(cacheKey), currentResourceFrame());

    if(!wireframeLines) {
        wireframeLines = primitive.generateWireframeLines();
    }

    return wireframeLines;
}

/******************************************************************************
* Renders the mesh wireframe edges.
******************************************************************************/
void VulkanSceneRenderer::renderMeshWireframeImplementation(const MeshPrimitive& primitive, const QMatrix4x4& mvp)
{
    bool useBlending = (primitive.uniformColor().a() < 1.0);
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    OVITO_ASSERT(!isPicking());

    // Bind the pipeline.
    if(!primitive.useInstancedRendering()) {
        createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_wireframe).bind(*context(), currentCommandBuffer(), useBlending);
        pipelineLayout = _meshPrimitivePipelines.mesh_wireframe.layout();
    }
    else {
        createMeshPrimitivePipeline(_meshPrimitivePipelines.mesh_wireframe_instanced).bind(*context(), currentCommandBuffer(), useBlending);
        pipelineLayout = _meshPrimitivePipelines.mesh_wireframe_instanced.layout();
    }

    // Pass transformation matrix to vertex shader as a push constant.
    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());

    // Pass uniform line color to fragment shader as a push constant.
    ColorAT<float> wireframeColor(0.1f, 0.1f, 0.1f, (float)primitive.uniformColor().a());
    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Matrix_4<float>), sizeof(wireframeColor), wireframeColor.data());

    // Bind vertex buffer for wireframe vertex positions.
    VkBuffer buffer = context()->uploadDataBuffer(generateMeshWireframeLines(primitive), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    VkDeviceSize offset = 0;
    deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, 1, &buffer, &offset);

    // Bind vertex buffer for instance TMs.
    if(primitive.useInstancedRendering()) {
        VkBuffer buffer = getMeshInstanceTMBuffer(primitive);
        if(!buffer) return;
        deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 1, 1, &buffer, &offset);
    }

    // Draw lines.
    deviceFunctions()->vkCmdDraw(currentCommandBuffer(), generateMeshWireframeLines(primitive)->size(), primitive.useInstancedRendering() ? primitive.perInstanceTMs()->size() : 1, 0, 0);
}

}	// End of namespace
