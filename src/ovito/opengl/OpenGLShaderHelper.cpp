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
#include <ovito/core/dataset/data/BufferAccess.h>
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Loads a shader program.
******************************************************************************/
void OpenGLShaderHelper::load(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile)
{
    if(_shader)
        _shader->release();

    // Prepend this to paths when loading GLSL shaders from the resource file.
    QString prefix = QStringLiteral(":/openglrenderer/glsl/");

    // Compile the shader program.
    _shader = _renderer->loadShaderProgram(id,
        prefix + vertexShaderFile,
        prefix + fragmentShaderFile,
        !geometryShaderFile.isEmpty() ? prefix + geometryShaderFile : QString());
    OVITO_REPORT_OPENGL_ERRORS(_renderer);

    // Are we using a geometry shader?
    _usingGeometryShader = !geometryShaderFile.isEmpty();

    // Bind the OpenGL shader program.
    if(!_shader->bind())
        throw SceneRenderer::RendererException(QStringLiteral("Failed to bind OpenGL shader '%1'.").arg(id));
    OVITO_REPORT_OPENGL_ERRORS(_renderer);

    // Set shader uniforms.
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_projection_matrix", static_cast<QMatrix4x4>(_renderer->projParams().projectionMatrix * _renderer->modelViewTM())));
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("projection_matrix", static_cast<QMatrix4x4>(_renderer->projParams().projectionMatrix)));
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("inverse_projection_matrix", static_cast<QMatrix4x4>(_renderer->projParams().inverseProjectionMatrix)));
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_matrix", static_cast<QMatrix4x4>(Matrix4(_renderer->modelViewTM()))));
    Matrix3 normalTM;
    if(!_renderer->modelViewTM().linear().inverse(normalTM))
        normalTM.setIdentity();
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("normal_tm", static_cast<QMatrix4x4>(Matrix4(normalTM.transposed()))));

    // Set up uniform constant data arrays.
    int unitCubeTriangleStripUniform = _shader->uniformLocation("unit_cube_triangle_strip");
    if(unitCubeTriangleStripUniform >= 0) {
        // Const array of vertex positions for the cube triangle strip.
        static constexpr QVector3D cubeVerts[14] = {
            { 1,  1,  1},
            { 1, -1,  1},
            { 1,  1, -1},
            { 1, -1, -1},
            {-1, -1, -1},
            { 1, -1,  1},
            {-1, -1,  1},
            { 1,  1,  1},
            {-1,  1,  1},
            { 1,  1, -1},
            {-1,  1, -1},
            {-1, -1, -1},
            {-1,  1,  1},
            {-1, -1,  1},
        };
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValueArray(unitCubeTriangleStripUniform, cubeVerts, 14));
    }
    int unitBoxTriangleStripUniform = _shader->uniformLocation("unit_box_triangle_strip");
    if(unitBoxTriangleStripUniform >= 0) {
        // Const array of vertex positions for the box triangle strip.
        static constexpr QVector3D boxVerts[14] = {
            { 1,  1,  1},
            { 1, -1,  1},
            { 1,  1,  0},
            { 1, -1,  0},
            {-1, -1,  0},
            { 1, -1,  1},
            {-1, -1,  1},
            { 1,  1,  1},
            {-1,  1,  1},
            { 1,  1,  0},
            {-1,  1,  0},
            {-1, -1,  0},
            {-1,  1,  1},
            {-1, -1,  1},
        };
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValueArray(unitBoxTriangleStripUniform, boxVerts, 14));
    }
    int unitCubeStripNormalsUniform = _shader->uniformLocation("unit_cube_strip_normals");
    if(unitCubeStripNormalsUniform >= 0) {
        // Const array of normal vectors for the cube triangle strip.
        static constexpr QVector3D normals[14] = {
            { 1,  0,  0},
            { 1,  0,  0},
            { 1,  0,  0},
            { 1,  0,  0},
            { 0,  0, -1},
            { 0, -1,  0},
            { 0, -1,  0},
            { 0,  0,  1},
            { 0,  0,  1},
            { 0,  1,  0},
            { 0,  1,  0},
            { 0,  0, -1},
            {-1,  0,  0},
            {-1,  0,  0}
        };
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValueArray(unitCubeStripNormalsUniform, normals, 14));
    }
    int unitQuadTriangleStripUniform = _shader->uniformLocation("unit_quad_triangle_strip");
    if(unitQuadTriangleStripUniform >= 0) {
        // Const array of vertex positions for the quad triangle strip.
        static constexpr QVector2D quadVerts[4] = {
            {-1, -1},
            { 1, -1},
            {-1,  1},
            { 1,  1}
        };
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValueArray(unitQuadTriangleStripUniform, quadVerts, 4));
    }

    // Get current viewport rectangle.
    const QRect& vpRect = _renderer->viewportRect();
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("viewport_origin", (float)vpRect.x(), (float)vpRect.y()));
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("inverse_viewport_size", 2.0f / vpRect.width(), 2.0f / vpRect.height()));

    // Need to render only the front-facing sides of the geometry.
    OVITO_CHECK_OPENGL(_renderer, _renderer->glCullFace(GL_BACK));
    OVITO_CHECK_OPENGL(_renderer, _renderer->glEnable(GL_CULL_FACE));
}

/******************************************************************************
* Uploads some data to a new OpenGL buffer object.
******************************************************************************/
QOpenGLBuffer OpenGLShaderHelper::createCachedBufferImpl(GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*)>&& fillMemoryFunc)
{
    // This method must be called from the main thread.
    OVITO_ASSERT(QThread::currentThread() == QOpenGLContext::currentContext()->thread());

    // Per-element data size must be positive.
    OVITO_ASSERT(elementSize > 0);

    // Drawing counts must have been specified.
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(instanceCount() > 0);
    OVITO_ASSERT(inputRate != PerIndex || indexCount() > 0);
    OVITO_ASSERT(inputRate != PerIndex || usage == QOpenGLBuffer::IndexBuffer);

    // Determine effective number of elements.
    GLsizei elementCount = (inputRate != PerIndex) ? instanceCount() : indexCount();

    // Prepare the OpenGL buffer object.
    QOpenGLBuffer bufferObject(usage);
    bufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw);
    if(!bufferObject.create())
        throw SceneRenderer::RendererException(QStringLiteral("Failed to create OpenGL buffer object."));

    if(!bufferObject.bind()) {
        qWarning() << "QOpenGLBuffer::bind() failed in function OpenGLShaderHelper::createCachedBufferImpl()";
        throw Exception(QStringLiteral("Failed to bind OpenGL buffer object."));
    }

    GLsizei bufferSize;
    bool exceedsLimits = false;
    // Are we using a geometry shader? If yes, there is just one input vertex per instance.
    if(usingGeometryShader()) {
        OVITO_ASSERT(inputRate == PerInstance || inputRate == PerIndex);
        exceedsLimits = elementCount > (std::numeric_limits<GLsizei>::max() / elementSize);
        bufferSize = elementSize * elementCount;
    }
    // Does the OpenGL implementation support instanced arrays (requires OpenGL 3.3+)?
    else if(_renderer->glversion() >= QT_VERSION_CHECK(3, 3, 0)) {
        if(inputRate == PerVertex) {
            exceedsLimits = verticesPerInstance() > (std::numeric_limits<GLsizei>::max() / elementSize);
            bufferSize = elementSize * verticesPerInstance();
        }
        else {
            exceedsLimits = elementCount > (std::numeric_limits<GLsizei>::max() / elementSize);
            bufferSize = elementSize * elementCount;
        }
    }
    else {
        // On older GL contexts, we have to emulate instanced arrays by duplicating all vertex data N times.
        exceedsLimits = elementCount > (std::numeric_limits<GLsizei>::max() / elementSize / verticesPerInstance());
        bufferSize = elementSize * verticesPerInstance() * elementCount;
    }

    // Check if the requested vertex buffer size exceeds limits.
    if(exceedsLimits)
        throw SceneRenderer::RendererException(QStringLiteral("OpenGL buffer allocation failed, because requested size exceeds limits."));

    // Allocate the buffer memory.
    bufferObject.allocate(bufferSize);

#ifndef Q_OS_WASM
    // Fill the buffer with data.
    void* p = bufferObject.map(QOpenGLBuffer::WriteOnly);
    if(p == nullptr)
        throw SceneRenderer::RendererException(QStringLiteral("Failed to map memory of newly created OpenGL buffer object of size %1 bytes.").arg(bufferSize));
#else
    // WebGL 1/OpenGL ES 2.0 does not support mapping a GL buffer to memory.
    // Need to emulate the map() method by providing a temporary memory buffer on the host.
    std::unique_ptr<std::byte[]> stagingBuffer = std::make_unique<std::byte[]>(bufferSize);
    void* p = stagingBuffer.get();
#endif

    // Call the user-supplied function that fills the buffer with data to be uploaded to GPU memory.
    std::move(fillMemoryFunc)(p);

    // On older GL contexts, we have to emulate instanced arrays by duplicating all vertex data N times.
    if(_renderer->glversion() < QT_VERSION_CHECK(3, 3, 0) && !usingGeometryShader()) {
        if(inputRate == PerVertex) {
            if(elementCount > 1) {
                size_t chunkSize = elementSize * verticesPerInstance();
                for(int i = 1; i < elementCount; i++)
                    std::memcpy(reinterpret_cast<std::byte*>(p) + (i * chunkSize), p, chunkSize);
            }
        }
        else {
            if(verticesPerInstance() > 1) {
                // Note: Doing copies in reverse order to not overwrite input data.
                for(int j = elementCount - 1; j >= 0; j--) {
                    const std::byte* src = reinterpret_cast<const std::byte*>(p) + (j * elementSize);
                    std::byte* dst = reinterpret_cast<std::byte*>(p) + (j * elementSize * verticesPerInstance());
                    for(int i = 0; i < verticesPerInstance(); i++, dst += elementSize) {
                        OVITO_ASSERT(dst >= src);
                        std::memcpy(dst, src, elementSize);
                    }
                }
            }
        }
    }

#ifndef Q_OS_WASM
    bufferObject.unmap();
#else
    bufferObject.write(0, stagingBuffer.get(), bufferSize);
#endif
    bufferObject.release();
    OVITO_ASSERT(bufferObject.isCreated());

    return bufferObject;
}

/******************************************************************************
* Uploads the data of an OVITO DataBuffer to an OpenGL buffer object.
******************************************************************************/
QOpenGLBuffer OpenGLShaderHelper::uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, VertexInputRate inputRate, QOpenGLBuffer::Type usage)
{
    OVITO_ASSERT(dataBuffer);

    // Determine the required buffer size.
    GLsizei elementSize = 0;
    if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Float64) {
        if(inputRate == PerVertex) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / verticesPerInstance() * sizeof(float);
            OVITO_ASSERT(elementSize * verticesPerInstance() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(float));
        }
        else if(inputRate == PerInstance) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / instanceCount() * sizeof(float);
            OVITO_ASSERT(elementSize * instanceCount() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(float));
        }
    }
    else if(dataBuffer->dataType() == DataBuffer::Int8) {
        if(inputRate == PerVertex) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / verticesPerInstance() * sizeof(int8_t);
            OVITO_ASSERT(elementSize * verticesPerInstance() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(int8_t));
        }
        else if(inputRate == PerInstance) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / instanceCount() * sizeof(int8_t);
            OVITO_ASSERT(elementSize * instanceCount() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(int8_t));
        }
    }
    else if(dataBuffer->dataType() == DataBuffer::Int32) {
        OVITO_STATIC_ASSERT(sizeof(GLint) == sizeof(int32_t));
        if(inputRate == PerVertex) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / verticesPerInstance() * sizeof(int32_t);
            OVITO_ASSERT(elementSize * verticesPerInstance() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(int32_t));
        }
        else if(inputRate == PerInstance) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / instanceCount() * sizeof(int32_t);
            OVITO_ASSERT(elementSize * instanceCount() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(int32_t));
        }
        else if(inputRate == PerIndex) {
            OVITO_ASSERT(dataBuffer->componentCount() == 1);
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / indexCount() * sizeof(int32_t);
            OVITO_ASSERT(elementSize * indexCount() == dataBuffer->size() * dataBuffer->componentCount() * sizeof(int32_t));
        }
    }

    if(elementSize == 0) {
        OVITO_ASSERT(false);
        throw SceneRenderer::RendererException(QStringLiteral("Cannot create OpenGL buffer object for DataBuffer with data type %1.").arg(dataBuffer->dataType()));
    }

    // Create an OpenGL buffer object and fill it with the data from the OVITO DataBuffer object.
    return createCachedBuffer(dataBuffer, elementSize, usage, inputRate, [&](void* p) {
        const auto srcStride = dataBuffer->componentCount();
        const auto dstStride = dataBuffer->componentCount();
        if(dataBuffer->dataType() == DataBuffer::Float32) {
            float* dst = static_cast<float*>(p);
            BufferAccess<const float*> arrayAccess(dataBuffer);
            if(dstStride == srcStride && dataBuffer->stride() == sizeof(float) * srcStride) {
                // Strides are the same for source and destination. Can do a simple memcpy.
                std::memcpy(dst, arrayAccess.cbegin(), arrayAccess.size() * dstStride * sizeof(float));
            }
            else {
                // Strides are different for source and destination. Need nested loops for copying.
                for(const float* src = arrayAccess.cbegin(); src != arrayAccess.cend(); src += srcStride, dst += dstStride) {
                    for(size_t i = 0; i < srcStride; i++)
                        dst[i] = src[i];
                }
            }
        }
        else if(dataBuffer->dataType() == DataBuffer::Float64) {
            float* dst = static_cast<float*>(p);
            // Convert from double to float data type.
            BufferAccess<const double*> arrayAccess(dataBuffer);
            if(dstStride == srcStride && dataBuffer->stride() == sizeof(double) * srcStride) {
                // Strides are the same for source and destination. Need only a single loop for copying.
                for(const double* src = arrayAccess.cbegin(); src != arrayAccess.cend(); ++src, ++dst)
                    *dst = static_cast<float>(*src);
            }
            else {
                // Strides are different for source and destination. Need nested loops for copying.
                for(const double* src = arrayAccess.cbegin(); src != arrayAccess.cend(); src += srcStride, dst += dstStride) {
                    for(size_t i = 0; i < srcStride; i++)
                        dst[i] = static_cast<float>(src[i]);
                }
            }
        }
        else if(dataBuffer->dataType() == DataBuffer::Int8) {
            int8_t* dst = static_cast<int8_t*>(p);
            BufferAccess<const int8_t*> arrayAccess(dataBuffer);
            if(dstStride == srcStride && dataBuffer->stride() == sizeof(int8_t) * srcStride) {
                // Strides are the same for source and destination. Can do a simple memcpy.
                std::memcpy(dst, arrayAccess.cbegin(), arrayAccess.size() * dstStride * sizeof(int8_t));
            }
            else {
                // Strides are different for source and destination. Need nested loops for copying.
                for(const int8_t* src = arrayAccess.cbegin(); src != arrayAccess.cend(); src += srcStride, dst += dstStride) {
                    for(size_t i = 0; i < srcStride; i++)
                        dst[i] = src[i];
                }
            }
        }
        else if(dataBuffer->dataType() == DataBuffer::Int32) {
            int32_t* dst = static_cast<int32_t*>(p);
            BufferAccess<const int32_t*> arrayAccess(dataBuffer);
            if(dstStride == srcStride && dataBuffer->stride() == sizeof(int32_t) * srcStride) {
                // Strides are the same for source and destination. Can do a simple memcpy.
                std::memcpy(dst, arrayAccess.cbegin(), arrayAccess.size() * dstStride * sizeof(int32_t));
            }
            else {
                // Strides are different for source and destination. Need nested loops for copying.
                for(const int32_t* src = arrayAccess.cbegin(); src != arrayAccess.cend(); src += srcStride, dst += dstStride) {
                    for(size_t i = 0; i < srcStride; i++)
                        dst[i] = src[i];
                }
            }
        }
    });
}

/******************************************************************************
* Binds an OpenGL buffer to a vertex attribute of the shader.
******************************************************************************/
void OpenGLShaderHelper::bindBuffer(QOpenGLBuffer& buffer, const char* attributeName, GLenum type, int tupleSize, int stride, int offset, VertexInputRate inputRate)
{
    int attrIndex = _shader->attributeLocation(attributeName);
    if(attrIndex < 0) {
        qWarning() << "OpenGLShaderHelper::bindBuffer() failed for shader" << _shader->objectName() << ": attribute with name" << attributeName << "does not exist in shader.";
        throw SceneRenderer::RendererException(QStringLiteral("Attribute with name %1 does not exist in OpenGL shader program '%2'.").arg(attributeName).arg(_shader->objectName()));
    }
    bindBuffer(buffer, attrIndex, type, tupleSize, stride, offset, inputRate);
}

/******************************************************************************
* Binds an OpenGL buffer to a vertex attribute of the shader.
******************************************************************************/
void OpenGLShaderHelper::bindBuffer(QOpenGLBuffer& buffer, int attrIndex, GLenum type, int tupleSize, int stride, int offset, VertexInputRate inputRate)
{
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(instanceCount() > 0);
    OVITO_ASSERT(attrIndex >= 0);
    OVITO_REPORT_OPENGL_ERRORS(_renderer);
    OVITO_ASSERT(buffer.isCreated());
    if(!buffer.bind()) {
        qWarning() << "OpenGLShaderHelper::bindBuffer() failed for shader" << _shader->objectName();
        throw SceneRenderer::RendererException(QStringLiteral("Failed to bind OpenGL vertex buffer for shader '%1'.").arg(_shader->objectName()));
    }
    OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeBuffer(attrIndex, type, offset, tupleSize, stride));
    OVITO_CHECK_OPENGL(_renderer, _shader->enableAttributeArray(attrIndex));
    // Does the OpenGL context support instanced arrays (requires OpenGL 3.3+)?
    if(inputRate == PerInstance && !usingGeometryShader() && _renderer->glversion() >= QT_VERSION_CHECK(3, 3, 0)) {
        OVITO_CHECK_OPENGL(_renderer, _renderer->glVertexAttribDivisor(attrIndex, 1));
        _instanceAttributes.push_back(attrIndex);
    }
    buffer.release();
}

/******************************************************************************
* Disables a vertex attribute of the shader.
******************************************************************************/
void OpenGLShaderHelper::unbindBuffer(const char* attributeName)
{
    int attrIndex = _shader->attributeLocation(attributeName);
    if(attrIndex < 0) {
        qWarning() << "OpenGLShaderHelper::unbindBuffer() failed for shader" << _shader->objectName() << ": attribute with name" << attributeName << "does not exist in shader.";
        throw SceneRenderer::RendererException(QStringLiteral("Attribute with name %1 does not exist in OpenGL shader program '%2'.").arg(attributeName).arg(_shader->objectName()));
    }
    unbindBuffer(attrIndex);
}

/******************************************************************************
* Disables a vertex attribute of the shader.
******************************************************************************/
void OpenGLShaderHelper::unbindBuffer(int attrIndex)
{
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(instanceCount() > 0);
    OVITO_ASSERT(attrIndex >= 0);
    OVITO_REPORT_OPENGL_ERRORS(_renderer);
    OVITO_CHECK_OPENGL(_renderer, _shader->disableAttributeArray(attrIndex));
}

/******************************************************************************
* Issues a regular drawing command.
******************************************************************************/
void OpenGLShaderHelper::drawArrays(GLenum mode)
{
    OVITO_ASSERT(verticesPerInstance() > 0);

    // Are we using a geometry shader? If yes, render point primitives only.
    if(usingGeometryShader()) {
        OVITO_ASSERT(verticesPerInstance() == 1);

        // Use native command for non-instanced drawing.
        OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(GL_POINTS, 0, instanceCount()));
    }
    // Does the OpenGL context support instanced arrays (requires OpenGL 3.3+)?
    else if(_renderer->glversion() >= QT_VERSION_CHECK(3, 3, 0)) {
        if(instanceCount() == 1) {
            // Use native command for non-instanced drawing.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, 0, verticesPerInstance()));
        }
        else if(instanceCount() > 1) {
            // Use native command for instanced drawing.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArraysInstanced(mode, 0, verticesPerInstance(), instanceCount()));
        }
    }
    else {
        // Fall-back implementation if glDrawArraysInstanced() or instanced arrays are not available.
        drawArraysOpenGL2(mode);
    }
}

/******************************************************************************
* Issues a drawing command using indexed rendering.
******************************************************************************/
void OpenGLShaderHelper::drawArraysIndexed(GLenum mode, const ConstDataBufferPtr& indices)
{
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(indices);
    OVITO_ASSERT(indices->dataType() == DataBuffer::Int32 && indices->componentCount() == 1);
    OVITO_ASSERT(indices->size() != 0);
    OVITO_STATIC_ASSERT(sizeof(GLuint) == sizeof(int32_t));

    // Remember the number of primitive instances to render.
    _indexCount = indices->size();

    // Are we using a geometry shader? If yes, render point primitives only.
    if(usingGeometryShader()) {
        OVITO_ASSERT(verticesPerInstance() == 1);

        // Upload the indices array.
        QOpenGLBuffer ibo = uploadDataBuffer(indices, PerIndex, QOpenGLBuffer::IndexBuffer);

        // Bind index buffer.
        if(!ibo.bind())
            throw SceneRenderer::RendererException(QStringLiteral("Failed to bind OpenGL index buffer for shader '%1'.").arg(shaderObject().objectName()));

        // Use native command for indexed drawing.
        OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawElements(GL_POINTS, indices->size(), GL_UNSIGNED_INT, nullptr));

        // Unbind index buffer.
        ibo.release();
    }
    // Does the OpenGL context support instanced arrays (requires OpenGL 3.3+)?
    else if(_renderer->glversion() >= QT_VERSION_CHECK(3, 3, 0)) {
        OVITO_ASSERT(false);
        // Use native command for instanced drawing.
//        OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArraysInstanced(mode, 0, verticesPerInstance(), instanceCount()));
    }
    else {
        OVITO_ASSERT(false);
        // Fall-back implementation if glDrawArraysInstanced() or instanced arrays are not available.
//        drawArraysOpenGL2(mode, indices);
    }
}

#ifndef Q_OS_WASM
/******************************************************************************
* Issues a drawing command with an ordering of the instances.
******************************************************************************/
void OpenGLShaderHelper::drawArraysOrderedOpenGL4(GLenum mode, QOpenGLBuffer& indirectBuffer, std::function<std::vector<uint32_t>()>&& computeOrderingFunc)
{
    // On OpenGL 4.3+ contexts, use glMultiDrawArraysIndirect() to render the instances in a prescribed order.
    OVITO_ASSERT(_renderer->glversion() >= QT_VERSION_CHECK(4, 3, 0));
    OVITO_ASSERT(_renderer->glMultiDrawArraysIndirect != nullptr);

    // Data structure used by the glMultiDrawArraysIndirect() command:
    struct DrawArraysIndirectCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
    };

    // Check if this indirect drawing buffer has already been uploaded to the GPU.
    if(!indirectBuffer.isCreated()) {
        indirectBuffer = createCachedBufferImpl(sizeof(DrawArraysIndirectCommand), static_cast<QOpenGLBuffer::Type>(GL_DRAW_INDIRECT_BUFFER), PerInstance, [&](void* buffer) {
            // Call user function to generate the element ordering.
            std::vector<uint32_t> sortedIndices = std::move(computeOrderingFunc)();
            OVITO_ASSERT(sortedIndices.size() == instanceCount());

            // Fill the buffer with DrawArraysIndirectCommand records.
            DrawArraysIndirectCommand* dst = reinterpret_cast<DrawArraysIndirectCommand*>(buffer);
            for(uint32_t index : sortedIndices) {
                dst->count = verticesPerInstance();
                dst->instanceCount = 1;
                dst->first = 0;
                dst->baseInstance = index;
                ++dst;
            }
        });
    }

    // Bind the indirect drawing GL buffer.
    if(!indirectBuffer.bind())
        throw SceneRenderer::RendererException(QStringLiteral("Failed to bind OpenGL indirect drawing buffer for shader '%1'.").arg(shaderObject().objectName()));

    // Draw instances in sorted order.
    OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArraysIndirect(mode, nullptr, instanceCount(), 0));

    // Done.
    indirectBuffer.release();
}
#endif

/******************************************************************************
* Renders the primtives using a geometry shader in a specified order.
******************************************************************************/
void OpenGLShaderHelper::drawArraysOrderedGeometryShader(QOpenGLBuffer& indexBuffer, std::function<std::vector<uint32_t>()>&& computeOrderingFunc)
{
    // Check if this index buffer has already been uploaded to the GPU.
    if(!indexBuffer.isCreated()) {
        indexBuffer = createCachedBufferImpl(sizeof(GLsizei), QOpenGLBuffer::IndexBuffer, PerInstance, [&](void* buffer) {
            // Call user function to generate the element ordering.
            std::vector<uint32_t> sortedIndices = std::move(computeOrderingFunc)();
            OVITO_ASSERT(sortedIndices.size() == instanceCount());
            // Copy sorted indices into the index buffer.
            std::memcpy(buffer, sortedIndices.data(), instanceCount() * sizeof(GLsizei));
        });
    }

    // Bind index buffer.
    if(!indexBuffer.bind())
        throw SceneRenderer::RendererException(QStringLiteral("Failed to bind OpenGL index buffer for shader '%1'.").arg(shaderObject().objectName()));

    // Draw point primitives in sorted order.
    OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawElements(GL_POINTS, instanceCount(), GL_UNSIGNED_INT, nullptr));

    indexBuffer.release();
}

/******************************************************************************
* Makes the gl_VertexID and gl_InstanceID special variables available in
* older OpenGL implementations.
******************************************************************************/
void OpenGLShaderHelper::setupVertexAndInstanceIDOpenGL2()
{
    if(_renderer->glversion() < QT_VERSION_CHECK(3, 0, 0)) {
        // In GLSL 1.20, the 'gl_VertexID' and 'gl_InstanceID' special variables are not available.
        // Then we have to emulate them by providing a buffer-backed vertex attribute named 'vertexID' instead.

        std::pair<QOpenGLBuffer, GLsizei>& buf = OpenGLResourceManager::instance()->lookup<std::pair<QOpenGLBuffer, GLsizei>>(RendererResourceKey<struct VertexIDCache>{}, _renderer->currentResourceFrame());

        if(!buf.first.isCreated() || buf.second < verticesPerInstance() * instanceCount()) {
            buf.second = verticesPerInstance() * instanceCount();
            buf.first = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            buf.first.setUsagePattern(QOpenGLBuffer::StaticDraw);
            if(!buf.first.create() || !buf.first.bind())
                throw SceneRenderer::RendererException(QStringLiteral("Failed to create OpenGL buffer object."));
            OVITO_CHECK_OPENGL(_renderer, buf.first.allocate(buf.second * sizeof(float)));
#ifndef Q_OS_WASM
            void* p = buf.first.map(QOpenGLBuffer::WriteOnly);
            if(p == nullptr)
                throw SceneRenderer::RendererException(QStringLiteral("Failed to map memory of newly created OpenGL vertexID buffer of size %1 bytes.").arg(buf.second * sizeof(float)));
            std::iota(reinterpret_cast<float*>(p), reinterpret_cast<float*>(p) + buf.second, 0);
            OVITO_CHECK_OPENGL(_renderer, buf.first.unmap());
#else
            // WebGL 1/OpenGL ES 2.0 does not support mapping a GL buffer to memory.
            // Need to emulate the map() method by providing a temporary memory buffer on the host.
            std::vector<float> stagingBuffer(buf.second);
            std::iota(stagingBuffer.begin(), stagingBuffer.end(), 0);
            OVITO_CHECK_OPENGL(_renderer, buf.first.write(0, stagingBuffer.data(), buf.second * sizeof(float)));
#endif
        }
        else {
            OVITO_CHECK_OPENGL(_renderer, buf.first.bind());
        }
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeBuffer("vertexID", GL_FLOAT, 0, 1, 0));
        OVITO_CHECK_OPENGL(_renderer, _shader->enableAttributeArray("vertexID"));
        OVITO_CHECK_OPENGL(_renderer, buf.first.release());
    }

    // This is needed to compute the special shader variable 'gl_VertexID' correctly when instanced arrays are not supported:
    if(_renderer->glversion() < QT_VERSION_CHECK(3, 3, 0)) {
        setUniformValue("vertices_per_instance", verticesPerInstance());
    }
}

/******************************************************************************
* Implemention of the drawArrays() method for OpenGL 2.x.
******************************************************************************/
void OpenGLShaderHelper::drawArraysOpenGL2(GLenum mode)
{
    // Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    setupVertexAndInstanceIDOpenGL2();

    if(instanceCount() == 1) {
        // Non-instanced drawing command:
        OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, 0, verticesPerInstance()));
    }
    else if(instanceCount() > 1) {
        // Use glMultiDrawArrays() if available to draw all instances in one go.
        if(_renderer->glMultiDrawArrays) {
            RendererResourceKey<struct IndexArrayCache, GLsizei, GLsizei> indexArrayCacheKey{ instanceCount(), verticesPerInstance() };
            std::pair<std::vector<GLint>, std::vector<GLsizei>>& indexArrays = OpenGLResourceManager::instance()->lookup<std::pair<std::vector<GLint>, std::vector<GLsizei>>>(indexArrayCacheKey, _renderer->currentResourceFrame());
            if(indexArrays.first.empty()) {
                // Fill the two arrays needed for glMultiDrawArrays().
                indexArrays.second.resize(instanceCount(), verticesPerInstance());
                indexArrays.first.resize(instanceCount());
                GLint index = 0;
                for(GLint& f : indexArrays.first)
                    f = (index++) * verticesPerInstance();
            }
            OVITO_ASSERT(indexArrays.first.size() == instanceCount());
            OVITO_ASSERT(indexArrays.second.size() == instanceCount());
            OVITO_ASSERT(indexArrays.second.front() == verticesPerInstance());
            OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArrays(mode, indexArrays.first.data(), indexArrays.second.data(), instanceCount()));
        }
        else {
            // If glMultiDrawArrays() is not available, fall back to drawing each instance with an individual glDrawArrays() call.
            for(GLsizei i = 0; i < instanceCount(); i++) {
                OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, i * verticesPerInstance(), verticesPerInstance()));
            }
        }
    }
}

/******************************************************************************
* Issues a drawing command with an ordering of the instances.
******************************************************************************/
void OpenGLShaderHelper::drawArraysOrderedOpenGL2or3(GLenum mode, std::pair<std::vector<GLint>, std::vector<GLsizei>>& indirectBuffers, std::function<std::vector<uint32_t>()>&& computeOrderingFunc)
{
    // This method is called for OpenGL versions before 4.3, when the glMultiDrawArraysIndirect() function is not available.

    // If the OpenGL implementation is old enough and doesn't support instanced arrays either,
    // we can make use of glMultiDrawArrays() to render the instanced in a prescribed order.
    // However, on newer OpenGL implementations (3.3 <= version < 4.3), there is nothing we can do to
    // draw the instanced arrays in a prescribed order.
    if(_renderer->glversion() >= QT_VERSION_CHECK(3, 3, 0)) {
        // Fall back to unsorted drawing.
        drawArrays(mode);
        return;
    }

    // Check if the indirect drawing buffers have already been filled.
    if(indirectBuffers.first.empty()) {
        // Call user function to generate the element ordering.
        std::vector<uint32_t> sortedIndices = std::move(computeOrderingFunc)();
        OVITO_ASSERT(sortedIndices.size() == instanceCount());

        // Fill the two arrays needed for glMultiDrawArrays().
        indirectBuffers.second.resize(instanceCount(), verticesPerInstance());
        indirectBuffers.first.resize(instanceCount());
        auto index = sortedIndices.cbegin();
        for(GLint& f : indirectBuffers.first)
            f = (*index++) * verticesPerInstance();
    }
    OVITO_ASSERT(indirectBuffers.first.size() == instanceCount());
    OVITO_ASSERT(indirectBuffers.second.size() == instanceCount());
    OVITO_ASSERT(indirectBuffers.second.front() == verticesPerInstance());

    // Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    setupVertexAndInstanceIDOpenGL2();

    // On older GL contexts, emulate instanced arrays by duplicating all vertex data N times.
    // Use glMultiDrawArrays() if available to draw all instances in one go.
    if(_renderer->glMultiDrawArrays) {
        OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArrays(mode, indirectBuffers.first.data(), indirectBuffers.second.data(), instanceCount()));
    }
    else {
        // If glMultiDrawArrays() is not available, fall back to drawing each instance with an individual glDrawArrays() call.
        for(GLsizei i = 0; i < instanceCount(); i++) {
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, indirectBuffers.first[i], indirectBuffers.second[i]));
        }
    }
}

}   // End of namespace
