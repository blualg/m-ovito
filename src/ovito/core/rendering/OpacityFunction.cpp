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

#include <ovito/core/Core.h>
#include "OpacityFunction.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpacityFunction);
OVITO_CLASSINFO(OpacityFunction, "DisplayName", "Opacity function");
DEFINE_PROPERTY_FIELD(OpacityFunction, table);

/******************************************************************************
* Constructor.
******************************************************************************/
void OpacityFunction::initializeObject(ObjectInitializationFlags flags)
{
    DataObject::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Initialize the opacity function with a linear ramp.
        reset();
    }
}

/******************************************************************************
* Restores the default opacity function state.
******************************************************************************/
void OpacityFunction::reset()
{
    // Reset the opacity function to a linear ramp.
    std::vector<FloatType> defaultTable(DEFAULT_TABULATION_SIZE);
    for(size_t i = 0; i < DEFAULT_TABULATION_SIZE; ++i)
        defaultTable[i] = static_cast<FloatType>(i) / (DEFAULT_TABULATION_SIZE - 1);
    setTable(std::move(defaultTable));
}

/******************************************************************************
* Produces a tabulated representation of the opacity function.
******************************************************************************/
void OpacityFunction::tabulateOpacityValues(std::span<float> buffer) const
{
    if(buffer.size() != table().size()) {
        throw Exception(tr("Provided tabulation buffer size is not compatible with opacity function size."));
    }

    std::transform(table().begin(), table().end(), buffer.begin(),
        [](FloatType value) { return static_cast<float>(value); });
}

/******************************************************************************
* Implements a free-hand drawing operation on the opacity function.
******************************************************************************/
void OpacityFunction::freeDraw(std::span<const Point2> drawPath)
{
    // Make a modifiable copy of the current opacity function table.
    // The tabulated function is a vector of floats, which describe the
    // function values over the x-axis interval [0, 1].
    auto curve = table();
    if(curve.size() < 2)
        throw Exception(tr("Opacity function must have at least two points for drawing."));

    // Iterate over the line segments that make up the draw path.
    for(size_t i = 0; i < drawPath.size() - 1; i++) {
        const Point2& p0 = drawPath[i];
        const Point2& p1 = drawPath[i + 1];

        // Calculate the slope of the line segment.
        FloatType deltaX = p1.x() - p0.x();
        if(std::abs(deltaX) <= FLOATTYPE_EPSILON) continue; // Avoid division by zero
        FloatType slope = (p1.y() - p0.y()) / deltaX;

        // Iterate over the x-coordinates of the opacity function table
        // that fall within the x-range of the line segment.
        for(size_t j = 0; j < curve.size(); j++) {
            FloatType x = static_cast<FloatType>(j) / (curve.size() - 1);
            if(x >= std::min(p0.x(), p1.x()) && x <= std::max(p0.x(), p1.x())) {
                // Calculate the corresponding y-coordinate on the line segment.
                FloatType y = p0.y() + slope * (x - p0.x());
                // Update the opacity function table with the new value.
                curve[j] = std::clamp<FloatType>(y, 0, 1);
            }
        }
    }

    setTable(std::move(curve));
}

}   // End of namespace
