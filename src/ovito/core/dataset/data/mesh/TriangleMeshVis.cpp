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
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TriangleMeshVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TriangleMeshVis);
OVITO_CLASSINFO(TriangleMeshVis, "DisplayName", "Triangle mesh");
OVITO_CLASSINFO(TriangleMeshVis, "ClassNameAlias", "TriMeshVis");  // For backward compatibility with OVITO 3.9.2
DEFINE_PROPERTY_FIELD(TriangleMeshVis, color);
DEFINE_REFERENCE_FIELD(TriangleMeshVis, transparencyController);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, highlightEdges);
DEFINE_PROPERTY_FIELD(TriangleMeshVis, backfaceCulling);
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, color, "Display color");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, transparencyController, "Transparency");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, highlightEdges, "Highlight edges");
SET_PROPERTY_FIELD_LABEL(TriangleMeshVis, backfaceCulling, "Back-face culling");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TriangleMeshVis, transparencyController, PercentParameterUnit, 0, 1);

/******************************************************************************
* Constructor.
******************************************************************************/
void TriangleMeshVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setTransparencyController(ControllerManager::createFloatController());
    }
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TriangleMeshVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    // Let the triangle mesh do the computing of the bounding box.
    if(const TriangleMesh* triMeshObj = path.lastAs<TriangleMesh>()) {
        return triMeshObj->boundingBox();
    }
    return {};
}

/******************************************************************************
* Lets the vis element produce a visual representation of a data object.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> TriangleMeshVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline)
{
    // Obtain transparency parameter value and display color value.
    FloatType transp = 0;
    TimeInterval iv;
    if(transparencyController())
        transp = transparencyController()->getFloatValue(frameGraph.time(), iv);

    // Prepare the mesh rendering primitive.
    auto primitive = std::make_unique<MeshPrimitive>();
    primitive->setEmphasizeEdges(highlightEdges());
    primitive->setUniformColor(ColorA(color(), FloatType(1) - transp));
    primitive->setMesh(path.lastAs<TriangleMesh>());
    primitive->setCullFaces(backfaceCulling());

    // Add render primitive to graph.
    frameGraph.addPrimitive(frameGraph.addCommandGroup(FrameGraph::SceneLayer), std::move(primitive), pipeline);

    return {};
}

}   // End of namespace
