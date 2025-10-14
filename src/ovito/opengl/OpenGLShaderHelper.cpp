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
#include <ovito/core/dataset/data/BufferAccess.h>
#include "OpenGLShaderHelper.h"

namespace Ovito {

/******************************************************************************
* Loads a shader program.
******************************************************************************/
void OpenGLShaderHelper::load(const QString& id, const QString& vertexShaderFile, const QString& fragmentShaderFile, const QString& geometryShaderFile, const QString& shaderPathPrefix)
{
    if(_shader)
        _shader->release();

    // Compile the shader program.
    _shader = _renderer->loadShaderProgram(id,
        shaderPathPrefix + vertexShaderFile,
        shaderPathPrefix + fragmentShaderFile,
        !geometryShaderFile.isEmpty() ? (shaderPathPrefix + geometryShaderFile) : QString());
    OVITO_REPORT_OPENGL_ERRORS(_renderer);

    // Are we using a geometry shader?
    _usingGeometryShader = !geometryShaderFile.isEmpty();

    // Bind the OpenGL shader program.
    if(!_shader->bind())
        throw RendererException(QStringLiteral("Failed to bind OpenGL shader '%1'.").arg(id));
    OVITO_REPORT_OPENGL_ERRORS(_renderer);

    // Set shader uniforms.
    if(!_renderer->_preprojectedCoordinates) {
        const ViewProjectionParameters& projParams = _renderer->frameGraph()->projectionParams();
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_projection_matrix", static_cast<QMatrix4x4>(projParams.projectionMatrix * _renderer->modelViewTM())));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("projection_matrix", static_cast<QMatrix4x4>(projParams.projectionMatrix)));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("inverse_projection_matrix", static_cast<QMatrix4x4>(projParams.inverseProjectionMatrix)));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_matrix", static_cast<QMatrix4x4>(Matrix4(_renderer->modelViewTM()))));
        Matrix3 normalTM;
        if(!_renderer->modelViewTM().linear().inverse(normalTM))
            normalTM.setIdentity();
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("normal_tm", static_cast<QMatrix4x4>(Matrix4(normalTM.transposed()))));
    }
    else {
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_projection_matrix", QMatrix4x4{}));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("projection_matrix", QMatrix4x4{}));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("inverse_projection_matrix", QMatrix4x4{}));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("modelview_matrix", QMatrix4x4{}));
        OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("normal_tm", QMatrix4x4{}));
    }

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
    const QSize& vpSize = _renderer->framebufferSize();
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("viewport_origin", 0.0f, 0.0f));
    OVITO_CHECK_OPENGL(_renderer, _shader->setUniformValue("inverse_viewport_size", 2.0f / vpSize.width(), 2.0f / vpSize.height()));

    // Need to render only the front-facing sides of the geometry.
    OVITO_CHECK_OPENGL(_renderer, _renderer->glCullFace(GL_BACK));
    OVITO_CHECK_OPENGL(_renderer, _renderer->glEnable(GL_CULL_FACE));
}

/******************************************************************************
* Uploads some data to a new OpenGL buffer object.
******************************************************************************/
QOpenGLBuffer OpenGLShaderHelper::createCachedBufferImpl(GLsizei elementSize, QOpenGLBuffer::Type usage, VertexInputRate inputRate, std::function<void(void*, BufferReadAccess<int32_t>)>&& fillMemoryFunc)
{
    // This method must be called from the main thread.
    OVITO_ASSERT(QThread::currentThread() == QOpenGLContext::currentContext()->thread());

    // Per-element data size must be positive.
    OVITO_ASSERT(elementSize > 0);

    // Drawing counts must have been specified.
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(instanceCount() > 0);

    // Determine effective number of elements.
    GLsizei elementCount = instanceCount();

    // When creating an index or GL_DRAW_INDIRECT_BUFFER buffer for rendering a subset of
    // instances, the index buffer comprises only the reduced number of elements.
    if(inputRate == PerInstance && usage != QOpenGLBuffer::VertexBuffer && _instancesSubset) {
        elementCount = _instancesSubset->size();
    }

    // When subset rendering is active, and glMultiDrawArraysIndirect() is not supported, we create reduced VBOs
    // that contain only the subset of data elements.
    const DataBuffer* subsetDataBuffer = nullptr;
    if(inputRate == PerInstance && usage == QOpenGLBuffer::VertexBuffer && _instancesSubset && !usingGeometryShader() && !usingMultiDrawArraysIndirect()) {
        elementCount = _instancesSubset->size();
        subsetDataBuffer = _instancesSubset;
    }

    // Prepare the OpenGL buffer object.
    QOpenGLBuffer bufferObject(usage);
    bufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw);
    if(!bufferObject.create())
        throw RendererException(QStringLiteral("Failed to create OpenGL buffer object."));

    if(!bufferObject.bind()) {
        qWarning() << "QOpenGLBuffer::bind() failed in function OpenGLShaderHelper::createCachedBufferImpl()";
        throw Exception(QStringLiteral("Failed to bind OpenGL buffer object."));
    }

    GLsizei bufferSize;
    bool exceedsLimits = false;
    // Are we using a geometry shader? If yes, there is just one input vertex per instance.
    if(usingGeometryShader()) {
        OVITO_ASSERT(inputRate == PerInstance);
        exceedsLimits = elementCount > (std::numeric_limits<GLsizei>::max() / elementSize);
        bufferSize = elementSize * elementCount;
    }
    // Does the OpenGL implementation support instanced arrays (requires OpenGL 3.3+)?
    else if(usingInstancedArrays()) {
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
        throw RendererException(QStringLiteral("OpenGL buffer allocation failed, because requested size exceeds limits."));

    // Allocate the buffer memory.
    bufferObject.allocate(bufferSize);

#ifndef Q_OS_WASM
    // Fill the buffer with data.
    void* p = bufferObject.map(QOpenGLBuffer::WriteOnly);
    if(p == nullptr)
        throw RendererException(QStringLiteral("Failed to map memory of newly created OpenGL buffer object of size %1 bytes.").arg(bufferSize));
#else
    // WebGL 1/OpenGL ES 2.0 does not support mapping a GL buffer to memory.
    // Need to emulate the map() method by providing a temporary memory buffer on the host.
    std::unique_ptr<std::byte[]> stagingBuffer = std::make_unique<std::byte[]>(bufferSize);
    void* p = stagingBuffer.get();
#endif

    // Call the user-supplied function that fills the buffer with data to be uploaded to GPU memory.
    std::move(fillMemoryFunc)(p, subsetDataBuffer);

    // On older GL contexts, we have to emulate instanced arrays by duplicating all vertex data N times.
    if(!usingInstancedArrays() && !usingGeometryShader()) {
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
    GLsizei uploadDataTypeSize = 0;
    if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Float64) {
        uploadDataTypeSize = sizeof(float);
    }
    else if(dataBuffer->dataType() == DataBuffer::Int8) {
        uploadDataTypeSize = sizeof(int8_t);
    }
    else if(dataBuffer->dataType() == DataBuffer::Int32) {
        OVITO_STATIC_ASSERT(sizeof(GLint) == sizeof(int32_t));
        uploadDataTypeSize = sizeof(int32_t);
    }

    if(uploadDataTypeSize == 0) {
        OVITO_ASSERT(false);
        throw RendererException(QStringLiteral("Cannot create OpenGL buffer object for DataBuffer with data type %1.").arg(dataBuffer->dataTypeName()));
    }

    GLsizei elementSize = 0;
    if(inputRate == PerVertex) {
        elementSize = dataBuffer->size() * dataBuffer->componentCount() / verticesPerInstance() * uploadDataTypeSize;
        OVITO_ASSERT(elementSize * verticesPerInstance() == dataBuffer->size() * dataBuffer->componentCount() * uploadDataTypeSize);
    }
    else if(inputRate == PerInstance) {
        if(usage == QOpenGLBuffer::VertexBuffer) {
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / instanceCount() * uploadDataTypeSize;
            OVITO_ASSERT(elementSize * instanceCount() == dataBuffer->size() * dataBuffer->componentCount() * uploadDataTypeSize);
        }
        else if(usage == QOpenGLBuffer::IndexBuffer) {
            GLsizei renderInstanceCount = _instancesSubset ? _instancesSubset->size() : instanceCount();
            elementSize = dataBuffer->size() * dataBuffer->componentCount() / renderInstanceCount * uploadDataTypeSize;
            OVITO_ASSERT(elementSize * renderInstanceCount == dataBuffer->size() * dataBuffer->componentCount() * uploadDataTypeSize);
        }
    }

    if(elementSize == 0) {
        OVITO_ASSERT(false);
        throw RendererException(QStringLiteral("Cannot create OpenGL buffer object for DataBuffer with data type %1.").arg(dataBuffer->dataTypeName()));
    }

    // Create an OpenGL buffer object and fill it with the data from the DataBuffer.
    return createCachedBuffer(dataBuffer, elementSize, usage, inputRate, [&](void* p, BufferReadAccess<int32_t> subset) {
        if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Int8 || dataBuffer->dataType() == DataBuffer::Int32) {
            RawBufferReadAccess bufferAccess(dataBuffer);
            if(!subset) {
                // Data types of source and destination are the same. Can do a simple memcpy.
                std::memcpy(p, bufferAccess.cdata(), bufferAccess.size() * bufferAccess.stride());
            }
            else {
                // Copy selected data elements only.
                std::byte* dst = static_cast<std::byte*>(p);
                for(int32_t i : subset) {
                    std::memcpy(dst, bufferAccess.cdata() + elementSize * i, elementSize);
                    dst += elementSize;
                }
            }
        }
        else if(dataBuffer->dataType() == DataBuffer::Float64) {
            // Convert from double to float data type.
            BufferReadAccess<double*> bufferAccess(dataBuffer);
            float* dst = static_cast<float*>(p);
            if(!subset) {
                // Copy all data elements.
                for(const double* src = bufferAccess.cbegin(); src != bufferAccess.cend(); ++src, ++dst)
                    *dst = static_cast<float>(*src);
            }
            else {
                // Copy selected data elements only.
                size_t nvalues = elementSize / sizeof(float);
                for(int32_t i : subset) {
                    for(const double* src = bufferAccess.cbegin() + (i * nvalues), *end = src + nvalues; src != end; ++src, ++dst)
                        *dst = static_cast<float>(*src);
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
        throw RendererException(QStringLiteral("Attribute with name %1 does not exist in OpenGL shader program '%2'.").arg(attributeName).arg(_shader->objectName()));
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
        throw RendererException(QStringLiteral("Failed to bind OpenGL vertex buffer for shader '%1'.").arg(_shader->objectName()));
    }
    OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeBuffer(attrIndex, type, offset, tupleSize, stride));
    OVITO_CHECK_OPENGL(_renderer, _shader->enableAttributeArray(attrIndex));
    // Does the OpenGL context support instanced arrays (requires OpenGL 3.3+)?
    if(inputRate == PerInstance && !usingGeometryShader() && usingInstancedArrays()) {
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
        throw RendererException(QStringLiteral("Attribute with name %1 does not exist in OpenGL shader program '%2'.").arg(attributeName).arg(_shader->objectName()));
    }
    unbindBuffer(attrIndex);
}

/******************************************************************************
* Disables a vertex attribute of the shader.
******************************************************************************/
void OpenGLShaderHelper::unbindBuffer(int attrIndex)
{
    OVITO_ASSERT(attrIndex >= 0);
    OVITO_REPORT_OPENGL_ERRORS(_renderer);
    OVITO_CHECK_OPENGL(_renderer, _shader->disableAttributeArray(attrIndex));
}

/******************************************************************************
* Renders the contents of the vertex buffers using the shader.
******************************************************************************/
void OpenGLShaderHelper::draw(GLenum mode)
{
    OVITO_ASSERT(verticesPerInstance() > 0);
    OVITO_ASSERT(instanceCount() > 0);
    OVITO_STATIC_ASSERT(sizeof(GLuint) == sizeof(int32_t));

    // Are we rendering all primitive instances or only a subset?
    if(!_instancesSubset) {
        // Are we using a geometry shader? If yes, render point primitives only.
        if(usingGeometryShader()) {
            OVITO_ASSERT(verticesPerInstance() == 1);

            // Use native command for non-instanced drawing.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(GL_POINTS, 0, instanceCount()));
        }
        // Does the OpenGL context support instanced arrays (requires OpenGL 3.3+)?
        else if(usingInstancedArrays()) {
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
            drawOpenGL2(mode, instanceCount());
        }
    }
    else {
        // Render a subset of primitive instances.
        OVITO_ASSERT(_instancesSubset->dataType() == DataBuffer::Int32 && _instancesSubset->componentCount() == 1);
        GLsizei renderInstanceCount = _instancesSubset->size();

        // Are we using a geometry shader? If yes, render point primitives only.
        if(usingGeometryShader()) {
            OVITO_ASSERT(verticesPerInstance() == 1);

            // Upload the indices array.
            QOpenGLBuffer ibo = uploadDataBuffer(_instancesSubset, PerInstance, QOpenGLBuffer::IndexBuffer);

            // Bind index buffer.
            if(!ibo.bind())
                throw RendererException(QStringLiteral("Failed to bind OpenGL index buffer for shader '%1'.").arg(shaderObject().objectName()));

            // Use native command for indexed drawing.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawElements(GL_POINTS, renderInstanceCount, GL_UNSIGNED_INT, nullptr));

            // Unbind index buffer.
            ibo.release();
        }
        else if(usingMultiDrawArraysIndirect()) {
            // On OpenGL 4.3+ contexts, use glMultiDrawArraysIndirect() to render the instances in a prescribed order.
            OVITO_ASSERT(_renderer->glversion() >= QT_VERSION_CHECK(4, 3, 0));
            OVITO_ASSERT(_renderer->glMultiDrawArraysIndirect != nullptr);

            // Look up indirect drawing buffer.
            const QOpenGLBuffer& indirectBuffer = _renderer->currentResourceFrame().lookup<QOpenGLBuffer>(
                RendererResourceKey<struct IndirectDrawingCache, ConstDataBufferPtr, int>{_instancesSubset, verticesPerInstance()},
                [&](QOpenGLBuffer& indirectBuffer) {
                    // Data structure used by the glMultiDrawArraysIndirect() command:
                    struct DrawArraysIndirectCommand {
                        GLuint count;
                        GLuint instanceCount;
                        GLuint first;
                        GLuint baseInstance;
                    };

                    // Check if this indirect drawing buffer has already been uploaded to the GPU.
                    indirectBuffer = createCachedBufferImpl(sizeof(DrawArraysIndirectCommand), static_cast<QOpenGLBuffer::Type>(GL_DRAW_INDIRECT_BUFFER), PerInstance, [&](void* buffer, BufferReadAccess<int32_t> subset) {
                        OVITO_ASSERT(!subset);
                        // Fill the buffer with DrawArraysIndirectCommand records.
                        DrawArraysIndirectCommand* dst = reinterpret_cast<DrawArraysIndirectCommand*>(buffer);
                        for(auto index : BufferReadAccess<int32_t>(_instancesSubset)) {
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

            // Draw instances in indexed order.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArraysIndirect(mode, nullptr, renderInstanceCount, 0));

            // Done.
            const_cast<QOpenGLBuffer&>(indirectBuffer).release();
        }
        else if(usingInstancedArrays()) {
            // Use native command for instanced drawing.
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArraysInstanced(mode, 0, verticesPerInstance(), renderInstanceCount));
        }
        else {
            // Fallback implementation used if glDrawArraysInstanced() and instanced arrays are not available.
            drawOpenGL2(mode, renderInstanceCount);
        }
    }
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

        // Store the VBO in the cache.
        auto& [vbo, vboSize] = const_cast<std::tuple<QOpenGLBuffer, GLsizei>&>(_renderer->currentResourceFrame().lookup<std::tuple<QOpenGLBuffer, GLsizei>>(
            RendererResourceKey<struct VertexIDCache>{},
            [&](QOpenGLBuffer& vbo, GLsizei& vboSize) { vboSize = 0; }));

        // Recreate/resize the buffer if necessary.
        if(!vbo.isCreated() || vboSize < verticesPerInstance() * instanceCount()) {
            vboSize = verticesPerInstance() * instanceCount();
            vbo = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
            if(!vbo.create() || !vbo.bind())
                throw RendererException(QStringLiteral("Failed to create OpenGL buffer object."));
            OVITO_CHECK_OPENGL(_renderer, vbo.allocate(vboSize * sizeof(float)));
#ifndef Q_OS_WASM
            void* p = vbo.map(QOpenGLBuffer::WriteOnly);
            if(p == nullptr)
                throw RendererException(QStringLiteral("Failed to map memory of newly created OpenGL vertexID buffer of size %1 bytes.").arg(vboSize * sizeof(float)));
            std::iota(reinterpret_cast<float*>(p), reinterpret_cast<float*>(p) + vboSize, 0);
            OVITO_CHECK_OPENGL(_renderer, vbo.unmap());
#else
            // WebGL 1/OpenGL ES 2.0 does not support mapping a GL buffer to memory.
            // Need to emulate the map() method by providing a temporary memory buffer on the host.
            std::vector<float> stagingBuffer(vboSize);
            boost::algorithm::iota(stagingBuffer, 0);
            OVITO_CHECK_OPENGL(_renderer, vbo.write(0, stagingBuffer.data(), vboSize * sizeof(float)));
#endif
        }
        else {
            OVITO_CHECK_OPENGL(_renderer, vbo.bind());
        }
        OVITO_CHECK_OPENGL(_renderer, _shader->setAttributeBuffer("vertexID", GL_FLOAT, 0, 1, 0));
        OVITO_CHECK_OPENGL(_renderer, _shader->enableAttributeArray("vertexID"));
        OVITO_CHECK_OPENGL(_renderer, vbo.release());
    }

    // This is needed to compute the special shader variable 'gl_VertexID' correctly when instanced arrays are not supported:
    if(!usingInstancedArrays()) {
        setUniformValue("vertices_per_instance", verticesPerInstance());
    }
}

/******************************************************************************
* Implementation of the drawArrays() method for OpenGL 2.x.
******************************************************************************/
void OpenGLShaderHelper::drawOpenGL2(GLenum mode, GLsizei renderInstanceCount)
{
    // Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    setupVertexAndInstanceIDOpenGL2();

    if(renderInstanceCount == 1) {
        // Non-instanced drawing command:
        OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, 0, verticesPerInstance()));
    }
    else if(renderInstanceCount > 1) {
        // Use glMultiDrawArrays() if available to draw all instances in one go.
        if(_renderer->glMultiDrawArrays) {
            const auto& [indexFirst, indexCount] = _renderer->currentResourceFrame().lookup<std::tuple<std::vector<GLint>, std::vector<GLsizei>>>(
                RendererResourceKey<struct IndexArrayCache, GLsizei, GLsizei>{ renderInstanceCount, verticesPerInstance() },
                [&](std::vector<GLint>& indexFirst, std::vector<GLsizei>& indexCount) {
                    // Fill the two arrays needed for glMultiDrawArrays().
                    indexCount.resize(renderInstanceCount, verticesPerInstance());
                    indexFirst.resize(renderInstanceCount);
                    GLint index = 0;
                    for(GLint& f : indexFirst)
                        f = (index++) * verticesPerInstance();
                });
            OVITO_ASSERT(indexFirst.size() == renderInstanceCount);
            OVITO_ASSERT(indexCount.size() == renderInstanceCount);
            OVITO_ASSERT(indexCount.front() == verticesPerInstance());
            OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArrays(mode, indexFirst.data(), indexCount.data(), renderInstanceCount));
        }
        else {
            // If glMultiDrawArrays() is not available, fall back to drawing each instance with an individual glDrawArrays() call.
            for(GLsizei i = 0; i < renderInstanceCount; i++) {
                OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, i * verticesPerInstance(), verticesPerInstance()));
            }
        }
    }
}

/******************************************************************************
* Issues a drawing command with an ordering of the instances.
******************************************************************************/
void OpenGLShaderHelper::drawReorderedOpenGL2or3(GLenum mode, std::pair<std::vector<GLint>, std::vector<GLsizei>>& indirectBuffers, std::function<void(std::span<GLuint>)>&& computeOrderingFunc)
{
    // This method is called for OpenGL versions before 3.3, when glMultiDrawArraysIndirect() and instanced arrays are not available.

    // The number of instances to be rendered.
    GLsizei renderInstanceCount = _instancesSubset ? _instancesSubset->size() : instanceCount();

    // Check if the indirect drawing buffers have already been filled.
    if(indirectBuffers.first.empty()) {
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
        indirectBuffers.second.resize(renderInstanceCount, verticesPerInstance());
        indirectBuffers.first.resize(renderInstanceCount);
        auto index = sortedIndices.cbegin();
        for(GLint& f : indirectBuffers.first)
            f = (*index++) * verticesPerInstance();
    }
    OVITO_ASSERT(indirectBuffers.first.size() == renderInstanceCount);
    OVITO_ASSERT(indirectBuffers.second.size() == renderInstanceCount);
    OVITO_ASSERT(indirectBuffers.second.front() == verticesPerInstance());

    // Makes the gl_VertexID and gl_InstanceID special variables available in older OpenGL implementations.
    setupVertexAndInstanceIDOpenGL2();

    // On older GL contexts, emulate instanced arrays by duplicating all vertex data N times.
    // Use glMultiDrawArrays() if available to draw all instances in one go.
    if(_renderer->glMultiDrawArrays) {
        OVITO_CHECK_OPENGL(_renderer, _renderer->glMultiDrawArrays(mode, indirectBuffers.first.data(), indirectBuffers.second.data(), renderInstanceCount));
    }
    else {
        // If glMultiDrawArrays() is not available, fall back to drawing each instance with an individual glDrawArrays() call.
        for(GLsizei i = 0; i < renderInstanceCount; i++) {
            OVITO_CHECK_OPENGL(_renderer, _renderer->glDrawArrays(mode, indirectBuffers.first[i], indirectBuffers.second[i]));
        }
    }
}

}   // End of namespace
