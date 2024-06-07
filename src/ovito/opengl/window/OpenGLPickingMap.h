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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/core/rendering/ObjectPickingIdentifierMap.h>

namespace Ovito {

/**
 * \brief Stores the result of an object picking render pass.
 */
class OVITO_OPENGLRENDERERWINDOW_EXPORT OpenGLPickingMap : public ObjectPickingIdentifierMap
{
public:

    /// Returns whether the picking buffer contains valid data.
    bool isValid() const { return !_image.isNull(); }

	/// Releases all data held by the object.
	virtual void reset() override {
		ObjectPickingIdentifierMap::reset();
        _image = {};
        _depthBuffer.reset();
        _numDepthBufferBits = 0;
    }

    /// Reads out the contents of the OpenGL framebuffer.
    void acquire(const OORef<AbstractRenderingFrameBuffer>& frameBuffer);

    /// Returns the frame buffer object ID at the given frame buffer location.
    virtual quint32 objectIdentifierAt(const QPoint& pos) const override;

    /// Returns the z-value at the given frame buffer location.
    virtual FloatType depthAt(const QPoint& frameBufferLocation) const override;

private:

    /// Color component of the OpenGL framebuffer.
    QImage _image;

    /// Depth component of the OpenGL framebuffer.
    std::unique_ptr<quint8[]> _depthBuffer;

    /// The number of depth buffer bits per pixel.
    int _numDepthBufferBits;
};

}   // End of namespace
