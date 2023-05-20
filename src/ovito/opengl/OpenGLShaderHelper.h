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

#pragma once


#include <ovito/core/Core.h>
#include "OpenGLSceneRenderer.h"
#include "OpenGLResourceManager.h"

#ifndef Q_OS_WASM
    #ifndef QT_OPENGL_4
        #error "Expected OpenGL 4.x function definitions to be available."
    #endif
#endif

namespace Ovito {

/**
 * \brief A helper class that creates and binds GLSL shader programs.
 */
class OpenGLShaderHelper
{
public:

    /// Enum specifying the rate at which vertex attributes are pulled from buffers.
    enum VertexInputRate {
        PerVertex,      ///< Specifies that vertex attribute addressing is a function of the vertex index.
        PerInstance     ///< Specifies that vertex attribute addressing is a function of the instance index.
    };

    /// Constructor.
    OpenGLShaderHelper(OpenGLSceneRenderer* renderer) : _renderer(renderer) {}

    /// Returns the internal OpenGL shader object.
    QOpenGLShaderProgram& shaderObject() const { return *_shader; }

    /// Loads a shader program.
    void load(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile = QString());

    /// Destructor.
    ~OpenGLShaderHelper() {
        if(_shader) {
            OVITO_REPORT_OPENGL_ERRORS(_renderer);

            // Reset attribute states.
            for(GLuint attrIndex : _instanceAttributes) {
                OVITO_CHECK_OPENGL(_renderer, _renderer->glVertexAttribDivisor(attrIndex, 0));
            }

            // Unbind the shader program.
            _shader->release();

            // Restore old context state.
            if(_disableBlendingWhenDone)
                _renderer->glDisable(GL_BLEND);
        }
    }

    /// Temporarily enables alpha blending.
    void enableBlending() {
        _disableBlendingWhenDone |= !_renderer->glIsEnabled(GL_BLEND);
        OVITO_CHECK_OPENGL(_renderer, _renderer->glEnable(GL_BLEND));
        OVITO_CHECK_OPENGL(_renderer, _renderer->glBlendEquation(GL_FUNC_ADD));
        OVITO_CHECK_OPENGL(_renderer, _renderer->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_COLOR, GL_ONE));
    }

    /// Binds an OpenGL buffer to a vertex attribute of the shader.
    void bindBuffer(QOpenGLBuffer& buffer, const char* attributeName, GLenum type, int tupleSize, int stride, int offset, VertexInputRate inputRate);

    /// Binds an OpenGL buffer to a vertex attribute of the shader.
    void bindBuffer(QOpenGLBuffer& buffer, int attrIndex, GLenum type, int tupleSize, int stride, int offset, VertexInputRate inputRate);

    /// Disables a vertex attribute of the shader.
    void unbindBuffer(const char* attributeName);

    /// Disables a vertex attribute of the shader.
    void unbindBuffer(int attrIndex);

    /// Passes the base object ID to the shader in picking mode.
    void setPickingBaseId(GLint baseId) {
        OVITO_ASSERT(_renderer->isPicking());
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("picking_base_id", baseId));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, const ColorA& color) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, color.r(), color.g(), color.b(), color.a()));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, const Color& color) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, color.r(), color.g(), color.b()));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, const Vector3& vec) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, vec.x(), vec.y(), vec.z()));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, const Vector4& vec) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, vec.x(), vec.y(), vec.z(), vec.w()));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, FloatType value) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, static_cast<GLfloat>(value)));
    }

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, GLint value) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, value));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, const ColorA& color) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, color.r(), color.g(), color.b(), color.a()));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, const Color& color) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, color.r(), color.g(), color.b()));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, const Vector2& vec) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, vec.x(), vec.y()));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, const Vector3& vec) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, vec.x(), vec.y(), vec.z()));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, const Vector4& vec) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, vec.x(), vec.y(), vec.z(), vec.w()));
    }

    /// Passes a constant vertex attribute to the shader.
    void setAttributeValue(const char* name, FloatType v) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeValue(name, v));
    }

    /// Indicates whether an OpenGL geometry is being used.
    bool usingGeometryShader() const { return _usingGeometryShader; }

    /// Indicates that we have OpenGL support for instanced arrays (requires OpenGL 3.3+).
    bool usingInstancedArrays() const { return _renderer->useInstancedArrays(); }

    /// Indicates that we have OpenGL support for glMultiDrawArraysIndirect (requires OpenGL 4.3+).
    bool usingMultiDrawArraysIndirect() const { return _renderer->useMultiDrawArraysIndirect(); }

    /// Returns the number of vertices per rendered instance.
    GLsizei verticesPerInstance() const { return _verticesPerInstance; }

    /// Specifies the number of vertices per rendered instance.
    void setVerticesPerInstance(GLsizei n) {
        OVITO_ASSERT(!usingGeometryShader() || n == 1);
        _verticesPerInstance = n;
    }

    /// Returns the number of primitive instances stored in the vertex buffers.
    GLsizei vboInstanceCount() const { return _vboInstanceCount; }

    /// Sets the number of primitive instances stored in the vertex buffers.
    void setVboInstanceCount(GLsizei vboInstanceCount) { _vboInstanceCount = vboInstanceCount; }

    /// Returns the number of primitive instances to be rendered.
    GLsizei renderInstanceCount() const { return _renderInstanceCount; }

    /// Sets the number of primitive instances to be rendered.
    void setRenderInstanceCount(GLsizei renderInstanceCount) { _renderInstanceCount = renderInstanceCount; }

    /// Uploads some data to the Vulkan device as a buffer object and caches it.
    template<typename KeyType>
    QOpenGLBuffer createCachedBuffer(KeyType&& cacheKey, GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*)>&& fillMemoryFunc) {

        QOpenGLBuffer* bufferObject;

        // Check if this OVITO data buffer has already been created and uploaded to the GPU.
        // Depending on whether the OpenGL implementation supports instanced arrays, we
        // have to take into account the instancing parameters in the cache lookup.

        // Does the OpenGL implementation support instanced arrays (requires OpenGL 3.3+) or are we using a geometry shader?
        if(usingInstancedArrays() || usingGeometryShader()) {
            bufferObject = &OpenGLResourceManager::instance()->lookup<QOpenGLBuffer>(std::forward<KeyType>(cacheKey), _renderer->currentResourceFrame());
        }
        else {
            // When not using instanced rendering, the no. of vertices per primitive must be included in the cache key,
            // because the data will be repeated in the VBO to emulate instanced rendering.
            std::tuple<std::decay_t<KeyType>, GLsizei, GLsizei> combinedKey{ std::forward<KeyType>(cacheKey), (usage == QOpenGLBuffer::VertexBuffer) ? vboInstanceCount() : renderInstanceCount(), verticesPerInstance() };
            bufferObject = &OpenGLResourceManager::instance()->lookup<QOpenGLBuffer>(std::move(combinedKey), _renderer->currentResourceFrame());
        }

        // If not, do it now.
        if(!bufferObject->isCreated())
            *bufferObject = createCachedBufferImpl(elementSize, usage, inputRate, std::move(fillMemoryFunc));

        OVITO_ASSERT(bufferObject->type() == usage);
        return *bufferObject;
    }

    /// Uploads an OVITO DataBuffer to the GPU device.
    QOpenGLBuffer uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, VertexInputRate inputRate, QOpenGLBuffer::Type usage = QOpenGLBuffer::VertexBuffer);

    /// Issues a drawing command.
    void drawArrays(GLenum mode);

    /// Issues a drawing command using indexed rendering.
    void drawArraysIndexed(GLenum mode, const ConstDataBufferPtr& indices);

    /// Issues a drawing command with an ordering of the instances.
    template<typename KeyType>
    void drawArraysOrdered(GLenum mode, KeyType&& cacheKey, std::function<std::vector<uint32_t>()>&& computeOrderingFunc) {

        // Ordered drawing is not support by picking shaders, which access the gl_InstanceID special variable.
        // That's because the 'baseInstance' parameter does not affect the shader-visible value of gl_InstanceID according to the OpenGL specification.
        OVITO_ASSERT(!_renderer->isPicking());

        // Are we using a geometry shader? If yes, render point primitives only.
        if(usingGeometryShader()) {
            // Look up the index drawing buffer from the cache and call implementation.
            RendererResourceKey<struct IndexBufferKey, std::decay_t<KeyType>> indexBufferKey{ std::forward<KeyType>(cacheKey) };
            drawArraysOrderedGeometryShader(OpenGLResourceManager::instance()->lookup<QOpenGLBuffer>(std::move(indexBufferKey), _renderer->currentResourceFrame()), std::move(computeOrderingFunc));
        }
#ifndef Q_OS_WASM
        else if(usingInstancedArrays()) {
            // On OpenGL 4.3+ contexts, use glMultiDrawArraysIndirect() to render the instances in a prescribed order.
            if(usingMultiDrawArraysIndirect()) {
                // Look up the indirect drawing buffer from the cache and call implementation.
                drawArraysOrderedOpenGL4(mode, OpenGLResourceManager::instance()->lookup<QOpenGLBuffer>(std::forward<KeyType>(cacheKey), _renderer->currentResourceFrame()), std::move(computeOrderingFunc));
            }
            else {
                // Fall back to unsorted drawing.
                drawArrays(mode);
            }
        }
#endif
        else {
            // On older contexts, use glMultiDrawArrays() to render the instances in a prescribed order.

            // Look up the glMultiDrawArrays() parameters from the cache and call implementation.
            drawArraysOrderedOpenGL2or3(mode, OpenGLResourceManager::instance()->lookup<std::pair<std::vector<GLint>, std::vector<GLsizei>>>(std::forward<KeyType>(cacheKey), _renderer->currentResourceFrame()), std::move(computeOrderingFunc));
        }
    }

private:

    /// Uploads some data to a new OpenGL buffer object.
    QOpenGLBuffer createCachedBufferImpl(GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*)>&& fillMemoryFunc);

#ifndef Q_OS_WASM
    /// Issues a drawing command with an ordering of the instances.
    void drawArraysOrderedOpenGL4(GLenum mode, QOpenGLBuffer& indirectBuffer, std::function<std::vector<uint32_t>()>&& computeOrderingFunc);
#endif

    /// Implemention of the drawArrays() method for OpenGL 2.x.
    void drawArraysOpenGL2(GLenum mode);

    /// Issues a drawing command with an ordering of the instances.
    void drawArraysOrderedOpenGL2or3(GLenum mode, std::pair<std::vector<GLint>, std::vector<GLsizei>>& indirectBuffers, std::function<std::vector<uint32_t>()>&& computeOrderingFunc);

    /// Renders the primtives using a geometry shader in a specified order.
    void drawArraysOrderedGeometryShader(QOpenGLBuffer& indexBuffer, std::function<std::vector<uint32_t>()>&& computeOrderingFunc);

    /// Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    void setupVertexAndInstanceIDOpenGL2();

    /// The GLSL shader object.
    QOpenGLShaderProgram* _shader = nullptr;

    /// The renderer object.
    OpenGLSceneRenderer* _renderer;

    /// List of shader vertex attributes that have been marked as per-instance attributes.
    QVarLengthArray<GLuint, 4> _instanceAttributes;

    /// Indicates that alpha blending should be turned off after rendering is done.
    bool _disableBlendingWhenDone = false;

    /// Number of vertices per primitive instance.
    GLsizei _verticesPerInstance = 0;

    /// Number of primitives stored in the vertex buffers.
    GLsizei _vboInstanceCount = 0;

    /// Number of primitives to be rendered.
    GLsizei _renderInstanceCount = 0;

    /// Indicates that a OpenGL geometry shader is active.
    bool _usingGeometryShader = false;
};

}   // End of namespace
