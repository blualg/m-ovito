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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetworkObject.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "DislocationReplicateModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationReplicateModifierDelegate);
OVITO_CLASSINFO(DislocationReplicateModifierDelegate, "DisplayName", "Dislocations");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> DislocationReplicateModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    if(input.containsObject<DislocationNetworkObject>())
        return { DataObjectReference(&DislocationNetworkObject::OOClass()) };
    return {};
}

/******************************************************************************
 * Applies the modifier operation to the data in a pipeline flow state.
 ******************************************************************************/
Future<PipelineFlowState> DislocationReplicateModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ReplicateModifier* modifier = static_object_cast<ReplicateModifier>(request.modifier());

    const Box3I& newImages = modifier->replicaRange();

    // The actual work can be performed in a separate thread.
    return asyncLaunch([state = std::move(state), newImages]() mutable {

        int nPBC[3] = { newImages.sizeX() + 1, newImages.sizeY() + 1, newImages.sizeZ() + 1};
        size_t numCopies = (size_t)nPBC[0] * (size_t)nPBC[1] * (size_t)nPBC[2];

        for(qsizetype i = 0; i < state.data()->objects().size(); i++) {
            if(const DislocationNetworkObject* existingDislocations = dynamic_object_cast<DislocationNetworkObject>(state.data()->objects()[i])) {

                // For periodic replication, a domain is needed.
                if(!existingDislocations->domain())
                    continue;

                AffineTransformation simCell = existingDislocations->domain()->cellMatrix();
                AffineTransformation inverseSimCell;
                if(!simCell.inverse(inverseSimCell))
                    continue;

                // Create the output copy of the input dislocation object.
                DislocationNetworkObject* newDislocations = state.makeMutable(existingDislocations);
                std::shared_ptr<DislocationNetwork> dislocations = newDislocations->modifiableStorage();

                // Shift existing vertices so that they form the first image at grid position (0,0,0).
                const Vector3 imageDelta = simCell * Vector3(newImages.minc.x(), newImages.minc.y(), newImages.minc.z());
                if(!imageDelta.isZero()) {
                    for(DislocationSegment* segment : dislocations->segments()) {
                        for(Point3& p : segment->line)
                            p += imageDelta;
                    }
                }

                // Replicate lines.
                size_t oldSegmentCount = dislocations->segments().size();
                for(int imageX = 0; imageX < nPBC[0]; imageX++) {
                    for(int imageY = 0; imageY < nPBC[1]; imageY++) {
                        for(int imageZ = 0; imageZ < nPBC[2]; imageZ++) {
                            if(imageX == 0 && imageY == 0 && imageZ == 0) continue;
                            // Shift vertex positions by the periodicity vector.
                            const Vector3 imageDelta = simCell * Vector3(imageX, imageY, imageZ);
                            for(size_t i = 0; i < oldSegmentCount; i++) {
                                DislocationSegment* oldSegment = dislocations->segments()[i];
                                DislocationSegment* newSegment = dislocations->createSegment(oldSegment->burgersVector);
                                newSegment->line = oldSegment->line;
                                newSegment->coreSize = oldSegment->coreSize;
                                for(Point3& p : newSegment->line)
                                    p += imageDelta;
                            }
                            // TODO: Replicate nodal connectivity.
                        }
                    }
                }
                OVITO_ASSERT(dislocations->segments().size() == oldSegmentCount * numCopies);

                // Extend the periodic domain the dislocation network is embedded in.
                simCell.translation() += (FloatType)newImages.minc.x() * simCell.column(0);
                simCell.translation() += (FloatType)newImages.minc.y() * simCell.column(1);
                simCell.translation() += (FloatType)newImages.minc.z() * simCell.column(2);
                simCell.column(0) *= (newImages.sizeX() + 1);
                simCell.column(1) *= (newImages.sizeY() + 1);
                simCell.column(2) *= (newImages.sizeZ() + 1);
                newDislocations->mutableDomain()->setCellMatrix(simCell);
            }
        }

        return std::move(state);
    });
}

}   // End of namespace
