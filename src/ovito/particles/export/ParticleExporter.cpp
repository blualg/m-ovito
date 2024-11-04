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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include "ParticleExporter.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ParticleExporter);

/******************************************************************************
* Evaluates the pipeline whose data is to be exported.
******************************************************************************/
Future<PipelineFlowState> ParticleExporter::getPipelineDataToBeExported(int frameNumber) const
{
    const PipelineFlowState state = co_await FileExporter::getPipelineDataToBeExported(frameNumber);

    const Particles* particles = state.getObject<Particles>();
    if(!particles)
        throw Exception(tr("The selected data collection does not contain any particles that can be exported."));
    if(!particles->getProperty(Particles::PositionProperty))
        throw Exception(tr("The particles to be exported do not have any coordinates ('Position' property is missing)."));

    // Verify per-particle data, make sure array lengths are consistent.
    particles->verifyIntegrity();

    // Verify topological data, make sure array lengths are consistent.
    if(particles->bonds())
        particles->bonds()->verifyIntegrity();
    if(particles->angles())
        particles->angles()->verifyIntegrity();
    if(particles->dihedrals())
        particles->dihedrals()->verifyIntegrity();
    if(particles->impropers())
        particles->impropers()->verifyIntegrity();

    co_return state;
}

}   // End of namespace
