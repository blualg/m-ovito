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


#include <ovito/particles/Particles.h>
#include "SyclNeighborFinderBase.h"

namespace Ovito {

/**
 * \brief This utility class finds all neighbor particles within a cutoff radius of a central particle.
 */
class OVITO_PARTICLES_EXPORT SyclNearestNeighborFinder : public SyclNeighborFinderBase
{
public:

    class Accessor;

public:

    /// \brief Prepares the neighbor finder by sorting particles into a tree node structure.
    /// \param positions The data buffer containing the particle coordinates.
    /// \param simCell The input simulation cell geometry and boundary conditions.
    /// \param selection Determines which particles are included in the neighbor search (optional).
    /// \throw Exception on error.
    void prepare(const Property* positions, const SimulationCell* simCell, const Property* selection = nullptr);

private:

    /// The wrapped particle coordinates.
    DataBufferPtr _positions;

    /// The reduced particle coordinates.
    DataBufferPtr _reducedPositions;
};

}   // End of namespace
