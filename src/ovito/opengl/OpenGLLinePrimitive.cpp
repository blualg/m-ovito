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
* Renders a set of lines.
******************************************************************************/
void OpenGLSceneRenderer::renderLinesImplementation(const LinePrimitive& primitive)
{
    // Step out early if there is nothing to render.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    rebindVAO();
    OVITO_REPORT_OPENGL_ERRORS(this);

    if(primitive.lineWidth() == 1 || (primitive.lineWidth() <= 0 && devicePixelRatio() <= 1))
        renderThinLinesImplementation(primitive);
    else
        renderThickLinesImplementation(primitive);

    OVITO_REPORT_OPENGL_ERRORS(this);
}

/******************************************************************************
* Renders a set of lines using GL_LINES mode.
******************************************************************************/
void OpenGLSceneRenderer::renderThinLinesImplementation(const LinePrimitive& primitive)
{
    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    if(isPicking())
        shader.load("line_thin_picking", "lines/line_picking.vert", "lines/line.frag");
    else if(primitive.colors())
        shader.load("line_thin", "lines/line.vert", "lines/line.frag");
    else
        shader.load("line_thin_uniform_color", "lines/line_uniform_color.vert", "lines/line_uniform_color.frag");

    shader.setVerticesPerInstance(primitive.positions()->size());
    shader.setInstanceCount(1);

    // Check size limits.
    if(primitive.positions()->size() > std::numeric_limits<int32_t>::max() / sizeof(Point_3<float>)) {
        qWarning() << "WARNING: OpenGL renderer - Trying to render too many lines at once, exceeding device limits.";
        return;
    }

    // Upload vertex positions.
    QOpenGLBuffer positionsBuffer = shader.uploadDataBuffer(primitive.positions(), OpenGLShaderHelper::PerVertex);
    shader.bindBuffer(positionsBuffer, "position", GL_FLOAT, 3, sizeof(Point_3<float>), 0, OpenGLShaderHelper::PerVertex);

    if(!isPicking()) {
        if(primitive.colors()) {
            OVITO_ASSERT(primitive.colors()->size() == primitive.positions()->size());
            // Upload vertex colors.
            QOpenGLBuffer colorsBuffer = shader.uploadDataBuffer(primitive.colors(), OpenGLShaderHelper::PerVertex);
            shader.bindBuffer(colorsBuffer, "color", GL_FLOAT, 4, sizeof(ColorAT<float>), 0, OpenGLShaderHelper::PerVertex);
        }
        else {
            // Pass uniform line color to fragment shader as a uniform value.
            shader.setUniformValue("color", primitive.uniformColor());
        }
    }
    else {
        // Pass picking base ID to shader.
        shader.setPickingBaseId(registerSubObjectIDs(primitive.positions()->size() / 2));
    }

    // Issue line drawing command.
    shader.draw(GL_LINES);
}

/******************************************************************************
* Renders a set of lines using triangle strips.
******************************************************************************/
void OpenGLSceneRenderer::renderThickLinesImplementation(const LinePrimitive& primitive)
{
    // Effective line width.
    FloatType effectiveLineWidth = (primitive.lineWidth() <= 0) ? devicePixelRatio() : primitive.lineWidth();

    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    if(isPicking())
        shader.load("line_thick_picking", "lines/thick_line_picking.vert", "lines/line.frag");
    else if(primitive.colors())
        shader.load("line_thick", "lines/thick_line.vert", "lines/line.frag");
    else
        shader.load("line_thick_uniform_color", "lines/thick_line_uniform_color.vert", "lines/line_uniform_color.frag");

    shader.setVerticesPerInstance(4);
    shader.setInstanceCount(primitive.positions()->size() / 2);

    // Check size limits.
    if(shader.instanceCount() > std::numeric_limits<int32_t>::max() / shader.verticesPerInstance() / (2 * sizeof(Point_3<float>))) {
        qWarning() << "WARNING: OpenGL renderer - Trying to render too many lines at once, exceeding device limits.";
        return;
    }

    // Put start/end vertex positions into one combined vertex buffer.
    QOpenGLBuffer positionsBuffer = shader.uploadDataBuffer(primitive.positions(), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionsBuffer, "position_from", GL_FLOAT, 3, 2 * sizeof(Point_3<float>), 0, OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionsBuffer, "position_to", GL_FLOAT, 3, 2 * sizeof(Point_3<float>), sizeof(Point_3<float>), OpenGLShaderHelper::PerInstance);

    if(!isPicking()) {
        if(primitive.colors()) {
            OVITO_ASSERT(primitive.colors()->size() == primitive.positions()->size());
            // Upload vertex colors.
            QOpenGLBuffer colorsBuffer = shader.uploadDataBuffer(primitive.colors(), OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(colorsBuffer, "color_from", GL_FLOAT, 4, 2 * sizeof(ColorAT<float>), 0, OpenGLShaderHelper::PerInstance);
            shader.bindBuffer(colorsBuffer, "color_to", GL_FLOAT, 4, 2 * sizeof(ColorAT<float>), sizeof(ColorAT<float>), OpenGLShaderHelper::PerInstance);
        }
        else {
            // Pass uniform line color to fragment shader as a uniform value.
            shader.setUniformValue("color", primitive.uniformColor());
        }
    }
    else {
        // Pass picking base ID to shader.
        shader.setPickingBaseId(registerSubObjectIDs(primitive.positions()->size() / 2));
    }

    // Compute line width in viewport space.
    shader.setUniformValue("line_thickness", effectiveLineWidth / viewportRect().height());

    // Issue instanced drawing command.
    shader.draw(GL_TRIANGLE_STRIP);
}

}   // End of namespace
