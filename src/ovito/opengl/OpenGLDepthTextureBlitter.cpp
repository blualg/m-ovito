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
#include "OpenGLDepthTextureBlitter.h"

namespace Ovito {

static const char vertex_shader150[] =
    "#version 150 core\n"
    "in vec3 vertexCoord;"
    "in vec2 textureCoord;"
    "out vec2 uv;"
    "void main() {"
    "   uv = textureCoord;"
    "   gl_Position = vec4(vertexCoord,1.0);"
    "}";

static const char fragment_shader150[] =
    "#version 150 core\n"
    "in vec2 uv;"
    "out vec4 fragcolor;"
    "uniform sampler2D textureSampler;"
    "void main() {"
    "   float depth    = texture(textureSampler, uv).x;"  // See https://stackoverflow.com/a/47945422
    "   float depthVal = depth * (256.0 * 256.0 * 256.0 - 1.0) / (256.0 * 256.0 * 256.0);"
    "   vec4 encode    = fract(depthVal * vec4(1.0, 256.0, 256.0 * 256.0, 256.0 * 256.0 * 256.0));"
    "   fragcolor      = vec4(encode.xyz - encode.yzw / 256.0 + 1.0 / 512.0, 0.0).wzyx;"
    "}";

static const char vertex_shader[] =
    "attribute highp vec3 vertexCoord;"
    "attribute highp vec2 textureCoord;"
    "varying highp vec2 uv;"
    "void main() {"
    "   uv = textureCoord;"
    "   gl_Position = vec4(vertexCoord,1.0);"
    "}";

static const char fragment_shader[] =
    "varying highp vec2 uv;"
    "uniform sampler2D textureSampler;"
    "void main() {"
    "   highp float depth    = texture2D(textureSampler, uv).x;" // See https://stackoverflow.com/a/47945422
    "   highp float depthVal = depth * (256.0 * 256.0 * 256.0 - 1.0) / (256.0 * 256.0 * 256.0);"
    "   highp vec4 encode    = fract(depthVal * vec4(1.0, 256.0, 256.0 * 256.0, 256.0 * 256.0 * 256.0));"
    "   gl_FragColor         = vec4(encode.xyz - encode.yzw / 256.0 + 1.0 / 512.0, 0.0).wzyx;"
    "}";

static const GLfloat vertex_buffer_data[] = {
        -1,-1, 0,
        -1, 1, 0,
         1,-1, 0,
        -1, 1, 0,
         1,-1, 0,
         1, 1, 0
};
static const GLfloat texture_buffer_data[] = {
        0, 0,
        0, 1,
        1, 0,
        0, 1,
        1, 0,
        1, 1
};

void OpenGLDepthTextureBlitter::blit(GLuint texture)
{
    QOpenGLContext* context = QOpenGLContext::currentContext();
    vertexBuffer.bind();
    glProgram->setAttributeBuffer(vertexCoordAttribPos, GL_FLOAT, 0, 3, 0);
    glProgram->enableAttributeArray(vertexCoordAttribPos);
    vertexBuffer.release();
    textureBuffer.bind();
    glProgram->setAttributeBuffer(textureCoordAttribPos, GL_FLOAT, 0, 2, 0);
    glProgram->enableAttributeArray(textureCoordAttribPos);
    textureBuffer.release();
    context->functions()->glBindTexture(GL_TEXTURE_2D, texture);
    context->functions()->glDrawArrays(GL_TRIANGLES, 0, 6);
    context->functions()->glBindTexture(GL_TEXTURE_2D, 0);
}

bool OpenGLDepthTextureBlitter::buildProgram(const char *vs, const char *fs)
{
    glProgram = std::make_unique<QOpenGLShaderProgram>();
    glProgram->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    glProgram->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    glProgram->link();
    if(!glProgram->isLinked()) {
        qWarning() << "Could not link shader program:\n" << glProgram->log();
        return false;
    }
    glProgram->bind();
    vertexCoordAttribPos = glProgram->attributeLocation("vertexCoord");
    textureCoordAttribPos = glProgram->attributeLocation("textureCoord");
    glProgram->release();
    return true;
}

bool OpenGLDepthTextureBlitter::create()
{
    QOpenGLContext *currentContext = QOpenGLContext::currentContext();
    if(!currentContext)
        return false;
    if(glProgram)
        return true;
    QSurfaceFormat format = currentContext->format();
    if(format.profile() == QSurfaceFormat::CoreProfile && format.version() >= qMakePair(3,2)) {
        if(!buildProgram(vertex_shader150, fragment_shader150))
            return false;
    } 
    else {
        if(!buildProgram(vertex_shader, fragment_shader))
            return false;
    }
    // Create and bind the VAO, if supported.
    vertexBuffer.create();
    vertexBuffer.bind();
    vertexBuffer.allocate(vertex_buffer_data, sizeof(vertex_buffer_data));
    vertexBuffer.release();
    textureBuffer.create();
    textureBuffer.bind();
    textureBuffer.allocate(texture_buffer_data, sizeof(texture_buffer_data));
    textureBuffer.release();
    return true;
}

bool OpenGLDepthTextureBlitter::isCreated() const
{
    return (bool)glProgram;
}

void OpenGLDepthTextureBlitter::destroy()
{
    if(!isCreated())
        return;
    glProgram.reset();
    vertexBuffer.destroy();
    textureBuffer.destroy();
}

void OpenGLDepthTextureBlitter::bind()
{
    glProgram->bind();
    vertexBuffer.bind();
    glProgram->setAttributeBuffer(vertexCoordAttribPos, GL_FLOAT, 0, 3, 0);
    glProgram->enableAttributeArray(vertexCoordAttribPos);
    vertexBuffer.release();
    textureBuffer.bind();
    glProgram->setAttributeBuffer(textureCoordAttribPos, GL_FLOAT, 0, 2, 0);
    glProgram->enableAttributeArray(textureCoordAttribPos);
    textureBuffer.release();
}

void OpenGLDepthTextureBlitter::release()
{
    glProgram->release();
}

}   // End of namespace
