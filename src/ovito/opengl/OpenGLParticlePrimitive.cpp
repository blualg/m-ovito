////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/utilities/SortZipped.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "OpenGLRenderingJob.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Renders a set of particles.
******************************************************************************/
void OpenGLRenderingJob::renderParticlesImplementation(const ParticlePrimitive& primitive, const FrameGraph::RenderingCommand& command)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;
    if(primitive.indices() && primitive.indices()->size() == 0)
        return;

    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    switch(primitive.particleShape()) {
        case ParticlePrimitive::SquareCubicShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(!useGeometryShaders()) {
                    if(!isPickingPass())
                        shader.load("cube", "particles/cube/cube.vert", "particles/cube/cube.frag");
                    else
                        shader.load("cube_picking", "particles/cube/cube_picking.vert", "particles/cube/cube_picking.frag");
                    shader.setVerticesPerInstance(14); // Cube rendered as triangle strip.
                }
                else {
                    if(!isPickingPass())
                        shader.load("cube.geom", "particles/cube/cube.geom.vert", "particles/cube/cube.frag", "particles/cube/cube.geom");
                    else
                        shader.load("cube_picking.geom", "particles/cube/cube_picking.geom.vert", "particles/cube/cube_picking.frag", "particles/cube/cube_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            else {
                if(!useGeometryShaders()) {
                    if(!isPickingPass())
                        shader.load("square", "particles/square/square.vert", "particles/square/square.frag");
                    else
                        shader.load("square_picking", "particles/square/square_picking.vert", "particles/square/square_picking.frag");
                    shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                }
                else {
                    if(!isPickingPass())
                        shader.load("square.geom", "particles/square/square.geom.vert", "particles/square/square.frag", "particles/square/square.geom");
                    else
                        shader.load("square_picking.geom", "particles/square/square_picking.geom.vert", "particles/square/square_picking.frag", "particles/square/square_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            break;
        case ParticlePrimitive::BoxShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(!useGeometryShaders()) {
                    if(!isPickingPass())
                        shader.load("box", "particles/box/box.vert", "particles/box/box.frag");
                    else
                        shader.load("box_picking", "particles/box/box_picking.vert", "particles/box/box_picking.frag");
                    shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
                }
                else {
                    if(!isPickingPass())
                        shader.load("box.geom", "particles/box/box.geom.vert", "particles/box/box.frag", "particles/box/box.geom");
                    else
                        shader.load("box_picking.geom", "particles/box/box_picking.geom.vert", "particles/box/box_picking.frag", "particles/box/box_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            else return;
            break;
        case ParticlePrimitive::SphericalShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(primitive.renderingQuality() >= ParticlePrimitive::HighQuality) {
                    if(!useGeometryShaders()) {
                        if(!isPickingPass())
                            shader.load("sphere", "particles/sphere/sphere.vert", "particles/sphere/sphere.frag");
                        else
                            shader.load("sphere_picking", "particles/sphere/sphere_picking.vert", "particles/sphere/sphere_picking.frag");
                        shader.setVerticesPerInstance(4); // Billboard quad geometry rendered as triangle strip.
                    }
                    else {
                        if(!isPickingPass())
                            shader.load("sphere.geom", "particles/sphere/sphere.geom.vert", "particles/sphere/sphere.frag", "particles/sphere/sphere.geom");
                        else
                            shader.load("sphere_picking.geom", "particles/sphere/sphere_picking.geom.vert", "particles/sphere/sphere_picking.frag", "particles/sphere/sphere_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a single point primitive.
                    }
                }
                else if(primitive.renderingQuality() >= ParticlePrimitive::MediumQuality) {
                    if(!useGeometryShaders()) {
                        if(!isPickingPass())
                            shader.load("imposter", "particles/imposter/imposter.vert", "particles/imposter/imposter.frag");
                        else
                            shader.load("imposter_picking", "particles/imposter/imposter_picking.vert", "particles/imposter/imposter_picking.frag");
                        shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                    }
                    else {
                        if(!isPickingPass())
                            shader.load("imposter.geom", "particles/imposter/imposter.geom.vert", "particles/imposter/imposter.frag", "particles/imposter/imposter.geom");
                        else
                            shader.load("imposter_picking.geom", "particles/imposter/imposter_picking.geom.vert", "particles/imposter/imposter_picking.frag", "particles/imposter/imposter_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                    }
                }
                else {
                    if(!useGeometryShaders()) {
                        if(!isPickingPass())
                            shader.load("imposter_flat", "particles/imposter_flat/imposter_flat.vert", "particles/imposter_flat/imposter_flat.frag");
                        else
                            shader.load("imposter_flat_picking", "particles/imposter_flat/imposter_flat_picking.vert", "particles/imposter_flat/imposter_flat_picking.frag");
                        shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                    }
                    else {
                        if(!isPickingPass())
                            shader.load("imposter_flat.geom", "particles/imposter_flat/imposter_flat.geom.vert", "particles/imposter_flat/imposter_flat.frag", "particles/imposter_flat/imposter_flat.geom");
                        else
                            shader.load("imposter_flat_picking.geom", "particles/imposter_flat/imposter_flat_picking.geom.vert", "particles/imposter_flat/imposter_flat_picking.frag", "particles/imposter_flat/imposter_flat_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                    }
                }
            }
            else {
                if(!useGeometryShaders()) {
                    if(!isPickingPass())
                        shader.load("circle", "particles/circle/circle.vert", "particles/circle/circle.frag");
                    else
                        shader.load("circle_picking", "particles/circle/circle_picking.vert", "particles/circle/circle_picking.frag");
                    shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                }
                else {
                    if(!isPickingPass())
                        shader.load("circle.geom", "particles/circle/circle.geom.vert", "particles/circle/circle.frag", "particles/circle/circle.geom");
                    else
                        shader.load("circle_picking.geom", "particles/circle/circle_picking.geom.vert", "particles/circle/circle_picking.frag", "particles/circle/circle_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            break;
        case ParticlePrimitive::EllipsoidShape:
            if(!useGeometryShaders()) {
                if(!isPickingPass())
                    shader.load("ellipsoid", "particles/ellipsoid/ellipsoid.vert", "particles/ellipsoid/ellipsoid.frag");
                else
                    shader.load("ellipsoid_picking", "particles/ellipsoid/ellipsoid_picking.vert", "particles/ellipsoid/ellipsoid_picking.frag");
                shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
            }
            else {
                if(!isPickingPass())
                    shader.load("ellipsoid.geom", "particles/ellipsoid/ellipsoid.geom.vert", "particles/ellipsoid/ellipsoid.frag", "particles/ellipsoid/ellipsoid.geom");
                else
                    shader.load("ellipsoid_picking.geom", "particles/ellipsoid/ellipsoid_picking.geom.vert", "particles/ellipsoid/ellipsoid_picking.frag", "particles/ellipsoid/ellipsoid_picking.geom");
                shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
            }
            break;
        case ParticlePrimitive::SuperquadricShape:
            if(!useGeometryShaders()) {
                if(!isPickingPass())
                    shader.load("superquadric", "particles/superquadric/superquadric.vert", "particles/superquadric/superquadric.frag");
                else
                    shader.load("superquadric_picking", "particles/superquadric/superquadric_picking.vert", "particles/superquadric/superquadric_picking.frag");
                shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
            }
            else {
                if(!isPickingPass())
                    shader.load("superquadric.geom", "particles/superquadric/superquadric.geom.vert", "particles/superquadric/superquadric.frag", "particles/superquadric/superquadric.geom");
                else
                    shader.load("superquadric_picking.geom", "particles/superquadric/superquadric_picking.geom.vert", "particles/superquadric/superquadric_picking.frag", "particles/superquadric/superquadric_picking.geom");
                shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
            }
            break;
        default:
            return;
    }

    // The number of particles in the input arrays:
    shader.setInstanceCount(primitive.positions()->size());

    // Specify the subset of particles to be rendered.
    if(primitive.indices())
        shader.enableSubsetRendering(primitive.indices());

    // Check VBO size limits.
    const int32_t maxVBOSize = std::numeric_limits<int32_t>::max() / sizeof(Vector4F); // Conservative estimate assuming 4-component vertex attributes.
    const int32_t maxNumParticles = maxVBOSize / shader.verticesPerInstance();
    if(shader.instanceCount() > maxNumParticles) {
        _renderBuffer->reportIssue(tr("Rendering skipped: Too many particles to render at once (%1), exceeding device limits. Maximum number of particles: %2").arg(shader.instanceCount()).arg(maxNumParticles));
        return;
    }

    // Are we rendering semi-transparent particles?
    bool useBlending = !isPickingPass() && (primitive.transparencies() != nullptr) && !orderIndependentTransparency();
    if(useBlending)
        shader.enableBlending();

    // Pass picking base ID to shader.
    if(isPickingPass()) {
        shader.setPickingBaseId(objectPickingMap()->allocateObjectPickingIDs(command, primitive.positions()->size()));
    }
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Upload particle coordinates.
    QOpenGLBuffer positionsBuffer = shader.uploadDataBuffer(primitive.positions(), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionsBuffer, "position", GL_FLOAT, 3, sizeof(Point3F), 0, OpenGLShaderHelper::PerInstance);

    // Upload particle radii.
    if(primitive.radii()) {
        QOpenGLBuffer radiiBuffer = shader.uploadDataBuffer(primitive.radii(), OpenGLShaderHelper::PerInstance);
        shader.bindBuffer(radiiBuffer, "radius", GL_FLOAT, 1, sizeof(float), 0, OpenGLShaderHelper::PerInstance);
    }
    else {
        shader.unbindBuffer("radius");
        shader.setAttributeValue("radius", primitive.uniformRadius());
    }

    if(!isPickingPass()) {
        // Upload particle colors.
        if(primitive.colors()) {
            QOpenGLBuffer colorsBuffer = shader.uploadDataBuffer(primitive.colors(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(colorsBuffer, "color", GL_FLOAT, 3, sizeof(Point3F), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("color");
            shader.setAttributeValue("color", primitive.uniformColor());
        }

        // Upload particle transparencies.
        if(primitive.transparencies()) {
            QOpenGLBuffer transparenciesBuffer = shader.uploadDataBuffer(primitive.transparencies(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(transparenciesBuffer, "transparency", GL_FLOAT, 1, sizeof(float), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("transparency");
            shader.setAttributeValue("transparency", 0.0);
        }

        // Upload particle selection.
        if(primitive.selection()) {
            QOpenGLBuffer selectionBuffer = shader.uploadDataBuffer(primitive.selection(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(selectionBuffer, "selection", GL_UNSIGNED_BYTE, 1, sizeof(int8_t), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("selection");
            shader.setAttributeValue("selection", 0);
        }
        shader.setUniformValue("selection_color", ColorA(primitive.selectionColor()));
    }

    // For box-shaped and ellipsoid particles, we need the shape & orientation vertex attributes.
    if(primitive.particleShape() == ParticlePrimitive::BoxShape || primitive.particleShape() == ParticlePrimitive::EllipsoidShape || primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {
        // Upload aspherical shapes.
        if(primitive.asphericalShapes()) {
            QOpenGLBuffer asphericalShapesBuffer = shader.uploadDataBuffer(primitive.asphericalShapes(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(asphericalShapesBuffer, "aspherical_shape", GL_FLOAT, 3, sizeof(Vector3F), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("aspherical_shape");
            shader.setAttributeValue("aspherical_shape", Vector3::Zero());
        }

        // Upload orientations.
        if(primitive.orientations()) {
            QOpenGLBuffer orientationsBuffer = shader.uploadDataBuffer(primitive.orientations(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(orientationsBuffer, "orientation", GL_FLOAT, 4, sizeof(QuaternionF), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("orientation");
            shader.setAttributeValue("orientation", Vector4(0,0,0,1));
        }
    }

    // For superquadric particles, we need the roundness vertex attribute.
    if(primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {
        // Upload roundness values.
        if(primitive.roundness()) {
            QOpenGLBuffer roundnessBuffer = shader.uploadDataBuffer(primitive.roundness(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(roundnessBuffer, "roundness", GL_FLOAT, 2, sizeof(Vector2F), 0, OpenGLShaderHelper::PerInstance);
        }
        else {
            shader.unbindBuffer("roundness");
            shader.setAttributeValue("roundness", Vector2(1,1));
        }
    }

    if(!useBlending) {
        // Draw triangle strip instances.
        shader.draw(GL_TRIANGLE_STRIP);
    }
    else {
        // Render the particles in back-to-front order.
        OVITO_ASSERT(!isPickingPass() && !orderIndependentTransparency());

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().linear().inverse().column(2);

        // Coarsen the direction vector's precision for real-time rendering to reduce the frequency at
        // which the particles must be reordered when moving the camera.
        Vector3 coarseDirection = direction;
        if(frameGraph()->isInteractive()) {
            for(size_t dim = 0; dim < 3; dim++)
                coarseDirection[dim] = std::round(2 * direction[dim]);
        }

        // The caching key for the particle ordering.
        RendererResourceKey<struct OrderingCache, ConstDataBufferPtr, ConstDataBufferPtr, Vector3> orderingCacheKey{
            primitive.indices(),
            primitive.positions(),
            coarseDirection
        };

        // Render primitives.
        shader.drawReordered(GL_TRIANGLE_STRIP, std::move(orderingCacheKey), [&](std::span<GLuint> sortedIndices) {

            // First, compute distance of each particle from the camera along the viewing direction (=camera z-axis).
            std::vector<GraphicsFloatType> distances(sortedIndices.size());
            if(primitive.positions()->dataType() == DataBuffer::Float64) {
                std::transform(sortedIndices.begin(), sortedIndices.end(), distances.begin(), [direction = direction.toDataType<double>(), positionsArray = BufferReadAccess<Vector_3<double>>(primitive.positions())](size_t i) {
                    return static_cast<GraphicsFloatType>(direction.dot(positionsArray[i]));
                });
            }
            else if(primitive.positions()->dataType() == DataBuffer::Float32) {
                std::transform(sortedIndices.begin(), sortedIndices.end(), distances.begin(), [direction = direction.toDataType<float>(), positionsArray = BufferReadAccess<Vector3F>(primitive.positions())](size_t i) {
                    return static_cast<GraphicsFloatType>(direction.dot(positionsArray[i]));
                });
            }
            else return;

            // Sort particle indices with respect to distance (back-to-front order).
            Ovito::sort_zipped(distances, sortedIndices);
        });
    }
}

}   // End of namespace
