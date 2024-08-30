////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/core/rendering/ImagePrimitive.h>
#include "OpenGLRenderingJob.h"
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Renders an image into a target rectangle given in device pixel coordinates.
******************************************************************************/
void OpenGLRenderingJob::renderImageImplementation(const ImagePrimitive& primitive)
{
    if(isPickingPass() || primitive.image().isNull() || primitive.windowRect().isEmpty())
        return;

    // Activate the OpenGL shader program.
    OpenGLShaderHelper shader(this);
    shader.load("image", "image/image.vert", "image/image.frag");

    shader.setVerticesPerInstance(4);
    shader.setInstanceCount(1);

    // Turn the image into an OpenGL texture.
    const OpenGLTexture& texture = uploadImage(primitive.image());
    texture.get().bind();

    // Transform rectangle to normalized device coordinates.
    Box2 b = primitive.windowRect();
    int aaLevel = multisamplingLevel();
    if(aaLevel > 1) {
        b.minc.x() = (int)(b.minc.x() / aaLevel) * aaLevel;
        b.minc.y() = (int)(b.minc.y() / aaLevel) * aaLevel;
        b.maxc.x() = (int)(b.maxc.x() / aaLevel) * aaLevel;
        b.maxc.y() = (int)(b.maxc.y() / aaLevel) * aaLevel;
    }
    const QSize& vpSize = framebufferSize();
    Vector4 image_rect(
        b.minc.x() / vpSize.width() * 2.0 - 1.0,
        1.0 - b.maxc.y() / vpSize.height() * 2.0,
        b.maxc.x() / vpSize.width() * 2.0 - 1.0,
        1.0 - b.minc.y() / vpSize.height() * 2.0);

    // Pass the image rectangle to the shader as a uniform.
    shader.setUniformValue("image_rect", image_rect);

    // Temporarily enable alpha blending.
    shader.enableBlending();

    // Draw a quad with 4 vertices.
    shader.draw(GL_TRIANGLE_STRIP);

    // Release the texture.
    texture.get().release();
}

}   // End of namespace
