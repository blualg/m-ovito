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

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/modifier/CreateIsosurfaceModifier.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include "VoxelGridSliceModifierDelegate.h"
#include "MarchingCubes.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridSliceModifierDelegate);
OVITO_CLASSINFO(VoxelGridSliceModifierDelegate, "DisplayName", "Voxel grids");
DEFINE_REFERENCE_FIELD(VoxelGridSliceModifierDelegate, surfaceMeshVis);

/******************************************************************************
* Constructor.
******************************************************************************/
void VoxelGridSliceModifierDelegate::initializeObject(ObjectInitializationFlags flags)
{
    SliceModifierDelegate::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the vis element for rendering the mesh.
        setSurfaceMeshVis(OORef<SurfaceMeshVis>::create(flags));
        surfaceMeshVis()->setShowCap(false);
        surfaceMeshVis()->setHighlightEdges(false);
        surfaceMeshVis()->setSmoothShading(false);
        surfaceMeshVis()->setSurfaceIsClosed(false);
        if(ExecutionContext::isInteractive())
            surfaceMeshVis()->setColorMappingMode(SurfaceMeshVis::VertexPseudoColoring);
        surfaceMeshVis()->setObjectTitle(tr("Volume slice"));
    }
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> VoxelGridSliceModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    SliceModifier* modifier = static_object_cast<SliceModifier>(request.modifier());

    // Obtain modifier parameter values.
    Plane3 plane;
    FloatType sliceWidth;
    std::tie(plane, sliceWidth) = modifier->slicingPlane(request.time(), state.mutableStateValidity(), state);

    // The actual work can be performed in a separate thread.
    return asyncLaunch([
            state = std::move(state),
            plane,
            createdByNode = request.modificationNodeWeak(),
            surfaceMeshVis = OORef<SurfaceMeshVis>(surfaceMeshVis())]() mutable {

        for(qsizetype i = 0; i < state.data()->objects().size(); i++) {
            if(const VoxelGrid* voxelGrid = dynamic_object_cast<VoxelGrid>(state.data()->objects()[i])) {
                // Verify consistency of input property container.
                voxelGrid->verifyIntegrity();

                // Get the simulation cell.
                DataOORef<const SimulationCell> cell = voxelGrid->domain();
                OVITO_ASSERT(cell);
                if(cell->is2D())
                    continue;

                // The slice plane does NOT exist in a periodic domain.
                // Remove any periodic boundary conditions from the surface mesh domain cell.
                if(cell->hasPbc()) {
                    DataOORef<SimulationCell> nonperiodicCell = cell.makeCopy();
                    nonperiodicCell->setPbcFlags(false, false, false);
                    cell = std::move(nonperiodicCell);
                }

                // Create an empty surface mesh object.
                SurfaceMesh* meshObj = state.createObjectWithVis<SurfaceMesh>(QStringLiteral("volume-slice"), createdByNode, surfaceMeshVis, tr("Volume slice"));
                meshObj->setDomain(cell);

                // Construct cross section mesh using a special version of the marching cubes algorithm.
                SurfaceMeshBuilder mesh(meshObj);

                // The level of subdivision.
                static constexpr int resolution = 2;

                // Get domain of voxel grid.
                VoxelGrid::GridDimensions gridShape = voxelGrid->shape();
                // Adjust for non-periodic point-based grids.
                if(voxelGrid->gridType() == VoxelGrid::GridType::PointData) {
                    for(size_t dim = 0; dim < 3; dim++) {
                        if(!voxelGrid->domain()->hasPbcCorrected(dim) && gridShape[dim] >= 2)
                            gridShape[dim]--;
                    }
                }

                // Indicates for each generated mesh face which voxel grid cell it is located in.
                std::vector<std::tuple<int,int,int>> meshFaceVoxelCoordinates;

                // Transform plane from world space to orthogonal grid space.
                Plane3 planeGridSpace = (Matrix3(
                    gridShape[0]*resolution, 0, 0,
                    0, gridShape[1]*resolution, 0,
                    0, 0, gridShape[2]*resolution) * cell->inverseMatrix()) * plane;

                // Set up callback function returning the field value, which will be passed to the marching cubes algorithm.
                auto getFieldValue = [&](int i, int j, int k) {
                    return planeGridSpace.pointDistance(Point3(i,j,k));
                };

                // Run the marching cubes algorithm to generate the mesh for the cross-section.
                {
                    MarchingCubes mc(mesh, gridShape[0]*resolution, gridShape[1]*resolution, gridShape[2]*resolution, false, std::move(getFieldValue), true, true);
                    mc.generateIsosurface(0.0);
                    this_task::throwIfCanceled();

                    // Take output data.
                    if(meshFaceVoxelCoordinates.empty())
                        meshFaceVoxelCoordinates = mc.takeMeshFaceVoxelCoordinates();
                    else
                        meshFaceVoxelCoordinates.insert(meshFaceVoxelCoordinates.end(), mc.meshFaceVoxelCoordinates().begin(), mc.meshFaceVoxelCoordinates().end());
                }

                // Create a manifold by connecting adjacent faces.
                mesh.connectOppositeHalfedges();
                this_task::throwIfCanceled();

                // Collect the set of voxel grid properties that should be transferred over to the isosurface mesh vertices and faces.
                std::vector<ConstPropertyPtr> fieldProperties;
                for(const Property* property : voxelGrid->properties())
                    fieldProperties.push_back(property);

                if(!fieldProperties.empty()) {
                    // Determine the mapping of mesh faces to voxel grid cells.
                    OVITO_ASSERT(mesh.faceCount() == meshFaceVoxelCoordinates.size());
                    std::vector<size_t> voxelFaceMapping(meshFaceVoxelCoordinates.size());
#ifndef Q_CC_MSVC
                    std::transform(meshFaceVoxelCoordinates.cbegin(), meshFaceVoxelCoordinates.cend(), voxelFaceMapping.begin(), [&, shape=voxelGrid->shape()](const auto& coords) {
                        const auto& [x, y, z] = coords;
                        OVITO_ASSERT(x >= 0 && y >= 0 && z >= 0);
                        return std::min((size_t)z / resolution, shape[2]-1) * (shape[0] * shape[1]) + std::min((size_t)y / resolution, shape[1]-1) * shape[0] + std::min((size_t)x / resolution, shape[0]-1);
                    });
#else // Workaround for MSVC compiler crash ("fatal error C1001: Internal compiler error. 10914(compiler file 'msc1.cpp', line 1603)"):
                    {
                        auto m = voxelFaceMapping.begin();
                        const auto shape = voxelGrid->shape();
                        for(const auto& coords : meshFaceVoxelCoordinates) {
                            const auto& [x, y, z] = coords;
                            OVITO_ASSERT(x >= 0 && y >= 0 && z >= 0);
                            size_t xs = std::min((size_t)x / resolution, shape[0]-1);
                            size_t ys = std::min((size_t)y / resolution, shape[1]-1);
                            size_t zs = std::min((size_t)z / resolution, shape[2]-1);
                            *m++ = zs * (shape[0] * shape[1]) + ys * shape[0] + xs;
                        }
                        OVITO_ASSERT(m == voxelFaceMapping.end());
                    }
#endif
                    this_task::throwIfCanceled();

                    // Copy field values from voxel grid to surface mesh faces.
                    for(const ConstPropertyPtr& fieldProperty : fieldProperties) {
                        Property* faceProperty;
                        if(SurfaceMeshFaces::OOClass().isValidStandardPropertyId(fieldProperty->typeId())) {
                            // Input voxel property is also a standard property for mesh faces.
                            faceProperty = mesh.createFaceProperty(DataBuffer::Uninitialized, static_cast<SurfaceMeshFaces::Type>(fieldProperty->typeId()));
                            OVITO_ASSERT(faceProperty->dataType() == fieldProperty->dataType());
                            OVITO_ASSERT(faceProperty->stride() == fieldProperty->stride());
                        }
                        else if(SurfaceMeshFaces::OOClass().standardPropertyTypeId(fieldProperty->name()) != 0) {
                            // Input property name is that of a standard property for mesh faces.
                            // Must rename the property to avoid conflict, because user properties may not have a standard property name.
                            QString newPropertyName = fieldProperty->name() + tr("_field");
                            faceProperty = mesh.createFaceProperty(DataBuffer::Uninitialized, newPropertyName, fieldProperty->dataType(), fieldProperty->componentCount(), fieldProperty->componentNames());
                        }
                        else {
                            // Input property is a user property for mesh faces.
                            faceProperty = mesh.createFaceProperty(DataBuffer::Uninitialized, fieldProperty->name(), fieldProperty->dataType(), fieldProperty->componentCount(), fieldProperty->componentNames());
                        }
                        // Copy property values from voxel cells over to mesh faces.
                        fieldProperty->mappedCopyTo(*faceProperty, voxelFaceMapping);
                        this_task::throwIfCanceled();
                    }
                }

                // Form quadrilaterals from pairs of triangles.
                // This only makes sense when the slicing plane is aligned with the grid cells axes such that only quads result from
                // the marching cubes algorithm.
                if(std::abs(planeGridSpace.normal.x()) <= FLOATTYPE_MAX || std::abs(planeGridSpace.normal.y()) <= FLOATTYPE_MAX || std::abs(planeGridSpace.normal.z()) <= FLOATTYPE_MAX) {
                    mesh.makeQuadrilateralFaces();
                    this_task::throwIfCanceled();
                }

                // Deletes all vertices from the mesh which are not connected to any half-edge.
                mesh.deleteIsolatedVertices();
                this_task::throwIfCanceled();

                // Transform from double resolution grid to single resolution.
                mesh.transformVertices(AffineTransformation(
                    FloatType(1) / resolution, 0, 0, -0.5 + FloatType(1) / resolution,
                    0, FloatType(1) / resolution, 0, -0.5 + FloatType(1) / resolution,
                    0, 0, FloatType(1) / resolution, -0.5 + FloatType(1) / resolution));
                this_task::throwIfCanceled();

                // Copy field values from voxel grid to surface mesh vertices.
                CreateIsosurfaceModifier::transferPropertiesFromGridToMesh(mesh, fieldProperties, *voxelGrid->domain(), voxelGrid->shape(), voxelGrid->gridType());
                this_task::throwIfCanceled();

                // Transform mesh vertices from orthogonal grid space to world space.
                const AffineTransformation tm = cell->matrix() * Matrix3(
                    FloatType(1) / gridShape[0], 0, 0,
                    0, FloatType(1) / gridShape[1], 0,
                    0, 0, FloatType(1) / gridShape[2]) *
                    AffineTransformation::translation(Vector3(0.5, 0.5, 0.5));
                mesh.transformVertices(tm);
                this_task::throwIfCanceled();

                // Flip surface orientation if cell matrix is a mirror transformation.
                if(tm.determinant() < 0) {
                    mesh.flipFaces();
                    this_task::throwIfCanceled();
                }
            }
        }

        return std::move(state);
    });
}

}   // End of namespace
