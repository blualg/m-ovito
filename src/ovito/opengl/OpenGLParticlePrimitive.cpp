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
#include "OpenGLSceneRenderer.h"
#include "OpenGLShaderHelper.h"

#include <boost/range/irange.hpp>

namespace Ovito {

/******************************************************************************
* Renders a set of particles.
******************************************************************************/
void OpenGLSceneRenderer::renderParticlesImplementation(const ParticlePrimitive& primitive)
{
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;
    if(primitive.indices() && primitive.indices()->size() == 0)
        return;

    rebindVAO();

    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    switch(primitive.particleShape()) {
        case ParticlePrimitive::SquareCubicShape:
            if(primitive.shadingMode() == ParticlePrimitive::NormalShading) {
                if(!useGeometryShaders()) {
                    if(!isPicking())
                        shader.load("cube", "particles/cube/cube.vert", "particles/cube/cube.frag");
                    else
                        shader.load("cube_picking", "particles/cube/cube_picking.vert", "particles/cube/cube_picking.frag");
                    shader.setVerticesPerInstance(14); // Cube rendered as triangle strip.
                }
                else {
                    if(!isPicking())
                        shader.load("cube.geom", "particles/cube/cube.geom.vert", "particles/cube/cube.frag", "particles/cube/cube.geom");
                    else
                        shader.load("cube_picking.geom", "particles/cube/cube_picking.geom.vert", "particles/cube/cube_picking.frag", "particles/cube/cube_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            else {
                if(!useGeometryShaders()) {
                    if(!isPicking()) 
                        shader.load("square", "particles/square/square.vert", "particles/square/square.frag");
                    else
                        shader.load("square_picking", "particles/square/square_picking.vert", "particles/square/square_picking.frag");
                    shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                }
                else {
                    if(!isPicking()) 
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
                    if(!isPicking())
                        shader.load("box", "particles/box/box.vert", "particles/box/box.frag");
                    else
                        shader.load("box_picking", "particles/box/box_picking.vert", "particles/box/box_picking.frag");
                    shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
                }
                else {
                    if(!isPicking())
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
                        if(!isPicking())
                            shader.load("sphere", "particles/sphere/sphere.vert", "particles/sphere/sphere.frag");
                        else
                            shader.load("sphere_picking", "particles/sphere/sphere_picking.vert", "particles/sphere/sphere_picking.frag");
                        shader.setVerticesPerInstance(4); // Billboard quad geometry rendered as triangle strip.
                    }
                    else {
                        if(!isPicking())
                            shader.load("sphere.geom", "particles/sphere/sphere.geom.vert", "particles/sphere/sphere.frag", "particles/sphere/sphere.geom");
                        else
                            shader.load("sphere_picking.geom", "particles/sphere/sphere_picking.geom.vert", "particles/sphere/sphere_picking.frag", "particles/sphere/sphere_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a single point primitive.
                    }
                }
                else if(primitive.renderingQuality() >= ParticlePrimitive::MediumQuality) {
                    if(!useGeometryShaders()) {
                        if(!isPicking())
                            shader.load("imposter", "particles/imposter/imposter.vert", "particles/imposter/imposter.frag");
                        else
                            shader.load("imposter_picking", "particles/imposter/imposter_picking.vert", "particles/imposter/imposter_picking.frag");
                        shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                    }
                    else {
                        if(!isPicking())
                            shader.load("imposter.geom", "particles/imposter/imposter.geom.vert", "particles/imposter/imposter.frag", "particles/imposter/imposter.geom");
                        else
                            shader.load("imposter_picking.geom", "particles/imposter/imposter_picking.geom.vert", "particles/imposter/imposter_picking.frag", "particles/imposter/imposter_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                    }
                }
                else {
                    if(!useGeometryShaders()) {
                        if(!isPicking())
                            shader.load("imposter_flat", "particles/imposter_flat/imposter_flat.vert", "particles/imposter_flat/imposter_flat.frag");
                        else
                            shader.load("imposter_flat_picking", "particles/imposter_flat/imposter_flat_picking.vert", "particles/imposter_flat/imposter_flat_picking.frag");
                        shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                    }
                    else {
                        if(!isPicking())
                            shader.load("imposter_flat.geom", "particles/imposter_flat/imposter_flat.geom.vert", "particles/imposter_flat/imposter_flat.frag", "particles/imposter_flat/imposter_flat.geom");
                        else
                            shader.load("imposter_flat_picking.geom", "particles/imposter_flat/imposter_flat_picking.geom.vert", "particles/imposter_flat/imposter_flat_picking.frag", "particles/imposter_flat/imposter_flat_picking.geom");
                        shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                    }
                }
            }
            else {
                if(!useGeometryShaders()) {
                    if(!isPicking())
                        shader.load("circle", "particles/circle/circle.vert", "particles/circle/circle.frag");
                    else
                        shader.load("circle_picking", "particles/circle/circle_picking.vert", "particles/circle/circle_picking.frag");
                    shader.setVerticesPerInstance(4); // Square rendered as triangle strip.
                }
                else {
                    if(!isPicking())
                        shader.load("circle.geom", "particles/circle/circle.geom.vert", "particles/circle/circle.frag", "particles/circle/circle.geom");
                    else
                        shader.load("circle_picking.geom", "particles/circle/circle_picking.geom.vert", "particles/circle/circle_picking.frag", "particles/circle/circle_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            break;
        case ParticlePrimitive::EllipsoidShape:
            if(!useGeometryShaders()) {
                if(!isPicking())
                    shader.load("ellipsoid", "particles/ellipsoid/ellipsoid.vert", "particles/ellipsoid/ellipsoid.frag");
                else
                    shader.load("ellipsoid_picking", "particles/ellipsoid/ellipsoid_picking.vert", "particles/ellipsoid/ellipsoid_picking.frag");
                shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
            }
            else {
                if(!isPicking())
                    shader.load("ellipsoid.geom", "particles/ellipsoid/ellipsoid.geom.vert", "particles/ellipsoid/ellipsoid.frag", "particles/ellipsoid/ellipsoid.geom");
                else
                    shader.load("ellipsoid_picking.geom", "particles/ellipsoid/ellipsoid_picking.geom.vert", "particles/ellipsoid/ellipsoid_picking.frag", "particles/ellipsoid/ellipsoid_picking.geom");
                shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
            }
            break;
        case ParticlePrimitive::SuperquadricShape:
            if(!useGeometryShaders()) {
                if(!isPicking())
                    shader.load("superquadric", "particles/superquadric/superquadric.vert", "particles/superquadric/superquadric.frag");
                else
                    shader.load("superquadric_picking", "particles/superquadric/superquadric_picking.vert", "particles/superquadric/superquadric_picking.frag");
                shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
            }
            else {
                if(!isPicking())
                    shader.load("superquadric.geom", "particles/superquadric/superquadric.geom.vert", "particles/superquadric/superquadric.frag", "particles/superquadric/superquadric.geom");
                else
                    shader.load("superquadric_picking.geom", "particles/superquadric/superquadric_picking.geom.vert", "particles/superquadric/superquadric_picking.frag", "particles/superquadric/superquadric_picking.geom");
                shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
            }
            break;
        default:
            return;
    }

    // The effective number of particles being rendered:
    shader.setInstanceCount(primitive.indices() ? primitive.indices()->size() : primitive.positions()->size());

    // Check size limits.
    int bytesPerVertex = (primitive.particleShape() == ParticlePrimitive::BoxShape || primitive.particleShape() == ParticlePrimitive::EllipsoidShape || primitive.particleShape() == ParticlePrimitive::SuperquadricShape) 
        ? sizeof(Matrix_4<float>) : sizeof(Vector_4<float>);
    if(shader.instanceCount() > std::numeric_limits<int32_t>::max() / shader.verticesPerInstance() / bytesPerVertex) {
        qWarning() << "WARNING: OpenGL renderer - Trying to render too many particles at once, exceeding device limits.";
        return;
    }

    // Are we rendering semi-transparent particles?
    bool useBlending = !isPicking() && (primitive.transparencies() != nullptr) && !orderIndependentTransparency();
    if(useBlending) shader.enableBlending();

    // Pass picking base ID to shader.
    if(isPicking()) {
        shader.setPickingBaseId(registerSubObjectIDs(primitive.positions()->size(), primitive.indices()));
    }
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Put positions and radii into one combined OpenGL buffer with 4 floats per particle.
    // Radii are optional and may be substituted with a uniform radius value.
    RendererResourceKey<struct PositionRadiusCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> positionRadiusCacheKey{
        primitive.indices(),
        primitive.positions(),
        primitive.radii(),
        primitive.radii() ? FloatType(0) : primitive.uniformRadius()
    };

    // Upload vertex buffer with the particle positions and radii.
    QOpenGLBuffer positionRadiusBuffer = shader.createCachedBuffer(positionRadiusCacheKey, sizeof(Vector_4<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
        OVITO_ASSERT(!primitive.radii() || primitive.radii()->size() == primitive.positions()->size());
        ConstDataBufferAccess<Point3> positionArray(primitive.positions());
        ConstDataBufferAccess<FloatType> radiusArray(primitive.radii());
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
            for(int index : ConstDataBufferAccess<int>(primitive.indices())) {
                const Point3& pos = positionArray[index];
                *dst++ = static_cast<float>(pos.x());
                *dst++ = static_cast<float>(pos.y());
                *dst++ = static_cast<float>(pos.z());
                *dst++ = static_cast<float>(radiusArray ? radiusArray[index] : primitive.uniformRadius());
            }
        }
    });

    // Bind vertex buffer to vertex attributes.
    shader.bindBuffer(positionRadiusBuffer, "position", GL_FLOAT, 3, sizeof(Vector_4<float>), 0, OpenGLShaderHelper::PerInstance);

    // Radius attribute is only required for certain particle shapes.
    if(primitive.particleShape() != ParticlePrimitive::BoxShape && primitive.particleShape() != ParticlePrimitive::EllipsoidShape && primitive.particleShape() != ParticlePrimitive::SuperquadricShape) {
        shader.bindBuffer(positionRadiusBuffer, "radius", GL_FLOAT, 1, sizeof(Vector_4<float>), sizeof(Vector_3<float>), OpenGLShaderHelper::PerInstance);
    }

    if(!isPicking()) {

        // Put colors, transparencies and selection state into one combined Vulkan buffer with 4 floats per particle.
        RendererResourceKey<struct ColorSelection, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, Color, GLsizei> colorSelectionCacheKey{ 
            primitive.indices(),
            primitive.colors(),
            primitive.transparencies(),
            primitive.selection(),
            primitive.colors() ? Color(0,0,0) : primitive.uniformColor(),
            shader.instanceCount() // This is needed to NOT use the same cached buffer for rendering different number of particles which happen to use the same uniform color.
        };

        // Upload vertex buffer with the particle colors.
        QOpenGLBuffer colorSelectionBuffer = shader.createCachedBuffer(colorSelectionCacheKey, sizeof(ColorAT<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
            OVITO_ASSERT(!primitive.transparencies() || primitive.transparencies()->size() == primitive.positions()->size());
            OVITO_ASSERT(!primitive.selection() || primitive.selection()->size() == primitive.positions()->size());
            ConstDataBufferAccess<FloatType> transparencyArray(primitive.transparencies());
            ConstDataBufferAccess<int> selectionArray(primitive.selection());
            const ColorT<float> uniformColor = primitive.uniformColor().toDataType<float>();
            const ColorAT<float> selectionColor = primitive.selectionColor().toDataType<float>();
            if(!primitive.indices()) {
                ConstDataBufferAccess<FloatType,true> colorArray(primitive.colors());
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
                ConstDataBufferAccess<Color> colorArray(primitive.colors());
                float* dst = reinterpret_cast<float*>(buffer);
                for(int index : ConstDataBufferAccess<int>(primitive.indices())) {
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
        shader.bindBuffer(colorSelectionBuffer, "color", GL_FLOAT, 4, sizeof(ColorAT<float>), 0, OpenGLShaderHelper::PerInstance);
    }

    // For box-shaped and ellipsoid particles, we need the shape/orientation vertex attribute.
    if(primitive.particleShape() == ParticlePrimitive::BoxShape || primitive.particleShape() == ParticlePrimitive::EllipsoidShape || primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {

        // Combine aspherical shape property and orientation property into one combined buffer containing a 4x4 transformation matrix per particle.
        RendererResourceKey<struct ShapeOrientation, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> shapeOrientationCacheKey{ 
            primitive.indices(),
            primitive.asphericalShapes(),
            primitive.orientations(),
            primitive.radii(),
            primitive.radii() ? FloatType(0) : primitive.uniformRadius()
        };

        // Upload vertex buffer with the particle transformation matrices.
        QOpenGLBuffer shapeOrientationBuffer = shader.createCachedBuffer(shapeOrientationCacheKey, sizeof(Matrix_4<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
            ConstDataBufferAccess<Vector3> asphericalShapeArray(primitive.asphericalShapes());
            ConstDataBufferAccess<Quaternion> orientationArray(primitive.orientations());
            ConstDataBufferAccess<FloatType> radiusArray(primitive.radii());
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
                            axes = shape->toDataType<float>();
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
                for(int index : ConstDataBufferAccess<int>(primitive.indices())) {
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
        GLuint attrIndex = shader.shaderObject().attributeLocation("shape_orientation");
        for(int i = 0; i < 4; i++)
            shader.bindBuffer(shapeOrientationBuffer, attrIndex + i, GL_FLOAT, 4, sizeof(Matrix_4<float>), i * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
    }

    // For superquadric particles, we need to prepare the roundness vertex attribute.
    if(primitive.particleShape() == ParticlePrimitive::SuperquadricShape) {

        RendererResourceKey<struct RoundnessCache, ConstDataBufferPtr, ConstDataBufferPtr> roundnessCacheKey{ 
            primitive.indices(),
            primitive.roundness()
        };

        // Upload vertex buffer with the roundness values.
        QOpenGLBuffer roundnessBuffer = shader.createCachedBuffer(roundnessCacheKey, sizeof(Vector_2<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
            Vector_2<float>* dst = reinterpret_cast<Vector_2<float>*>(buffer);
            if(primitive.roundness()) {
                OVITO_ASSERT(primitive.roundness()->size() == primitive.positions()->size());
                if(!primitive.indices()) {
                    for(const Vector2& r : ConstDataBufferAccess<Vector2>(primitive.roundness())) {
                        *dst++ = r.toDataType<float>();
                    }
                }
                else {
                    ConstDataBufferAccess<Vector2> roundnessArray(primitive.roundness());
                    for(int index : ConstDataBufferAccess<int>(primitive.indices())) {
                        *dst++ = roundnessArray[index].toDataType<float>();
                    }
                }
            }
            else {
                std::fill(dst, dst + shader.instanceCount(), Vector_2<float>(1,1));
            }
        });

        // Bind vertex buffer.
        shader.bindBuffer(roundnessBuffer, "roundness", GL_FLOAT, 2, sizeof(Vector_2<float>), 0, OpenGLShaderHelper::PerInstance);
    }

    if(!useBlending) {
        // Draw triangle strip instances in regular storage order (unsorted).
        shader.drawArrays(GL_TRIANGLE_STRIP);
    }
    else {
        // Render the particles in back-to-front order. 
        OVITO_ASSERT(!isPicking() && !orderIndependentTransparency());

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the particle ordering.
        RendererResourceKey<struct OrderingCache, ConstDataBufferPtr, ConstDataBufferPtr, Vector3, int> orderingCacheKey{
            primitive.indices(),
            primitive.positions(),
            direction,
            shader.verticesPerInstance()
        };

        // Render primitives.
        shader.drawArraysOrdered(GL_TRIANGLE_STRIP, std::move(orderingCacheKey), [&]() {

            // First, compute distance of each particle from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(shader.instanceCount());
            if(!primitive.indices()) {
                boost::transform(boost::irange<size_t>(0, shader.instanceCount()), distances.begin(), [direction, positionsArray = ConstDataBufferAccess<Vector3>(primitive.positions())](size_t i) {
                    return direction.dot(positionsArray[i]);
                });
            }
            else {
                boost::transform(ConstDataBufferAccess<int>(primitive.indices()), distances.begin(), [direction, positionsArray = ConstDataBufferAccess<Vector3>(primitive.positions())](size_t i) {
                    return direction.dot(positionsArray[i]);
                });
            }

            // Create index array with all particle indices.
            std::vector<uint32_t> sortedIndices(shader.instanceCount());
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort particle indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            return sortedIndices;
        });
    }

    OVITO_REPORT_OPENGL_ERRORS(this);
}

}   // End of namespace
