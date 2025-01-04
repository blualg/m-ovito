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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ParticlesSliceModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesSliceModifierDelegate);
OVITO_CLASSINFO(ParticlesSliceModifierDelegate, "DisplayName", "Particles");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesSliceModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());

    const Particles* inputParticles = state.expectObject<Particles>();
    inputParticles->verifyIntegrity();

    // Get the required input properties.
    const Property* posProperty = inputParticles->expectProperty(Particles::PositionProperty);
    const Property* selProperty = modifier->applyToSelection() ? inputParticles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);
    sliceWidth /= 2;

    // The actual work can be performed in a separate thread.
    return asyncLaunch([state = std::move(state), plane, sliceWidth, invert = modifier->inverse(),
                        createSelection = modifier->createSelection(), posProperty, selProperty, inputParticles]() mutable {
        // Create mask array to be computed.
        PropertyPtr mask = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, inputParticles->elementCount(), Particles::SelectionProperty);
        OVITO_ASSERT(posProperty->size() == mask->size());
        OVITO_ASSERT(!selProperty || selProperty->size() == mask->size());

        // Number of marked/selected particles.
        size_t numMarked = SliceModifier::sliceCoordinatesToMask(plane, sliceWidth, invert, posProperty, mask, selProperty);

        QString statusMessage = tr("%1 input particles").arg(inputParticles->elementCount());

        // Make sure we can safely modify the particles object.
        Particles* outputParticles = state.makeMutable(inputParticles);
        if(createSelection == false) {
            // Delete the marked particles.
            outputParticles->deleteElements(std::move(mask), numMarked);
            statusMessage += tr("\n%1 particles deleted").arg(numMarked);
            statusMessage += tr("\n%1 particles remaining").arg(outputParticles->elementCount());
        }
        else {
            // Create or replace the selection particle property.
            outputParticles->createProperty(std::move(mask));
            statusMessage += tr("\n%1 particles selected").arg(numMarked);
            statusMessage += tr("\n%1 particles unselected").arg(outputParticles->elementCount() - numMarked);
        }
        outputParticles->verifyIntegrity();

        state.setStatus(std::move(statusMessage));

        return std::move(state);
    });
}

}   // End of namespace
