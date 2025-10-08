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
#include <ovito/core/rendering/ObjectPickingMap.h>

namespace Ovito {

class OpenGLRenderBuffer; // Forward declaration

/**
 * \brief Stores the result of an object picking render pass.
 */
class OVITO_OPENGLRENDERER_EXPORT OpenGLPickingMap : public ObjectPickingMap
{
public:

    /// Returns whether the picking buffer contains valid data.
    bool isValid() const { return !_image.isNull(); }

	/// Releases all data held by the object.
	virtual void reset() override {
		ObjectPickingMap::reset();
		_nextAvailablePickingID = 1;
        _image = {};
        _depthBuffer.reset();
        _numDepthBufferBits = 0;
    }

	/// Registers a range of unique object IDs for a rendering command.
	uint32_t allocateObjectPickingIDs(const FrameGraph::RenderingCommand& command, uint32_t objectCount, ConstDataBufferPtr indices = {});

	/// Finds the picked object at the given frame buffer pixel position.
	virtual std::optional<ViewportWindow::PickResult> pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const override;

    /// Returns the linear object ID at the given frame buffer location.
    uint32_t linearIdAt(const QPoint& frameBufferLocation) const;

    /// Returns the z-value at the given frame buffer location.
    virtual FloatType depthAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const override;

    /// Reads out the contents of the OpenGL framebuffer.
    void acquireFramebufferContents(OpenGLRenderBuffer& renderBuffer);

private:

	/// The next available frame buffer object ID to be used.
	uint32_t _nextAvailablePickingID = 1;

    /// Color component of the OpenGL framebuffer.
    QImage _image;

    /// Depth component of the OpenGL framebuffer.
    std::unique_ptr<quint8[]> _depthBuffer;

    /// The number of depth buffer bits per pixel.
    int _numDepthBufferBits;
};

}   // End of namespace
