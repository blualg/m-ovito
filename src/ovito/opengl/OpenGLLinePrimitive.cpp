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
#include <ovito/core/rendering/LinePrimitive.h>
#include "OpenGLRenderingJob.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Renders a set of lines.
******************************************************************************/
void OpenGLRenderingJob::renderLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command)
{
    FloatType lineWidth = !isPickingPass() ? primitive.lineWidth() : primitive.pickingLineWidth();
    OVITO_ASSERT(lineWidth > 0);

    // Step out early if there is nothing to render.
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    if(lineWidth == 1)
        renderThinLinesImplementation(primitive, command);
    else
        renderThickLinesImplementation(primitive, command);
}

/******************************************************************************
* Renders a set of lines using GL_LINES mode.
******************************************************************************/
void OpenGLRenderingJob::renderThinLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command)
{
    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    if(isPickingPass())
        shader.load("line_thin_picking", "lines/line_picking.vert", "lines/line.frag");
    else if(primitive.colors())
        shader.load("line_thin", "lines/line.vert", "lines/line.frag");
    else
        shader.load("line_thin_uniform_color", "lines/line_uniform_color.vert", "lines/line_uniform_color.frag");

    shader.setVerticesPerInstance(primitive.positions()->size());
    shader.setInstanceCount(1);

    // Check size limits.
    const int32_t maxNumLines = std::numeric_limits<int32_t>::max() / (2 * sizeof(Point3F));
    if(primitive.positions()->size() / 2 > maxNumLines) {
        _renderBuffer->reportIssue(tr("Rendering skipped: Too many lines to render at once (%1), exceeding device limits. Maximum number of lines: %2").arg(primitive.positions()->size() / 2).arg(maxNumLines));
    }

    // Upload vertex positions.
    QOpenGLBuffer positionsBuffer = shader.uploadDataBuffer(primitive.positions(), OpenGLShaderHelper::PerVertex);
    shader.bindBuffer(positionsBuffer, "position", GL_FLOAT, 3, sizeof(Point3F), 0, OpenGLShaderHelper::PerVertex);

    if(!isPickingPass()) {
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
        if(auto pickingID = objectPickingMap()->allocateObjectPickingIDs(command, primitive.positions()->size() / 2)) {
            shader.setPickingBaseId(*pickingID);
        }
        else {
            // Failed to allocate picking IDs. Step out without rendering anything.
            qWarning() << "WARNING: OpenGL renderer - Picking ID overflow. The scene contains too many pickable objects.";
            return;
        }
    }

    // Issue line drawing command.
    shader.draw(GL_LINES);
}

/******************************************************************************
* Renders a set of lines using triangle strips.
******************************************************************************/
void OpenGLRenderingJob::renderThickLinesImplementation(const LinePrimitive& primitive, const FrameGraph::RenderingCommand& command)
{
    FloatType lineWidth = !isPickingPass() ? primitive.lineWidth() : primitive.pickingLineWidth();

    // Activate the right OpenGL shader program.
    OpenGLShaderHelper shader(this);
    if(isPickingPass())
        shader.load("line_thick_picking", "lines/thick_line_picking.vert", "lines/line.frag");
    else if(primitive.colors())
        shader.load("line_thick", "lines/thick_line.vert", "lines/line.frag");
    else
        shader.load("line_thick_uniform_color", "lines/thick_line_uniform_color.vert", "lines/line_uniform_color.frag");

    shader.setVerticesPerInstance(4);
    shader.setInstanceCount(primitive.positions()->size() / 2);

    // Check size limits.
    int32_t maxNumLines = std::numeric_limits<int32_t>::max() / shader.verticesPerInstance() / (2 * sizeof(Point3F));
    if(shader.instanceCount() > maxNumLines) {
        _renderBuffer->reportIssue(tr("Rendering skipped: Too many lines to render at once (%1), exceeding device limits. Maximum number of lines: %2").arg(shader.instanceCount()).arg(maxNumLines));
        return;
    }

    // Put start/end vertex positions into one combined vertex buffer.
    QOpenGLBuffer positionsBuffer = shader.uploadDataBuffer(primitive.positions(), OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionsBuffer, "position_from", GL_FLOAT, 3, 2 * sizeof(Point3F), 0, OpenGLShaderHelper::PerInstance);
    shader.bindBuffer(positionsBuffer, "position_to", GL_FLOAT, 3, 2 * sizeof(Point3F), sizeof(Point3F), OpenGLShaderHelper::PerInstance);

    if(!isPickingPass()) {
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
        if(auto pickingID = objectPickingMap()->allocateObjectPickingIDs(command, primitive.positions()->size() / 2)) {
            shader.setPickingBaseId(*pickingID);
        }
        else {
            // Failed to allocate picking IDs. Step out without rendering anything.
            qWarning() << "WARNING: OpenGL renderer - Picking ID overflow. The scene contains too many pickable objects.";
            return;
        }
    }

    // Compute line width in viewport space.
    shader.setUniformValue("line_thickness", lineWidth / framebufferSize().height());

    // Issue instanced drawing command.
    shader.draw(GL_TRIANGLE_STRIP);
}

}   // End of namespace
