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

#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include "SurfaceMeshDeleteSelectedModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshRegionsDeleteSelectedModifierDelegate);

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshRegionsDeleteSelectedModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshRegions::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus SurfaceMeshRegionsDeleteSelectedModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    size_t numRegions = 0;
    size_t numDeleted = 0;

    for(const DataObject* obj : state.data()->objects()) {
        if(const SurfaceMesh* existingSurface = dynamic_object_cast<SurfaceMesh>(obj)) {
            // Make sure the input mesh data structure is valid.
            existingSurface->verifyMeshIntegrity();

            // Count total number of input regions.
            numRegions += existingSurface->regions()->elementCount();

            // Check if there is a region selection set.
            BufferReadAccessAndRef<SelectionIntType> regionMask = existingSurface->regions()->getProperty(SurfaceMeshRegions::SelectionProperty);
            if(!regionMask)
                continue; // Nothing to do if there is no selection.

            // Mesh faces must have the "Region" property.
            if(!existingSurface->faces()->getProperty(SurfaceMeshFaces::RegionProperty))
                continue; // Nothing to do if there is no face region information.

            // Check if at least one mesh region is currently selected.
            size_t selectionCount = regionMask.buffer()->nonzeroCount();
            if(selectionCount == 0)
                continue;

            // Count total number of regions being deleted.
            numDeleted += selectionCount;

            // Create a mutable copy of the SurfaceMesh.
            SurfaceMesh* mutableSurface = state.makeMutable(existingSurface);

            // Create a working data structure for modifying the mesh.
            SurfaceMeshBuilder mesh(mutableSurface);

            // Remove selection property from the regions.
            mesh.removeRegionProperty(SurfaceMeshRegions::SelectionProperty);

            // Get access to the per-face region information.
            BufferReadAccess<int32_t> regionProperty = mesh.expectFaceProperty(SurfaceMeshFaces::RegionProperty);

            // Delete all faces that belong to one of the selected mesh regions.
            BufferFactory<SelectionIntType> faceMask(mesh.faceCount());
            for(SurfaceMesh::face_index face : mesh.facesRange()) {
                SurfaceMesh::region_index region = regionProperty[face];
                faceMask[face] = (region >= 0 && region < regionMask.size() && regionMask[region]);
            }
            regionProperty.reset();

            // Delete the selected faces and regions.
            mesh.deleteFaces(faceMask.take());
            mesh.deleteRegions(regionMask.take());
        }
    }

    // Report some statistics:
    QString statusMessage = tr("%1 of %2 regions deleted (%3%)")
        .arg(numDeleted)
        .arg(numRegions)
        .arg((FloatType)numDeleted * 100 / std::max(numRegions, (size_t)1), 0, 'f', 1);

    return PipelineStatus(PipelineStatus::Success, std::move(statusMessage));
}

}   // End of namespace
