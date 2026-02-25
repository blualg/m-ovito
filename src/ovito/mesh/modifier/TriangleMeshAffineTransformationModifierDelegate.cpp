////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "TriangleMeshAffineTransformationModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TriangleMeshAffineTransformationModifierDelegate);
OVITO_CLASSINFO(TriangleMeshAffineTransformationModifierDelegate, "DisplayName", "Triangle meshes");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> TriangleMeshAffineTransformationModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all triangle meshes in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(TriangleMesh::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> TriangleMeshAffineTransformationModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    AffineTransformationModifier* modifier = static_object_cast<AffineTransformationModifier>(request.modifier());

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            tm = modifier->effectiveAffineTransformation(originalState),
            createdByNode = request.modificationNodeWeak(),
            inputObjectRef = inputDataObject()]() mutable {

        // Process TriangleMesh objects.
        visitObjectsToBeProcessed<TriangleMesh>(state, inputObjectRef, createdByNode, [&](const TriangleMesh* existingMeshObj) {
            // Create a copy of the TriangleMesh.
            TriangleMesh* newMeshObj = state.makeMutable(existingMeshObj);

            // Apply transformation to the vertices coordinates.
            for(Point3& p : newMeshObj->vertices())
                p = tm * p;
            newMeshObj->invalidateVertices();

            // Apply inverse transpose of the linear part to transform normal vectors correctly,
            // ensuring they remain perpendicular to the surface after non-uniform scaling or shear.
            if(newMeshObj->hasNormals()) {
                Matrix3 inv_tm;
                if(tm.linear().inverse(inv_tm)) {
                    const auto normalMatrix = inv_tm.transposed().toDataType<GraphicsFloatType>();
                    for(auto& n : newMeshObj->normals()) {
                        n = (normalMatrix * n).safelyNormalized();
                    }
                }
            }

            this_task::throwIfCanceled();
        });

        return std::move(state);
    });
}

}   // End of namespace
