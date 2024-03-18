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
#include "SyclNeighborFinderBase.h"

namespace Ovito {

/**
 * \brief This utility class finds all neighbor particles within a cutoff radius of a central particle.
 */
class OVITO_PARTICLES_EXPORT SyclCutoffNeighborFinder : public SyclNeighborFinderBase
{
public:

    class Accessor;

    /// \brief Represents a single neighbor particle found by the neighbor finder.
    struct Neighbor
    {
        Vector3 delta;
        FloatType distanceSquared;
        size_t localNeighborIndex;
        Vector3I pbcShift;
        const Accessor* _accessor;

        FloatType distance() const { return std::sqrt(distanceSquared); }
        size_t neighborIndex() const { return _accessor->mapToGlobalParticleIndex(localNeighborIndex); }
    };

public:

    /// \brief Prepares the neighbor finder by sorting particles into a grid of bin cells.
    /// \param cutoffRadius The cutoff radius for neighbor lists.
    /// \param positions The data buffer containing the particle coordinates.
    /// \param simCell The input simulation cell geometry and boundary conditions.
    /// \param selection Determines which particles are included in the neighbor search (optional).
    /// \throw Exception on error.
    void prepare(FloatType cutoffRadius, const Property* positions, const SimulationCell* simCell, const Property* selection = nullptr);

    /// Returns the cutoff radius set via prepare().
    FloatType cutoffRadius() const { return _cutoffRadius; }

    /// Returns the square of the cutoff radius set via prepare().
    FloatType cutoffRadiusSquared() const { return _cutoffRadiusSquared; }

public:

    /// The accessor is needed to access the SyclCutoffNeighborFinder's precomputed data in SYCL kernels.
    class Accessor
    {
    public:

        /// Constructor.
        Accessor(SyclCutoffNeighborFinder& finder, sycl::handler& cgh) :
            _packMapping(finder._packMapping, cgh),
            _unpackMapping(finder._unpackMapping, cgh),
            _positions(finder._positions, cgh),
            _stencil(finder._stencil, cgh),
            _nextCellParticle(finder._nextCellParticle, cgh),
            _firstCellParticle(finder._firstCellParticle, cgh),
            _binDim(finder._binDim),
            _cutoffRadiusSquared(finder._cutoffRadiusSquared),
            _reciprocalBinCell(finder._reciprocalBinCell),
            _reciprocalCellMatrix(finder.simulationCell()->reciprocalCellMatrix()),
            _pbcFlags(finder.simulationCell()->pbcFlagsCorrected()),
            _pbcCellMatrix(
                _pbcFlags[0] * finder.simulationCell()->cellMatrix().column(0),
                _pbcFlags[1] * finder.simulationCell()->cellMatrix().column(1),
                _pbcFlags[2] * finder.simulationCell()->cellMatrix().column(2)) {}

        /// Visits all neighbors of the given particle.
        template<typename Visitor>
        void visitNeighbors(size_t globalParticleIndex, Visitor&& visitor) const {
            // Map global particle index to local index and obtain position of central particle.
            visitNeighborsLocal(mapToLocalParticleIndex(globalParticleIndex), std::forward<Visitor>(visitor));
        }

        /// Visits all neighbors of the given particle.
        template<typename Visitor>
        void visitNeighborsLocal(size_t localParticleIndex, Visitor&& visitor) const {
            const Point3& centerPosition = _positions[localParticleIndex];
            // Delegate to implementation method.
            visitNeighborsImpl(localParticleIndex, centerPosition, std::forward<Visitor>(visitor));
        }

        /// Visits all neighbors of the given spatial location.
        template<typename Visitor>
        void visitNeighbors(Point3 location, Visitor&& visitor) const {
            // Wrap position at periodic cell boundaries.
            const Point3 rp = _reciprocalCellMatrix * location;
            for(size_t dim = 0; dim < 3; dim++)
                location -= std::floor(rp[dim]) * _pbcCellMatrix.column(dim);

            // Delegate to implementation method.
            visitNeighborsImpl(std::numeric_limits<size_t>::max(), location, std::forward<Visitor>(visitor));
        }

        /// Maps an index from the global particles list to the local list used by the neighbor finder.
        size_t mapToLocalParticleIndex(size_t globalParticleIndex) const {
            OVITO_ASSERT(!_packMapping || (globalParticleIndex < _packMapping.size() && _packMapping[globalParticleIndex] >= 0));
            return _packMapping ? static_cast<size_t>(_packMapping[globalParticleIndex]) : globalParticleIndex;
        }

        /// Maps an index from the local particles list to the global list.
        size_t mapToGlobalParticleIndex(size_t localParticleIndex) const {
            OVITO_ASSERT(!_unpackMapping || localParticleIndex < _unpackMapping.size());
            return _unpackMapping ? static_cast<size_t>(_unpackMapping[localParticleIndex]) : localParticleIndex;
        }

        /// Returns the square of the cutoff radius set via prepare().
        FloatType cutoffRadiusSquared() const { return _cutoffRadiusSquared; }

    private:

        /// Visits all neighbors of the given particle.
        template<typename Visitor>
        void visitNeighborsImpl(size_t localCenterParticleIndex, const Point3& center, Visitor&& visitor) const {

            // Determine which bin cell the center is located in.
            Point3I centerBin;
            for(size_t k = 0; k < 3; k++) {
                FloatType rc = _reciprocalBinCell.prodrow(center, k);
                OVITO_ASSERT(!_pbcFlags[k] || (rc >= -FLOATTYPE_EPSILON && rc <= _binDim[k]+FLOATTYPE_EPSILON));
                centerBin[k] = std::clamp((int)std::floor(rc), 0, _binDim[k] - 1);
            }

            // Visit all adjacent cell as given by the precomputed stencil.
            for(const Vector3I& stencilCell : _stencil) {

                Neighbor neighbor;
                neighbor._accessor = this;
                Vector3I& pbcShift = neighbor.pbcShift = Vector3I::Zero();
                Point3 shiftedCenter = center;
                Point3I currentBin = centerBin + stencilCell;
                bool skipBin = false;
                for(size_t k = 0; k < 3; k++) {
                    int& s = pbcShift[k];
                    if(currentBin[k] >= _binDim[k]) {
                        skipBin |= !_pbcFlags[k];
                        s = currentBin[k] / _binDim[k];
                    }
                    else if(currentBin[k] < 0) {
                        skipBin |= !_pbcFlags[k];
                        s = (currentBin[k] - _binDim[k] + 1) / _binDim[k];
                    }
                    currentBin[k] -= s * _binDim[k];
                    shiftedCenter -= _pbcCellMatrix.column(k) * s;
                }
                if(skipBin)
                    continue;
                size_t adjacentCellIndex = currentBin[0] + currentBin[1] * _binDim[0] + currentBin[2] * _binDim[0] * _binDim[1];
                OVITO_ASSERT(adjacentCellIndex < _firstCellParticle.size());
                int64_t localNeighborIndex = _firstCellParticle[adjacentCellIndex];
                while(localNeighborIndex != -1) {
                    OVITO_ASSERT(localNeighborIndex >= 0 && localNeighborIndex < _positions.size());
                    neighbor.localNeighborIndex = static_cast<size_t>(localNeighborIndex);
                    neighbor.delta = _positions[localNeighborIndex] - shiftedCenter;
                    neighbor.distanceSquared = neighbor.delta.squaredLength();
                    if(neighbor.distanceSquared <= _cutoffRadiusSquared && (neighbor.localNeighborIndex != localCenterParticleIndex || pbcShift != Vector3I::Zero())) {
                        visitor(std::as_const(neighbor));
                    }
                    OVITO_ASSERT(_nextCellParticle[localNeighborIndex] == -1 || _nextCellParticle[localNeighborIndex] < localNeighborIndex);
                    localNeighborIndex = _nextCellParticle[localNeighborIndex];
                }
            }
        }

    private:

        const SyclBufferAccess<int64_t, access_mode::read> _packMapping;
        const SyclBufferAccess<int64_t, access_mode::read> _unpackMapping;
        const SyclBufferAccess<Point3, access_mode::read> _positions;
        const SyclBufferAccess<Vector3I, access_mode::read> _stencil;
        const SyclBufferAccess<int64_t, access_mode::read> _nextCellParticle;
        const SyclBufferAccess<int64_t, access_mode::read> _firstCellParticle;
        const std::array<int,3> _binDim;
        const AffineTransformation _reciprocalBinCell;
        const AffineTransformation _reciprocalCellMatrix;
        const std::array<bool, 3> _pbcFlags;
        const Matrix3 _pbcCellMatrix;
        const FloatType _cutoffRadiusSquared;
    };

private:

    /// The neighbor distance criterion.
    FloatType _cutoffRadius = 0;

    /// The squared cutoff distance.
    FloatType _cutoffRadiusSquared = 0;

    /// Number of bins in each cell direction.
    std::array<int,3> _binDim;

    /// Used to determine the bin from a particle position.
    AffineTransformation _reciprocalBinCell;

    /// The list of adjacent cells to visit while finding the neighbors of a particle located in a central cell.
    ConstDataBufferPtr _stencil;

    /// The wrapped particle coordinates.
    DataBufferPtr _positions;

    /// The original PBC image flags of the input particles (before wrapping them at periodic boundaries).
    DataBufferPtr _pbcImageFlags;

    /// Pointers to next items of per-cell linked-lists.
    DataBufferPtr _nextCellParticle;

    /// Pointers to first items of per-cell linked-lists.
    DataBufferPtr _firstCellParticle;
};

}   // End of namespace
