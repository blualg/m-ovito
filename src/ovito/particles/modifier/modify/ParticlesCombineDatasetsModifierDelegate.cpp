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
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ParticlesCombineDatasetsModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesCombineDatasetsModifierDelegate);
OVITO_CLASSINFO(ParticlesCombineDatasetsModifierDelegate, "DisplayName", "Particles");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesCombineDatasetsModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Always return a non-empty vector, because this delegate wants to be called always.
    return { DataObjectReference(&DataCollection::OOClass()) };
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ParticlesCombineDatasetsModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    // Get the secondary dataset.
    if(additionalInputs.empty())
        return std::move(state);
    const PipelineFlowState& secondaryState = additionalInputs.front();

    // Get the secondary dataset.
    const Particles* secondaryParticles = secondaryState.getObject<Particles>();
    if(!secondaryParticles)
        return std::move(state);

    // Get the positions from the primary dataset.
    const Particles* primaryParticles = state.getObject<Particles>();
    // If primary dataset does not contain particles yet, simply copy the particles from the secondary dataset over to the first.
    if(!primaryParticles) {
        state.addObject(secondaryParticles);
        return std::move(state);
    }
    Particles* particles = state.makeMutable(primaryParticles);

    size_t primaryParticleCount = particles->elementCount();
    size_t secondaryParticleCount = secondaryParticles->elementCount();
    size_t totalParticleCount = primaryParticleCount + secondaryParticleCount;

    CloneHelper cloneHelper;

    // Extend all property arrays of primary dataset and copy data from secondary set if it contains a matching property.
    if(secondaryParticleCount != 0) {
        particles->setElementCount(totalParticleCount);
        for(Property* prop : particles->makePropertiesMutable()) {
            OVITO_ASSERT(prop->size() == totalParticleCount);

            // Find corresponding property in second dataset.
            const Property* secondProp = secondaryParticles->getPropertyLike(prop);
            if(secondProp && secondProp->size() == secondaryParticleCount) {
                prop->copyRangeFrom(*secondProp, 0, primaryParticleCount, secondaryParticleCount);
            }
            else if(prop->isStandardProperty()) {
                ConstDataObjectPath containerPath = { secondaryParticles };
                PropertyPtr temporaryProp = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, secondaryParticles->elementCount(), prop->typeId(), containerPath);
                prop->copyRangeFrom(*temporaryProp, 0, primaryParticleCount, secondaryParticleCount);
            }

            // Combine particle types lists.
            mergeElementTypes(prop, secondProp, cloneHelper);

            // Assign unique particle and molecule IDs.
            if(prop->typeId() == Particles::IdentifierProperty && primaryParticleCount != 0) {
                // First, compute maximum range of existing particle IDs.
                // Then, generate unique consecutive IDs for the new particles (offset by 1).
#ifdef OVITO_USE_SYCL
                sycl::buffer maxId = SyclBufferAccess<IdentifierIntType, access_mode::read>(prop, 0, primaryParticleCount).max();
                SyclBufferAccess<IdentifierIntType, access_mode::discard_write>(prop, primaryParticleCount, prop->size() - primaryParticleCount).iota(maxId, 1);
#else
                BufferWriteAccess<IdentifierIntType, access_mode::read_write> identifiers(prop);
                auto maxId = *std::max_element(identifiers.cbegin(), identifiers.cbegin() + primaryParticleCount);
                std::iota(identifiers.begin() + primaryParticleCount, identifiers.end(), maxId + 1);
#endif
            }
            else if(prop->typeId() == Particles::MoleculeProperty && primaryParticleCount != 0) {
                // First, compute maximum range of existing molecule IDs in first dataset.
                // Then, shift molecule IDs of second dataset to avoid collisions.
#ifdef OVITO_USE_SYCL
                sycl::buffer maxId = SyclBufferAccess<IdentifierIntType, access_mode::read>(prop, 0, primaryParticleCount).max();
                SyclBufferAccess<IdentifierIntType, access_mode::read_write>(prop, primaryParticleCount, prop->size() - primaryParticleCount).add(maxId);
#else
                BufferWriteAccess<IdentifierIntType, access_mode::read_write> identifiers(prop);
                auto maxId = *std::max_element(identifiers.cbegin(), identifiers.cbegin() + primaryParticleCount);
                for(auto* mol_id = identifiers.begin() + primaryParticleCount; mol_id != identifiers.end(); ++mol_id)
                    *mol_id += maxId;
#endif
            }
        }
    }

    // Copy particle properties from second dataset which do not exist in the primary dataset yet.
    for(const Property* prop : secondaryParticles->properties()) {
        if(prop->size() != secondaryParticleCount) continue;

        // Check if the property already exists in the output container.
        if(prop->isStandardProperty()) {
            if(particles->getProperty(prop->typeId()))
                continue;
        }
        else {
            if(particles->getProperty(prop->name()))
                continue;
        }

        // Copy the property into the output container.
        if(primaryParticleCount == 0) {
            particles->addProperty(prop);
        }
        else {
            // Shift values of second dataset to the end of the buffer and initialize values of first dataset to zero:
            PropertyPtr clonedProperty = prop->cloneWithoutData(totalParticleCount);
            clonedProperty->fillZero();
            clonedProperty->copyRangeFrom(*prop, 0, primaryParticleCount, secondaryParticleCount);
            particles->addProperty(std::move(clonedProperty));
        }
    }

    // Helper function that merges two sets of either bonds/angles/dihredrals/impropers.
    auto mergeTopologyLists = [&](const PropertyContainer* primaryElements, const PropertyContainer* secondaryElements, int topologyPropertyId) {

        size_t primaryElementCount = primaryElements->elementCount();
        size_t secondaryElementCount = secondaryElements ? secondaryElements->elementCount() : 0;
        size_t totalElementCount = primaryElementCount + secondaryElementCount;

        // Extend all property arrays of primary dataset and copy data from secondary set if it contains a matching property.
        if(secondaryElementCount != 0) {
            PropertyContainer* primaryMutableElements = particles->makeMutable(primaryElements);
            primaryElements = primaryMutableElements;
            primaryMutableElements->makePropertiesMutable();
            primaryMutableElements->setElementCount(totalElementCount);
            for(Property* prop : primaryMutableElements->makePropertiesMutable()) {
                OVITO_ASSERT(prop->size() == totalElementCount);

                // Find corresponding property in second dataset.
                const Property* secondProp = secondaryElements->getPropertyLike(prop);
                if(secondProp && secondProp->size() == secondaryElementCount) {
                    OVITO_ASSERT(prop->stride() == secondProp->stride());
                    prop->copyRangeFrom(*secondProp, 0, primaryElementCount, secondaryElementCount);
                }
                else if(prop->isStandardProperty()) {
                    ConstDataObjectPath containerPath = { secondaryParticles, secondaryElements };
                    PropertyPtr temporaryProp = secondaryElements->getOOMetaClass().createStandardProperty(DataBuffer::Initialized, secondaryElementCount, prop->typeId(), containerPath);
                    prop->copyRangeFrom(*temporaryProp, 0, primaryElementCount, secondaryElementCount);
                }

                // Combine type lists.
                mergeElementTypes(prop, secondProp, cloneHelper);
            }
        }

        // Copy properties from second dataset which do not exist in the primary dataset yet.
        if(secondaryElements) {
            PropertyContainer* primaryMutableElements = particles->makeMutable(primaryElements);
            for(const Property* prop : secondaryElements->properties()) {
                if(prop->size() != secondaryElementCount) continue;

                // Check if the property already exists in the output.
                if(prop->isStandardProperty()) {
                    if(primaryMutableElements->getProperty(prop->typeId()))
                        continue;
                }
                else {
                    if(primaryMutableElements->getProperty(prop->name()))
                        continue;
                }

                if(primaryElementCount == 0) {
                    primaryMutableElements->addProperty(prop);
                }
                else {
                    // Shift values of second dataset to the end of the buffer and initialize values of first dataset to zero:
                    PropertyPtr clonedProperty = prop->cloneWithoutData(totalElementCount);
                    clonedProperty->fillZero();
                    clonedProperty->copyRangeFrom(*prop, 0, primaryElementCount, secondaryElementCount);
                    primaryMutableElements->addProperty(std::move(clonedProperty));
                }
            }

            // Shift particle indices stored in the topology array of the second container.
            const Property* topologyProperty = primaryMutableElements->getProperty(topologyPropertyId);
            if(topologyProperty && primaryParticleCount != 0) {
                Property* mutableTopologyProperty = primaryMutableElements->makeMutable(topologyProperty);
#ifdef OVITO_USE_SYCL
                SyclBufferAccess<int64_t*, access_mode::read_write>(mutableTopologyProperty, primaryElementCount, secondaryElementCount).add(primaryParticleCount);
#else
                BufferWriteAccess<int64_t*, access_mode::read_write> accessor = mutableTopologyProperty;
                for(auto idx = accessor.begin() + (primaryElementCount * accessor.componentCount()); idx != accessor.end(); ++idx)
                    *idx += primaryParticleCount;
#endif
            }
        }
    };

    // Merge bonds.
    const Bonds* primaryBonds = particles->bonds();
    const Bonds* secondaryBonds = secondaryParticles->bonds();
    if(primaryBonds || secondaryBonds) {
        // Create the primary bonds object if it doesn't exist yet.
        if(!primaryBonds) {
            particles->setBonds(DataOORef<Bonds>::create());
            particles->makeBondsMutable()->setVisElements(secondaryBonds->visElements());
            primaryBonds = particles->bonds();
        }
        mergeTopologyLists(primaryBonds, secondaryBonds, Bonds::TopologyProperty);
    }

    // Merge angles.
    const Angles* primaryAngles = particles->angles();
    const Angles* secondaryAngles = secondaryParticles->angles();
    if(primaryAngles || secondaryAngles) {
        // Create the primary angles object if it doesn't exist yet.
        if(!primaryAngles) {
            particles->setAngles(DataOORef<Angles>::create());
            particles->makeAnglesMutable()->setVisElements(secondaryAngles->visElements());
            primaryAngles = particles->angles();
        }
        mergeTopologyLists(primaryAngles, secondaryAngles, Angles::TopologyProperty);
    }

    // Merge dihedrals.
    const Dihedrals* primaryDihedrals = particles->dihedrals();
    const Dihedrals* secondaryDihedrals = secondaryParticles->dihedrals();
    if(primaryDihedrals || secondaryDihedrals) {
        // Create the primary dihedrals object if it doesn't exist yet.
        if(!primaryDihedrals) {
            particles->setDihedrals(DataOORef<Dihedrals>::create());
            particles->makeDihedralsMutable()->setVisElements(secondaryDihedrals->visElements());
            primaryDihedrals = particles->dihedrals();
        }
        mergeTopologyLists(primaryDihedrals, secondaryDihedrals, Dihedrals::TopologyProperty);
    }

    // Merge impropers.
    const Impropers* primaryImpropers = particles->impropers();
    const Impropers* secondaryImpropers = secondaryParticles->impropers();
    if(primaryImpropers || secondaryImpropers) {
        // Create the primary impropers object if it doesn't exist yet.
        if(!primaryImpropers) {
            particles->setImpropers(DataOORef<Impropers>::create());
            particles->makeImpropersMutable()->setVisElements(secondaryImpropers->visElements());
            primaryImpropers = particles->impropers();
        }
        mergeTopologyLists(primaryImpropers, secondaryImpropers, Impropers::TopologyProperty);
    }

    int secondaryFrame = secondaryState.data() ? secondaryState.data()->sourceFrame() : 1;
    if(secondaryFrame < 0)
        secondaryFrame = request.time().frame();

    state.setStatus(PipelineStatus(secondaryState.status().type(), tr("Merged %1 existing particles with %2 particles from frame %3 of second dataset.")
            .arg(primaryParticleCount)
            .arg(secondaryParticleCount)
            .arg(secondaryFrame)));

    return std::move(state);
}

}   // End of namespace
