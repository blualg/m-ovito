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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "WrapPeriodicImagesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WrapPeriodicImagesModifier);
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "DisplayName", "Wrap at periodic boundaries");
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "Description", "Fold particle coordinates back into the periodic simulation box.");
OVITO_CLASSINFO(WrapPeriodicImagesModifier, "ModifierCategory", "Modification");

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool WrapPeriodicImagesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>() && input.containsObject<SimulationCell>();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> WrapPeriodicImagesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Get the periodic simulation cell.
    const SimulationCell* simCell = state.expectObject<SimulationCell>();
    if(!simCell->hasPbcCorrected()) {
        state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("No periodic boundary conditions are enabled for the simulation cell.")));
        return std::move(state);
    }

    // The actual work can be performed in a separate thread.
    return asyncLaunch([state = std::move(state), simCell]() mutable
    {
        // Make a modifiable copy of the particles object.
        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->verifyIntegrity();

        // Perform the actual coordinate wrapping.
        outputParticles->wrapCoordinates(*simCell);

        // Return the modified data.
        return std::move(state);
    });
}

}   // End of namespace
