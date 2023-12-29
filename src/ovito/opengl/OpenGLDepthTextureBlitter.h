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

#pragma once


#include <ovito/core/Core.h>
#include "OpenGLSceneRenderer.h"

namespace Ovito {

class OpenGLDepthTextureBlitter
{
public:
    ~OpenGLDepthTextureBlitter() { destroy(); }
    bool create();
    bool isCreated() const;
    void bind();
    void release();
    void destroy();
    void blit(GLuint texture);

private:

    bool buildProgram(const char *vs, const char *fs);

    QOpenGLBuffer vertexBuffer;
    QOpenGLBuffer textureBuffer;
    std::unique_ptr<QOpenGLShaderProgram> glProgram;
    GLuint vertexCoordAttribPos = 0;
    GLuint textureCoordAttribPos = 0;
};

}   // End of namespace
