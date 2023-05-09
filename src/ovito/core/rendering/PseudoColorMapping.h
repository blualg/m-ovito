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
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {

/**
 * \brief Transfer function that defines the mapping from scalar pseudo-color values to RGB values.
 */
class OVITO_CORE_EXPORT PseudoColorMapping
{
public:

    /// Default constructor.
    PseudoColorMapping() = default;

    /// Constructor.
    PseudoColorMapping(FloatType minValue, FloatType maxValue, OORef<ColorCodingGradient> gradient) : _minValue(minValue), _maxValue(maxValue), _gradient(std::move(gradient)) {
        OVITO_ASSERT(_gradient);
    }

    /// Returns true if this is not the null mapping.
    bool isValid() const { return (bool)_gradient && std::isfinite(_minValue) && std::isfinite(_maxValue); }

    /// Returns the lower bound of the mapping interval.
    FloatType minValue() const { return _minValue; }

    /// Returns the upper bound of the mapping interval.
    FloatType maxValue() const { return _maxValue; }

    /// Returns the color gradient.
    const OORef<ColorCodingGradient>& gradient() const { return _gradient; }

    /// Converts a scalar value to an RGB color value.
    template<typename T>
    ColorT<T> valueToColor(T v) const {
        OVITO_ASSERT(isValid());
        OVITO_ASSERT(std::isfinite(v));
        // Handle a degenerate mapping interval.
        if(_maxValue == _minValue) {
            if(v == static_cast<T>(_maxValue)) return gradient()->valueToColor(T(0.5));
            else if(v < static_cast<T>(_maxValue)) return gradient()->valueToColor(T(0));
            else return gradient()->valueToColor(T(1));
        }
        // Compute linear interpolation.
        T t = (v - static_cast<T>(_minValue)) / static_cast<T>(_maxValue - _minValue);
        // Clamp values.
        if(std::isnan(t)) t = T(0);
        else if(t ==  std::numeric_limits<T>::infinity()) t = T(1);
        else if(t == -std::numeric_limits<T>::infinity()) t = T(0);
        else if(t < T(0)) t = T(0);
        else if(t > T(1)) t = T(1);
        return gradient()->valueToColor(t);
    }

    /// Comparison operator. Is required so PseudoColorMapping can be used as key value in the vis cache.
    bool operator==(const PseudoColorMapping& other) const {
        return _minValue == other._minValue && _maxValue == other._maxValue && _gradient == other._gradient;
    }

private:

    /// The lower bound of the mapping interval.
    FloatType _minValue = 0;

    /// The upper bound of the mapping interval.
    FloatType _maxValue = 0;

    /// The color gradient.
    OORef<ColorCodingGradient> _gradient;
};

}   // End of namespace
