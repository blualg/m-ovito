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
#include "ObjectPickingMap.h"

namespace Ovito {

/******************************************************************************
* Given an object ID from the picking render buffer, looks up the corresponding picking record.
******************************************************************************/
std::pair<uint32_t, const ObjectPickingMap::PickingRecord*> ObjectPickingMap::lookupPickingRecordFromLinearId(uint32_t objectID) const
{
    if(objectID == 0 || _pickingRecords.empty())
        return {0, nullptr};

    // Find the records that succeeds the one containing the given object ID.
    auto iter = _pickingRecords.upper_bound(objectID);

    if(iter == _pickingRecords.begin())
        return {0, nullptr};

    if(iter != _pickingRecords.end()) {
        OVITO_ASSERT(iter->first > objectID);
        OVITO_ASSERT(objectID >= std::prev(iter)->first);
        return { std::prev(iter)->first, &std::prev(iter)->second };
    }
    else {
        OVITO_ASSERT(objectID >= _pickingRecords.crbegin()->first);
        return { _pickingRecords.crbegin()->first, &_pickingRecords.crbegin()->second };
    }
}

/******************************************************************************
* Computes the 3d world-space location corresponding to the given 2d window position.
******************************************************************************/
Point3 ObjectPickingMap::worldPositionAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const
{
    if(!framebufferSize.isEmpty()) {
        FloatType zvalue = depthAt(frameBufferLocation, projectionParams, framebufferSize);
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
