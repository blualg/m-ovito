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

#include <boost/range/irange.hpp>

namespace Ovito {

/******************************************************************************
* Creates the Vulkan pipelines for the particle rendering primitive.
******************************************************************************/
VulkanPipeline& VulkanSceneRenderer::createParticlePrimitivePipeline(VulkanPipeline& pipeline)
{
    if(pipeline.isCreated())
        return pipeline;

    std::array<VkVertexInputBindingDescription, 4> vertexBindingDesc;

    // Position + radius:
    vertexBindingDesc[0].binding = 0;
    vertexBindingDesc[0].stride = sizeof(Vector_4<float>);
    vertexBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Color + alpha
    vertexBindingDesc[1].binding = 1;
    vertexBindingDesc[1].stride = sizeof(Vector_4<float>);
    vertexBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Shape + orientation
    vertexBindingDesc[2].binding = 2;
    vertexBindingDesc[2].stride = sizeof(Matrix_4<float>);
    vertexBindingDesc[2].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Roundness
    vertexBindingDesc[3].binding = 3;
    vertexBindingDesc[3].stride = sizeof(Vector_2<float>);
    vertexBindingDesc[3].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    VkVertexInputAttributeDescription vertexAttrDesc[] = {
        VkVertexInputAttributeDescription{ // position:
            0, // location
            0, // binding
            VK_FORMAT_R32G32B32_SFLOAT,
            0 // offset
        },
        VkVertexInputAttributeDescription{ // radius:
            1, // location
            0, // binding
            VK_FORMAT_R32_SFLOAT,
            3 * sizeof(float) // offset
        },
        VkVertexInputAttributeDescription{ // color:
            2, // location
            1, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0 // offset
        },
        VkVertexInputAttributeDescription{ // shape_orientation matrix (column 1):
            3, // location
            2, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            0 * sizeof(Matrix_4<float>::column_type) // offset
        },
        VkVertexInputAttributeDescription{ // shape_orientation matrix (column 2):
            4, // location
            2, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            1 * sizeof(Matrix_4<float>::column_type) // offset
        },
        VkVertexInputAttributeDescription{ // shape_orientation matrix (column 3):
            5, // location
            2, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            2 * sizeof(Matrix_4<float>::column_type) // offset
        },
        VkVertexInputAttributeDescription{ // shape_orientation matrix (column 4):
            6, // location
            2, // binding
            VK_FORMAT_R32G32B32A32_SFLOAT,
            3 * sizeof(Matrix_4<float>::column_type) // offset
        },
        VkVertexInputAttributeDescription{ // roundness:
            7, // location
            3, // binding
            VK_FORMAT_R32G32_SFLOAT,
            0 // offset
        },
    };

    std::array<VkDescriptorSetLayout, 1> descriptorSetLayouts = { globalUniformsDescriptorSetLayout() };

    if(&pipeline == &_particlePrimitivePipelines.cube)
        pipeline.create(*context(),
            QStringLiteral("particles/cube/cube"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.cube_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/cube/cube_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.sphere)
        pipeline.create(*context(),
            QStringLiteral("particles/sphere/sphere"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.sphere_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/sphere/sphere_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.square)
        pipeline.create(*context(),
            QStringLiteral("particles/square/square"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.square_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/square/square_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.circle)
        pipeline.create(*context(),
            QStringLiteral("particles/circle/circle"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.circle_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/circle/circle_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.imposter)
        pipeline.create(*context(),
            QStringLiteral("particles/imposter/imposter"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            3, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.imposter_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/imposter/imposter_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            1, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            2, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.box)
        pipeline.create(*context(),
            QStringLiteral("particles/box/box"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(Matrix_4<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            3, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            7, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    VkVertexInputBindingDescription vertexBindingDescBoxPicking[3] {
        vertexBindingDesc[0], vertexBindingDesc[2]
    };
    vertexBindingDescBoxPicking[1].binding = 1;
    VkVertexInputAttributeDescription vertexAttrDescBoxPicking[7] = {
        vertexAttrDesc[0], vertexAttrDesc[1],
        vertexAttrDesc[3], vertexAttrDesc[4],
        vertexAttrDesc[5], vertexAttrDesc[6]
    };
    vertexAttrDescBoxPicking[2].binding = vertexAttrDescBoxPicking[3].binding = vertexAttrDescBoxPicking[4].binding = vertexAttrDescBoxPicking[5].binding = 1;

    if(&pipeline == &_particlePrimitivePipelines.box_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/box/box_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDescBoxPicking,
            6, // vertexAttributeDescriptionCount
            vertexAttrDescBoxPicking,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.ellipsoid)
        pipeline.create(*context(),
            QStringLiteral("particles/ellipsoid/ellipsoid"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            3, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            7, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.ellipsoid_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/ellipsoid/ellipsoid_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            2, // vertexBindingDescriptionCount
            vertexBindingDescBoxPicking,
            6, // vertexAttributeDescriptionCount
            vertexAttrDescBoxPicking,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    if(&pipeline == &_particlePrimitivePipelines.superquadric)
        pipeline.create(*context(),
            QStringLiteral("particles/superquadric/superquadric"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            4, // vertexBindingDescriptionCount
            vertexBindingDesc.data(),
            8, // vertexAttributeDescriptionCount
            vertexAttrDesc,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            true, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    // Roundness
    vertexBindingDescBoxPicking[2].binding = 2;
    vertexBindingDescBoxPicking[2].stride = sizeof(Vector_2<float>);
    vertexBindingDescBoxPicking[2].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    vertexAttrDescBoxPicking[6] =
        VkVertexInputAttributeDescription{ // roundness:
            7, // location
            2, // binding
            VK_FORMAT_R32G32_SFLOAT,
            0 // offset
        };

    if(&pipeline == &_particlePrimitivePipelines.superquadric_picking)
        pipeline.create(*context(),
            QStringLiteral("particles/superquadric/superquadric_picking"),
            defaultRenderPass(),
            sizeof(Matrix_4<float>) + sizeof(AffineTransformationT<float>) + sizeof(uint32_t), // vertexPushConstantSize
            0, // fragmentPushConstantSize
            3, // vertexBindingDescriptionCount
            vertexBindingDescBoxPicking,
            7, // vertexAttributeDescriptionCount
            vertexAttrDescBoxPicking,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // topology
            0, // extraDynamicStateCount
            nullptr, // pExtraDynamicStates
            false, // supportAlphaBlending
            descriptorSetLayouts.size(), // setLayoutCount
            descriptorSetLayouts.data()
        );

    OVITO_ASSERT(pipeline.isCreated());
    return pipeline;
}

/******************************************************************************
* Destroys the Vulkan pipelines for this rendering primitive.
******************************************************************************/
void VulkanSceneRenderer::releaseParticlePrimitivePipelines()
{
    _particlePrimitivePipelines.cube.release(*context());
    _particlePrimitivePipelines.cube_picking.release(*context());
    _particlePrimitivePipelines.sphere.release(*context());
    _particlePrimitivePipelines.sphere_picking.release(*context());
    _particlePrimitivePipelines.square.release(*context());
    _particlePrimitivePipelines.square_picking.release(*context());
    _particlePrimitivePipelines.circle.release(*context());
    _particlePrimitivePipelines.circle_picking.release(*context());
    _particlePrimitivePipelines.imposter.release(*context());
    _particlePrimitivePipelines.imposter_picking.release(*context());
    _particlePrimitivePipelines.box.release(*context());
    _particlePrimitivePipelines.box_picking.release(*context());
    _particlePrimitivePipelines.ellipsoid.release(*context());
    _particlePrimitivePipelines.ellipsoid_picking.release(*context());
    _particlePrimitivePipelines.superquadric.release(*context());
    _particlePrimitivePipelines.superquadric_picking.release(*context());
}

/******************************************************************************
* Renders the particles.
******************************************************************************/
void VulkanSceneRenderer::renderParticlesImplementation(const ParticlePrimitive& primitive)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;
    if(primitive.indices() && primitive.indices()->size() == 0)
        return;

    // Compute full view-projection matrix including correction for OpenGL/Vulkan convention difference.
    QMatrix4x4 mvp = clipCorrection() * projParams().projectionMatrix * modelViewTM();

    // The effective number of particles being rendered:
    uint32_t particleCount = primitive.indices() ? primitive.indices()->size() : primitive.positions()->size();
    uint32_t verticesPerParticle = 0;

    // Are we rendering semi-transparent particles?
    bool useBlending = !isPicking() && (primitive.transparencies() != nullptr);

    // Bind the right Vulkan pipeline.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    switch(primitive.particleShape()) {
        case ParticlePrimitive::SquareCubicShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(!isPicking()) {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.cube).layout();
                    _particlePrimitivePipelines.cube.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.cube_picking).layout();
                    _particlePrimitivePipelines.cube_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerParticle = 14; // Cube rendered as triangle strip.
            }
            else {
                if(!isPicking()) {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.square).layout();
                    _particlePrimitivePipelines.square.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.square_picking).layout();
                    _particlePrimitivePipelines.square_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerParticle = 4; // Square rendered as triangle strip.
            }
            break;
        case ParticlePrimitive::BoxShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(!isPicking()) {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.box).layout();
                    _particlePrimitivePipelines.box.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.box_picking).layout();
                    _particlePrimitivePipelines.box_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerParticle = 14; // Box rendered as triangle strip.
            }
            else return;
            break;
        case ParticlePrimitive::SphericalShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(primitive.renderingQuality() >= ParticlePrimitive::HighQuality) {
                    if(!isPicking()) {
                        pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.sphere).layout();
                        _particlePrimitivePipelines.sphere.bind(*context(), currentCommandBuffer(), useBlending);
                    }
                    else {
                        pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.sphere_picking).layout();
                        _particlePrimitivePipelines.sphere_picking.bind(*context(), currentCommandBuffer());
                    }
                    verticesPerParticle = 14; // Cube rendered as triangle strip.
                }
                else {
                    if(!isPicking()) {
                        pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.imposter).layout();
                        _particlePrimitivePipelines.imposter.bind(*context(), currentCommandBuffer(), useBlending);
                    }
                    else {
                        pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.imposter_picking).layout();
                        _particlePrimitivePipelines.imposter_picking.bind(*context(), currentCommandBuffer());
                    }
                    verticesPerParticle = 4; // Square rendered as triangle strip.
                }
            }
            else {
                if(!isPicking()) {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.circle).layout();
                    _particlePrimitivePipelines.circle.bind(*context(), currentCommandBuffer(), useBlending);
                }
                else {
                    pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.circle_picking).layout();
                    _particlePrimitivePipelines.circle_picking.bind(*context(), currentCommandBuffer());
                }
                verticesPerParticle = 4; // Square rendered as triangle strip.
            }
            break;
        case ParticlePrimitive::EllipsoidShape:
            if(!isPicking()) {
                pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.ellipsoid).layout();
                _particlePrimitivePipelines.ellipsoid.bind(*context(), currentCommandBuffer(), useBlending);
            }
            else {
                pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.ellipsoid_picking).layout();
                _particlePrimitivePipelines.ellipsoid_picking.bind(*context(), currentCommandBuffer());
            }
            verticesPerParticle = 14; // Box rendered as triangle strip.
            break;
        case ParticlePrimitive::SuperquadricShape:
            if(!isPicking()) {
                pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.superquadric).layout();
                _particlePrimitivePipelines.superquadric.bind(*context(), currentCommandBuffer(), useBlending);
            }
            else {
                pipelineLayout = createParticlePrimitivePipeline(_particlePrimitivePipelines.superquadric_picking).layout();
                _particlePrimitivePipelines.superquadric_picking.bind(*context(), currentCommandBuffer());
            }
            verticesPerParticle = 14; // Box rendered as triangle strip.
            break;
        default:
            return;
    }

    // Check size limits.
    int bytesPerVertex = (primitive.particleShape() == ParticlePrimitive::BoxShape || primitive.particleShape() == ParticlePrimitive::EllipsoidShape || primitive.particleShape() == ParticlePrimitive::SuperquadricShape)
        ? sizeof(Matrix_4<float>) : sizeof(Vector_4<float>);
    if(particleCount > std::numeric_limits<int32_t>::max() / verticesPerParticle / bytesPerVertex) {
        qWarning() << "WARNING: Vulkan renderer - Trying to render too many particles at once, exceeding device limits.";
        return;
    }

    // Set up push constants.
    switch(primitive.particleShape()) {
        case ParticlePrimitive::SquareCubicShape:

            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
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
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size(), primitive.indices());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(pickingBaseId), &pickingBaseId);
                }
            }
            else {
                // Pass projection matrix to vertex shader as a push constant.
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), (clipCorrection() * projParams().projectionMatrix).toDataType<float>().data());

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
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size(), primitive.indices());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(transposed_modelview_matrix), sizeof(pickingBaseId), &pickingBaseId);
                }
            }

            break;

        case ParticlePrimitive::BoxShape:

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
                uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size(), primitive.indices());
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>), sizeof(pickingBaseId), &pickingBaseId);
            }
            break;

        case ParticlePrimitive::SphericalShape:
        case ParticlePrimitive::EllipsoidShape:
        case ParticlePrimitive::SuperquadricShape:

            if(primitive.particleShape() != ParticlePrimitive::SphericalShape || (primitive.shadingMode() == ParticlePrimitive::NormalShading && primitive.renderingQuality() >= ParticlePrimitive::HighQuality)) {
                // Pass model-view-projection matrix to vertex shader as a push constant.
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), mvp.data());

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
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size(), primitive.indices());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(transposed_modelview_matrix), sizeof(pickingBaseId), &pickingBaseId);
                }
            }
            else {
                // Pass projection matrix to vertex shader as a push constant.
                deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix_4<float>), (clipCorrection() * projParams().projectionMatrix).toDataType<float>().data());

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
                    uint32_t pickingBaseId = registerSubObjectIDs(primitive.positions()->size(), primitive.indices());
                    deviceFunctions()->vkCmdPushConstants(currentCommandBuffer(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(Matrix_4<float>) + sizeof(transposed_modelview_matrix), sizeof(pickingBaseId), &pickingBaseId);
                }
            }

            break;

        default:
            return;
    }

    // Bind the descriptor set to the pipeline.
    VkDescriptorSet globalUniformsSet = getGlobalUniformsDescriptorSet();
    deviceFunctions()->vkCmdBindDescriptorSets(currentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalUniformsSet, 0, nullptr);

    // Put positions and radii into one combined Vulkan buffer with 4 floats per particle.
    // Radii are optional and may be substituted with a uniform radius value.
    RendererResourceKey<struct VulkanParticlePrimitiveCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> positionRadiusCacheKey{
        primitive.indices(),
        primitive.positions(),
        primitive.radii(),
        primitive.radii() ? FloatType(0) : primitive.uniformRadius()
    };

    // Upload vertex buffer with the particle positions and radii.
    VkBuffer positionRadiusBuffer = context()->createCachedBuffer(positionRadiusCacheKey, particleCount * 4 * sizeof(float), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
        OVITO_ASSERT(!primitive.radii() || primitive.radii()->size() == primitive.positions()->size());
        ConstBufferAccess<Point3> positionArray(primitive.positions());
        ConstBufferAccess<FloatType> radiusArray(primitive.radii());
        float* dst = reinterpret_cast<float*>(buffer);
        if(!primitive.indices()) {
            const FloatType* radius = radiusArray ? radiusArray.cbegin() : nullptr;
            for(const Point3& pos : positionArray) {
                *dst++ = static_cast<float>(pos.x());
                *dst++ = static_cast<float>(pos.y());
                *dst++ = static_cast<float>(pos.z());
                *dst++ = static_cast<float>(radius ? *radius++ : primitive.uniformRadius());
            }
        }
        else {
            for(int index : ConstBufferAccess<int32_t>(primitive.indices())) {
                const Point3& pos = positionArray[index];
                *dst++ = static_cast<float>(pos.x());
                *dst++ = static_cast<float>(pos.y());
                *dst++ = static_cast<float>(pos.z());
                *dst++ = static_cast<float>(radiusArray ? radiusArray[index] : primitive.uniformRadius());
            }
        }
    });

    // The list of buffers that will be bound to vertex attributes.
    // We will bind the particle positions and radii for sure. More buffers may be added to the list below.
    std::array<VkBuffer, 4> buffers = { positionRadiusBuffer };
    std::array<VkDeviceSize, 4> offsets = { 0, 0, 0, 0 };
    uint32_t buffersCount = 1;

    if(!isPicking()) {

        // Put colors, transparencies and selection state into one combined Vulkan buffer with 4 floats per particle.
        RendererResourceKey<struct VulkanParticlePrimitiveColorCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, Color, uint32_t> colorSelectionCacheKey{
            primitive.indices(),
            primitive.colors(),
            primitive.transparencies(),
            primitive.selection(),
            primitive.colors() ? Color(0,0,0) : primitive.uniformColor(),
            particleCount // This is needed to NOT use the same cached buffer for rendering different number of particles which happen to use the same uniform color.
        };

        // Upload vertex buffer with the particle colors.
        VkBuffer colorSelectionBuffer = context()->createCachedBuffer(colorSelectionCacheKey, particleCount * 4 * sizeof(float), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
            OVITO_ASSERT(!primitive.transparencies() || primitive.transparencies()->size() == primitive.positions()->size());
            OVITO_ASSERT(!primitive.selection() || primitive.selection()->size() == primitive.positions()->size());
            ConstBufferAccess<FloatType> transparencyArray(primitive.transparencies());
            ConstBufferAccess<int> selectionArray(primitive.selection());
            const ColorT<float> uniformColor = primitive.uniformColor().toDataType<float>();
            const ColorAT<float> selectionColor = primitive.selectionColor().toDataType<float>();
            if(!primitive.indices()) {
                ConstBufferAccess<FloatType,true> colorArray(primitive.colors());
                const FloatType* color = colorArray ? colorArray.cbegin() : nullptr;
                const FloatType* transparency = transparencyArray ? transparencyArray.cbegin() : nullptr;
                const int* selection = selectionArray ? selectionArray.cbegin() : nullptr;
                for(float* dst = reinterpret_cast<float*>(buffer), *dst_end = dst + primitive.positions()->size() * 4; dst != dst_end;) {
                    if(selection && *selection++) {
                        *dst++ = selectionColor.r();
                        *dst++ = selectionColor.g();
                        *dst++ = selectionColor.b();
                        *dst++ = selectionColor.a();
                        if(color) color += 3;
                        if(transparency) transparency += 1;
                    }
                    else {
                        // RGB:
                        if(color) {
                            *dst++ = static_cast<float>(*color++);
                            *dst++ = static_cast<float>(*color++);
                            *dst++ = static_cast<float>(*color++);
                        }
                        else {
                            *dst++ = uniformColor.r();
                            *dst++ = uniformColor.g();
                            *dst++ = uniformColor.b();
                        }
                        // Alpha:
                        *dst++ = transparency ? qBound(0.0f, 1.0f - static_cast<float>(*transparency++), 1.0f) : 1.0f;
                    }
                }
            }
            else {
                ConstBufferAccess<Color> colorArray(primitive.colors());
                float* dst = reinterpret_cast<float*>(buffer);
                for(int index : ConstBufferAccess<int>(primitive.indices())) {
                    if(selectionArray && selectionArray[index]) {
                        *dst++ = selectionColor.r();
                        *dst++ = selectionColor.g();
                        *dst++ = selectionColor.b();
                        *dst++ = selectionColor.a();
                    }
                    else {
                        // RGB:
                        if(colorArray) {
                            const Color& color = colorArray[index];
                            *dst++ = static_cast<float>(color.r());
                            *dst++ = static_cast<float>(color.g());
                            *dst++ = static_cast<float>(color.b());
                        }
                        else {
                            *dst++ = uniformColor.r();
                            *dst++ = uniformColor.g();
                            *dst++ = uniformColor.b();
                        }
                        // Alpha:
                        *dst++ = transparencyArray ? qBound(0.0f, 1.0f - static_cast<float>(transparencyArray[index]), 1.0f) : 1.0f;
                    }
                }
            }
        });

        // Bind color vertex buffer.
        buffers[buffersCount++] = colorSelectionBuffer;
    }

    // For box-shaped and ellipsoid particles, we need the shape/orientation vertex attribute.
    if(primitive.particleShape() == ParticlePrimitive::BoxShape || primitive.particleShape() == ParticlePrimitive::EllipsoidShape || primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {

        // Combine aspherical shape property and orientation property into one combined Vulkan buffer containing a 4x4 transformation matrix per particle.
        RendererResourceKey<struct VulkanParticlePrimitiveShapeCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> shapeOrientationCacheKey{
            primitive.indices(),
            primitive.asphericalShapes(),
            primitive.orientations(),
            primitive.radii(),
            primitive.radii() ? FloatType(0) : primitive.uniformRadius()
        };

        // Upload vertex buffer with the particle transformation matrices.
        VkBuffer shapeOrientationBuffer = context()->createCachedBuffer(shapeOrientationCacheKey, particleCount * sizeof(Matrix_4<float>), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
            ConstBufferAccess<Vector3> asphericalShapeArray(primitive.asphericalShapes());
            ConstBufferAccess<Quaternion> orientationArray(primitive.orientations());
            ConstBufferAccess<FloatType> radiusArray(primitive.radii());
            OVITO_ASSERT(!primitive.asphericalShapes() || primitive.asphericalShapes()->size() == primitive.positions()->size());
            OVITO_ASSERT(!primitive.orientations() || primitive.orientations()->size() == primitive.positions()->size());
            if(!primitive.indices()) {
                const Vector3* shape = asphericalShapeArray ? asphericalShapeArray.cbegin() : nullptr;
                const Quaternion* orientation = orientationArray ? orientationArray.cbegin() : nullptr;
                const FloatType* radius = radiusArray ? radiusArray.cbegin() : nullptr;
                for(Matrix_4<float>* dst = reinterpret_cast<Matrix_4<float>*>(buffer), *dst_end = dst + primitive.positions()->size(); dst != dst_end; ++dst) {
                    Vector_3<float> axes;
                    if(shape) {
                        if(*shape != Vector3::Zero()) {
                            axes = (*shape).toDataType<float>();
                        }
                        else {
                            axes = Vector_3<float>(static_cast<float>(radius ? (*radius) : primitive.uniformRadius()));
                        }
                        ++shape;
                    }
                    else {
                        axes = Vector_3<float>(static_cast<float>(radius ? (*radius) : primitive.uniformRadius()));
                    }
                    if(radius)
                        ++radius;

                    if(orientation) {
                        QuaternionT<float> quat = (orientation++)->toDataType<float>();
                        float c = sqrt(quat.dot(quat));
                        if(c <= (float)FLOATTYPE_EPSILON)
                            quat.setIdentity();
                        else
                            quat /= c;
                        *dst = Matrix_4<float>(
                                quat * Vector_3<float>(axes.x(), 0.0f, 0.0f),
                                quat * Vector_3<float>(0.0f, axes.y(), 0.0f),
                                quat * Vector_3<float>(0.0f, 0.0f, axes.z()),
                                Vector_3<float>::Zero());
                    }
                    else {
                        *dst = Matrix_4<float>(
                                axes.x(), 0.0f, 0.0f, 0.0f,
                                0.0f, axes.y(), 0.0f, 0.0f,
                                0.0f, 0.0f, axes.z(), 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f);
                    }
                }
            }
            else {
                Matrix_4<float>* dst = reinterpret_cast<Matrix_4<float>*>(buffer);
                for(int index : ConstBufferAccess<int>(primitive.indices())) {
                    Vector_3<float> axes;
                    if(asphericalShapeArray && asphericalShapeArray[index] != Vector3::Zero()) {
                        axes = asphericalShapeArray[index].toDataType<float>();
                    }
                    else {
                        axes = Vector_3<float>(static_cast<float>(radiusArray ? radiusArray[index] : primitive.uniformRadius()));
                    }

                    if(orientationArray) {
                        QuaternionT<float> quat = orientationArray[index].toDataType<float>();
                        float c = sqrt(quat.dot(quat));
                        if(c <= (float)FLOATTYPE_EPSILON)
                            quat.setIdentity();
                        else
                            quat /= c;
                        *dst = Matrix_4<float>(
                                quat * Vector_3<float>(axes.x(), 0.0f, 0.0f),
                                quat * Vector_3<float>(0.0f, axes.y(), 0.0f),
                                quat * Vector_3<float>(0.0f, 0.0f, axes.z()),
                                Vector_3<float>::Zero());
                    }
                    else {
                        *dst = Matrix_4<float>(
                                axes.x(), 0.0f, 0.0f, 0.0f,
                                0.0f, axes.y(), 0.0f, 0.0f,
                                0.0f, 0.0f, axes.z(), 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f);
                    }
                    ++dst;
                }
            }
        });

        // Bind shape/orientation vertex buffer.
        buffers[buffersCount++] = shapeOrientationBuffer;
    }

    // For superquadric particles, we need to prepare the roundness vertex attribute.
    if(primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {

        RendererResourceKey<struct VulkanParticlePrimitiveSQCache, ConstDataBufferPtr, ConstDataBufferPtr> roundnessCacheKey{
            primitive.indices(),
            primitive.roundness()
        };

        // Upload vertex buffer with the roundness values.
        VkBuffer roundnessBuffer = context()->createCachedBuffer(roundnessCacheKey, particleCount * sizeof(Vector_2<float>), currentResourceFrame(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, [&](void* buffer) {
            Vector_2<float>* dst = reinterpret_cast<Vector_2<float>*>(buffer);
            if(primitive.roundness()) {
                OVITO_ASSERT(primitive.roundness()->size() == primitive.positions()->size());
                if(!primitive.indices()) {
                    for(const Vector2& r : ConstBufferAccess<Vector2>(primitive.roundness())) {
                        *dst++ = r.toDataType<float>();
                    }
                }
                else {
                    ConstBufferAccess<Vector2> roundnessArray(primitive.roundness());
                    for(int index : ConstBufferAccess<int>(primitive.indices())) {
                        *dst++ = roundnessArray[index].toDataType<float>();
                    }
                }
            }
            else {
                std::fill(dst, dst + particleCount, Vector_2<float>(1,1));
            }
        });

        // Bind vertex buffer.
        buffers[buffersCount++] = roundnessBuffer;
    }

    // Bind vertex buffers.
    deviceFunctions()->vkCmdBindVertexBuffers(currentCommandBuffer(), 0, buffersCount, buffers.data(), offsets.data());

    // Check indirect drawing capabilities of Vulkan device, which are needed for depth-sorted rendering.
    bool indirectDrawingSupported = context()->supportsMultiDrawIndirect()
        && context()->supportsDrawIndirectFirstInstance()
        && particleCount <= context()->physicalDeviceProperties()->limits.maxDrawIndirectCount;

    if(!useBlending || !indirectDrawingSupported) {
        // Draw triangle strip instances in regular storage order (not sorted).
        deviceFunctions()->vkCmdDraw(currentCommandBuffer(), verticesPerParticle, particleCount, 0, 0);
    }
    else {
        // Create a buffer for an indirect drawing command to render the particles in back-to-front order.

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the indirect drawing command buffer.
        RendererResourceKey<struct VulkanParticlePrimitiveOrderCache, ConstDataBufferPtr, ConstDataBufferPtr, Vector3, int> indirectBufferCacheKey{
            primitive.indices(),
            primitive.positions(),
            direction,
            verticesPerParticle
        };

        // Create indirect drawing buffer.
        VkBuffer indirectBuffer = context()->createCachedBuffer(indirectBufferCacheKey, particleCount * sizeof(VkDrawIndirectCommand), currentResourceFrame(), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, [&](void* buffer) {

            // First, compute distance of each particle from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(particleCount);
            if(!primitive.indices()) {
                boost::transform(boost::irange<size_t>(0, particleCount), distances.begin(), [direction, positionsArray = ConstBufferAccess<Vector3>(primitive.positions())](size_t i) {
                    return direction.dot(positionsArray[i]);
                });
            }
            else {
                boost::transform(ConstBufferAccess<int>(primitive.indices()), distances.begin(), [direction, positionsArray = ConstBufferAccess<Vector3>(primitive.positions())](size_t i) {
                    return direction.dot(positionsArray[i]);
                });
            }

            // Create index array with all particle indices.
            std::vector<uint32_t> sortedIndices(particleCount);
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort particle indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            // Fill the buffer with VkDrawIndirectCommand records.
            VkDrawIndirectCommand* dst = reinterpret_cast<VkDrawIndirectCommand*>(buffer);
            for(uint32_t index : sortedIndices) {
                dst->vertexCount = verticesPerParticle;
                dst->instanceCount = 1;
                dst->firstVertex = 0;
                dst->firstInstance = index;
                ++dst;
            }
        });

        // Draw triangle strip instances in sorted order.
        deviceFunctions()->vkCmdDrawIndirect(currentCommandBuffer(), indirectBuffer, 0, particleCount, sizeof(VkDrawIndirectCommand));
    }
}

}   // End of namespace
