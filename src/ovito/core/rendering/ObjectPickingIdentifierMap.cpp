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

#include <ovito/core/Core.h>
#include "ObjectPickingIdentifierMap.h"

namespace Ovito {

/******************************************************************************
* Registers a range of unique IDs for the current object picking group being rendered.
******************************************************************************/
uint32_t ObjectPickingIdentifierMap::allocateObjectPickingIDs(const FrameGraph::RenderingCommand& command, uint32_t objectCount, const ConstDataBufferPtr& indices)
{
    OVITO_ASSERT(!command.skipInPickingPass());

    auto baseObjectID = _nextAvailablePickingID;
    _pickingRecords.emplace_back(baseObjectID, indices, command);
    _nextAvailablePickingID += objectCount;
    return baseObjectID;
}

/******************************************************************************
* Finds the picked object at the given frame buffer pixel position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> ObjectPickingIdentifierMap::pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(uint32_t objectID = objectIdentifierAt(frameBufferLocation)) {
        if(const PickingRecord* pickingRecord = lookupPickingRecordFromObjectId(objectID)) {
            OVITO_ASSERT(pickingRecord->pipeline());
            return ViewportWindow::PickResult(
                const_cast<Pipeline*>(pickingRecord->pipeline().get()),
                pickingRecord->pickInfo(),
                worldPositionAt(frameBufferLocation, projectionParams, framebufferSize),
                pickingRecord->resolveObjectID(objectID));
        }
    }
    return std::nullopt;
}

/******************************************************************************
* Returns the informational text to be displayed in the status bar for a pickable scene object.
******************************************************************************/
QString ObjectPickingIdentifierMap::pickableObjectInformationText(uint32_t objectID) const
{
    if(const PickingRecord* pickingRecord = lookupPickingRecordFromObjectId(objectID)) {
        if(pickingRecord->pickInfo()) {
            OVITO_ASSERT(pickingRecord->pipeline());
            return pickingRecord->pickInfo()->infoString(pickingRecord->pipeline(), pickingRecord->resolveObjectID(objectID));
        }
    }
    return {};
}

/******************************************************************************
* Given an object ID from the picking render buffer, looks up the corresponding picking record.
******************************************************************************/
const ObjectPickingIdentifierMap::PickingRecord* ObjectPickingIdentifierMap::lookupPickingRecordFromObjectId(uint32_t objectID) const
{
    if(objectID == 0 || _pickingRecords.empty())
        return nullptr;

    // Make sure the records are sorted by base object ID.
    OVITO_ASSERT(std::is_sorted(_pickingRecords.begin(), _pickingRecords.end(),
        [](const PickingRecord& a, const PickingRecord& b) { return a.baseObjectID() < b.baseObjectID(); }));

    // Find the records that succeeds the one containing the given object ID.
    auto iter = std::upper_bound(_pickingRecords.begin(), _pickingRecords.end(), objectID,
        [](uint32_t id, const PickingRecord& record) { return id < record.baseObjectID(); });

    if(iter == _pickingRecords.begin())
        return nullptr;

    if(iter != _pickingRecords.end()) {
        OVITO_ASSERT(iter->baseObjectID() > objectID);
        OVITO_ASSERT(objectID >= std::prev(iter)->baseObjectID());
        return &*std::prev(iter);
    }
    else {
        OVITO_ASSERT(objectID >= _pickingRecords.back().baseObjectID());
        return &_pickingRecords.back();
    }
}

/******************************************************************************
* Computes the 3d world-space location corresponding to the given 2d window position.
******************************************************************************/
Point3 ObjectPickingIdentifierMap::worldPositionAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(!framebufferSize.isEmpty()) {
        FloatType zvalue = depthAt(frameBufferLocation);
        if(zvalue != 0) {
            Point3 ndc(
                    (FloatType)frameBufferLocation.x() / framebufferSize.width() * 2 - 1,
                    1 - (FloatType)frameBufferLocation.y() / framebufferSize.height() * 2,
                    zvalue * 2 - 1);
            return projectionParams.inverseViewMatrix * (projectionParams.inverseProjectionMatrix * ndc);
        }
    }
    return Point3::Origin();
}

}   // End of namespace
