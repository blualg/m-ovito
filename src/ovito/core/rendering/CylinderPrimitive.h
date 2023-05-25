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
#include "PseudoColorMapping.h"

namespace Ovito {

/**
 * \brief A set of cylinders or arrow glyphs to be rendered by a SceneRenderer implementation.
 */
class OVITO_CORE_EXPORT CylinderPrimitive
{
    Q_GADGET

public:

    enum ShadingMode {
        NormalShading,
        FlatShading,
    };
    Q_ENUM(ShadingMode);

    enum Shape {
        CylinderShape,
        ArrowShape,
    };
    Q_ENUM(Shape);

    /// \brief Returns the shading mode for elements.
    ShadingMode shadingMode() const { return _shadingMode; }

    /// \brief Changes the shading mode for elements.
    void setShadingMode(ShadingMode mode) { _shadingMode = mode; }

    /// \brief Returns the selected element shape.
    Shape shape() const { return _shape; }

    /// \brief Changes the element shape.
    void setShape(Shape shape) { _shape = shape; }

    /// Returns the cylinder diameter assigned to all primtives.
    FloatType uniformWidth() const { return _uniformWidth; }

    /// Sets the cylinder diameter of all primitives to the given value.
    void setUniformWidth(FloatType width) {
        _uniformWidth = width;
    }

    /// Returns the color assigned to all primitives.
    const Color& uniformColor() const { return _uniformColor; }

    /// Sets the color of all primitives to the given value.
    void setUniformColor(const Color& color) {
        _uniformColor = color;
    }

    /// Returns whether only one of the two cylinder caps is rendered.
    bool renderSingleCylinderCap() const { return _renderSingleCylinderCap; }

    /// Controls whether only one of the two cylinder caps is rendered.
    void setRenderSingleCylinderCap(bool singleCap) { _renderSingleCylinderCap = singleCap; }

    /// Returns the buffer storing the base positions.
    const ConstDataBufferPtr& basePositions() const { return _basePositions; }

    /// Returns the buffer storing the head positions.
    const ConstDataBufferPtr& headPositions() const { return _headPositions; }

    /// Sets the coordinates of the base and the head points.
    void setPositions(ConstDataBufferPtr baseCoordinates, ConstDataBufferPtr headCoordinates) {
        OVITO_ASSERT((baseCoordinates != nullptr) == (headCoordinates != nullptr));
        OVITO_ASSERT(!baseCoordinates || baseCoordinates->componentCount() == 3);
        OVITO_ASSERT(!headCoordinates || headCoordinates->componentCount() == 3);
        OVITO_ASSERT(!baseCoordinates || baseCoordinates->size() == headCoordinates->size());
        _basePositions = std::move(baseCoordinates);
        _headPositions = std::move(headCoordinates);
    }

    /// Returns the buffer storing the colors.
    const ConstDataBufferPtr& colors() const { return _colors; }

    /// Sets the per-primitive (or per-vertex) colors.
    void setColors(ConstDataBufferPtr colors) {
        OVITO_ASSERT(!colors || (colors->componentCount() == 3 || colors->componentCount() == 1));
        _colors = std::move(colors);
    }

    /// Sets the transparency values of the primitives.
    void setTransparencies(ConstDataBufferPtr transparencies) {
        OVITO_ASSERT(!transparencies || transparencies->componentCount() == 1);
        _transparencies = std::move(transparencies);
    }

    /// Returns the buffer storing the transparancy values.
    const ConstDataBufferPtr& transparencies() const { return _transparencies; }

    /// Sets the diameters of the primitives.
    void setWidths(ConstDataBufferPtr widths) {
        OVITO_ASSERT(!widths || widths->componentCount() == 1);
        _widths = std::move(widths);
    }

    /// Returns the buffer storing the per-primitive diameter values.
    const ConstDataBufferPtr& widths() const { return _widths; }

    /// Returns the mapping from pseudo-color values to RGB colors.
    const PseudoColorMapping& pseudoColorMapping() const { return _pseudoColorMapping; }

    /// Sets the mapping from pseudo-color values to RGB colors.
    void setPseudoColorMapping(const PseudoColorMapping& mapping) {
        _pseudoColorMapping = mapping;
    }

private:

    /// Controls the shading.
    ShadingMode _shadingMode = NormalShading;

    /// The shape of the elements.
    Shape _shape = CylinderShape;

    /// The mapping from pseudo-color values to RGB colors.
    PseudoColorMapping _pseudoColorMapping;

    /// Indicates that only one of the two cylinder caps should be rendered.
    bool _renderSingleCylinderCap = false;

    /// The color to be used if no per-primitive colors have been specified.
    Color _uniformColor{1,1,1};

    /// The diameter of the cylinders.
    FloatType _uniformWidth{2.0};

    /// Buffer storing the coordinates of the arrow/cylinder base points.
    ConstDataBufferPtr _basePositions; // Array of Point3

    /// Buffer storing the coordinates of the arrow/cylinder head points.
    ConstDataBufferPtr _headPositions; // Array of Point3

    /// Buffer storing the colors of the arrows/cylinders.
    ConstDataBufferPtr _colors; // Array of Color (RGB) or Float (pseudocolor values)

    /// Buffer storing the semi-transparency values.
    ConstDataBufferPtr _transparencies; // Array of FloatType

    /// Buffer storing the per-primitive width values.
    ConstDataBufferPtr _widths; // Array of FloatType
};

}   // End of namespace
