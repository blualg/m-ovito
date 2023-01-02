////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
 * \brief A set of lines to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT LinePrimitive final
{
	Q_GADGET

public:

	/// \brief Sets the coordinates of the line vertices.
	void setPositions(ConstDataBufferPtr coordinates) {
		OVITO_ASSERT(coordinates);
		OVITO_ASSERT(coordinates->dataType() == DataBuffer::Float && coordinates->componentCount() == 3);
		_positions = std::move(coordinates);
	}

	/// \brief Sets the coordinates of the line vertices.
	template<typename InputIterator>
	void makePositions(InputIterator begin, InputIterator end) {
		size_t count = std::distance(begin, end);
		DataBufferAccessAndRef<Point3> buffer = DataBufferPtr::create(count, DataBuffer::Float, 3);
		std::copy(std::move(begin), std::move(end), buffer.begin());
		setPositions(buffer.take());
	}

	/// \brief Sets the coordinates of the line vertices.
	template<typename Range>
	void makePositions(const Range& range) {
		makePositions(std::begin(range), std::end(range));
	}

	/// Returns the buffer storing the vertex positions.
	const ConstDataBufferPtr& positions() const { return _positions; }

	/// \brief Sets the colors of the vertices.
	void setColors(ConstDataBufferPtr colors) {
		OVITO_ASSERT(!colors || colors->dataType() == DataBuffer::Float && colors->componentCount() == 4);
		_colors = std::move(colors);
	}

	/// \brief Sets the colors of the vertices.
	template<typename InputIterator>
	void makeColors(InputIterator begin, InputIterator end) {
		size_t count = std::distance(begin, end);
		DataBufferAccessAndRef<ColorA> buffer = DataBufferPtr::create(count, DataBuffer::Float, 4);
		std::copy(std::move(begin), std::move(end), buffer.begin());
		setColors(buffer.take());
	}

	/// \brief Sets the colors of the vertices.
	template<typename Range>
	void makeColors(const Range& range) {
		makeColors(std::begin(range), std::end(range));
	}

	/// Returns the buffer storing the per-vertex colors.
	const ConstDataBufferPtr& colors() const { return _colors; }

	/// \brief Sets the color of all vertices to the given value.
	void setUniformColor(const ColorA& color) { _uniformColor = color; }

	/// \brief Returns the uniform color of all vertices.
	const ColorA& uniformColor() const { return _uniformColor; }

	/// \brief Returns the line width in pixels.
	FloatType lineWidth() const { return _lineWidth; }

	/// \brief Sets the line width in pixels.
	void setLineWidth(FloatType width) { _lineWidth = width; }

private:

	/// The uniform line color.
	ColorA _uniformColor{1,1,1,1};

	/// The line width in pixels.
	FloatType _lineWidth = 0.0;

	/// The buffer storing the vertex positions.
	ConstDataBufferPtr _positions; // Array of Point3

	/// The buffer storing the vertex colors.
	ConstDataBufferPtr _colors; // Array of ColorA
};

}	// End of namespace
