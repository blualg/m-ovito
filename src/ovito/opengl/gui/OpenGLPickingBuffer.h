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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>

namespace Ovito {

/**
 * \brief Stores the result of an object picking render pass.
 */
class OpenGLPickingBuffer
{
public:

    /// Returns whether the picking buffer contains valid data.
    bool isValid() const { return !_image.isNull(); }

    /// Discards the contents of the picking buffer.
    void reset() {
        _image = {};
        _depthBuffer.reset();
        _numDepthBufferBits = 0;
    }

    /// Reads out the contents of the OpenGL framebuffer.
    void acquire(const QSize& size, QOpenGLFunctions* glfuncs);

    /// Returns the object ID at the given window position.
    quint32 objectAt(const QPoint& pos) const;

    /// Returns the z-value at the given window position.
    FloatType depthAt(const QPoint& pos) const;

private:

    /// Color component of the OpenGL framebuffer.
    QImage _image;

    /// Depth component of the OpenGL framebuffer.
    std::unique_ptr<quint8[]> _depthBuffer;

    /// The number of depth buffer bits per pixel.
    int _numDepthBufferBits;
};

}   // End of namespace
