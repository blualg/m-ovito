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
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include "LoadTrajectoryModifier.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(LoadTrajectoryModifier);
DEFINE_REFERENCE_FIELD(LoadTrajectoryModifier, trajectorySource);
SET_PROPERTY_FIELD_LABEL(LoadTrajectoryModifier, trajectorySource, "Trajectory source");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
LoadTrajectoryModifier::LoadTrajectoryModifier(ObjectInitializationFlags flags) : Modifier(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the file source object, which will be responsible for loading
        // and caching the trajectory data.
        setTrajectorySource(OORef<FileSource>::create(flags));
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool LoadTrajectoryModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<ParticlesObject>();
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval LoadTrajectoryModifier::validityInterval(const ModifierEvaluationRequest& request) const
{
    TimeInterval iv = Modifier::validityInterval(request);

    if(trajectorySource())
        iv.intersect(trajectorySource()->validityInterval(request));

    return iv;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> LoadTrajectoryModifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    OVITO_ASSERT(input);

    // Get the trajectory data source.
    if(!trajectorySource())
        throw Exception(tr("No trajectory data source has been set."));

    // Obtain the trajectory frame from the secondary pipeline.
    SharedFuture<PipelineFlowState> trajStateFuture = trajectorySource()->evaluate(request);

    // Wait for the data to become available.
    return trajStateFuture.then(*request.modApp(), [state = input, request](const PipelineFlowState& trajState) mutable {

        if(LoadTrajectoryModifier* trajModifier = dynamic_object_cast<LoadTrajectoryModifier>(request.modifier())) {
            // Make sure the obtained configuration is valid and ready to use.
            if(trajState.status().type() == PipelineStatus::Error) {
                if(FileSource* fileSource = dynamic_object_cast<FileSource>(trajModifier->trajectorySource())) {
                    if(fileSource->sourceUrls().empty())
                        throw Exception(tr("Please pick a trajectory file."));
                }
                state.setStatus(trajState.status());
            }
            else {
                trajModifier->applyTrajectoryState(state, trajState);

                // Invalidate the synchronous state cache of the modifier application.
                // This is needed to force the pipeline system to call our evaluateSynchronous() method
                // again next time the system request a synchronous state from the pipeline.
                request.modApp()->pipelineCache().invalidateSynchronousState();
            }
        }

        return std::move(state);
    });
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void LoadTrajectoryModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    if(trajectorySource()) {
        const PipelineFlowState& trajState = trajectorySource()->evaluateSynchronous(request);
        applyTrajectoryState(state, trajState);
    }
}

/******************************************************************************
* Transfers the particle positions from the trajectory frame to the current
* pipeline input state.
******************************************************************************/
void LoadTrajectoryModifier::applyTrajectoryState(PipelineFlowState& state, const PipelineFlowState& trajState)
{
    if(!trajState)
        throw Exception(tr("Data source has not been specified yet or is empty. Please pick a trajectory file."));

    // Merge validity intervals of topology and trajectory datasets.
    state.intersectStateValidity(trajState.stateValidity());

    // Get the current particle positions.
    const ParticlesObject* trajectoryParticles = trajState.getObject<ParticlesObject>();
    if(!trajectoryParticles)
        throw Exception(tr("Trajectory dataset does not contain any particle dataset."));
    trajectoryParticles->verifyIntegrity();

    // Get the topology particle dataset.
    ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
    particles->verifyIntegrity();

    if(trajectoryParticles->elementCount() != 0) {

        // Build particle-to-particle index map.
        std::vector<size_t> indexToIndexMap(particles->elementCount());
        ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty);
        ConstDataBufferAccess<IdentifierIntType> trajIdentifierProperty = trajectoryParticles->getProperty(ParticlesObject::IdentifierProperty);
        if(identifierProperty && trajIdentifierProperty) {

            // Build map of particle identifiers in trajectory dataset.
            std::map<IdentifierIntType, size_t> refMap;
            size_t index = 0;
            for(auto id : trajIdentifierProperty) {
                if(refMap.insert(std::make_pair(id, index++)).second == false)
                    throw Exception(tr("Particles with duplicate identifiers detected in trajectory dataset."));
            }
            trajIdentifierProperty.reset();

            // Check for duplicate identifiers in topology dataset.
            std::vector<IdentifierIntType> idSet(identifierProperty.cbegin(), identifierProperty.cend());
            boost::sort(idSet);
            if(boost::adjacent_find(idSet) != idSet.cend())
                throw Exception(tr("Particles with duplicate identifiers detected in topology dataset."));

            // Used to keep track of which topology particles are going to be deleted.
            boost::dynamic_bitset<> deletionMask;

            // Build mapping of particle indices from the topology dataset to the corresponding indices in the trajectory dataset.
            auto mappedIndex = indexToIndexMap.begin();
            size_t idx = 0;
            for(auto id : identifierProperty) {
                auto iter = refMap.find(id);
                if(iter == refMap.end()) {
                    // Existing particle from topology dataset was not found in the trajectory dataset.
                    // Mark the particle for deletion.
                    if(deletionMask.size() == 0)
                        deletionMask = boost::dynamic_bitset<>(indexToIndexMap.size());
                    deletionMask.set(idx);
                }
                else {
                    *mappedIndex++ = iter->second;
                    refMap.erase(iter);
                }
                idx++;
            }
            identifierProperty.reset();

            // Delete outdated particles, which have disappeared during the course of the simulation.
            if(deletionMask.size() != 0) {
                OVITO_ASSERT(mappedIndex < indexToIndexMap.end());
                indexToIndexMap.erase(mappedIndex, indexToIndexMap.end());
                particles->deleteElements(deletionMask);
            }
            else {
                OVITO_ASSERT(mappedIndex == indexToIndexMap.end());
            }
            OVITO_ASSERT(indexToIndexMap.size() == particles->elementCount());

            // Check if the trajectory dataset contains excess particles that are not present in the topology dataset yet.
            if(!refMap.empty()) {
                // Insert the new particles after the existing particles in the topology dataset.
                particles->setElementCount(particles->elementCount() + refMap.size());
                indexToIndexMap.reserve(indexToIndexMap.size() + refMap.size());

                // Extend index mapping and particle identifier property.
                DataBufferAccess<IdentifierIntType> identifierProperty = particles->expectMutableProperty(ParticlesObject::IdentifierProperty);
                auto id = identifierProperty.begin() + indexToIndexMap.size();
                for(const auto& entry : refMap) {
                    *id++ = entry.first;
                    indexToIndexMap.push_back(entry.second);
                }
                OVITO_ASSERT(id == identifierProperty.end());
                OVITO_ASSERT(indexToIndexMap.size() == particles->elementCount());
            }
        }
        else {
            // Topology dataset and trajectory data must contain the same number of particles.
            // Also prevent a common mistake: User forgot to dump atom IDs to the trajectory file.
            if(trajectoryParticles->elementCount() != particles->elementCount()) {
                throw Exception(tr("Cannot apply trajectories to current particle dataset. Numbers of particles in the trajectory file and in the topology file do not match."));
            }
            else if(identifierProperty) {
                // We make an exception if topology identifiers are in sorted order forming a consecutive sequence.
                // Gromacs GRO files, for example, contain atom numbers (IDs) and Gromacs XTC files do not.
                // This exception has been introduced to support this particular combination of topology & trajectory files.
                IdentifierIntType idx = 1;
                for(const auto& id : identifierProperty) {
                    if(id != idx++)
                        throw Exception(tr("Particles in the topology dataset have identifiers, but trajectory particles do not. This likely is a mistake. Please ensure the trajectory file contains identifiers too."));
                }
            }
            else if(trajIdentifierProperty) {
                throw Exception(tr("Particles in the trajectory dataset have identifiers, but topology particles do not. This likely is a mistake. Please ensure the topology file contains identifiers too."));
            }

            // When particle identifiers are not available, use trivial 1-to-1 mapping.
            std::iota(indexToIndexMap.begin(), indexToIndexMap.end(), size_t(0));
        }

        // Transfer particle properties from the trajectory file.
        for(const PropertyObject* property : trajectoryParticles->properties()) {
            if(property->type() == ParticlesObject::IdentifierProperty)
                continue;

            // Get or create the output particle property.
            PropertyObject* outputProperty;
            bool replacingProperty;
            if(property->type() != ParticlesObject::UserProperty) {
                replacingProperty = (particles->getProperty(property->type()) != nullptr);
                outputProperty = particles->createProperty(DataBuffer::Initialized, property->type());
                if(outputProperty->dataType() != property->dataType()
                    || outputProperty->componentCount() != property->componentCount())
                    continue; // Types of source property and output property are not compatible.
            }
            else {
                replacingProperty = (particles->getProperty(property->name()) != nullptr);
                outputProperty = particles->createProperty(DataBuffer::Initialized, property->name(),
                    property->dataType(), property->componentCount());
            }
            OVITO_ASSERT(outputProperty->stride() == property->stride());

            // Copy and reorder property data.
            property->mappedCopyTo(*outputProperty, indexToIndexMap);

            // Transfer the visual element(s) unless the property already existed in the topology dataset.
            if(!replacingProperty) {
                outputProperty->setVisElements(property->visElements());
            }
        }

        // Transfer box geometry.
        const SimulationCellObject* topologyCell = state.getObject<SimulationCellObject>();
        const SimulationCellObject* trajectoryCell = trajState.getObject<SimulationCellObject>();
        if(topologyCell && trajectoryCell) {
            SimulationCellObject* outputCell = state.makeMutable(topologyCell);
            outputCell->setCellMatrix(trajectoryCell->cellMatrix());
            const AffineTransformation& simCell = trajectoryCell->cellMatrix();

            // Trajectories of atoms may cross periodic boundaries and if atomic positions are
            // stored in wrapped coordinates, then it becomes necessary to fix bonds using the minimum image convention.
            std::array<bool, 3> pbc = topologyCell->pbcFlags();
            if((pbc[0] || pbc[1] || pbc[2]) && particles->bonds() && std::abs(simCell.determinant()) > FLOATTYPE_EPSILON) {
                ConstDataBufferAccess<Point3> outputPosProperty = particles->expectProperty(ParticlesObject::PositionProperty);
                AffineTransformation inverseCellMatrix = simCell.inverse();

                BondsObject* bonds = particles->makeBondsMutable();
                if(ConstDataBufferAccess<ParticleIndexPair> topologyProperty = bonds->getProperty(BondsObject::TopologyProperty)) {
                    DataBufferAccess<Vector3I> periodicImageProperty = bonds->createProperty(DataBuffer::Initialized, BondsObject::PeriodicImageProperty);

                    // Wrap bonds crossing a periodic boundary by resetting their PBC shift vectors.
                    for(size_t bondIndex = 0; bondIndex < topologyProperty.size(); bondIndex++) {
                        size_t particleIndex1 = topologyProperty[bondIndex][0];
                        size_t particleIndex2 = topologyProperty[bondIndex][1];
                        if(particleIndex1 >= outputPosProperty.size() || particleIndex2 >= outputPosProperty.size())
                            continue;
                        const Point3& p1 = outputPosProperty[particleIndex1];
                        const Point3& p2 = outputPosProperty[particleIndex2];
                        Vector3 delta = p1 - p2;
                        for(int dim = 0; dim < 3; dim++) {
                            if(pbc[dim])
                                periodicImageProperty[bondIndex][dim] = std::lround(inverseCellMatrix.prodrow(delta, dim));
                        }
                    }
                }
            }
        }
    }

    if(const BondsObject* trajectoryBonds = trajectoryParticles->bonds()) {
        trajectoryBonds->verifyIntegrity();

        // Create a mutable copy of the particles object.
        ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
        particles->verifyIntegrity();

        // If the trajectory file contains a bond topology, completely replace all existing bonds
        // from the topology dataset with the new set of bonds.
        // The topology can be specified either in the form of the "Topology" bond property (two 0-based particle indices)
        // or in the form of the "Particle Identifiers" bond property (pairs of atom IDs).
        const PropertyObject* bondTopology = trajectoryBonds->getProperty(BondsObject::TopologyProperty);
        const PropertyObject* bondParticleIdentifiers = trajectoryBonds->getProperty(BondsObject::ParticleIdentifiersProperty);

        if(bondTopology || bondParticleIdentifiers) {
            if(particles->bonds()) {
                // Replace the property arrays, but make sure that BondType instances
                // as well as the visual elements from the topology dataset are preserved.
                particles->makeBondsMutable()->setContent(trajectoryBonds->elementCount(), trajectoryBonds->properties());
            }
            else {
                // We can simply adopt the bonds object from the trajectory dataset as a whole
                // if the topology dataset didn't contain any bonds yet.
                particles->setBonds(trajectoryBonds);
            }

            // If the input bonds are defined in terms of atom IDs (i.e. bond property "Particle Identifiers"), then map the IDs
            // to corresponding particle indices and store them in the "Topology" bond property.
            if(bondParticleIdentifiers) {
                // Build map from particle identifiers to particle indices.
                std::map<IdentifierIntType, size_t> idToIndexMap;
                if(ConstDataBufferAccess<IdentifierIntType> particleIdentifierProperty = particles->getProperty(ParticlesObject::IdentifierProperty)) {
                    size_t index = 0;
                    for(auto id : particleIdentifierProperty) {
                        if(idToIndexMap.insert(std::make_pair(id, index++)).second == false)
                            throw Exception(tr("Duplicate particle identifier %1 detected. Please make sure particle identifiers are unique.").arg(id));
                    }
                }
                else {
                    // Generate implicit IDs if the "Particle Identifier" property is not defined.
                    for(size_t i = 0; i < particles->elementCount(); i++)
                        idToIndexMap[i+1] = i;
                }

                // Perform lookup of particle IDs.
                DataBufferAccess<ParticleIndexPair> bondTopologyArray = particles->makeBondsMutable()->createProperty(BondsObject::TopologyProperty);
                auto t = bondTopologyArray.begin();
                for(const ParticleIndexPair& bond : ConstDataBufferAccess<ParticleIndexPair>(bondParticleIdentifiers)) {
                    auto iter1 = idToIndexMap.find(bond[0]);
                    auto iter2 = idToIndexMap.find(bond[1]);
                    if(iter1 == idToIndexMap.end())
                        throw Exception(tr("Particle id %1 referenced by bond #%2 does not exist.").arg(bond[0]).arg(std::distance(bondTopologyArray.begin(), t)));
                    if(iter2 == idToIndexMap.end())
                        throw Exception(tr("Particle id %1 referenced by bond #%2 does not exist.").arg(bond[1]).arg(std::distance(bondTopologyArray.begin(), t)));
                    (*t)[0] = iter1->second;
                    (*t)[1] = iter2->second;
                    ++t;
                }

                // Remove the "Particle Identifiers" property from bonds again, because it is no longer needed.
                particles->makeBondsMutable()->removeProperty(bondParticleIdentifiers);
            }

            // Compute the PBC shift vectors of the bonds based on current particle positions.
            if(!trajectoryBonds->getProperty(BondsObject::PeriodicImageProperty)) {
                if(const SimulationCellObject* simCellObj = state.getObject<SimulationCellObject>()) {
                    if(simCellObj->pbcX() || simCellObj->pbcY() || simCellObj->pbcZ()) {
                        particles->makeBondsMutable()->generatePeriodicImageProperty(particles, simCellObj);
                    }
                }
            }
        }
        else if(particles->bonds()) {
            // If the trajectory dataset doesn't contain the "Topology" nor the "Particle Identifiers" bond property,
            // then add the bond properties to the existing bonds from the topology dataset.
            // This requires that the number of bonds remains constant.
            if(trajectoryBonds->elementCount() != particles->bonds()->elementCount()) {
                throw Exception(tr("Cannot merge bond properties of trajectory dataset with topology dataset, because numbers of bonds in the two datasets do not match."));
            }

            if(!trajectoryBonds->properties().empty()) {
                BondsObject* bonds = particles->makeBondsMutable();

                // Add the properties to the existing bonds, overwriting existing values if necessary.
                for(const PropertyObject* newProperty : trajectoryBonds->properties()) {
                    const PropertyObject* existingPropertyObj = (newProperty->type() != 0) ? bonds->getProperty(newProperty->type()) : bonds->getProperty(newProperty->name());
                    if(existingPropertyObj) {
                        bonds->makeMutable(existingPropertyObj)->copyFrom(*newProperty);
                    }
                    else {
                        bonds->addProperty(newProperty);
                    }
                }
            }
        }
        else {
            throw Exception(tr("Neither the trajectory nor the topology dataset contain bond connectivity information."));
        }
    }

    // Merge global attributes of topology and trajectory datasets.
    // If there is a naming collision, attributes from the trajectory dataset override those from the topology dataset.
    for(const DataObject* obj : trajState.data()->objects()) {
        if(const AttributeDataObject* attribute = dynamic_object_cast<AttributeDataObject>(obj)) {
            const AttributeDataObject* existingAttribute = nullptr;
            for(const DataObject* obj2 : state.data()->objects()) {
                if(const AttributeDataObject* attribute2 = dynamic_object_cast<AttributeDataObject>(obj2)) {
                    if(attribute2->identifier() == attribute->identifier()) {
                        existingAttribute = attribute2;
                        break;
                    }
                }
            }
            if(existingAttribute)
                state.mutableData()->replaceObject(existingAttribute, attribute);
            else
                state.addObject(attribute);
        }
    }
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool LoadTrajectoryModifier::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::AnimationFramesChanged && source == trajectorySource()) {
        // Propagate animation interval events from the trajectory source.
        return true;
    }
    return Modifier::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data object of the node has been replaced.
******************************************************************************/
void LoadTrajectoryModifier::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(trajectorySource) && !isBeingLoaded() && !isAboutToBeDeleted()) {
        // The animation length might have changed when the trajectory source has been replaced.
        notifyDependents(ReferenceEvent::AnimationFramesChanged);
    }
    Modifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

}   // End of namespace
