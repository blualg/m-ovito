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
#include "OpenGLRenderer.h"
#include "OpenGLRenderingFrameBuffer.h"
#include "OpenGLPickingMap.h"

namespace Ovito {

/******************************************************************************
* Registers a range of unique object IDs for a rendering command.
******************************************************************************/
uint32_t OpenGLPickingMap::allocateObjectPickingIDs(const FrameGraph::RenderingCommand& command, uint32_t objectCount, ConstDataBufferPtr indices)
{
    OVITO_ASSERT(!command.skipInPickingPass());
    OVITO_ASSERT(objectCount != 0);

    auto baseObjectID = _nextAvailablePickingID;
    _pickingRecords.emplace(baseObjectID, PickingRecord(command, std::move(indices)));
    _nextAvailablePickingID += objectCount;
    return baseObjectID;
}

/******************************************************************************
* Finds the picked object at the given frame buffer pixel position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> OpenGLPickingMap::pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(uint32_t linearId = linearIdAt(frameBufferLocation)) {
        auto [baseObjectID, pickingRecord] = lookupPickingRecordFromLinearId(linearId);
        if(pickingRecord) {
            OVITO_ASSERT(pickingRecord->pipeline());
            return ViewportWindow::PickResult(
                const_cast<Pipeline*>(pickingRecord->pipeline().get()),
                pickingRecord->pickInfo(),
                worldPositionAt(frameBufferLocation, projectionParams, framebufferSize),
                pickingRecord->resolveSubObjectID(linearId - baseObjectID));
        }
    }
    return std::nullopt;
}

/******************************************************************************
* Reads out the contents of the OpenGL framebuffer.
******************************************************************************/
void OpenGLPickingMap::acquireFramebufferContents(const OORef<AbstractRenderingFrameBuffer>& frameBuffer)
{
    OORef<OpenGLRenderingFrameBuffer> glFrameBuffer = static_object_cast<OpenGLRenderingFrameBuffer>(frameBuffer);
    OVITO_ASSERT(glFrameBuffer->framebufferObject());
    const QSize& size = glFrameBuffer->framebufferSize();

    // The following requires an active GL context.
    OpenGLContextRestore contextRestore = glFrameBuffer->renderingJob()->activateContext();
    QOpenGLContext* glcontext = QOpenGLContext::currentContext();
    QOpenGLFunctions* glfuncs = glcontext->functions();

    if(!glFrameBuffer->framebufferObject()->bind())
        throw RendererException(QStringLiteral("Failed to bind OpenGL framebuffer object."));

    // Read out the color buffer.
    // Try GL_BGRA pixel format first. If not supported, use GL_RGBA instead and convert back to GL_BGRA.
    _image = QImage(size, QImage::Format_ARGB32);
    glfuncs->glReadPixels(0, 0, size.width(), size.height(), 0x80E1 /*GL_BGRA*/, GL_UNSIGNED_BYTE, _image.bits());
    if(glfuncs->glGetError() != GL_NO_ERROR) {
        glfuncs->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, _image.bits());
        _image = std::move(_image).rgbSwapped();
    }

    // Acquire OpenGL depth buffer data.
    // The depth information is used to compute the XYZ coordinate of the point under the mouse cursor.
    _numDepthBufferBits = glcontext->format().depthBufferSize();

    if(_numDepthBufferBits == 16) {
        _depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLushort));
        glfuncs->glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, _depthBuffer.get());
    }
    else if(_numDepthBufferBits == 24) {
        _depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLuint));
        while(glfuncs->glGetError() != GL_NO_ERROR);
        glfuncs->glReadPixels(0, 0, size.width(), size.height(), 0x84F9 /*GL_DEPTH_STENCIL*/, 0x84FA /*GL_UNSIGNED_INT_24_8*/, _depthBuffer.get());
        if(glfuncs->glGetError() != GL_NO_ERROR) {
            glfuncs->glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_FLOAT, _depthBuffer.get());
            _numDepthBufferBits = 0;
        }
    }
    else if(_numDepthBufferBits == 32) {
        _depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLuint));
        glfuncs->glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, _depthBuffer.get());
    }
    else {
        _depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLfloat));
        glfuncs->glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_FLOAT, _depthBuffer.get());
        _numDepthBufferBits = 0;
    }
}

/******************************************************************************
* Returns the linear object ID at the given frame buffer location.
******************************************************************************/
uint32_t OpenGLPickingMap::linearIdAt(const QPoint& frameBufferLocation) const
{
    if(!_image.isNull()) {
        if(frameBufferLocation.x() >= 0 && frameBufferLocation.x() < _image.width() && frameBufferLocation.y() >= 0 && frameBufferLocation.y() < _image.height()) {
            QPoint mirroredPos(frameBufferLocation.x(), _image.height() - 1 - frameBufferLocation.y());
            QRgb pixel = _image.pixel(mirroredPos);
            uint32_t red = qRed(pixel);
            uint32_t green = qGreen(pixel);
            uint32_t blue = qBlue(pixel);
            uint32_t alpha = qAlpha(pixel);
            uint32_t objectID = red + (green << 8) + (blue << 16) + (alpha << 24);
            return objectID;
        }
    }
    return 0;
}

/******************************************************************************
* Returns the z-value at the given window position.
******************************************************************************/
FloatType OpenGLPickingMap::depthAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(!_image.isNull() && _depthBuffer) {
        int w = _image.width();
        int h = _image.height();
        if(frameBufferLocation.x() >= 0 && frameBufferLocation.x() < w && frameBufferLocation.y() >= 0 && frameBufferLocation.y() < h) {
            QPoint mirroredPos(frameBufferLocation.x(), _image.height() - 1 - frameBufferLocation.y());
            if(_image.pixel(mirroredPos) != 0) {
                if(_numDepthBufferBits == 16) {
                    GLushort bval = reinterpret_cast<const GLushort*>(_depthBuffer.get())[(mirroredPos.y()) * w + frameBufferLocation.x()];
                    return (FloatType)bval / FloatType(65535.0);
                }
                else if(_numDepthBufferBits == 24) {
                    GLuint bval = reinterpret_cast<const GLuint*>(_depthBuffer.get())[(mirroredPos.y()) * w + frameBufferLocation.x()];
                    return (FloatType)((bval >> 8) & 0x00FFFFFF) / FloatType(16777215.0);
                }
                else if(_numDepthBufferBits == 32) {
                    GLuint bval = reinterpret_cast<const GLuint*>(_depthBuffer.get())[(mirroredPos.y()) * w + frameBufferLocation.x()];
                    return (FloatType)bval / FloatType(4294967295.0);
                }
                else if(_numDepthBufferBits == 0) {
                    return reinterpret_cast<const GLfloat*>(_depthBuffer.get())[(mirroredPos.y()) * w + frameBufferLocation.x()];
                }
            }
        }
    }
    return 0;
}

}   // End of namespace
