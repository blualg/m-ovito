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
#include "SyclNeighborFinderBase.h"

namespace Ovito {

/******************************************************************************
* Initialization function.
******************************************************************************/
void SyclNeighborFinderBase::prepare(const Property* positions, const SimulationCell* cell, const Property* selection)
{
    OVITO_ASSERT(positions);
    OVITO_ASSERT(positions->dataType() == DataBuffer::FloatDefault && positions->componentCount() == 3);
    OVITO_ASSERT(!selection || (selection->dataType() == DataBuffer::IntSelection && selection->componentCount() == 1));
    OVITO_ASSERT(!selection || selection->size() == positions->size());

    // Check input simulation cell.
    // If it is periodic, make sure it is not degenerate.
    // If it is non-periodic and degenerate, replace the box with a non-degenerate one.
    bool is2D = false;
    if(cell && cell->isDegenerate()) {
        is2D = cell->is2D();
        if(cell->hasPbcCorrected())
            throw Exception("Invalid input: Periodic simulation cell is degenerate.");
        else
            cell = nullptr;
    }
    _simCell = cell;

    // If no simulation cell was provided as input, create an ad-hoc cell that is non-periodic and non-degenerate.
    if(!_simCell) {
        // Compute bounding box of input particles (possible restricted to the subset of selected particles).
        Box3 boundingBox = SyclBufferAccess<Point3, access_mode::read>{positions}.boundingBox(selection);
        if(boundingBox.isEmpty()) boundingBox.addPoint(Point3::Origin());
        if(boundingBox.sizeX() <= FLOATTYPE_EPSILON) boundingBox.maxc.x() = boundingBox.minc.x() + FLOATTYPE_EPSILON;
        if(boundingBox.sizeY() <= FLOATTYPE_EPSILON) boundingBox.maxc.y() = boundingBox.minc.y() + FLOATTYPE_EPSILON;
        if(boundingBox.sizeZ() <= FLOATTYPE_EPSILON) boundingBox.maxc.z() = boundingBox.minc.z() + FLOATTYPE_EPSILON;
        _simCell = DataOORef<SimulationCell>::create(
                ObjectInitializationFlag::DontCreateVisElement, AffineTransformation(
                    Vector3(boundingBox.sizeX(), 0, 0),
                    Vector3(0, boundingBox.sizeY(), 0),
                    Vector3(0, 0, boundingBox.sizeZ()),
                    boundingBox.minc - Point3::Origin()), false, false, false, is2D);
    }
    OVITO_ASSERT(!_simCell->is2D() || !_simCell->matrix().column(2).isZero());

    const AffineTransformation cellMatrix = simulationCell()->matrix();
    const std::array<bool,3> pbcFlags = simulationCell()->pbcFlagsCorrected();

    // If using only a subset of the input particles, compute mapping of global particle indices to local indices.
    if(selection)
        _packMapping = selection->computePackedMapping();

    // Determine the effective number of particles used by the neighbor finder.
    _localParticleCount = selection ? selection->nonzeroCount() : positions->size();

    // Compute reverse mapping, from local indices to global indices.
    if(_packMapping) {
        BufferFactory<int64_t> unpackMapping(_localParticleCount);
        size_t globalIndex = 0;
        for(auto localIndex : BufferReadAccess<int64_t>{_packMapping}) {
            if(localIndex != -1)
                unpackMapping[localIndex] = globalIndex;
            globalIndex++;
        }
        _unpackMapping = unpackMapping.take();
    }

    this_task::throwIfCanceled();
}

}   // End of namespace
