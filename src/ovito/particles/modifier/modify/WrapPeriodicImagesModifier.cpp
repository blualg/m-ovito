////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "WrapPeriodicImagesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WrapPeriodicImagesModifier);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool WrapPeriodicImagesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void WrapPeriodicImagesModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    const SimulationCell* simCellObj = state.expectObject<SimulationCell>();
    std::array<bool, 3> pbc = simCellObj->pbcFlagsCorrected();
    if(!pbc[0] && !pbc[1] && !pbc[2]) {
        state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("No periodic boundary conditions are enabled for the simulation cell.")));
        return;
    }

    const AffineTransformation& simCell = simCellObj->cellMatrix();
    if((simCellObj->is2D() ? simCellObj->volume2D() : simCellObj->volume3D()) < FLOATTYPE_EPSILON)
         throw Exception(tr("The simulation cell is degenerate."));
    AffineTransformation inverseSimCell = simCellObj->reciprocalCellMatrix();

    // Make a modifiable copy of the particles object.
    Particles* outputParticles = state.expectMutableObject<Particles>();
    outputParticles->verifyIntegrity();

    // Make a modifiable copy of the particle position property.
    BufferWriteAccess<Point3, access_mode::read_write> posProperty = outputParticles->expectMutableProperty(Particles::PositionProperty);

    // Wrap bonds by adjusting their PBC shift vectors.
    if(outputParticles->bonds()) {
        if(BufferReadAccess<ParticleIndexPair> topologyProperty = outputParticles->bonds()->getProperty(Bonds::TopologyProperty)) {
            BufferWriteAccess<Vector3I, access_mode::read_write> periodicImageProperty = outputParticles->makeBondsMutable()->createProperty(DataBuffer::Initialized, Bonds::PeriodicImageProperty);
            for(size_t bondIndex = 0; bondIndex < topologyProperty.size(); bondIndex++) {
                size_t particleIndex1 = topologyProperty[bondIndex][0];
                size_t particleIndex2 = topologyProperty[bondIndex][1];
                if(particleIndex1 >= posProperty.size() || particleIndex2 >= posProperty.size())
                    continue;
                const Point3& p1 = posProperty[particleIndex1];
                const Point3& p2 = posProperty[particleIndex2];
                for(size_t dim = 0; dim < 3; dim++) {
                    if(pbc[dim]) {
                        periodicImageProperty[bondIndex][dim] +=
                              (int)std::floor(inverseSimCell.prodrow(p2, dim))
                            - (int)std::floor(inverseSimCell.prodrow(p1, dim));
                    }
                }
            }
        }
    }

    // Wrap particles coordinates.
    for(size_t dim = 0; dim < 3; dim++) {
        if(pbc[dim]) {
            for(Point3& p : posProperty) {
                if(FloatType n = std::floor(inverseSimCell.prodrow(p, dim)))
                    p -= simCell.column(dim) * n;
            }
        }
    }
}

}   // End of namespace
