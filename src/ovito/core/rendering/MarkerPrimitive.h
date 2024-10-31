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
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A point-like marker primitive to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT MarkerPrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    enum MarkerShape {
        DotShape,
        BoxShape
    };
    Q_ENUM(MarkerShape);

    /// Constructor.
    explicit MarkerPrimitive(MarkerShape shape = DotShape) : _shape(shape) {}

    /// Sets the coordinates of the markers.
    void setPositions(ConstDataBufferPtr coordinates) {
        OVITO_ASSERT(coordinates);
        OVITO_ASSERT(coordinates->componentCount() == 3);
        _positions = std::move(coordinates);
    }

    /// Sets the coordinates of the markers.
    template<typename InputIterator>
    void makePositions(InputIterator begin, InputIterator end) {
        using PointType = typename std::iterator_traits<InputIterator>::value_type;
        using ValueType = typename PointType::value_type;
        OVITO_STATIC_ASSERT((std::is_same_v<PointType, Point_3<ValueType>>));
        setPositions(BufferFactory<PointType>(begin, end).take());
    }

    /// Sets the coordinates of the markers.
    template<typename Range>
    void makePositions(const Range& range) {
        makePositions(std::begin(range), std::end(range));
    }

    /// Returns the buffer storing the marker positions.
    const ConstDataBufferPtr& positions() const { return _positions; }

    /// Sets the color of all markers to the given value.
    void setColor(const ColorA& color) { _color = color; }

    /// Returns the color of the markers.
    const ColorA& color() const { return _color; }

    /// Returns the display shape of markers.
    MarkerShape shape() const { return _shape; }

	/// Computes the 3d bounding box of the primitive in local coordinate space.
	virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const override {
        return visCache.lookup<Box3>(RendererResourceKey<struct MarkerBoundingBoxCache, ConstDataBufferPtr>{positions()}, [this](Box3& bb) {
            if(positions())
                bb = positions()->boundingBox3();
        });
    }

private:

    /// Controls the shape of markers.
    MarkerShape _shape = DotShape;

    /// The color of the markers to be rendered.
    ColorA _color{1,1,1,1};

    /// The internal buffer storing the marker positions.
    ConstDataBufferPtr _positions; // Array of Point3
};

}   // End of namespace
