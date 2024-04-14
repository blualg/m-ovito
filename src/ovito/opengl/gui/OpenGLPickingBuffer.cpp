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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include "OpenGLPickingBuffer.h"

namespace Ovito {

/******************************************************************************
* Reads out the contents of the OpenGL framebuffer.
******************************************************************************/
void OpenGLPickingBuffer::acquire(const QSize& size, QOpenGLFunctions* glfuncs)
{
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

    _numDepthBufferBits = QOpenGLContext::currentContext()->format().depthBufferSize();

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
* Returns the frame buffer object ID at the given frame buffer location.
******************************************************************************/
quint32 OpenGLPickingBuffer::objectIdentifierAt(const QPoint& pos) const
{
    if(!_image.isNull()) {
        if(pos.x() >= 0 && pos.x() < _image.width() && pos.y() >= 0 && pos.y() < _image.height()) {
            QPoint mirroredPos(pos.x(), _image.height() - 1 - pos.y());
            QRgb pixel = _image.pixel(mirroredPos);
            quint32 red = qRed(pixel);
            quint32 green = qGreen(pixel);
            quint32 blue = qBlue(pixel);
            quint32 alpha = qAlpha(pixel);
            quint32 objectID = red + (green << 8) + (blue << 16) + (alpha << 24);
            return objectID;
        }
    }
    return 0;
}

/******************************************************************************
* Returns the z-value at the given window position.
******************************************************************************/
FloatType OpenGLPickingBuffer::depthAt(const QPoint& frameBufferLocation) const
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
