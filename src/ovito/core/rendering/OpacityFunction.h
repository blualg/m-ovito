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
#include <ovito/core/dataset/data/DataObject.h>

namespace Ovito {

/**
 * \brief A transfer function for the opacity used in volume rendering.
 */
class OVITO_CORE_EXPORT OpacityFunction : public DataObject
{
    OVITO_CLASS(OpacityFunction)

public:

    static constexpr size_t DEFAULT_TABULATION_SIZE = 256;

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the optimal number of samples for tabulating the opacity function.
    size_t optimalTabulationSize() const {
        return table().size();
    }

    /// Produces a tabulated representation of the opacity function.
    void tabulateOpacityValues(std::span<float> buffer) const;

    /// Implements a free-hand drawing operation on the opacity function.
    void freeDraw(std::span<const Point2> drawPath);

private:

    /// The tabulated opacity function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(std::vector<FloatType>{}, table, setTable);
};

}   // End of namespace
