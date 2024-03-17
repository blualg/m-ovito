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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ParticlesDeleteSelectedModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesDeleteSelectedModifierDelegate);
IMPLEMENT_CREATABLE_OVITO_CLASS(BondsDeleteSelectedModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([state = std::move(state)]() mutable {

        size_t numParticles = 0;
        size_t numSelected = 0;
        size_t numDeletedBonds = 0;
        size_t numDeletedAngles = 0;
        size_t numDeletedDihedrals = 0;
        size_t numDeletedImpropers = 0;

        // Get the particle selection.
        if(const Particles* inputParticles = state.getObject<Particles>()) {
            inputParticles->verifyIntegrity();
            numParticles += inputParticles->elementCount();
            if(ConstPropertyPtr selProperty = inputParticles->getProperty(Particles::SelectionProperty)) {
                // Make sure we can safely modify the particles object.
                Particles* outputParticles = state.makeMutable(inputParticles);

                // Keep track of how many bonds, angles, etc there are.
                size_t oldBondCount = outputParticles->bonds() ? outputParticles->bonds()->elementCount() : 0;
                size_t oldAngleCount = outputParticles->angles() ? outputParticles->angles()->elementCount() : 0;
                size_t oldDihedralCount = outputParticles->dihedrals() ? outputParticles->dihedrals()->elementCount() : 0;
                size_t oldImproperCount = outputParticles->impropers() ? outputParticles->impropers()->elementCount() : 0;

                // Remove selection property.
                outputParticles->removeProperty(selProperty);

                // Delete the selected particles.
                numSelected += outputParticles->deleteElements(std::move(selProperty));

                // Detect if dangling bonds/angles/dihedrals/impropers have been deleted due to the particle removal.
                numDeletedBonds += oldBondCount - (outputParticles->bonds() ? outputParticles->bonds()->elementCount() : 0);
                numDeletedAngles += oldAngleCount - (outputParticles->angles() ? outputParticles->angles()->elementCount() : 0);
                numDeletedDihedrals += oldDihedralCount - (outputParticles->dihedrals() ? outputParticles->dihedrals()->elementCount() : 0);
                numDeletedImpropers += oldImproperCount - (outputParticles->impropers() ? outputParticles->impropers()->elementCount() : 0);
            }
        }

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 particles deleted (%3%)")
            .arg(numSelected)
            .arg(numParticles)
            .arg((FloatType)numSelected * 100 / std::max(numParticles, (size_t)1), 0, 'f', 1);
        if(numDeletedBonds)
            statusMessage += tr("\n%1 dangling bonds deleted").arg(numDeletedBonds);
        if(numDeletedAngles)
            statusMessage += tr("\n%1 dangling angles deleted").arg(numDeletedAngles);
        if(numDeletedDihedrals)
            statusMessage += tr("\n%1 dangling dihedrals deleted").arg(numDeletedDihedrals);
        if(numDeletedImpropers)
            statusMessage += tr("\n%1 dangling impropers deleted").arg(numDeletedImpropers);

        state.combineStatus(std::move(statusMessage));

        return std::move(state);
    });
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const Particles* particles = input.getObject<Particles>()) {
        if(particles->bonds() && particles->bonds()->getProperty(Bonds::SelectionProperty))
            return { DataObjectReference(&Particles::OOClass()) };
    }
    return {};
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> BondsDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    // The actual computation can be performed in a separate worker thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([state = std::move(state)]() mutable {

        size_t numBonds = 0;
        size_t numSelected = 0;

        // Get the bond selection.
        if(const Particles* inputParticles = state.getObject<Particles>()) {
            if(const Bonds* inputBonds = inputParticles->bonds()) {
                inputBonds->verifyIntegrity();
                numBonds += inputBonds->elementCount();
                if(ConstPropertyPtr selProperty = inputBonds->getProperty(Bonds::SelectionProperty)) {
                    // Make sure we can safely modify the particles and the bonds object it contains.
                    Particles* outputParticles = state.makeMutable(inputParticles);
                    Bonds* outputBonds = outputParticles->makeBondsMutable();

                    // Remove selection property.
                    outputBonds->removeProperty(selProperty);

                    // Delete the selected bonds.
                    numSelected += outputBonds->deleteElements(std::move(selProperty));
                }
            }
        }

        // Report some statistics:
        QString statusMessage = tr("%1 of %2 bonds deleted (%3%)")
            .arg(numSelected)
            .arg(numBonds)
            .arg((FloatType)numSelected * 100 / std::max(numBonds, (size_t)1), 0, 'f', 1);

        state.combineStatus(std::move(statusMessage));

        return std::move(state);
    });
}

}   // End of namespace
