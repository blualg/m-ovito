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
#include "OpenGLSceneRenderer.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Renders a set of cylinders or arrow glyphs.
******************************************************************************/
void OpenGLSceneRenderer::renderCylindersImplementation(const CylinderPrimitive& primitive)
{
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
        return;

    rebindVAO();

    // The OpenGL drawing primitive.
    GLenum primitiveDrawMode = GL_TRIANGLE_STRIP;

    // Decide whether per-pixel pseudo-color mapping is used (instead of direct RGB coloring).
    bool renderWithPseudoColorMapping = false;
    if(primitive.pseudoColorMapping().isValid() && !isPicking() && primitive.colors() && primitive.colors()->componentCount() == 1)
        renderWithPseudoColorMapping = true;
    QOpenGLTexture* colorMapTexture = nullptr;

    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    switch(primitive.shape()) {
        case CylinderPrimitive::CylinderShape:
            if(primitive.shadingMode() == CylinderPrimitive::NormalShading) {
                if(!useGeometryShaders()) {
                    if(!isPicking())
                        shader.load("cylinder", "cylinder/cylinder.vert", "cylinder/cylinder.frag");
                    else
                        shader.load("cylinder_picking", "cylinder/cylinder_picking.vert", "cylinder/cylinder_picking.frag");
                    shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
                }
                else {
                    if(!isPicking())
                        shader.load("cylinder", "cylinder/cylinder.geom.vert", "cylinder/cylinder.frag", "cylinder/cylinder.geom");
                    else
                        shader.load("cylinder_picking", "cylinder/cylinder_picking.geom.vert", "cylinder/cylinder_picking.frag", "cylinder/cylinder_picking.geom");
                    shader.setVerticesPerInstance(1); // Geometry shader generates the triangle strip from a point primitive.
                }
            }
            else {
                if(!isPicking())
                    shader.load("cylinder_flat", "cylinder/cylinder_flat.vert", "cylinder/cylinder_flat.frag");
                else
                    shader.load("cylinder_flat_picking", "cylinder/cylinder_flat_picking.vert", "cylinder/cylinder_flat_picking.frag");
                shader.setVerticesPerInstance(4); // Quad rendered as triangle strip.
            }
            break;

        case CylinderPrimitive::ArrowShape:
            OVITO_ASSERT(!renderWithPseudoColorMapping);
            if(primitive.shadingMode() == CylinderPrimitive::NormalShading) {
                if(!isPicking())
                    shader.load("arrow_head", "cylinder/arrow_head.vert", "cylinder/arrow_head.frag");
                else
                    shader.load("arrow_head_picking", "cylinder/arrow_head_picking.vert", "cylinder/arrow_head_picking.frag");
                shader.setVerticesPerInstance(14); // Box rendered as triangle strip.
            }
            else {
                if(!isPicking())
                    shader.load("arrow_flat", "cylinder/arrow_flat.vert", "cylinder/arrow_flat.frag");
                else
                    shader.load("arrow_flat_picking", "cylinder/arrow_flat_picking.vert", "cylinder/arrow_flat_picking.frag");
                shader.setVerticesPerInstance(7); // 2D arrow rendered as triangle fan.
                primitiveDrawMode = GL_TRIANGLE_FAN;
            }
            break;

        default:
            return;
    }

    shader.setInstanceCount(primitive.basePositions()->size());

    struct BaseHeadRadius {
        Vector_3<float> base;
        Vector_3<float> head;
        float radius;
    };

    // Check size limits.
    if(shader.instanceCount() > std::numeric_limits<int32_t>::max() / shader.verticesPerInstance() / std::max(sizeof(BaseHeadRadius), 2 * sizeof(ColorAT<float>))) {
        qWarning() << "WARNING: OpenGL renderer - Trying to render too many cylinders at once, exceeding device limits.";
        return;
    }

    // Are we rendering semi-transparent cylinders?
    bool useBlending = !isPicking() && (primitive.transparencies() != nullptr) && !orderIndependentTransparency();
    if(useBlending) shader.enableBlending();

    // Pass picking base ID to shader.
    GLint pickingBaseId;
    if(isPicking()) {
        pickingBaseId = registerSubObjectIDs(primitive.basePositions()->size());
        shader.setPickingBaseId(pickingBaseId);
    }
    OVITO_REPORT_OPENGL_ERRORS(this);

    // Pass camera viewing direction (parallel) or camera position (perspective) in object space to vertex shader.  
    if(primitive.shadingMode() == CylinderPrimitive::FlatShading) {
        Vector3 view_dir_eye_pos;
        if(projParams().isPerspective)
            view_dir_eye_pos = modelViewTM().inverse().column(3); // Camera position in object space
        else
            view_dir_eye_pos = modelViewTM().inverse().column(2); // Camera viewing direction in object space.
        shader.setUniformValue("view_dir_eye_pos", view_dir_eye_pos);
    }

    if(primitive.shape() == CylinderPrimitive::CylinderShape && primitive.shadingMode() == CylinderPrimitive::NormalShading) {
        shader.setUniformValue("single_cylinder_cap", (int)primitive.renderSingleCylinderCap());
    }

    // Put base/head positions and radii into one combined GL buffer.
    // Radii are optional and may be substituted with a uniform radius value.
    RendererResourceKey<struct PositionRadiusCache, ConstDataBufferPtr, ConstDataBufferPtr, ConstDataBufferPtr, FloatType> positionRadiusCacheKey{
        primitive.basePositions(),
        primitive.headPositions(),
        primitive.widths(),
        primitive.widths() ? FloatType(0) : primitive.uniformWidth()
    };

    // Upload vertex buffer with the base and head positions and radii.
    QOpenGLBuffer positionRadiusBuffer = shader.createCachedBuffer(positionRadiusCacheKey, sizeof(BaseHeadRadius), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
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

    // Bind vertex buffer to vertex attributes.
    shader.bindBuffer(positionRadiusBuffer, "base", GL_FLOAT, 3, sizeof(BaseHeadRadius), offsetof(BaseHeadRadius, base), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionRadiusBuffer, "head", GL_FLOAT, 3, sizeof(BaseHeadRadius), offsetof(BaseHeadRadius, head), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionRadiusBuffer, "radius", GL_FLOAT, 1, sizeof(BaseHeadRadius), offsetof(BaseHeadRadius, radius), OpenGLShaderHelper::PerInstance);

    if(!isPicking()) {

        // Put colors and transparencies into one combined GL buffer with 2*4 floats per primitive (two RGBA values).
        RendererResourceKey<struct ColorCache, ConstDataBufferPtr, ConstDataBufferPtr, Color, GLsizei> colorCacheKey{ 
            primitive.colors(),
            primitive.transparencies(),
            primitive.colors() ? Color(0,0,0) : primitive.uniformColor(),
            shader.instanceCount() // This is needed to NOT use the same cached buffer for rendering different number of cylinders which happen to use the same uniform color.
        };

        // Upload vertex buffer with the RGB color data.
        QOpenGLBuffer colorBuffer = shader.createCachedBuffer(colorCacheKey, 2 * sizeof(ColorAT<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
            // The color and the transparency arrays may contain either 1 or 2 values per primitive.
            // In case two colors/transparencies have been specified, linear interpolation 
            // along the primitive is performed by the renderer.
            OVITO_ASSERT(!primitive.colors() || primitive.colors()->size() == primitive.basePositions()->size() || primitive.colors()->size() == 2 * primitive.basePositions()->size());
            OVITO_ASSERT(!primitive.colors() || primitive.colors()->componentCount() == 1 || primitive.colors()->componentCount() == 3);
            OVITO_ASSERT(!primitive.transparencies() || primitive.transparencies()->size() == primitive.basePositions()->size() || primitive.transparencies()->size() == 2 * primitive.basePositions()->size());
            const ColorT<float> uniformColor = primitive.uniformColor().toDataType<float>();
            ConstDataBufferAccess<FloatType,true> colorArray(primitive.colors());
            ConstDataBufferAccess<FloatType> transparencyArray(primitive.transparencies());
            const FloatType* color = colorArray ? colorArray.cbegin() : nullptr;
            const FloatType* transparency = transparencyArray ? transparencyArray.cbegin() : nullptr;
            bool twoColorsPerPrimitive = (primitive.colors() && primitive.colors()->size() == 2 * primitive.basePositions()->size());
            bool twoTransparenciesPerPrimitive = (primitive.transparencies() && primitive.transparencies()->size() == 2 * primitive.basePositions()->size());
            for(float* dst = reinterpret_cast<float*>(buffer), *dst_end = dst + 8 * shader.instanceCount(); dst != dst_end; dst += 8) {
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
        shader.bindBuffer(colorBuffer, "color1", GL_FLOAT, 4, 2 * sizeof(ColorAT<float>), 0, OpenGLShaderHelper::PerInstance);
        if(primitive.shape() == CylinderPrimitive::CylinderShape)
            shader.bindBuffer(colorBuffer, "color2", GL_FLOAT, 4, 2 * sizeof(ColorAT<float>), sizeof(ColorAT<float>), OpenGLShaderHelper::PerInstance);

        if(renderWithPseudoColorMapping) {
            // Rendering with pseudo-colors and a color mapping function.
            float minValue = primitive.pseudoColorMapping().minValue();
            float maxValue = primitive.pseudoColorMapping().maxValue();
            // Avoid division by zero due to degenerate value interval.
            if(minValue == maxValue) maxValue = std::nextafter(maxValue, std::numeric_limits<float>::max());
            shader.setUniformValue("color_range_min", minValue);
            shader.setUniformValue("color_range_max", maxValue);

            // Upload color map as a 1-d OpenGL texture.
            colorMapTexture = OpenGLResourceManager::instance()->uploadColorMap(primitive.pseudoColorMapping().gradient(), currentResourceFrame());
            colorMapTexture->bind();
        }
        else {
            // This will turn pseudocolor mapping off in the fragment shader.
            shader.setUniformValue("color_range_min", 0.0f);
            shader.setUniformValue("color_range_max", 0.0f);

#ifdef Q_OS_MACOS
            // Upload a null color map to satisfy the picky OpenGL driver on macOS, which complains about 
            // no texture being bound when a sampler1D is defined in the fragment shader.
            if(!isPicking() && primitive.shape() == CylinderPrimitive::CylinderShape) {
                colorMapTexture = OpenGLResourceManager::instance()->uploadColorMap(nullptr, currentResourceFrame());
                colorMapTexture->bind();
            }
#endif
        }
    }

    // Draw triangle strip or fan instances in regular storage order (not sorted).
    shader.drawArrays(primitiveDrawMode);

    // Draw cylindric part of the arrows.
    if(primitive.shape() == CylinderPrimitive::ArrowShape && primitive.shadingMode() == CylinderPrimitive::NormalShading) {
        if(!isPicking())
            shader.load("arrow_tail", "cylinder/arrow_tail.vert", "cylinder/arrow_tail.frag");
        else {
            shader.load("arrow_tail_picking", "cylinder/arrow_tail_picking.vert", "cylinder/arrow_tail_picking.frag");
            shader.setPickingBaseId(pickingBaseId);
        }

        shader.drawArrays(GL_TRIANGLE_STRIP);
    }

    // Unbind color mapping texture.
    if(colorMapTexture) {
        colorMapTexture->release();
    }

    OVITO_REPORT_OPENGL_ERRORS(this);
}

}   // End of namespace
