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

#include <ovito/particles/Particles.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "SyclCutoffNeighborFinder.h"

namespace Ovito {

/******************************************************************************
* Initialization function.
******************************************************************************/
void SyclCutoffNeighborFinder::prepare(FloatType cutoffRadius, const Property* positions, const SimulationCell* cell, const Property* selection)
{
    SyclNeighborFinderBase::prepare(positions, cell, selection);

    _cutoffRadius = cutoffRadius;
    _cutoffRadiusSquared = cutoffRadius * cutoffRadius;
    if(_cutoffRadius <= 0)
        throw Exception("Invalid parameter: Neighbor cutoff radius must be positive.");

    const AffineTransformation cellMatrix = simulationCell()->matrix();
    const std::array<bool,3> pbcFlags = simulationCell()->pbcFlagsCorrected();

    AffineTransformation binCell;
    binCell.translation() = cellMatrix.translation();
    std::array<Vector3,3> planeNormals;
    std::array<int,3> binDim;

    // Determine the number of bins along each simulation cell vector.
    constexpr double binCountLimit = 128*128*128;
    for(size_t i = 0; i < 3; i++) {
        planeNormals[i] = simulationCell()->cellNormalVector(i);
        OVITO_ASSERT(planeNormals[i] != Vector3::Zero());
        FloatType x = std::abs(cellMatrix.column(i).dot(planeNormals[i]) / _cutoffRadius);
        binDim[i] = std::max((int)floor(std::min(x, FloatType(binCountLimit))), 1);
    }
    if(simulationCell()->is2D())
        binDim[2] = 1;

    // Impose limit on the total number of bins.
    double estimatedBinCount = (double)binDim[0] * (double)binDim[1] * (double)binDim[2];

    // Reduce bin count in each dimension by the same fraction to stay below total bin count limit.
    if(estimatedBinCount > binCountLimit) {
        if(!simulationCell()->is2D()) {
            double factor = std::pow(binCountLimit / estimatedBinCount, 1.0/3.0);
            for(size_t i = 0; i < 3; i++)
                binDim[i] = std::max((int)(binDim[i] * factor), 1);
        }
        else {
            double factor = std::pow(binCountLimit / estimatedBinCount, 1.0/2.0);
            for(size_t i = 0; i < 2; i++)
                binDim[i] = std::max((int)(binDim[i] * factor), 1);
        }
    }

    size_t binCount = (size_t)binDim[0] * (size_t)binDim[1] * (size_t)binDim[2];
    OVITO_ASSERT(binCount > 0 && binCount < (size_t)0xFFFFFFFF);

    // Compute bin cell.
    for(size_t i = 0; i < 3; i++) {
        binCell.column(i) = cellMatrix.column(i) / binDim[i];
    }

    // Used to determine the bin from a particle position.
    AffineTransformation reciprocalBinCell;
    if(!binCell.inverse(reciprocalBinCell))
        throw Exception("Invalid input: Simulation cell is degenerate.");

    // Compute the stencil of adjacent bin cells that need to be visited to find all neighbors of a particle located
    // in some central cell.
    std::vector<Vector3I> stencil;

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
                denom -= u.squaredLength() * v.squaredLength();
                a /= denom;
                b /= denom;
                if(a > 0 && b > 0 && a < 1 && b < 1)
                    distSq = t*t;
            }
        }
        return distSq;
    };

    // Generate actual stencil.
    for(int stencilRadius = 0; stencilRadius < 100; stencilRadius++) {
        size_t oldCount = stencil.size();
        if(oldCount > 100*100)
            throw Exception("Neighbor cutoff radius is too large compared to the simulation cell size.");
        int stencilRadiusX = pbcFlags[0] ? stencilRadius : std::min(stencilRadius, binDim[0] - 1);
        int stencilRadiusY = pbcFlags[1] ? stencilRadius : std::min(stencilRadius, binDim[1] - 1);
        int stencilRadiusZ = pbcFlags[2] ? stencilRadius : std::min(stencilRadius, binDim[2] - 1);
        for(int ix = -stencilRadiusX; ix <= stencilRadiusX; ix++) {
            for(int iy = -stencilRadiusY; iy <= stencilRadiusY; iy++) {
                for(int iz = -stencilRadiusZ; iz <= stencilRadiusZ; iz++) {
                    if(std::abs(ix) < stencilRadius && std::abs(iy) < stencilRadius && std::abs(iz) < stencilRadius)
                        continue;
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
                        stencil.emplace_back(ix,iy,iz);
                    }
                }
            }
        }
        if(stencil.size() == oldCount)
            break;
    }

    // Copy final stencil list into SYCL buffer.
    BufferFactory<Vector3I> stencilAcc(stencil.size());
    boost::copy(stencil, stencilAcc.begin());
    _stencil = stencilAcc.take();

    // Allocate data arrays.
    _positions = DataBufferPtr::create(DataBuffer::Uninitialized, localParticleCount(), DataBuffer::FloatDefault, 3);
    _pbcImageFlags = DataBufferPtr::create(DataBuffer::Uninitialized, localParticleCount(), DataBuffer::Int32, 3);
    _nextCellParticle = DataBufferPtr::create(DataBuffer::Uninitialized, localParticleCount(), DataBuffer::Int64);
    _firstCellParticle = DataBufferPtr::create(DataBuffer::Uninitialized, binCount, DataBuffer::Int64);
    _firstCellParticle->fill<int64_t>(-1);

    // Sort input particles into bins.
    if(localParticleCount() != 0) {
        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {

            SyclBufferAccess<int64_t, access_mode::read> mappingAcc{_packMapping, cgh};
            SyclBufferAccess<Point3, access_mode::read> positionInAcc{positions, cgh};
            SyclBufferAccess<Point3, access_mode::discard_write> positionOutAcc{_positions, cgh};
            SyclBufferAccess<Vector3I, access_mode::discard_write> pbcImageFlagsAcc{_pbcImageFlags, cgh};
            SyclBufferAccess<int64_t, access_mode::discard_write> nextCellParticleAcc{_nextCellParticle, cgh};
            SyclBufferAccess<int64_t, access_mode::read_write> firstCellParticleAcc{_firstCellParticle, cgh};

            OVITO_SYCL_PARALLEL_FOR(cgh, SyclCutoffNeighborFinder_prepare)(sycl::range(localParticleCount()), [=](size_t i) {
                // Determine mapping of global particle indices to local indices.
                // Completely skip non-selected particles if a selection was specified.
                size_t iout;
                if(mappingAcc) {
                    iout = mappingAcc[i];
                    if(iout == -1) return;
                }
                else iout = i;

                // Determine the bin the particle is located in.
                Point3I binLocation;
                Point3 wp = positionInAcc[i];
                Point3 rp = reciprocalBinCell * wp;
                Vector3I pbcShift = Vector3I::Zero();
                for(size_t k = 0; k < 3; k++) {
                    binLocation[k] = (int)sycl::floor(rp[k]);
                    if(pbcFlags[k]) {
                        if(binLocation[k] < 0 || binLocation[k] >= binDim[k]) {
                            int shift;
                            if(binLocation[k] < 0)
                                shift = -(binLocation[k]+1) / binDim[k] + 1;
                            else
                                shift = -binLocation[k] / binDim[k];
                            pbcShift[k] = shift;
                            wp += cellMatrix.column(k) * shift;
                            binLocation[k] += shift * binDim[k];
                        }
                    }
                    else if(binLocation[k] < 0) {
                        binLocation[k] = 0;
                    }
                    else if(binLocation[k] >= binDim[k]) {
                        binLocation[k] = binDim[k] - 1;
                    }
                }
                pbcImageFlagsAcc[iout] = pbcShift;
                positionOutAcc[iout] = wp;

                // Insert the particle into the linked-list of its bin.
                size_t binIndex = binLocation[0] + binLocation[1]*binDim[0] + binLocation[2]*binDim[0]*binDim[1];
                int64_t newParticle = (int64_t)iout;
#if 0
                // Note: Insertion at the beginning of the linked-list leads to non-deterministic behavior of the neighbor finder.
                nextCellParticleAcc[iout] = sycl::atomic_ref<int64_t, sycl::memory_order_relaxed, sycl::memory_scope::device>(firstCellParticleAcc[binIndex]).exchange(newParticle);
#else
                // Insert new particle into linked-list such that we get an ordered list. This is needed for a deterministic behavior of the neighbor finder.
                // Note: The particles in each cell are sorted in descending order to be compatible with previous OVITO versions, which used a serial algorithm to build the cell lists.
                for(;;) {
                    // Find linked-list position where to insert the new particle.
                    int64_t insertBefore, insertAfter;
                    int64_t entry = sycl::atomic_ref<int64_t, sycl::memory_order_relaxed, sycl::memory_scope::device>(firstCellParticleAcc[binIndex]).load();
                    if(entry == -1) {
                        insertBefore = insertAfter = -1;
                    }
                    else if(newParticle > entry) {
                        insertBefore = entry;
                        insertAfter = -1;
                    }
                    else {
                        for(;;) {
                            int64_t next = sycl::atomic_ref<int64_t, sycl::memory_order_relaxed, sycl::memory_scope::device>(nextCellParticleAcc[entry]).load();
                            OVITO_ASSERT(next < entry || next == -1);
                            if(newParticle > next || next == -1) {
                                insertBefore = next;
                                insertAfter = entry;
                                break;
                            }
                            entry = next;
                        }
                    }
                    nextCellParticleAcc[iout] = insertBefore;
                    OVITO_ASSERT(newParticle > insertBefore || insertBefore == -1);
                    OVITO_ASSERT(newParticle < insertAfter || insertAfter == -1);

                    // Try to insert the new particle into the list using a CAS operation.
                    if(insertAfter == -1) {
                        if(sycl::atomic_ref<int64_t, sycl::memory_order_relaxed, sycl::memory_scope::device>(firstCellParticleAcc[binIndex]).compare_exchange_weak(
                                insertBefore, newParticle, sycl::memory_order_release, sycl::memory_order_relaxed))
                            break;
                    }
                    else {
                        if(sycl::atomic_ref<int64_t, sycl::memory_order_relaxed, sycl::memory_scope::device>(nextCellParticleAcc[insertAfter]).compare_exchange_weak(
                                insertBefore, newParticle, sycl::memory_order_release, sycl::memory_order_relaxed))
                            break;
                    }

                    // If the CAS operation did not succeed, start over and determine the new insertion location again.
                }
#endif
            });
        });
    }

    // Store information for later use.
    _binDim = binDim;
    _reciprocalBinCell = reciprocalBinCell;

    this_task::throwIfCanceled();
}

}   // End of namespace
