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
* Prepares the mapping.
******************************************************************************/
void ObjectPickingIdentifierMap::prepare(const FrameGraph& frameGraph, quint32 startObjectID)
{
    OVITO_ASSERT(startObjectID > 0);
    _nextAvailablePickingID = startObjectID;

    // Copy the picking groups from the frame graph.
    _pickingGroups.clear();
    _pickingGroups.reserve(frameGraph.pickingGroups().size());
    for(const auto& group : frameGraph.pickingGroups()) {
        _pickingGroups.emplace_back(group.pipeline(), group.pickInfo());
    }
}

/******************************************************************************
* Registers a range of unique IDs for the current object picking group being rendered.
******************************************************************************/
quint32 ObjectPickingIdentifierMap::allocateObjectPickingIDs(int pickingGroupID, quint32 objectCount, const ConstDataBufferPtr& indices)
{
    OVITO_ASSERT(pickingGroupID > 0 && pickingGroupID <= _pickingGroups.size());
    MappedObjectGroup& group = _pickingGroups[pickingGroupID - 1]; // Convert 1-based group ID to 0-based index.

    quint32 baseObjectID = _nextAvailablePickingID;
    if(group.baseObjectID() == 0)
        group.setBaseObjectID(baseObjectID);
    if(indices)
        group.addIndexedRange(indices, _nextAvailablePickingID - group.baseObjectID());
    _nextAvailablePickingID += objectCount;
    return baseObjectID;
}

/******************************************************************************
* Post-processes the mapping after acquisition.
******************************************************************************/
void ObjectPickingIdentifierMap::postprocess()
{
    // Discard all groups that have not been rendered.
    std::erase_if(_pickingGroups, [](const MappedObjectGroup& group) { return group.baseObjectID() == 0; });
}

/******************************************************************************
* Finds the picked object at the given frame buffer pixel position.
******************************************************************************/
std::optional<ViewportWindow::PickResult> ObjectPickingIdentifierMap::pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(quint32 objectID = objectIdentifierAt(frameBufferLocation)) {
        if(const MappedObjectGroup* pickingGroup = lookupPickingGroupFromObjectId(objectID)) {
            OVITO_ASSERT(pickingGroup->pipeline());
            return ViewportWindow::PickResult(
                pickingGroup->pipeline(),
                pickingGroup->pickInfo(),
                worldPositionAt(frameBufferLocation, projectionParams, framebufferSize),
                pickingGroup->resolveObjectID(objectID));
        }
    }
    return std::nullopt;
}

/******************************************************************************
* Given an object ID from the picking render buffer, looks up the corresponding picking group.
******************************************************************************/
const ObjectPickingIdentifierMap::MappedObjectGroup* ObjectPickingIdentifierMap::lookupPickingGroupFromObjectId(quint32 objectID) const
{
    if(objectID == 0 || _pickingGroups.empty())
        return nullptr;

    // Make sure the groups are sorted by base object ID.
    OVITO_ASSERT(std::is_sorted(_pickingGroups.begin(), _pickingGroups.end(),
        [](const MappedObjectGroup& a, const MappedObjectGroup& b) { return a.baseObjectID() < b.baseObjectID(); }));

    // Find the group that succeeds the one containing the given object ID.
    auto iter = std::upper_bound(_pickingGroups.begin(), _pickingGroups.end(), objectID,
        [](quint32 id, const MappedObjectGroup& group) { return id < group.baseObjectID(); });

    if(iter == _pickingGroups.begin())
        return nullptr;

    if(iter != _pickingGroups.end()) {
        OVITO_ASSERT(iter->baseObjectID() > objectID);
        OVITO_ASSERT(objectID >= std::prev(iter)->baseObjectID());
        return &*std::prev(iter);
    }
    else {
        OVITO_ASSERT(objectID >= _pickingGroups.back().baseObjectID());
        return &_pickingGroups.back();
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
