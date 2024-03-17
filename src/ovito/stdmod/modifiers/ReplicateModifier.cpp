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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "ReplicateModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ReplicateModifier);
DEFINE_PROPERTY_FIELD(ReplicateModifier, numImagesX);
DEFINE_PROPERTY_FIELD(ReplicateModifier, numImagesY);
DEFINE_PROPERTY_FIELD(ReplicateModifier, numImagesZ);
DEFINE_PROPERTY_FIELD(ReplicateModifier, adjustBoxSize);
DEFINE_PROPERTY_FIELD(ReplicateModifier, uniqueIdentifiers);
SET_PROPERTY_FIELD_LABEL(ReplicateModifier, numImagesX, "Number of images - X");
SET_PROPERTY_FIELD_LABEL(ReplicateModifier, numImagesY, "Number of images - Y");
SET_PROPERTY_FIELD_LABEL(ReplicateModifier, numImagesZ, "Number of images - Z");
SET_PROPERTY_FIELD_LABEL(ReplicateModifier, adjustBoxSize, "Adjust simulation box size");
SET_PROPERTY_FIELD_LABEL(ReplicateModifier, uniqueIdentifiers, "Assign unique IDs");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ReplicateModifier, numImagesX, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ReplicateModifier, numImagesY, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ReplicateModifier, numImagesZ, IntegerParameterUnit, 1);

IMPLEMENT_ABSTRACT_OVITO_CLASS(ReplicateModifierDelegate);

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesReplicateModifierDelegate);

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesReplicateModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> LinesReplicateModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ReplicateModifier* modifier = static_object_cast<ReplicateModifier>(request.modifier());

    // Get range of new images
    const Box3I& newImages = modifier->replicaRange();

    // The actual work can be performed in a separate thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([state = std::move(state), newImages]() mutable {

        size_t numCopies = (newImages.sizeX() + 1) * (newImages.sizeY() + 1) * (newImages.sizeZ() + 1);

        // Get the simulation cell
        const SimulationCell* cell = state.expectObject<SimulationCell>();
        const AffineTransformation& cellMatrix = cell->matrix();

        // Loop over all lines objects in the data collection
        for(const DataObject* obj : state.data()->objects()) {
            // Replicate the Lines.
            if(const Lines* inputLines = dynamic_object_cast<Lines>(obj)) {

                // Skip if there's nothing to do
                if(numCopies <= 1 || !inputLines || inputLines->elementCount() == 0)
                    continue;

                // Extend lines property arrays.
                size_t oldVertexCount = inputLines->elementCount();
                size_t newVertexCount = oldVertexCount * numCopies;

                // Ensure that the lines can be modified.
                Lines* outputLines = state.makeMutable(inputLines);
                outputLines->replicate(numCopies);

                // Replicate lines (vertex) property values.
                for(Property* property : outputLines->makePropertiesMutable()) {
                    OVITO_ASSERT(property->size() == newVertexCount);

                    // Shift vertex positions by the periodicity vector.
                    if(property->type() == Lines::PositionProperty) {
                        BufferWriteAccess<Point3, access_mode::read_write> positionArray(property);
                        Point3* p = positionArray.begin();
                        for(int imageX = newImages.minc.x(); imageX <= newImages.maxc.x(); imageX++) {
                            for(int imageY = newImages.minc.y(); imageY <= newImages.maxc.y(); imageY++) {
                                for(int imageZ = newImages.minc.z(); imageZ <= newImages.maxc.z(); imageZ++) {
                                    if(imageX != 0 || imageY != 0 || imageZ != 0) {
                                        const Vector3 imageDelta = cellMatrix * Vector3(imageX, imageY, imageZ);
                                        for(size_t i = 0; i < oldVertexCount; i++) {
                                            *p++ += imageDelta;
                                        }
                                    }
                                    else {
                                        p += oldVertexCount;
                                    }
                                }
                            }
                        }
                    }
                    else if(property->type() == Lines::SectionProperty) {
                        BufferWriteAccess<int64_t, access_mode::read_write> sectionsArray(property);
                        int64_t* s = sectionsArray.begin();
                        auto minmax = std::minmax_element(sectionsArray.cbegin(), sectionsArray.cbegin() + oldVertexCount);
                        auto minSec = *minmax.first;
                        auto maxSec = *minmax.second;
                        for(size_t c = 1; c < numCopies; c++) {
                            auto offset = (maxSec - minSec + 1) * c;
                            for(auto id = sectionsArray.begin() + c * oldVertexCount, id_end = id + oldVertexCount; id != id_end; ++id)
                                *id += offset;
                        }
                    }
                }
            }
        }

        return std::move(state);
    });
}

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ReplicateModifier::ReplicateModifier(ObjectInitializationFlags flags) : MultiDelegatingModifier(flags),
    _numImagesX(1),
    _numImagesY(1),
    _numImagesZ(1),
    _adjustBoxSize(true),
    _uniqueIdentifiers(true)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Generate the list of delegate objects.
        createModifierDelegates(ReplicateModifierDelegate::OOClass());
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool ReplicateModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return MultiDelegatingModifier::OOMetaClass::isApplicableTo(input)
        && input.containsObject<SimulationCell>();
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ReplicateModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if((field == PROPERTY_FIELD(ReplicateModifier::numImagesX) || field == PROPERTY_FIELD(ReplicateModifier::numImagesY) || field == PROPERTY_FIELD(ReplicateModifier::numImagesZ)) && !isBeingLoaded()) {
        // Changes of some modifier parameters affect the result of ReplicateModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    MultiDelegatingModifier::propertyChanged(field);
}

/******************************************************************************
* Helper function that returns the range of replicated boxes.
******************************************************************************/
Box3I ReplicateModifier::replicaRange() const
{
    std::array<int,3> nPBC;
    nPBC[0] = std::max(numImagesX(),1);
    nPBC[1] = std::max(numImagesY(),1);
    nPBC[2] = std::max(numImagesZ(),1);
    Box3I replicaBox;
    replicaBox.minc[0] = -(nPBC[0]-1)/2;
    replicaBox.minc[1] = -(nPBC[1]-1)/2;
    replicaBox.minc[2] = -(nPBC[2]-1)/2;
    replicaBox.maxc[0] = nPBC[0]/2;
    replicaBox.maxc[1] = nPBC[1]/2;
    replicaBox.maxc[2] = nPBC[2]/2;
    OVITO_ASSERT(
        nPBC[0] * nPBC[1] * nPBC[2] ==
        (replicaBox.sizeX() + 1) *
        (replicaBox.sizeY() + 1) *
        (replicaBox.sizeZ() + 1));
    return replicaBox;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ReplicateModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Return unmodified state if replication is not necessary.
    if(numImagesX() <= 1 && numImagesY() <= 1 && numImagesZ() <= 1)
        return std::move(state);

    // First, apply all delegates to the input data.
    Future<PipelineFlowState> future = MultiDelegatingModifier::evaluateModifier(request, std::move(state));

    // Additionally, resize the simulation cell if enabled.
    if(adjustBoxSize()) {
        future.postprocess(*this, [newImages = replicaRange()](PipelineFlowState state) {
            SimulationCell* cellObj = state.expectMutableObject<SimulationCell>();
            AffineTransformation simCell = cellObj->cellMatrix();
            simCell.translation() += (FloatType)newImages.minc.x() * simCell.column(0);
            simCell.translation() += (FloatType)newImages.minc.y() * simCell.column(1);
            simCell.translation() += (FloatType)newImages.minc.z() * simCell.column(2);
            simCell.column(0) *= (newImages.sizeX() + 1);
            simCell.column(1) *= (newImages.sizeY() + 1);
            simCell.column(2) *= (newImages.sizeZ() + 1);
            cellObj->setCellMatrix(simCell);
            return std::move(state);
        });
    }

    return future;
}

}   // End of namespace
