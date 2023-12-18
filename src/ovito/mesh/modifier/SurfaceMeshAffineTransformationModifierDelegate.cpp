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

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "SurfaceMeshAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshAffineTransformationModifierDelegate);

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus SurfaceMeshAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* mod = static_object_cast<AffineTransformationModifier>(request.modifier());

    for(const DataObject* obj : state.data()->objects()) {
        // Process SurfaceMesh objects.
        if(const SurfaceMesh* existingSurface = dynamic_object_cast<SurfaceMesh>(obj)) {

            // Make sure the input mesh data structure is valid.
            existingSurface->verifyMeshIntegrity();

            // Create a copy of the SurfaceMesh.
            SurfaceMesh* newSurface = state.makeMutable(existingSurface);
            // Create a copy of the vertices sub-object (no need to copy the topology when only moving vertices).
            SurfaceMeshVertices* newVertices = newSurface->makeVerticesMutable();

            // Get the input vertex coordinates (as strong reference to force creation of a mutable clone below).
            ConstPropertyPtr inputPositionProperty = newVertices->expectProperty(SurfaceMeshVertices::PositionProperty);

            // Create an uninitialized copy of the vertex position property.
            Property* outputPositionProperty = newVertices->makePropertyMutable(inputPositionProperty, DataBuffer::Uninitialized);

            // Let the modifier do the actual coordinate transformation work.
            mod->transformCoordinates(inputState, inputPositionProperty, outputPositionProperty, newVertices->getProperty(SurfaceMeshVertices::SelectionProperty));

            // Apply transformation to the cutting planes attached to the surface mesh.
            if(!newSurface->cuttingPlanes().empty()) {
                const AffineTransformation tm = mod->effectiveAffineTransformation(inputState);
                QVector<Plane3> cuttingPlanes = newSurface->cuttingPlanes();
                for(Plane3& plane : cuttingPlanes)
                    plane = tm * plane;
                newSurface->setCuttingPlanes(std::move(cuttingPlanes));
            }
        }
        // Process TriangleMesh objects.
        else if(const TriangleMesh* existingMeshObj = dynamic_object_cast<TriangleMesh>(obj)) {
            const AffineTransformation tm = mod->effectiveAffineTransformation(inputState);

            // Create a copy of the TriangleMesh.
            TriangleMesh* newMeshObj = state.makeMutable(existingMeshObj);

            // Apply transformation to the vertices coordinates.
            for(Point3& p : newMeshObj->vertices())
                p = tm * p;
            newMeshObj->invalidateVertices();

            // Apply transformation to the normal vectors.
            if(newMeshObj->hasNormals()) {
                const auto& tm_g = tm.toDataType<GraphicsFloatType>();
                for(auto& n : newMeshObj->normals())
                    n = tm_g * n;
            }
        }
    }

    return PipelineStatus::Success;
}

}   // End of namespace
