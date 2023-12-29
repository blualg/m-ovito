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


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/core/rendering/PseudoColorMapping.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {

/**
 * \brief A transfer function that maps property values to display colors.
 */
class OVITO_STDOBJ_EXPORT PropertyColorMapping : public RefTarget
{
    OVITO_CLASS(PropertyColorMapping)
    OVITO_CLASSINFO("DisplayName", "Color mapping");

public:

    /// Constructor.
    explicit PropertyColorMapping(ObjectInitializationFlags flags);

    /// Creates a PseudoColorMapping that can be used for rendering of graphics primitives.
    PseudoColorMapping pseudoColorMapping() const;

    /// Determines the min/max range of values stored in the given property array.
    std::optional<std::pair<FloatType, FloatType>> determineValueRange(const Property* pseudoColorProperty, int pseudoColorPropertyComponent) const;

public Q_SLOTS:

    /// Swaps the minimum and maximum values to reverse the color scale.
    void reverseRange();

private:

    /// This object converts a scalar values to an RGB color.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<ColorCodingGradient>, colorGradient, setColorGradient);

    /// This lower bound of the input value internal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, startValue, setStartValue);

    /// This upper bound of the input value internal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType, endValue, setEndValue);

    /// The input property that is used as data source for the coloring.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference, sourceProperty, setSourceProperty);
};

}   // End of namespace
