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

#include <ovito/particles/Particles.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "CutoffNeighborFinder.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
CutoffNeighborFinder::CutoffNeighborFinder(FloatType cutoffRadius, BufferReadAccess<Point3> positions, const SimulationCellData& cellData, BufferReadAccess<SelectionIntType> selectionProperty) :
    _cutoffRadius(cutoffRadius),
    _cutoffRadiusSquared(cutoffRadius * cutoffRadius),
    _simCell(cellData)
{
    OVITO_ASSERT(positions);

    if(_cutoffRadius <= 0)
        throw Exception("Invalid parameter: Neighbor cutoff radius must be positive.");

    // Check input simulation cell.
    // If it is periodic, make sure it is not degenerate.
    // If it is non-periodic and degenerate, replace the box with a non-degenerate one.
    if(simCell().isDegenerate()) {
        if(simCell().hasPbc())
            throw Exception("Invalid input: Periodic simulation cell is degenerate.");
        // If needed, create an ad-hoc simulation cell that is non-periodic and non-degenerate.
        Box3 boundingBox;
        boundingBox.addPoints(positions);
        if(boundingBox.isEmpty()) boundingBox.addPoint(Point3::Origin());
        if(boundingBox.sizeX() <= FLOATTYPE_EPSILON) boundingBox.maxc.x() = boundingBox.minc.x() + cutoffRadius/2;
        if(boundingBox.sizeY() <= FLOATTYPE_EPSILON) boundingBox.maxc.y() = boundingBox.minc.y() + cutoffRadius/2;
        if(boundingBox.sizeZ() <= FLOATTYPE_EPSILON) boundingBox.maxc.z() = boundingBox.minc.z() + cutoffRadius/2;
        _simCell = SimulationCellData(boundingBox, simCell().is2D());
    }
    OVITO_ASSERT(!simCell().is2D() || !simCell().cellMatrix().column(2).isZero());

    const AffineTransformation cellMatrix = simCell().cellMatrix();
    AffineTransformation binCell;
    binCell.translation() = cellMatrix.translation();
    std::array<Vector3,3> planeNormals;

    // Determine the number of bins along each simulation cell vector.
    const double binCountLimit = 128*128*128;
    for(size_t i = 0; i < 3; i++) {
        planeNormals[i] = simCell().cellNormalVector(i);
        OVITO_ASSERT(planeNormals[i] != Vector3::Zero());
        FloatType x = std::abs(cellMatrix.column(i).dot(planeNormals[i]) / _cutoffRadius);
        _binDim[i] = std::max((int)floor(std::min(x, FloatType(binCountLimit))), 1);
    }
    if(simCell().is2D())
        _binDim[2] = 1;

    // Impose limit on the total number of bins.
    double estimatedBinCount = (double)_binDim[0] * (double)_binDim[1] * (double)_binDim[2];

    // Reduce bin count in each dimension by the same fraction to stay below total bin count limit.
    if(estimatedBinCount > binCountLimit) {
        if(!simCell().is2D()) {
            double factor = pow(binCountLimit / estimatedBinCount, 1.0/3.0);
            for(size_t i = 0; i < 3; i++)
                _binDim[i] = std::max((int)(_binDim[i] * factor), 1);
        }
        else {
            double factor = pow(binCountLimit / estimatedBinCount, 1.0/2.0);
            for(size_t i = 0; i < 2; i++)
                _binDim[i] = std::max((int)(_binDim[i] * factor), 1);
        }
    }

    qint64 binCount = (qint64)_binDim[0] * (qint64)_binDim[1] * (qint64)_binDim[2];
    OVITO_ASSERT(binCount > 0 && binCount < (qint64)0xFFFFFFFF);

    // Deliberately choose a smaller bin size if the cutoff radius is as large as the entire simulation cell.
    // This is to avoid having a single bin that contains all particles, which would lead to
    // performance issues when searching for neighbors.
    if(binCount == 1) {
        _binDim[0] = _binDim[1] = _binDim[2] = 4;
        if(simCell().is2D())
            _binDim[2] = 1;
        binCount = _binDim[0] * _binDim[1] * _binDim[2];
    }

    // Compute bin cell.
    for(size_t i = 0; i < 3; i++) {
        binCell.column(i) = cellMatrix.column(i) / _binDim[i];
    }
    if(!binCell.inverse(_reciprocalBinCell))
        throw Exception("Invalid input: Simulation cell is degenerate.");

    // Generate stencil.

    // This helper functions computes the shortest distance between a point and a bin cell located at the origin.
    auto shortestCellCellDistance = [binCell, planeNormals](const Vector3I& d) {
        Vector3 p = binCell * d.toDataType<FloatType>();
        // Compute distance from point to corner.
        FloatType distSq = p.squaredLength();
        for(size_t dim = 0; dim < 3; dim++) {
            // Compute shortest distance from point to edge.
            FloatType t = p.dot(binCell.column(dim)) / binCell.column(dim).squaredLength();
            if(t > 0.0 && t < 1.0)
                distSq = std::min(distSq, (p - t * binCell.column(dim)).squaredLength());
            // Compute shortest distance from point to cell face.
            const Vector3& n = planeNormals[dim];
            t = n.dot(p);
            if(t*t < distSq) {
                Vector3 p0 = p - t * n;
                const Vector3& u = binCell.column((dim+1)%3);
                const Vector3& v = binCell.column((dim+2)%3);
                FloatType a = u.dot(v)*p0.dot(v) - v.squaredLength()*p0.dot(u);
                FloatType b = u.dot(v)*p0.dot(u) - u.squaredLength()*p0.dot(v);
                FloatType denom = u.dot(v);
                denom *= denom;
                denom -= u.squaredLength()*v.squaredLength();
                a /= denom;
                b /= denom;
                if(a > 0 && b > 0 && a < 1 && b < 1)
                    distSq = t*t;
            }
        }
        return distSq;
    };

    // Compute the stencil of bin cells that need to be visited to find all neighbors of a particle located
    // in some central cell.
    for(int stencilRadius = 0; stencilRadius < 100; stencilRadius++) {
        size_t oldCount = _stencil.size();
        if(oldCount > 100*100)
            throw Exception("Neighbor cutoff radius is too large compared to the simulation cell size.");
        int stencilRadiusX = _simCell.hasPbc(0) ? stencilRadius : std::min(stencilRadius, _binDim[0] - 1);
        int stencilRadiusY = _simCell.hasPbc(1) ? stencilRadius : std::min(stencilRadius, _binDim[1] - 1);
        int stencilRadiusZ = _simCell.hasPbc(2) ? stencilRadius : std::min(stencilRadius, _binDim[2] - 1);
        for(int ix = -stencilRadiusX; ix <= stencilRadiusX; ix++) {
            for(int iy = -stencilRadiusY; iy <= stencilRadiusY; iy++) {
                for(int iz = -stencilRadiusZ; iz <= stencilRadiusZ; iz++) {
                    if(std::abs(ix) < stencilRadius && std::abs(iy) < stencilRadius && std::abs(iz) < stencilRadius)
                        continue;
                    this_task::throwIfCanceled();
                    FloatType shortestDistance = FLOATTYPE_MAX;
                    for(int dx = -1; dx <= 1; dx++) {
                        for(int dy = -1; dy <= 1; dy++) {
                            for(int dz = -1; dz <= 1; dz++) {
                                Vector3I d(dx + ix, dy + iy, dz + iz);
                                shortestDistance = std::min(shortestDistance, shortestCellCellDistance(d));
                            }
                        }
                    }
                    if(shortestDistance < cutoffRadiusSquared()) {
                        _stencil.push_back(Vector3I(ix,iy,iz));
                    }
                }
            }
        }
        if(_stencil.size() == oldCount)
            break;
    }

    // An 3d array of bins.
    // Each bin is a linked list of particles.
    _bins = std::make_unique<std::atomic<const NeighborListParticle*>[]>(binCount);
    _particleCount = positions.size();
    _particles = std::make_unique<NeighborListParticle[]>(_particleCount);

    // Sort particles into bins.
    parallelFor(_particleCount, 4096, TaskProgress::Ignore, [&](size_t pindex) {
        const Point3& p = positions[pindex];

        NeighborListParticle& a = _particles[pindex];
        a.pos = p;
        a.pbcShift.setZero();

        if(selectionProperty && !selectionProperty[pindex])
            return;

        // Determine the bin the particle is located in.
        Point3 rp = _reciprocalBinCell * p;

        Point3I binLocation;
        for(size_t k = 0; k < 3; k++) {
            binLocation[k] = (int)std::floor(rp[k]);
            if(_simCell.hasPbc(k)) {
                if(binLocation[k] < 0 || binLocation[k] >= _binDim[k]) {
                    int shift;
                    if(binLocation[k] < 0)
                        shift = -(binLocation[k]+1) / _binDim[k]+1;
                    else
                        shift = -binLocation[k] / _binDim[k];
                    a.pbcShift[k] = shift;
                    a.pos += (FloatType)shift * cellMatrix.column(k);
                    binLocation[k] = SimulationCell::modulo(binLocation[k], _binDim[k]);
                }
            }
            else if(binLocation[k] < 0) {
                binLocation[k] = 0;
            }
            else if(binLocation[k] >= _binDim[k]) {
                binLocation[k] = _binDim[k] - 1;
            }
            OVITO_ASSERT(binLocation[k] >= 0 && binLocation[k] < _binDim[k]);
        }

        // Put particle into its bin.
        size_t binIndex = binLocation[0] + binLocation[1]*_binDim[0] + binLocation[2]*_binDim[0]*_binDim[1];

        // Insert new particle into linked-list such that the list stays ordered. This is needed for a deterministic neighbor finder.
        // Note: The particles in each cell are sorted in descending order to be compatible with previous OVITO versions, which used a serial algorithm to build the cell lists.
        const NeighborListParticle* newParticle = &a;
        for(;;) {
            // Find linked-list position where to insert the new particle.
            const NeighborListParticle* insertBefore;
            const NeighborListParticle* insertAfter;
            auto entry = _bins[binIndex].load(std::memory_order_relaxed);
            if(!entry) {
                insertBefore = insertAfter = nullptr;
            }
            else if(newParticle > entry) {
                insertBefore = entry;
                insertAfter = nullptr;
            }
            else {
                for(;;) {
                    const NeighborListParticle* next = entry->nextInBin.load(std::memory_order_relaxed);
                    OVITO_ASSERT(next < entry || next == nullptr);
                    if(newParticle > next || next == nullptr) {
                        insertBefore = next;
                        insertAfter = entry;
                        break;
                    }
                    entry = next;
                }
            }
            a.nextInBin = insertBefore;
            OVITO_ASSERT(newParticle > insertBefore || insertBefore == nullptr);
            OVITO_ASSERT(newParticle < insertAfter || insertAfter == nullptr);

            // Try to insert the new particle into the list using a CAS operation.
            if(insertAfter == nullptr) {
                if(_bins[binIndex].compare_exchange_weak(insertBefore, newParticle, std::memory_order_release, std::memory_order_relaxed))
                    break;
            }
            else {
                if(const_cast<NeighborListParticle*>(insertAfter)->nextInBin.compare_exchange_weak(insertBefore, newParticle, std::memory_order_release, std::memory_order_relaxed))
                    break;
            }

            // If the CAS operation did not succeed, start over and determine the new insertion location again.
        }
    });
}

/******************************************************************************
* Iterator constructor
******************************************************************************/
CutoffNeighborFinder::Query::Query(const CutoffNeighborFinder& finder, size_t particleIndex)
    : _builder(finder), _centerIndex(particleIndex), _pbcFlags(finder._simCell.pbcFlags()), _cellMatrix(finder._simCell.cellMatrix())
{
    OVITO_ASSERT(particleIndex < _builder.particleCount());

    _stencilIter = _builder._stencil.begin();
    _center = _builder._particles[particleIndex].pos;

    // Determine the bin the central particle is located in.
    for(size_t k = 0; k < 3; k++) {
        _centerBin[k] = std::clamp((int)std::floor(_builder._reciprocalBinCell.prodrow(_center, k)), 0, _builder._binDim[k] - 1);
    }

    next();
}

/******************************************************************************
* Iterator constructor
******************************************************************************/
CutoffNeighborFinder::Query::Query(const CutoffNeighborFinder& finder, const Point3& location)
    : _builder(finder), _center(finder._simCell.wrapPoint(location)), _pbcFlags(finder._simCell.pbcFlags()), _cellMatrix(finder._simCell.cellMatrix())
{
    _stencilIter = _builder._stencil.begin();

    // Determine the bin the central particle is located in.
    for(size_t k = 0; k < 3; k++) {
        _centerBin[k] = std::clamp((int)std::floor(_builder._reciprocalBinCell.prodrow(_center, k)), 0, _builder._binDim[k] - 1);
    }

    next();
}

/******************************************************************************
* Iterator function.
******************************************************************************/
void CutoffNeighborFinder::Query::next()
{
    OVITO_ASSERT(!_atEnd);

    for(;;) {
        while(_neighbor) {
            _delta = _neighbor->pos - _shiftedCenter;
            _neighborIndex = _neighbor - _builder._particles.get();
            _neighbor = _neighbor->nextInBin.load(std::memory_order_relaxed);
            _distsq = _delta.squaredLength();
            if(_distsq <= _builder._cutoffRadiusSquared && (_neighborIndex != _centerIndex || _pbcShift != Vector3I::Zero()))
                return;
        };

        for(;;) {
            if(_stencilIter == _builder._stencil.end()) {
                _atEnd = true;
                _neighborIndex = std::numeric_limits<size_t>::max();
                return;
            }

            _shiftedCenter = _center;
            _pbcShift.setZero();
            bool skipBin = false;
            for(size_t k = 0; k < 3; k++) {
                _currentBin[k] = _centerBin[k] + (*_stencilIter)[k];
                if(!_pbcFlags[k]) {
                    if(_currentBin[k] < 0 || _currentBin[k] >= _builder._binDim[k]) {
                        skipBin = true;
                        break;
                    }
                }
                else {
                    if(_currentBin[k] >= _builder._binDim[k]) {
                        int s = _currentBin[k] / _builder._binDim[k];
                        _pbcShift[k] = s;
                        _currentBin[k] -= s * _builder._binDim[k];
                        _shiftedCenter -= _cellMatrix.column(k) * (FloatType)s;
                    }
                    else if(_currentBin[k] < 0) {
                        int s = (_currentBin[k] - _builder._binDim[k] + 1) / _builder._binDim[k];
                        _pbcShift[k] = s;
                        _currentBin[k] -= s * _builder._binDim[k];
                        _shiftedCenter -= _cellMatrix.column(k) * (FloatType)s;
                    }
                }
                OVITO_ASSERT(_currentBin[k] >= 0 && _currentBin[k] < _builder._binDim[k]);
            }
            ++_stencilIter;
            if(!skipBin) {
                _neighbor = _builder._bins[_currentBin[0] + _currentBin[1] * _builder._binDim[0] + _currentBin[2] * _builder._binDim[0] * _builder._binDim[1]].load(std::memory_order_relaxed);
                break;
            }
        }
    }
}

}   // End of namespace
