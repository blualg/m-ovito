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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ParticlesReplicateModifierDelegate.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ParticlesReplicateModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> ParticlesReplicateModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<Particles>())
        return { DataObjectReference(&Particles::OOClass()) };
    return {};
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus ParticlesReplicateModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ReplicateModifier* mod = static_object_cast<ReplicateModifier>(request.modifier());
    const Particles* inputParticles = state.getObject<Particles>();

    std::array<int,3> nPBC;
    nPBC[0] = std::max(mod->numImagesX(),1);
    nPBC[1] = std::max(mod->numImagesY(),1);
    nPBC[2] = std::max(mod->numImagesZ(),1);

    // Calculate new number of particles.
    size_t numCopies = nPBC[0] * nPBC[1] * nPBC[2];
    if(numCopies <= 1 || !inputParticles || inputParticles->elementCount() == 0)
        return PipelineStatus::Success;

    // Extend particle property arrays.
    size_t oldParticleCount = inputParticles->elementCount();
    size_t newParticleCount = oldParticleCount * numCopies;

    const SimulationCell* cell = state.expectObject<SimulationCell>();
    const AffineTransformation cellMatrix = cell->matrix();

    // Ensure that the particles can be modified.
    Particles* outputParticles = state.makeMutable(inputParticles);
    outputParticles->replicate(numCopies);

    // Replicate particle property values.
    Box3I newImages = mod->replicaRange();
    for(Property* property : outputParticles->makePropertiesMutable()) {
        OVITO_ASSERT(property->size() == newParticleCount);

        // Shift particle positions by the periodicity vector.
        if(property->type() == Particles::PositionProperty) {
            BufferWriteAccess<Point3, access_mode::read_write> positionArray(property);
            Point3* p = positionArray.begin();
            for(int imageX = newImages.minc.x(); imageX <= newImages.maxc.x(); imageX++) {
                for(int imageY = newImages.minc.y(); imageY <= newImages.maxc.y(); imageY++) {
                    for(int imageZ = newImages.minc.z(); imageZ <= newImages.maxc.z(); imageZ++) {
                        if(imageX != 0 || imageY != 0 || imageZ != 0) {
                            const Vector3 imageDelta = cellMatrix * Vector3(imageX, imageY, imageZ);
                            for(size_t i = 0; i < oldParticleCount; i++)
                                *p++ += imageDelta;
                        }
                        else {
                            p += oldParticleCount;
                        }
                    }
                }
            }
        }

        // Assign unique IDs to duplicated particles.
        if(mod->uniqueIdentifiers() && (property->type() == Particles::IdentifierProperty || property->type() == Particles::MoleculeProperty)) {
            BufferWriteAccess<IdentifierIntType, access_mode::read_write> propertyData(property);
            auto minmax = std::minmax_element(propertyData.cbegin(), propertyData.cbegin() + oldParticleCount);
            auto minID = *minmax.first;
            auto maxID = *minmax.second;
            for(size_t c = 1; c < numCopies; c++) {
                auto offset = (maxID - minID + 1) * c;
                for(auto id = propertyData.begin() + c * oldParticleCount, id_end = id + oldParticleCount; id != id_end; ++id)
                    *id += offset;
            }
        }
    }

    // Replicate bonds.
    if(outputParticles->bonds()) {
        size_t oldBondCount = outputParticles->bonds()->elementCount();
        size_t newBondCount = oldBondCount * numCopies;

        BufferReadAccessAndRef<Vector3I> oldPeriodicImages = outputParticles->bonds()->getProperty(Bonds::PeriodicImageProperty);

        // Replicate bond property values.
        Bonds* mutableBonds = outputParticles->makeBondsMutable();
        mutableBonds->replicate(numCopies);
        for(Property* property : mutableBonds->makePropertiesMutable()) {
            OVITO_ASSERT(property->size() == newBondCount);

            size_t destinationIndex = 0;
            Point3I image;

            // TODO: Special handling of the particle identifiers property.
            OVITO_ASSERT(property->type() != Bonds::ParticleIdentifiersProperty);

            // Special handling for the topology property.
            if(property->type() == Bonds::TopologyProperty) {
                BufferWriteAccess<ParticleIndexPair, access_mode::read_write> topologyArray(property);
                for(image[0] = newImages.minc.x(); image[0] <= newImages.maxc.x(); image[0]++) {
                    for(image[1] = newImages.minc.y(); image[1] <= newImages.maxc.y(); image[1]++) {
                        for(image[2] = newImages.minc.z(); image[2] <= newImages.maxc.z(); image[2]++) {
                            for(size_t bindex = 0; bindex < oldBondCount; bindex++, destinationIndex++) {
                                Point3I newImage;
                                for(size_t dim = 0; dim < 3; dim++) {
                                    int i = image[dim] + (oldPeriodicImages ? oldPeriodicImages[bindex][dim] : 0) - newImages.minc[dim];
                                    newImage[dim] = SimulationCell::modulo(i, nPBC[dim]) + newImages.minc[dim];
                                }
                                OVITO_ASSERT(newImage.x() >= newImages.minc.x() && newImage.x() <= newImages.maxc.x());
                                OVITO_ASSERT(newImage.y() >= newImages.minc.y() && newImage.y() <= newImages.maxc.y());
                                OVITO_ASSERT(newImage.z() >= newImages.minc.z() && newImage.z() <= newImages.maxc.z());
                                size_t imageIndex1 =  ((image.x()-newImages.minc.x()) * nPBC[1] * nPBC[2])
                                                    + ((image.y()-newImages.minc.y()) * nPBC[2])
                                                    +  (image.z()-newImages.minc.z());
                                size_t imageIndex2 =  ((newImage.x()-newImages.minc.x()) * nPBC[1] * nPBC[2])
                                                    + ((newImage.y()-newImages.minc.y()) * nPBC[2])
                                                    +  (newImage.z()-newImages.minc.z());
                                topologyArray[destinationIndex][0] += imageIndex1 * oldParticleCount;
                                topologyArray[destinationIndex][1] += imageIndex2 * oldParticleCount;
                                OVITO_ASSERT((size_t)topologyArray[destinationIndex][0] < newParticleCount);
                                OVITO_ASSERT((size_t)topologyArray[destinationIndex][1] < newParticleCount);
                            }
                        }
                    }
                }
            }
            else if(property->type() == Bonds::PeriodicImageProperty) {
                // Special handling for the PBC shift vector property.
                OVITO_ASSERT(oldPeriodicImages);
                BufferWriteAccess<Vector3I, access_mode::read_write> pbcImagesArray(property);
                for(image[0] = newImages.minc.x(); image[0] <= newImages.maxc.x(); image[0]++) {
                    for(image[1] = newImages.minc.y(); image[1] <= newImages.maxc.y(); image[1]++) {
                        for(image[2] = newImages.minc.z(); image[2] <= newImages.maxc.z(); image[2]++) {
                            for(size_t bindex = 0; bindex < oldBondCount; bindex++, destinationIndex++) {
                                Vector3I newShift;
                                for(size_t dim = 0; dim < 3; dim++) {
                                    int i = image[dim] + oldPeriodicImages[bindex][dim] - newImages.minc[dim];
                                    newShift[dim] = i >= 0 ? (i / nPBC[dim]) : ((i-nPBC[dim]+1) / nPBC[dim]);
                                    if(!mod->adjustBoxSize())
                                        newShift[dim] *= nPBC[dim];
                                }
                                pbcImagesArray[destinationIndex] = newShift;
                            }
                        }
                    }
                }
            }
        }
    }

    // Replicate angles.
    if(outputParticles->angles()) {
        size_t oldAngleCount = outputParticles->angles()->elementCount();

        // Replicate angle property values.
        Angles* mutableAngles = outputParticles->makeAnglesMutable();
        mutableAngles->replicate(numCopies);
        for(Property* property : mutableAngles->makePropertiesMutable()) {
            size_t destinationIndex = 0;
            Point3I image;

            // Special handling for the topology property.
            if(property->type() == Angles::TopologyProperty) {
                BufferWriteAccess<ParticleIndexTriplet, access_mode::read_write> topologyArray(property);
                BufferReadAccess<Point3> positionArray(inputParticles->expectProperty(Particles::PositionProperty));
                for(image[0] = newImages.minc.x(); image[0] <= newImages.maxc.x(); image[0]++) {
                    for(image[1] = newImages.minc.y(); image[1] <= newImages.maxc.y(); image[1]++) {
                        for(image[2] = newImages.minc.z(); image[2] <= newImages.maxc.z(); image[2]++) {
                            for(size_t index = 0; index < oldAngleCount; index++, destinationIndex++) {
                                auto referenceParticle = topologyArray[destinationIndex][1];
                                for(auto& pindex : topologyArray[destinationIndex]) {
                                    Point3I newImage = image;
                                    if(pindex >= 0 && (size_t)pindex < positionArray.size() && referenceParticle >= 0 && (size_t)referenceParticle < positionArray.size()) {
                                        Vector3 delta = positionArray[pindex] - positionArray[referenceParticle];
                                        for(size_t dim = 0; dim < 3; dim++) {
                                            if(cell->hasPbc(dim)) {
                                                int imageDelta = (int)std::floor(cell->inverseMatrix().prodrow(delta, dim) + FloatType(0.5));
                                                int i = image[dim] - newImages.minc[dim] - imageDelta;
                                                newImage[dim] = SimulationCell::modulo(i, nPBC[dim]) + newImages.minc[dim];
                                            }
                                        }
                                    }
                                    int imageIndex =   ((newImage.x() - newImages.minc.x()) * nPBC[1] * nPBC[2])
                                                     + ((newImage.y() - newImages.minc.y()) * nPBC[2])
                                                     +  (newImage.z() - newImages.minc.z());
                                    pindex += imageIndex * oldParticleCount;
                                    OVITO_ASSERT((size_t)pindex < newParticleCount);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Replicate dihedrals.
    if(outputParticles->dihedrals()) {
        size_t oldDihedralCount = outputParticles->dihedrals()->elementCount();

        // Replicate dihedral property values.
        Dihedrals* mutableDihedrals = outputParticles->makeDihedralsMutable();
        mutableDihedrals->replicate(numCopies);
        for(Property* property : mutableDihedrals->makePropertiesMutable()) {
            size_t destinationIndex = 0;
            Point3I image;

            // Special handling for the topology property.
            if(property->type() == Dihedrals::TopologyProperty) {
                BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> topologyArray(property);
                BufferReadAccess<Point3> positionArray(inputParticles->expectProperty(Particles::PositionProperty));
                for(image[0] = newImages.minc.x(); image[0] <= newImages.maxc.x(); image[0]++) {
                    for(image[1] = newImages.minc.y(); image[1] <= newImages.maxc.y(); image[1]++) {
                        for(image[2] = newImages.minc.z(); image[2] <= newImages.maxc.z(); image[2]++) {
                            for(size_t index = 0; index < oldDihedralCount; index++, destinationIndex++) {
                                auto referenceParticle = topologyArray[destinationIndex][1];
                                for(auto& pindex : topologyArray[destinationIndex]) {
                                    Point3I newImage = image;
                                    if(pindex >= 0 && (size_t)pindex < positionArray.size() && referenceParticle >= 0 && (size_t)referenceParticle < positionArray.size()) {
                                        Vector3 delta = positionArray[pindex] - positionArray[referenceParticle];
                                        for(size_t dim = 0; dim < 3; dim++) {
                                            if(cell->hasPbc(dim)) {
                                                int imageDelta = (int)std::floor(cell->inverseMatrix().prodrow(delta, dim) + FloatType(0.5));
                                                int i = image[dim] - newImages.minc[dim] - imageDelta;
                                                newImage[dim] = SimulationCell::modulo(i, nPBC[dim]) + newImages.minc[dim];
                                            }
                                        }
                                    }
                                    int imageIndex =   ((newImage.x() - newImages.minc.x()) * nPBC[1] * nPBC[2])
                                                     + ((newImage.y() - newImages.minc.y()) * nPBC[2])
                                                     +  (newImage.z() - newImages.minc.z());
                                    pindex += imageIndex * oldParticleCount;
                                    OVITO_ASSERT((size_t)pindex < newParticleCount);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Replicate impropers.
    if(outputParticles->impropers()) {
        size_t oldImproperCount = outputParticles->impropers()->elementCount();

        // Replicate improper property values.
        Impropers* mutableImpropers = outputParticles->makeImpropersMutable();
        mutableImpropers->replicate(numCopies);
        for(Property* property : mutableImpropers->makePropertiesMutable()) {
            size_t destinationIndex = 0;
            Point3I image;

            // Special handling for the topology property.
            if(property->type() == Impropers::TopologyProperty) {
                BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> topologyArray(property);
                BufferReadAccess<Point3> positionArray(inputParticles->expectProperty(Particles::PositionProperty));
                for(image[0] = newImages.minc.x(); image[0] <= newImages.maxc.x(); image[0]++) {
                    for(image[1] = newImages.minc.y(); image[1] <= newImages.maxc.y(); image[1]++) {
                        for(image[2] = newImages.minc.z(); image[2] <= newImages.maxc.z(); image[2]++) {
                            for(size_t index = 0; index < oldImproperCount; index++, destinationIndex++) {
                                auto referenceParticle = topologyArray[destinationIndex][1];
                                for(auto& pindex : topologyArray[destinationIndex]) {
                                    Point3I newImage = image;
                                    if(pindex >= 0 && (size_t)pindex < positionArray.size() && referenceParticle >= 0 && (size_t)referenceParticle < positionArray.size()) {
                                        Vector3 delta = positionArray[pindex] - positionArray[referenceParticle];
                                        for(size_t dim = 0; dim < 3; dim++) {
                                            if(cell->hasPbc(dim)) {
                                                int imageDelta = (int)std::floor(cell->inverseMatrix().prodrow(delta, dim) + FloatType(0.5));
                                                int i = image[dim] - newImages.minc[dim] - imageDelta;
                                                newImage[dim] = SimulationCell::modulo(i, nPBC[dim]) + newImages.minc[dim];
                                            }
                                        }
                                    }
                                    int imageIndex =   ((newImage.x() - newImages.minc.x()) * nPBC[1] * nPBC[2])
                                                     + ((newImage.y() - newImages.minc.y()) * nPBC[2])
                                                     +  (newImage.z() - newImages.minc.z());
                                    pindex += imageIndex * oldParticleCount;
                                    OVITO_ASSERT((size_t)pindex < newParticleCount);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return PipelineStatus::Success;
}

}   // End of namespace
