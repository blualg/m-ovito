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
#include <ovito/core/rendering/ColorCodingGradient.h>

namespace Ovito {
namespace DiscreteColorMap {
/// Returns the bin count in a discrete colormap.
template<typename T>
inline int binCount(T startValue, T endValue)
{
    // Protect against overflow
    return (int)std::min(std::round(std::abs(endValue - startValue)), (T)255) + 1;
}

/// Maps the color value t [0,1] to its discrete value based on the discrete colormap bin count.
template<typename T, typename V>
inline T mapValue(T t, V binCount)
{
    static_assert(std::is_floating_point_v<T>, "T must be a floating point type.");
    static_assert(std::is_integral_v<V>, "V must be an integral type.");

    if(binCount <= 1) {
        return (T)0.5;
    }
    if(t >= (T)1) {
        return (T)1;
    }
    if(t <= 0) {
        return (T)0;
    }
    const T binSize = (T)1 / (T)(binCount - 1);
    const T binIndex = std::trunc(t * (T)binCount);

    return binIndex * binSize;
}

}  // namespace DiscreteColorMap

namespace ColorMap {

/// Generates a color gradient image for the given color map.
/// binCount <= 0 indicates that the color gradient is not a discrete colormap.
template<int legendHeight>
QImage generateImage(const ColorCodingGradient* gradient, int binCount = -1)
{
    QImage image{1, legendHeight, QImage::Format_RGB32};
    for(int y = 0; y < legendHeight; y++) {
        FloatType t = (FloatType)y / (legendHeight - 1);
        // binCount <= 0 indicates that the color gradient is not a discrete colormap.
        t = (binCount <= 0) ? t : DiscreteColorMap::mapValue(t, binCount);
        const Color color = gradient->valueToColor(1.0 - t);
        image.setPixel(0, y, QColor(color).rgb());
    }
    return image;
}

}  // namespace ColorMap

}  // namespace Ovito