////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include "SelectOverlappingParticlesModifier.h"
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SelectOverlappingParticlesModifier);
OVITO_CLASSINFO(SelectOverlappingParticlesModifier, "DisplayName", "Select overlapping atoms");
OVITO_CLASSINFO(SelectOverlappingParticlesModifier, "Description", "Deletes atoms that are closer than a specified distance.");
OVITO_CLASSINFO(SelectOverlappingParticlesModifier, "ModifierCategory", "Selection");

DEFINE_PROPERTY_FIELD(SelectOverlappingParticlesModifier, overlapDistance);
DEFINE_PROPERTY_FIELD(SelectOverlappingParticlesModifier, applyToSelection);
DEFINE_PROPERTY_FIELD(SelectOverlappingParticlesModifier, keepOne);

SET_PROPERTY_FIELD_LABEL(SelectOverlappingParticlesModifier, overlapDistance, "Overlap distance");
SET_PROPERTY_FIELD_LABEL(SelectOverlappingParticlesModifier, applyToSelection, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(SelectOverlappingParticlesModifier, keepOne, "On Overlap: Keep one");

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool SelectOverlappingParticlesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> SelectOverlappingParticlesModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                               PipelineFlowState&& state)
{
    // Create local copies of co-routine input objects
    const ModifierEvaluationRequest request_l = request;
    PipelineFlowState state_l = std::move(state);

    const FloatType overlapDistance_l = overlapDistance();
    const bool applyToSelection_l = applyToSelection();
    const bool keepOne_l = keepOne();

    // Get the input particles.
    const Particles* particles = state_l.expectObject<Particles>();
    particles->verifyIntegrity();

    // Get the required input properties.
    ConstPropertyPtr selProperty = applyToSelection_l ? particles->expectProperty(Particles::SelectionProperty) : nullptr;

    // Perform the following in a worker thread.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Deleting overlapping atoms"));

    // Output mask / selection property
    Particles* mutableParticles = state_l.expectMutableObject<Particles>();
    Property* mutableSelectionProperty = mutableParticles->createProperty(Particles::SelectionProperty);
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> mutableSelectionPropertyAcc(mutableSelectionProperty);
    std::ranges::fill(mutableSelectionPropertyAcc, 0);

    BufferReadAccess<SelectionIntType> selectionAcc(selProperty);

    CutoffNeighborFinder neighborFinder(
        overlapDistance_l, particles->expectProperty(Particles::PositionProperty), state_l.getObject<SimulationCell>(), selProperty);

    std::vector<size_t> neighs(5);

    std::optional<std::minstd_rand> rng;
    if(keepOne_l) {
        rng.emplace(1323);
    }

    for(size_t particleIndex = 0; particleIndex < particles->elementCount(); particleIndex++) {
        this_task::throwIfCanceled();

        // Skip particles that are not included in the analysis.
        if(selectionAcc && selectionAcc[particleIndex] == 0) continue;
        // Skip particles that have already been marked for deletion.
        if(mutableSelectionPropertyAcc[particleIndex] != 0) continue;

        neighs.clear();
        neighs.emplace_back(particleIndex);
        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, particleIndex); !neighQuery.atEnd(); neighQuery.next()) {
            const size_t neighIndex = neighQuery.current();
            if(mutableSelectionPropertyAcc[neighIndex] == 0) {
                neighs.emplace_back(neighIndex);
            }
        }
        if(keepOne_l) {
            OVITO_ASSERT(rng.has_value());
            std::ranges::shuffle(neighs, *rng);
            neighs.pop_back();
            for(size_t neigh : neighs) {
                mutableSelectionPropertyAcc[neigh] = 1;
            }
        }
        else {
            if(neighs.size() > 1) {
                for(size_t neigh : neighs) {
                    mutableSelectionPropertyAcc[neigh] = 1;
                }
            }
        }
    }
    mutableSelectionPropertyAcc.reset();
    const size_t selectedCount = mutableSelectionProperty->nonzeroCount();
    state_l.addAttribute(
        QStringLiteral("SelectOverlappingParticles.count"), QVariant::fromValue(selectedCount), request_l.modificationNode());
    state_l.setStatus(PipelineStatus(tr("%1 out of %2 elements selected (%3%)")
                                         .arg(selectedCount)
                                         .arg(particles->elementCount())
                                         .arg(particles->elementCount() > 0 ? selectedCount * 100 / particles->elementCount() : 0)));

    co_return std::move(state_l);
}

}  // namespace Ovito