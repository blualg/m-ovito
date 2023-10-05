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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TriangleMeshVis.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(TriangleMeshVis);
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
TriangleMeshVis::TriangleMeshVis(ObjectInitializationFlags flags) : DataVis(flags),
    _color(0.85, 0.85, 1),
    _highlightEdges(false),
    _backfaceCulling(false)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setTransparencyController(ControllerManager::createFloatController());
    }
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TriangleMeshVis::boundingBox(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval)
{
    // Compute bounding box.
    if(const TriangleMesh* triMeshObj = path.lastAs<TriangleMesh>()) {
        return triMeshObj->boundingBox();
    }
    return Box3();
}

/******************************************************************************
* Lets the vis element render a data object.
******************************************************************************/
PipelineStatus TriangleMeshVis::render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const Pipeline* pipeline)
{
    if(!renderer->isBoundingBoxPass()) {

        // Obtains transparency parameter value and display color value.
        FloatType transp = 0;
        TimeInterval iv;
        if(transparencyController()) transp = transparencyController()->getFloatValue(time, iv);

        // Prepare the mesh rendering primitive.
        MeshPrimitive primitive;
        primitive.setEmphasizeEdges(highlightEdges());
        primitive.setUniformColor(ColorA(color(), FloatType(1) - transp));
        primitive.setMesh(path.lastAs<TriangleMesh>());
        primitive.setCullFaces(backfaceCulling());

        // Submit primitive to the renderer.
        renderer->beginPickObject(pipeline);
        renderer->renderMesh(primitive);
        renderer->endPickObject();
    }
    else {
        // Add mesh to bounding box.
        TimeInterval validityInterval;
        renderer->addToLocalBoundingBox(boundingBox(time, path, pipeline, flowState, renderer->visCache(), validityInterval));
    }

    return {};
}

}   // End of namespace
