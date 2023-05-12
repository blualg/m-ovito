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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include <ovito/core/dataset/data/DataBufferAccess.h>

namespace Ovito {

/**
 * \brief A point-like marker primitive to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT MarkerPrimitive final
{
    Q_GADGET

public:

    enum MarkerShape {
        DotShape,
        BoxShape
    };
    Q_ENUM(MarkerShape);

    /// Constructor.
    explicit MarkerPrimitive(MarkerShape shape = DotShape) : _shape(shape) {}

    /// \brief Sets the coordinates of the markers.
    void setPositions(ConstDataBufferPtr coordinates) {
        OVITO_ASSERT(coordinates);
        OVITO_ASSERT(coordinates->dataType() == DataBuffer::Float && coordinates->componentCount() == 3);
        _positions = std::move(coordinates);
    }

    /// \brief Sets the coordinates of the markers.
    template<typename InputIterator>
    void makePositions(InputIterator begin, InputIterator end) {
        size_t count = std::distance(begin, end);
        DataBufferAccessAndRef<Point3> buffer = DataBufferPtr::create(count, DataBuffer::Float, 3);
        std::copy(std::move(begin), std::move(end), buffer.begin());
        setPositions(buffer.take());
    }

    /// \brief Sets the coordinates of the markers.
    template<typename Range>
    void makePositions(const Range& range) {
        makePositions(std::begin(range), std::end(range));
    }

    /// Returns the buffer storing the marker positions.
    const ConstDataBufferPtr& positions() const { return _positions; }

    /// \brief Sets the color of all markers to the given value.
    void setColor(const ColorA& color) { _color = color; }

    /// \brief Returns the color of the markers.
    const ColorA& color() const { return _color; }

    /// \brief Returns the display shape of markers.
    MarkerShape shape() const { return _shape; }

private:

    /// Controls the shape of markers.
    MarkerShape _shape = DotShape;

    /// The color of the markers to be rendered.
    ColorA _color{1,1,1,1};

    /// The internal buffer storing the marker positions.
    ConstDataBufferPtr _positions; // Array of Point3
};

}   // End of namespace
