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
#include "OpenGLSceneRenderer.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Renders a set of cylinders or arrow glyphs.
******************************************************************************/
void OpenGLSceneRenderer::renderCylindersImplementation(const CylinderPrimitive& primitive)
{
    // Make sure there is something to be rendered. Otherwise, step out early.
    if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
        return;

    rebindVAO();
    OVITO_REPORT_OPENGL_ERRORS(this);

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

    // Check size limits.
    if(shader.instanceCount() > std::numeric_limits<int32_t>::max() / shader.verticesPerInstance() / (2 * sizeof(ColorT<float>))) {
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

    // Upload and bind base vertex positions.
    QOpenGLBuffer basePositionBuffer = shader.uploadDataBuffer(primitive.basePositions(), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(basePositionBuffer, "base", GL_FLOAT, 3, sizeof(Point_3<float>), 0, OpenGLShaderHelper::PerInstance);

    // Upload and bind head vertex positions.
    QOpenGLBuffer headPositionBuffer = shader.uploadDataBuffer(primitive.headPositions(), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(headPositionBuffer, "head", GL_FLOAT, 3, sizeof(Point_3<float>), 0, OpenGLShaderHelper::PerInstance);

    // Upload and bind cylinder diameters.
    if(primitive.widths()) {
        QOpenGLBuffer diametersBuffer = shader.uploadDataBuffer(primitive.widths(), OpenGLShaderHelper::PerInstance);
        shader.bindBuffer(diametersBuffer, "diameter", GL_FLOAT, 1, sizeof(float), 0, OpenGLShaderHelper::PerInstance);
    }
    else {
        shader.unbindBuffer("diameter");
        shader.setAttributeValue("diameter", primitive.uniformWidth());
    }

    if(!isPicking()) {
        // The color and transparency arrays may contain either 1 or 2 values per cylinder primitive.
        // In case two are given, linear interpolation along the primitive will be performed by the
        // renderer (for cylinders but not arrows).

        // Upload RGB or pseudo-colors.
        if(primitive.colors() && !renderWithPseudoColorMapping && primitive.colors()->componentCount() == 3) {
            QOpenGLBuffer rgbBuffer = shader.uploadDataBuffer(primitive.colors(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(rgbBuffer, "color1", GL_FLOAT, 3, sizeof(ColorT<float>) * (primitive.colors()->size() / primitive.basePositions()->size()), 0, OpenGLShaderHelper::PerInstance);
            if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                if(primitive.colors()->size() == 2 * primitive.basePositions()->size())
                    shader.bindBuffer(rgbBuffer, "color2", GL_FLOAT, 3, 2 * sizeof(ColorT<float>), sizeof(ColorT<float>), OpenGLShaderHelper::PerInstance);
                else
                    shader.bindBuffer(rgbBuffer, "color2", GL_FLOAT, 3, sizeof(ColorT<float>), 0, OpenGLShaderHelper::PerInstance);
            }
        }
        else if(primitive.colors() && renderWithPseudoColorMapping && primitive.colors()->componentCount() == 1) {
            QOpenGLBuffer pseudoColorBuffer = shader.uploadDataBuffer(primitive.colors(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(pseudoColorBuffer, "color1", GL_FLOAT, 1, sizeof(float) * (primitive.colors()->size() / primitive.basePositions()->size()), 0, OpenGLShaderHelper::PerInstance);
            if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                if(primitive.colors()->size() == 2 * primitive.basePositions()->size())
                    shader.bindBuffer(pseudoColorBuffer, "color2", GL_FLOAT, 1, 2 * sizeof(float), sizeof(float), OpenGLShaderHelper::PerInstance);
                else
                    shader.bindBuffer(pseudoColorBuffer, "color2", GL_FLOAT, 1, sizeof(float), 0, OpenGLShaderHelper::PerInstance);
            }
        }
        else {
            shader.unbindBuffer("color1");
            shader.setAttributeValue("color1", primitive.uniformColor());
            if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                shader.unbindBuffer("color2");
                shader.setAttributeValue("color2", primitive.uniformColor());
            }
        }

        // Upload transparency values.
        if(primitive.transparencies()) {
            QOpenGLBuffer transparencyBuffer = shader.uploadDataBuffer(primitive.transparencies(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(transparencyBuffer, "transparency1", GL_FLOAT, 1, sizeof(float) * (primitive.transparencies()->size() / primitive.basePositions()->size()), 0, OpenGLShaderHelper::PerInstance);
            if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                if(primitive.transparencies()->size() == 2 * primitive.basePositions()->size())
                    shader.bindBuffer(transparencyBuffer, "transparency2", GL_FLOAT, 1, 2 * sizeof(float), sizeof(float), OpenGLShaderHelper::PerInstance);
                else
                    shader.bindBuffer(transparencyBuffer, "transparency2", GL_FLOAT, 1, sizeof(float), 0, OpenGLShaderHelper::PerInstance);
            }
        }
        else {
            shader.unbindBuffer("transparency1");
            shader.setAttributeValue("transparency1", 0.0);
            if(primitive.shape() == CylinderPrimitive::CylinderShape) {
                shader.unbindBuffer("transparency2");
                shader.setAttributeValue("transparency2", 0.0);
            }
        }

        if(renderWithPseudoColorMapping) {
            // Rendering with pseudo-colors and a color mapping function.
            float minValue = primitive.pseudoColorMapping().minValue();
            float maxValue = primitive.pseudoColorMapping().maxValue();
            // Avoid division by zero due to degenerate value interval.
            if(minValue == maxValue) {
                minValue = std::min(minValue - FloatTypeEpsilon<float>(), std::nextafter(minValue, std::numeric_limits<float>::lowest()));
                maxValue = std::max(maxValue + FloatTypeEpsilon<float>(), std::nextafter(maxValue, std::numeric_limits<float>::max()));
            }
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
    shader.draw(primitiveDrawMode);

    // Draw cylindric part of the arrows.
    if(primitive.shape() == CylinderPrimitive::ArrowShape && primitive.shadingMode() == CylinderPrimitive::NormalShading) {
        if(!isPicking())
            shader.load("arrow_tail", "cylinder/arrow_tail.vert", "cylinder/arrow_tail.frag");
        else {
            shader.load("arrow_tail_picking", "cylinder/arrow_tail_picking.vert", "cylinder/arrow_tail_picking.frag");
            shader.setPickingBaseId(pickingBaseId);
        }

        shader.draw(GL_TRIANGLE_STRIP);
    }

    // Unbind color mapping texture.
    if(colorMapTexture) {
        colorMapTexture->release();
    }

    OVITO_REPORT_OPENGL_ERRORS(this);
}

}   // End of namespace
