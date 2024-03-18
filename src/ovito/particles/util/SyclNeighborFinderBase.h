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


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief This utility class finds all neighbor particles within a cutoff radius of a central particle.
 */
class OVITO_PARTICLES_EXPORT SyclNeighborFinderBase
{
public:

    /// Default constructor.
    SyclNeighborFinderBase() = default;

    /// Copying is not supported by this class.
    SyclNeighborFinderBase(const SyclNeighborFinderBase&) = delete;

    /// Copying is not supported by this class.
    SyclNeighborFinderBase& operator=(const SyclNeighborFinderBase&) = delete;

    /// Returns the simulation cell used by the neighbor finder. This may be an ad-hoc cell constructed from the bounding box of particle coordinates.
    const SimulationCell* simulationCell() const { return _simCell; }

    /// Returns the number of particles stored by the neighbor finder.
    size_t localParticleCount() const { return _localParticleCount; }

protected:

    /// \brief Prepares the neighbor finder by sorting particles into a grid of bin cells.
    /// \param positions The data buffer containing the particle coordinates.
    /// \param simCell The input simulation cell geometry and boundary conditions.
    /// \param selection Determines which particles are included in the neighbor search (optional).
    /// \throw Exception on error.
    void prepare(const Property* positions, const SimulationCell* simCell, const Property* selection);

protected:

    /// The number of particles stored by the neighbor finder.
    size_t _localParticleCount;

    /// The simulation cell and boundary conditions.
    DataOORef<const SimulationCell> _simCell;

    /// Mapping of global particle indices to local indices.
    ConstDataBufferPtr _packMapping;

    /// Mapping of local particle indices to global indices.
    ConstDataBufferPtr _unpackMapping;
};

}   // End of namespace
