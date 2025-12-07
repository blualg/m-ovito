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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include "OpenGLRenderingJob.h"

#ifndef Q_OS_WASM
    #ifndef QT_OPENGL_4
        #error "Expected OpenGL 4.x function definitions to be available."
    #endif
#endif

namespace Ovito {

/**
 * \brief A helper class that creates and binds GLSL shader programs.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLShaderHelper
{
public:

    /// Enum specifying the rate at which vertex attributes are pulled from buffers.
    enum VertexInputRate {
        PerVertex,      ///< Specifies that vertex attribute addressing is a function of the vertex index.
        PerInstance     ///< Specifies that vertex attribute addressing is a function of the instance index.
    };

    /// Constructor.
    OpenGLShaderHelper(OpenGLRenderingJob* renderer) : _renderer(renderer) {}

    /// Returns the internal OpenGL shader object.
    QOpenGLShaderProgram& shaderObject() const { return *_shader; }

    /// Loads a shader program.
    void load(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile = QString(), const QString& shaderPathPrefix = QStringLiteral(":/openglrenderer/glsl/"));

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
    void enableBlending(bool usePremultipliedAlpha = false) {
        _disableBlendingWhenDone |= !_renderer->glIsEnabled(GL_BLEND);
        OVITO_CHECK_OPENGL(_renderer, _renderer->glEnable(GL_BLEND));
        OVITO_CHECK_OPENGL(_renderer, _renderer->glBlendEquation(GL_FUNC_ADD));
        if(usePremultipliedAlpha == false) {
            OVITO_CHECK_OPENGL(_renderer, _renderer->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_COLOR, GL_ONE));
        }
        else {
            OVITO_CHECK_OPENGL(_renderer, _renderer->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
        }
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
        OVITO_ASSERT(_renderer->isPickingPass());
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

    /// Passes a uniform value to the shader.
    void setUniformValue(const char* name, GLfloat x, GLfloat y) {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue(name, x, y));
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
    GLsizei instanceCount() const { return _instanceCount; }

    /// Sets the number of primitive instances stored in the vertex buffers.
    void setInstanceCount(GLsizei instanceCount) { _instanceCount = instanceCount; }

    /// Activates the rendering of only a subset of the primitives. Must be called before uploadDataBuffer().
    void enableSubsetRendering(const ConstDataBufferPtr& indices) {
        _instancesSubset = indices;
    }

    /// Uploads some data to the graphics device as a buffer object and caches it.
    template<typename KeyType>
    QOpenGLBuffer createCachedBuffer(KeyType&& cacheKey, GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*, size_t, BufferReadAccess<int32_t>)>&& fillMemoryFunc) {

        // Check if this OVITO data buffer has already been created and uploaded to the GPU.
        // Depending on whether the OpenGL implementation supports instanced arrays, we
        // have to take into account the instancing parameters in the cache lookup.

        // Does the OpenGL implementation support instanced arrays (requires OpenGL 3.3+) or are we using a geometry shader?
        if(usingInstancedArrays() || usingGeometryShader()) {
            return _renderer->currentResourceFrame().lookup<QOpenGLBuffer>(
                std::tuple<std::decay_t<KeyType>, ConstDataBufferPtr>{
                    std::forward<KeyType>(cacheKey),
                    (inputRate == PerInstance && usage == QOpenGLBuffer::VertexBuffer) ? _instancesSubset : nullptr
                },
                [&](QOpenGLBuffer& bufferObject) {
                    bufferObject = createCachedBufferImpl(elementSize, usage, inputRate, std::move(fillMemoryFunc));
                });
        }
        else {
            // When not using instanced rendering, the no. of vertices per primitive must be included in the cache key,
            // because the data will be repeated in the VBO to emulate instanced rendering.
            return _renderer->currentResourceFrame().lookup<QOpenGLBuffer>(
                std::tuple<std::decay_t<KeyType>, GLsizei, GLsizei, ConstDataBufferPtr>{
                    std::forward<KeyType>(cacheKey),
                    instanceCount(),
                    verticesPerInstance(),
                    (inputRate == PerInstance && usage == QOpenGLBuffer::VertexBuffer) ? _instancesSubset : nullptr
                },
                [&](QOpenGLBuffer& bufferObject) {
                    bufferObject = createCachedBufferImpl(elementSize, usage, inputRate, std::move(fillMemoryFunc));
                });
        }
    }

    /// Uploads an OVITO DataBuffer to the GPU device.
    QOpenGLBuffer uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, VertexInputRate inputRate, QOpenGLBuffer::Type usage = QOpenGLBuffer::VertexBuffer) {
        return uploadDataBuffer(dataBuffer, 0, dataBuffer->size(), inputRate, usage);
    }

    /// Uploads a subrange of an OVITO DataBuffer to the GPU device.
    QOpenGLBuffer uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, size_t elementOffset, size_t elementCount, VertexInputRate inputRate, QOpenGLBuffer::Type usage = QOpenGLBuffer::VertexBuffer);

    /// Issues a drawing command.
    void draw(GLenum mode);

    /// Paints the instances in a prescribed order, which may be different from the storage order.
    template<typename KeyType>
    void drawReordered(GLenum mode, KeyType&& cacheKey, std::function<void(std::span<GLuint>)>&& computeOrderingFunc) {

        // Ordered drawing is not support by picking shaders, which access the gl_InstanceID special variable.
        // That's because the 'baseInstance' parameter does not affect the shader-visible value of gl_InstanceID according to the OpenGL specification.
        OVITO_ASSERT(!_renderer->isPickingPass());

        // Are we using a geometry shader? If yes, render point primitives only.
        if(usingGeometryShader()) {
            // Look up the index drawing buffer from the cache and call implementation.
            RendererResourceKey<struct IndexBufferKey,
                std::decay_t<KeyType>, // the primitive paint order
                GLsizei,               // the number of instances
                ConstDataBufferPtr     // the instances subset (if any)
            >
            indexBufferKey{ std::forward<KeyType>(cacheKey), instanceCount(), _instancesSubset };

            // The number of instances to be rendered.
            GLsizei renderInstanceCount = _instancesSubset ? _instancesSubset->size() : instanceCount();

            // Check if this index buffer has already been uploaded to the GPU.
            const QOpenGLBuffer& indexBuffer = _renderer->currentResourceFrame().lookup<QOpenGLBuffer>(
                std::move(indexBufferKey),
                [&](QOpenGLBuffer& indexBuffer) {
                    indexBuffer = createCachedBufferImpl(sizeof(GLsizei), QOpenGLBuffer::IndexBuffer, PerInstance, [&](void* buffer, size_t numBytes, BufferReadAccess<int32_t> subset) {
                        OVITO_ASSERT(!subset);
                        OVITO_ASSERT(numBytes == renderInstanceCount * sizeof(GLuint));
                        OVITO_ASSERT(!_instancesSubset || _instancesSubset->size() == renderInstanceCount);
                        auto sortedIndices = std::span(static_cast<GLuint*>(buffer), renderInstanceCount);
                        if(!_instancesSubset)
                            std::iota(sortedIndices.begin(), sortedIndices.end(), (GLuint)0);
                        else
                            std::ranges::copy(BufferReadAccess<int32_t>(_instancesSubset), sortedIndices.begin());
                        // Call user function to generate the element ordering.
                        std::move(computeOrderingFunc)(sortedIndices);
                    });
                });

            // Bind index buffer.
            if(!const_cast<QOpenGLBuffer&>(indexBuffer).bind())
                throw RendererException(QStringLiteral("Failed to bind OpenGL index buffer for shader '%1'.").arg(shaderObject().objectName()));

            // Draw point primitives in sorted order.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawElements(GL_POINTS, renderInstanceCount, GL_UNSIGNED_INT, nullptr));

            const_cast<QOpenGLBuffer&>(indexBuffer).release();
        }
#ifndef Q_OS_WASM
        // On OpenGL 4.3+ contexts, use glMultiDrawArraysIndirect() to render the instances in a prescribed order.
        else if(usingMultiDrawArraysIndirect()) {
            OVITO_ASSERT(_renderer->glversion() >= QT_VERSION_CHECK(4, 3, 0));
            OVITO_ASSERT(_renderer->glMultiDrawArraysIndirect != nullptr);

            // The number of instances to be rendered.
            GLsizei renderInstanceCount = _instancesSubset ? _instancesSubset->size() : instanceCount();

            // Data structure used by the glMultiDrawArraysIndirect() command:
            struct DrawArraysIndirectCommand {
                GLuint count;
                GLuint instanceCount;
                GLuint first;
                GLuint baseInstance;
            };

            // Check if this indirect drawing buffer has already been uploaded to the GPU.
            const QOpenGLBuffer& indirectBuffer = _renderer->currentResourceFrame().lookup<QOpenGLBuffer>(
                std::forward<KeyType>(cacheKey),
                [&](QOpenGLBuffer& indirectBuffer) {
                    indirectBuffer = createCachedBufferImpl(sizeof(DrawArraysIndirectCommand), static_cast<QOpenGLBuffer::Type>(GL_DRAW_INDIRECT_BUFFER), PerInstance, [&](void* buffer, size_t numBytes, BufferReadAccess<int32_t> subset) {
                        OVITO_ASSERT(!subset);
                        OVITO_ASSERT(numBytes == renderInstanceCount * sizeof(GLuint));
                        OVITO_ASSERT(!_instancesSubset || _instancesSubset->size() == renderInstanceCount);
                        auto sortedIndices = std::span(static_cast<GLuint*>(buffer), renderInstanceCount);
                        if(!_instancesSubset)
                            std::iota(sortedIndices.begin(), sortedIndices.end(), (GLuint)0);
                        else
                            std::ranges::copy(BufferReadAccess<int32_t>(_instancesSubset), sortedIndices.begin());
                        // Call user function to generate the element ordering.
                        std::move(computeOrderingFunc)(sortedIndices);

                        // Fill the buffer with DrawArraysIndirectCommand records.
                        DrawArraysIndirectCommand* dst = reinterpret_cast<DrawArraysIndirectCommand*>(buffer);
                        for(auto index : sortedIndices) {
                            dst->count = verticesPerInstance();
                            dst->instanceCount = 1;
                            dst->first = 0;
                            dst->baseInstance = index;
                            ++dst;
                        }
                    });
                });

            // Bind the indirect drawing GL buffer.
            if(!const_cast<QOpenGLBuffer&>(indirectBuffer).bind())
                throw RendererException(QStringLiteral("Failed to bind OpenGL indirect drawing buffer for shader '%1'.").arg(shaderObject().objectName()));

            // Draw instances in sorted order.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArraysIndirect(mode, nullptr, renderInstanceCount, 0));

            // Done.
            const_cast<QOpenGLBuffer&>(indirectBuffer).release();
        }
        else if(usingInstancedArrays()) {
            // Give up and fall back to unsorted drawing.
            // Todo: find a better solution for this case in the future.
            draw(mode);
        }
#endif
        else {
            // On older contexts, use glMultiDrawArrays() to render the instances in a prescribed order.
            // Look up the glMultiDrawArrays() parameters from the cache and call implementation.
            // Look up the index drawing buffer from the cache and call implementation.
            RendererResourceKey<struct IndexBufferKey,
                std::decay_t<KeyType>, // the primitive paint order
                GLsizei,               // the number of instances
                ConstDataBufferPtr     // the instances subset (if any)
            >
            indexBufferKey{ std::forward<KeyType>(cacheKey), instanceCount(), _instancesSubset };

            // The number of instances to be rendered.
            GLsizei renderInstanceCount = _instancesSubset ? _instancesSubset->size() : instanceCount();

            // Check if the indirect drawing buffers have already been filled.
            const auto& [indirectFirst, indirectCount] = _renderer->currentResourceFrame().lookup<std::tuple<std::vector<GLint>, std::vector<GLsizei>>>(
                std::move(indexBufferKey),
                [&](std::vector<GLint>& indirectFirst, std::vector<GLsizei>& indirectCount) {
                    std::vector<GLuint> sortedIndices(renderInstanceCount);
                    if(!_instancesSubset)
                        std::iota(sortedIndices.begin(), sortedIndices.end(), (GLuint)0);
                    else
                        std::ranges::copy(BufferReadAccess<int32_t>(_instancesSubset), sortedIndices.begin());
                    // Call user function to generate the element ordering.
                    std::move(computeOrderingFunc)(sortedIndices);

                    // Remap indices to compacted range.
                    if(_instancesSubset) {
                        std::vector<GLuint> mapping(instanceCount());
                        GLuint j = 0;
                        for(auto i : BufferReadAccess<int32_t>(_instancesSubset))
                            mapping[i] = j++;
                        for(auto& k : sortedIndices)
                            k = mapping[k];
                    }

                    // Fill the two arrays needed for glMultiDrawArrays().
                    indirectCount.resize(renderInstanceCount, verticesPerInstance());
                    indirectFirst.resize(renderInstanceCount);
                    auto index = sortedIndices.cbegin();
                    for(GLint& f : indirectFirst)
                        f = (*index++) * verticesPerInstance();
                });
            OVITO_ASSERT(indirectFirst.size() == renderInstanceCount);
            OVITO_ASSERT(indirectCount.size() == renderInstanceCount);
            OVITO_ASSERT(indirectCount.front() == verticesPerInstance());

            // Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
            setupVertexAndInstanceIDOpenGL2();

            // On older GL contexts, emulate instanced arrays by duplicating all vertex data N times.
            // Use glMultiDrawArrays() if available to draw all instances in one go.
            if(_renderer->glMultiDrawArrays) {
                OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArrays(mode, indirectFirst.data(), indirectCount.data(), renderInstanceCount));
            }
            else {
                // If glMultiDrawArrays() is not available, fall back to drawing each instance with an individual glDrawArrays() call.
                for(GLsizei i = 0; i < renderInstanceCount; i++) {
                    OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, indirectFirst[i], indirectCount[i]));
                }
            }
        }
    }

private:

    /// Uploads some data to a new OpenGL buffer object.
    QOpenGLBuffer createCachedBufferImpl(GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*, size_t, BufferReadAccess<int32_t>)>&& fillMemoryFunc);

    /// Implementation of the draw() method for OpenGL 2.x.
    void drawOpenGL2(GLenum mode, GLsizei renderInstanceCount);

    /// Issues a drawing command with an ordering of the instances.
    void drawReorderedOpenGL2or3(GLenum mode, std::pair<std::vector<GLint>, std::vector<GLsizei>>& indirectBuffers, std::function<void(std::span<GLuint>)>&& computeOrderingFunc);

    /// Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    void setupVertexAndInstanceIDOpenGL2();

    /// The GLSL shader object.
    QOpenGLShaderProgram* _shader = nullptr;

    /// The rendering job.
    OpenGLRenderingJob* _renderer;

    /// List of shader vertex attributes that have been marked as per-instance attributes.
    QVarLengthArray<GLuint, 4> _instanceAttributes;

    /// Indicates that alpha blending should be turned off after rendering is done.
    bool _disableBlendingWhenDone = false;

    /// Number of vertices per primitive instance.
    GLsizei _verticesPerInstance = 0;

    /// Number of primitives stored in the vertex buffers.
    GLsizei _instanceCount = 0;

    /// Indices specifying the subset of primitives to be rendered.
    ConstDataBufferPtr _instancesSubset;

    /// Indicates that a OpenGL geometry shader is active.
    bool _usingGeometryShader = false;
};

}   // End of namespace
