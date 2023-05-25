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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/BondsObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "ParticlesDeleteSelectedModifierDelegate.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(ParticlesDeleteSelectedModifierDelegate);
IMPLEMENT_OVITO_CLASS(BondsDeleteSelectedModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<ParticlesObject>())
        return { DataObjectReference(&ParticlesObject::OOClass()) };
    return {};
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus ParticlesDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    size_t numParticles = 0;
    size_t numSelected = 0;
    size_t numDeletedBonds = 0;
    size_t numDeletedAngles = 0;
    size_t numDeletedDihedrals = 0;
    size_t numDeletedImpropers = 0;

    // Get the particle selection.
    if(const ParticlesObject* inputParticles = state.getObject<ParticlesObject>()) {
        inputParticles->verifyIntegrity();
        numParticles = inputParticles->elementCount();
        if(const PropertyObject* selProperty = inputParticles->getProperty(ParticlesObject::SelectionProperty)) {

            // Generate filter mask.
            boost::dynamic_bitset<> mask(selProperty->size());
            boost::dynamic_bitset<>::size_type i = 0;
            for(auto s : BufferAccess<const SelectionIntType>(selProperty)) {
                if(s != 0) {
                    mask.set(i++);
                    numSelected++;
                }
                else {
                    mask.reset(i++);
                }
            }

            if(numSelected) {
                // Make sure we can safely modify the particles object.
                ParticlesObject* outputParticles = state.makeMutable(inputParticles);

                // Keep track of how many bonds, angles, etc there are.
                size_t oldBondCount = outputParticles->bonds() ? outputParticles->bonds()->elementCount() : 0;
                size_t oldAngleCount = outputParticles->angles() ? outputParticles->angles()->elementCount() : 0;
                size_t oldDihedralCount = outputParticles->dihedrals() ? outputParticles->dihedrals()->elementCount() : 0;
                size_t oldImproperCount = outputParticles->impropers() ? outputParticles->impropers()->elementCount() : 0;

                // Remove selection property.
                outputParticles->removeProperty(selProperty);

                // Delete the particles.
                outputParticles->deleteElements(mask);

                // Detect if dangling bonds have been deleted as part of the particle deletion.
                numDeletedBonds = oldBondCount - (outputParticles->bonds() ? outputParticles->bonds()->elementCount() : 0);
                numDeletedAngles = oldAngleCount - (outputParticles->angles() ? outputParticles->angles()->elementCount() : 0);
                numDeletedDihedrals = oldDihedralCount - (outputParticles->dihedrals() ? outputParticles->dihedrals()->elementCount() : 0);
                numDeletedImpropers = oldImproperCount - (outputParticles->impropers() ? outputParticles->impropers()->elementCount() : 0);
            }
        }
    }

    // Report some statistics:
    QString statusMessage = tr("%n of %1 particles deleted (%2%)", 0, numSelected)
        .arg(numParticles)
        .arg((FloatType)numSelected * 100 / std::max(numParticles, (size_t)1), 0, 'f', 1);
    if(numDeletedBonds)
        statusMessage += tr("\n%n dangling bonds deleted", 0, numDeletedBonds);
    if(numDeletedAngles)
        statusMessage += tr("\n%n dangling angles deleted", 0, numDeletedAngles);
    if(numDeletedDihedrals)
        statusMessage += tr("\n%n dangling dihedrals deleted", 0, numDeletedDihedrals);
    if(numDeletedImpropers)
        statusMessage += tr("\n%n dangling impropers deleted", 0, numDeletedImpropers);

    return PipelineStatus(PipelineStatus::Success, std::move(statusMessage));
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> BondsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(const ParticlesObject* particles = input.getObject<ParticlesObject>()) {
        if(particles->bonds() && particles->bonds()->getProperty(BondsObject::SelectionProperty))
            return { DataObjectReference(&ParticlesObject::OOClass()) };
    }
    return {};
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus BondsDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    size_t numBonds = 0;
    size_t numSelected = 0;

    // Get the bond selection.
    if(const ParticlesObject* inputParticles = state.getObject<ParticlesObject>()) {
        if(const BondsObject* inputBonds = inputParticles->bonds()) {
            inputBonds->verifyIntegrity();
            numBonds = inputBonds->elementCount();
            if(const PropertyObject* selProperty = inputBonds->getProperty(BondsObject::SelectionProperty)) {
                // Generate filter mask.
                boost::dynamic_bitset<> mask(selProperty->size());
                boost::dynamic_bitset<>::size_type i = 0;
                for(auto s : BufferAccess<const SelectionIntType>(selProperty)) {
                    if(s != 0) {
                        mask.set(i++);
                        numSelected++;
                    }
                    else {
                        mask.reset(i++);
                    }
                }

                if(numSelected) {
                    // Make sure we can safely modify the particles and the bonds object it contains.
                    ParticlesObject* outputParticles = state.makeMutable(inputParticles);
                    BondsObject* outputBonds = outputParticles->makeBondsMutable();

                    // Remove selection property.
                    outputBonds->removeProperty(selProperty);

                    // Delete the bonds.
                    outputBonds->deleteElements(mask);
                }
            }
        }
    }

    // Report some statistics:
    QString statusMessage = tr("%n of %1 bonds deleted (%2%)", 0, numSelected)
        .arg(numBonds)
        .arg((FloatType)numSelected * 100 / std::max(numBonds, (size_t)1), 0, 'f', 1);

    return PipelineStatus(PipelineStatus::Success, std::move(statusMessage));
}

}   // End of namespace
