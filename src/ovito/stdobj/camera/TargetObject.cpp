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

#include <ovito/stdobj/StdObj.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/rendering/FrameGraph.h>
#include "TargetObject.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TargetObject);
OVITO_CLASSINFO(TargetObject, "DisplayName", "Target");
IMPLEMENT_CREATABLE_OVITO_CLASS(TargetVis);
OVITO_CLASSINFO(TargetVis, "DisplayName", "Target icon");

/******************************************************************************
* Constructs a target object.
******************************************************************************/
void TargetObject::initializeObject(ObjectInitializationFlags flags)
{
    DataObject::initializeObject(flags);

    if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement))) {
        setVisElement(OORef<TargetVis>::create(flags));
    }
}

/******************************************************************************
* Lets the vis element render a data object.
******************************************************************************/
PipelineStatus TargetVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Target objects are only visible in the interactive viewport windows.
    if(!frameGraph.isInteractive())
        return {};

    // Setup transformation matrix to always show the icon at the same size.
    TimeInterval interval;
    const AffineTransformation& nodeTM = pipeline->getWorldTransform(frameGraph.time(), interval);
    FloatType scaling = FloatType(0.2) * frameGraph.nonScalingSize(Point3::Origin() + nodeTM.translation());

    // Cache the line vertices for the icon.
    auto& vertexPositions = frameGraph.visCache().lookup<ConstDataBufferPtr>(RendererResourceKey<struct WireframeCube>{});

    // Initialize geometry of wireframe cube.
    if(!vertexPositions) {
        const Point3G linePoints[] = {
            {-1, -1, -1}, { 1,-1,-1},
            {-1, -1,  1}, { 1,-1, 1},
            {-1, -1, -1}, {-1,-1, 1},
            { 1, -1, -1}, { 1,-1, 1},
            {-1,  1, -1}, { 1, 1,-1},
            {-1,  1,  1}, { 1, 1, 1},
            {-1,  1, -1}, {-1, 1, 1},
            { 1,  1, -1}, { 1, 1, 1},
            {-1, -1, -1}, {-1, 1,-1},
            { 1, -1, -1}, { 1, 1,-1},
            { 1, -1,  1}, { 1, 1, 1},
            {-1, -1,  1}, {-1, 1, 1}
        };
        BufferFactory<Point3G> vertices(std::size(linePoints));
        boost::copy(linePoints, vertices.begin());
        vertexPositions = vertices.take();
    }

    // Create line rendering primitive.
    std::unique_ptr<LinePrimitive> iconPrimitive = std::make_unique<LinePrimitive>();
    iconPrimitive->setUniformColor(ViewportSettings::getSettings().viewportColor(pipeline->isSelected() ? ViewportSettings::COLOR_SELECTION : ViewportSettings::COLOR_CAMERAS));
    iconPrimitive->setPositions(vertexPositions);

    // Render the lines.
    frameGraph.addPrimitive(std::move(iconPrimitive), nodeTM * AffineTransformation::scaling(scaling), frameGraph.addPickingGroup(pipeline), Box3(Point3::Origin(), 1));

    return {};
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TargetVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    // This is not a physical object. It is point-like and doesn't have any size.
    return Box3(Point3::Origin(), Point3::Origin());
}

}   // End of namespace
