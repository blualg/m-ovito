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
#include "SyclNearestNeighborFinder.h"

namespace Ovito {

/******************************************************************************
* Initialization function.
******************************************************************************/
void SyclNearestNeighborFinder::prepare(const Property* positions, const SimulationCell* cell, const Property* selection)
{
    SyclNeighborFinderBase::prepare(positions, cell, selection);

    const AffineTransformation cellMatrix = simulationCell()->matrix();
    const AffineTransformation inverseCellMatrix = simulationCell()->reciprocalCellMatrix();
    const FloatType cellVectorLengthsSquared[3] = {
        cellMatrix.column(0).squaredLength(),
        cellMatrix.column(1).squaredLength(),
        cellMatrix.column(2).squaredLength()
    };
    const std::array<bool,3> pbcFlags = simulationCell()->pbcFlagsCorrected();

    // Compute normal vectors of simulation cell faces.
    const Vector3 planeNormals[3] = {
        simulationCell()->cellNormalVector(0),
        simulationCell()->cellNormalVector(1),
        simulationCell()->cellNormalVector(2)
    };
    OVITO_ASSERT(planeNormals[0] != Vector3::Zero());
    OVITO_ASSERT(planeNormals[1] != Vector3::Zero());
    OVITO_ASSERT(planeNormals[2] != Vector3::Zero());

    // Determine the effective number of particles used by the neighbor finder.
    size_t numParticlesLocal = selection ? selection->nonzeroCount() : positions->size();

    // Compute reverse mapping, from local indices to global indices.
    if(_packMapping) {
        BufferFactory<int64_t> unpackMapping(numParticlesLocal);
        size_t globalIndex = 0;
        for(auto localIndex : BufferReadAccess<int64_t>{_packMapping}) {
            if(localIndex != -1)
                unpackMapping[localIndex] = globalIndex;
            globalIndex++;
        }
        _unpackMapping = unpackMapping.take();
    }

    // For small simulation cells, it cannot hurt much to consider more periodic images than needed.
    // At the very least, consider one periodic image in each direction (when cell is orthogonal),
    // and two periodic images if cell is tilted.
    int nimages = 200 / std::clamp<size_t>(numParticlesLocal, 50, 200);
    if(nimages < 2 && !simulationCell()->isAxisAligned())
        nimages = 2;

    // Create list of periodic image shift vectors.
    std::vector<Vector3> pbcImages;
    int nx = simulationCell()->hasPbcCorrected(0) ? nimages : 0;
    int ny = simulationCell()->hasPbcCorrected(1) ? nimages : 0;
    int nz = simulationCell()->hasPbcCorrected(2) ? nimages : 0;
    for(int iz = -nz; iz <= nz; iz++) {
        for(int iy = -ny; iy <= ny; iy++) {
            for(int ix = -nx; ix <= nx; ix++) {
                pbcImages.push_back(cellMatrix * Vector3(ix,iy,iz));
            }
        }
    }
    // Sort PBC images by distance from the primary image.
    std::sort(pbcImages.begin(), pbcImages.end(), [](const Vector3& a, const Vector3& b) {
        return a.squaredLength() < b.squaredLength();
    });

    // Allocate data arrays.
    _positions = DataBufferPtr::create(DataBuffer::Uninitialized, localParticleCount(), DataBuffer::FloatDefault, 3);
    _reducedPositions = DataBufferPtr::create(DataBuffer::Uninitialized, localParticleCount(), DataBuffer::FloatDefault, 3);

    // Compute reduced particle coordinates and bounding box.
    Box3 boundingBox(Point3(0,0,0), Point3(1,1,1));
    if(localParticleCount() != 0) {
        sycl::buffer<FloatType> mincX{&boundingBox.minc.x(), 1};
        sycl::buffer<FloatType> mincY{&boundingBox.minc.y(), 1};
        sycl::buffer<FloatType> mincZ{&boundingBox.minc.z(), 1};
        sycl::buffer<FloatType> maxcX{&boundingBox.maxc.x(), 1};
        sycl::buffer<FloatType> maxcY{&boundingBox.maxc.y(), 1};
        sycl::buffer<FloatType> maxcZ{&boundingBox.maxc.z(), 1};

        this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
            SyclBufferAccess<int64_t, access_mode::read> mappingAcc{_packMapping, cgh};
            SyclBufferAccess<Point3, access_mode::read> positionInAcc{positions, cgh};
            SyclBufferAccess<Point3, access_mode::discard_write> positionOutAcc{_positions, cgh};
            SyclBufferAccess<Point3, access_mode::discard_write> reducedPositionOutAcc{_reducedPositions, cgh};
#ifdef OVITO_USE_SYCL_ACPP
            auto minrX = sycl::reduction(sycl::accessor{mincX, cgh, sycl::no_init}, FloatType(0), sycl::minimum<FloatType>());
            auto minrY = sycl::reduction(sycl::accessor{mincY, cgh, sycl::no_init}, FloatType(0), sycl::minimum<FloatType>());
            auto minrZ = sycl::reduction(sycl::accessor{mincZ, cgh, sycl::no_init}, FloatType(0), sycl::minimum<FloatType>());
            auto maxrX = sycl::reduction(sycl::accessor{maxcX, cgh, sycl::no_init}, FloatType(1), sycl::maximum<FloatType>());
            auto maxrY = sycl::reduction(sycl::accessor{maxcY, cgh, sycl::no_init}, FloatType(1), sycl::maximum<FloatType>());
            auto maxrZ = sycl::reduction(sycl::accessor{maxcZ, cgh, sycl::no_init}, FloatType(1), sycl::maximum<FloatType>());
#else
            auto minrX = sycl::reduction(mincX, cgh, FloatType(0), sycl::minimum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
            auto minrY = sycl::reduction(mincY, cgh, FloatType(0), sycl::minimum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
            auto minrZ = sycl::reduction(mincZ, cgh, FloatType(0), sycl::minimum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
            auto maxrX = sycl::reduction(maxcX, cgh, FloatType(1), sycl::maximum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
            auto maxrY = sycl::reduction(maxcY, cgh, FloatType(1), sycl::maximum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
            auto maxrZ = sycl::reduction(maxcZ, cgh, FloatType(1), sycl::maximum<FloatType>(), sycl::property::reduction::initialize_to_identity{});
#endif

            OVITO_SYCL_PARALLEL_FOR(cgh, SyclNearestNeighborFinder_prepare)(sycl::range(localParticleCount()), minrX, minrY, minrZ, maxrX, maxrY, maxrZ, [=](size_t i, auto& minrX, auto& minrY, auto& minrZ, auto& maxrX, auto& maxrY, auto& maxrZ) {
                // Determine mapping of global particle indices to local indices.
                // Completely skip non-selected particles if a selection was specified.
                size_t iout;
                if(mappingAcc) {
                    iout = mappingAcc[i];
                    if(iout == -1) return;
                }
                else iout = i;

                // Convert coordinates from absolute to reduced and wrap at periodic boundaries.
                const Point3 p = positionInAcc[i];
                Point3 rp = inverseCellMatrix * p;
                const Vector3 rv(
                    pbcFlags[0] * sycl::floor(rp.x()),
                    pbcFlags[1] * sycl::floor(rp.y()),
                    pbcFlags[2] * sycl::floor(rp.z())
                );
                positionOutAcc[iout] = p - cellMatrix * rv;
                rp -= rv;
                reducedPositionOutAcc[iout] = rp;

                minrX.combine(rp.x());
                minrY.combine(rp.y());
                minrZ.combine(rp.z());
                maxrX.combine(rp.x());
                maxrY.combine(rp.y());
                maxrZ.combine(rp.z());
            });
        });
    }

    this_task::throwIfCancelled();
}

}   // End of namespace
