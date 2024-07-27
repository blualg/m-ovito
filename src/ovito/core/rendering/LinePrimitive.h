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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include "FrameGraphPrimitive.h"

namespace Ovito {

/**
 * \brief A set of lines to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT LinePrimitive final : public FrameGraphPrimitive
{
    Q_GADGET

public:

    /// Sets the coordinates of the line vertices.
    void setPositions(ConstDataBufferPtr coordinates) {
        OVITO_ASSERT(coordinates);
        OVITO_ASSERT(coordinates->componentCount() == 3);
        _positions = std::move(coordinates);
    }

    /// Sets the coordinates of the line vertices.
    template<typename InputIterator>
    void makePositions(InputIterator begin, InputIterator end) {
        using PointType = typename std::iterator_traits<InputIterator>::value_type;
        using ValueType = typename PointType::value_type;
        OVITO_STATIC_ASSERT((std::is_same_v<PointType, Point_3<ValueType>>));
        setPositions(BufferFactory<PointType>(begin, end).take());
    }

    /// Sets the coordinates of the line vertices.
    template<typename Range>
    void makePositions(const Range& range) {
        makePositions(std::begin(range), std::end(range));
    }

    /// Returns the buffer storing the vertex positions.
    const ConstDataBufferPtr& positions() const { return _positions; }

    /// Sets the colors of the vertices.
    void setColors(ConstDataBufferPtr colors) {
        OVITO_ASSERT(!colors || colors->componentCount() == 4);
        _colors = std::move(colors);
    }

    /// Sets the colors of the vertices.
    template<typename InputIterator>
    void makeColors(InputIterator begin, InputIterator end) {
        using ColorType = typename std::iterator_traits<InputIterator>::value_type;
        using ValueType = typename ColorType::value_type;
        OVITO_STATIC_ASSERT((std::is_same_v<ColorType, ColorAT<ValueType>>));
        setColors(BufferFactory<ColorType>(begin, end).take());
    }

    /// Sets the colors of the vertices.
    template<typename Range>
    void makeColors(const Range& range) {
        makeColors(std::begin(range), std::end(range));
    }

    /// Returns the buffer storing the per-vertex colors.
    const ConstDataBufferPtr& colors() const { return _colors; }

    /// Sets the color of all vertices to the given value.
    void setUniformColor(const ColorA& color) { _uniformColor = color; }

    /// Returns the uniform color of all vertices.
    const ColorA& uniformColor() const { return _uniformColor; }

    /// Returns the line width in device pixels.
    FloatType lineWidth() const { return _lineWidth; }

    /// Sets the line width in device pixels.
    void setLineWidth(FloatType width) { _lineWidth = width; }

    /// Returns the pikcing line width in device pixels.
    FloatType pickingLineWidth() const { return _pickingLineWidth; }

    /// Sets the picking line width in device pixels.
    void setPickingLineWidth(FloatType width) { _pickingLineWidth = width; }

	/// Computes the 3d bounding box of the primitive in local coordinate space.
	virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const override {
        auto& bb = visCache.lookup<Box3>(
            RendererResourceKey<struct LineBoundingBoxCache, ConstDataBufferPtr>{positions()},
            [this](Box3& bb) {
                if(positions())
                    bb = positions()->boundingBox3();
            });
        return bb;
    }

private:

    /// The uniform line color.
    ColorA _uniformColor{1,1,1,1};

    /// The visual line width in device pixels.
    FloatType _lineWidth = 0.0;

    /// The picking line width in device pixels.
    FloatType _pickingLineWidth = 0.0;

    /// The buffer storing the vertex positions.
    ConstDataBufferPtr _positions; // Array of Point3

    /// The buffer storing the vertex colors.
    ConstDataBufferPtr _colors; // Array of ColorA
};

}   // End of namespace
