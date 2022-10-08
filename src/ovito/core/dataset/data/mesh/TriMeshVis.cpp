////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/data/mesh/TriMeshObject.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "TriMeshVis.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(TriMeshVis);
DEFINE_PROPERTY_FIELD(TriMeshVis, color);
DEFINE_REFERENCE_FIELD(TriMeshVis, transparencyController);
DEFINE_PROPERTY_FIELD(TriMeshVis, highlightEdges);
DEFINE_PROPERTY_FIELD(TriMeshVis, backfaceCulling);
SET_PROPERTY_FIELD_LABEL(TriMeshVis, color, "Display color");
SET_PROPERTY_FIELD_LABEL(TriMeshVis, transparencyController, "Transparency");
SET_PROPERTY_FIELD_LABEL(TriMeshVis, highlightEdges, "Highlight edges");
SET_PROPERTY_FIELD_LABEL(TriMeshVis, backfaceCulling, "Back-face culling");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(TriMeshVis, transparencyController, PercentParameterUnit, 0, 1);

/******************************************************************************
* Constructor.
******************************************************************************/
TriMeshVis::TriMeshVis(ObjectCreationParams params) : DataVis(params),
	_color(0.85, 0.85, 1),
	_highlightEdges(false),
	_backfaceCulling(false)
{
	if(params.createSubObjects()) {
		setTransparencyController(ControllerManager::createFloatController(dataset()));
	}
}

/******************************************************************************
* Computes the bounding box of the object.
******************************************************************************/
Box3 TriMeshVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
	// Compute bounding box.
	if(const TriMeshObject* triMeshObj = path.lastAs<TriMeshObject>()) {
		return triMeshObj->boundingBox();
	}
	return Box3();
}

/******************************************************************************
* Lets the vis element render a data object.
******************************************************************************/
PipelineStatus TriMeshVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
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
		primitive.setMesh(path.lastAs<TriMeshObject>());
		primitive.setCullFaces(backfaceCulling());

		// Submit primitive to the renderer.
		renderer->beginPickObject(contextNode);
		renderer->renderMesh(primitive);
		renderer->endPickObject();
	}
	else {
		// Add mesh to bounding box.
		TimeInterval validityInterval;
		renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, validityInterval));
	}

	return {};
}

}	// End of namespace
