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
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include "VoxelGridVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridVis);
OVITO_CLASSINFO(VoxelGridVis, "DisplayName", "Voxel grid");
DEFINE_REFERENCE_FIELD(VoxelGridVis, transparencyController);
DEFINE_PROPERTY_FIELD(VoxelGridVis, highlightGridLines);
DEFINE_PROPERTY_FIELD(VoxelGridVis, interpolateColors);
DEFINE_REFERENCE_FIELD(VoxelGridVis, colorMapping);
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, transparencyController, "Surface transparency");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, highlightGridLines, "Show grid lines");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, interpolateColors, "Color interpolation");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, colorMapping, "Color mapping");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(VoxelGridVis, transparencyController, PercentParameterUnit, 0, 1);

IMPLEMENT_ABSTRACT_OVITO_CLASS(VoxelGridPickInfo);

/******************************************************************************
* Constructor.
******************************************************************************/
void VoxelGridVis::initializeObject(ObjectInitializationFlags flags)
{
    DataVis::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create animation controller for the transparency parameter.
        setTransparencyController(ControllerManager::createFloatController());

        // Create a color mapping object for pseudo-color visualization of a grid property.
        setColorMapping(OORef<PropertyColorMapping>::create(flags));
    }
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void VoxelGridVis::loadFromStreamComplete(ObjectLoadStream& stream)
{
    DataVis::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.5.4.
    // Create a color mapping sub-object if it wasn't loaded from the state file.
    if(!colorMapping()) {
        // Create a color mapping object for pseudo-color visualization of a grid property.
        setColorMapping(OORef<PropertyColorMapping>::create());
    }
}

/******************************************************************************
* Computes the bounding box of the displayed data.
******************************************************************************/
Box3 VoxelGridVis::boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval)
{
    if(const VoxelGrid* gridObj = path.lastAs<VoxelGrid>()) {
        if(gridObj->domain()) {
            AffineTransformation matrix = gridObj->domain()->cellMatrix();
            if(gridObj->domain()->is2D()) {
                matrix.column(2).setZero();
            }
            return Box3(Point3(0), Point3(1)).transformed(matrix);
        }
    }
    return {};
}

/******************************************************************************
* Lets the visualization element render the data object.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> VoxelGridVis::render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode)
{
    PipelineStatus status;

    // Get the grid object being rendered.
    const VoxelGrid* gridObj = path.lastAs<VoxelGrid>();
    if(!gridObj)
        return status;

    // Throws an exception if the input data structure is corrupt.
    gridObj->verifyIntegrity();

    // Look for 'Color' voxel property.
    const Property* colorProperty = gridObj->getProperty(VoxelGrid::ColorProperty);
    BufferReadAccess<ColorG> colorArray(colorProperty);

    // Look for selected pseudo-coloring property.
    const Property* pseudoColorProperty = nullptr;
    int pseudoColorPropertyComponent = 0;
    if(!colorProperty && colorMapping() && colorMapping()->sourceProperty()) {
        QString errorDescription;
        std::tie(pseudoColorProperty, pseudoColorPropertyComponent) = colorMapping()->sourceProperty().findInContainerWithComponent(gridObj, errorDescription);
        if(!pseudoColorProperty) {
            status = PipelineStatus(PipelineStatus::Error, std::move(errorDescription));
        }
    }
    RawBufferReadAccess pseudoColorArray(pseudoColorProperty);
    OVITO_ASSERT(!(colorArray && pseudoColorArray));

    // The key type used for caching the geometry primitive:
    using CacheKey = RendererResourceKey<struct VoxelGridSurface,
        ConstDataObjectRef,         // Voxel grid object
        ConstDataObjectRef,         // Color property
        ConstDataObjectRef,         // Pseudo-color property
        int,                        // Pseudo-color vector component
        FloatType,                  // Transparency
        bool,                       // Grid line highlighting
        bool                        // Interpolate colors
    >;

    // The values stored in the vis cache.
    struct CacheValue {
        MeshPrimitive volumeFaces;
        OORef<ObjectPickInfo> pickInfo;
        Box3 boundingBox;
    };

    // Determine the opacity value for rendering the mesh.
    FloatType transp = 0;
    TimeInterval iv;
    if(transparencyController()) {
        transp = transparencyController()->getFloatValue(frameGraph.time(), iv);
        if(transp >= 1.0) return status;
    }

    // Look up the rendering primitive in the vis cache.
    const auto& [volumeFaces, pickInfo, boundingBox] = frameGraph.visCache().lookup<std::tuple<MeshPrimitive, OORef<ObjectPickInfo>, Box3>>(
        CacheKey{
            gridObj,
            colorProperty,
            pseudoColorProperty,
            pseudoColorPropertyComponent,
            transp,
            highlightGridLines(),
            interpolateColors()
        },
        [&](MeshPrimitive& volumeFaces, OORef<ObjectPickInfo>& pickInfo, Box3& boundingBox) {
            GraphicsFloatType alpha = GraphicsFloatType(1) - transp;
            if(gridObj->domain() && gridObj->elementCount() != 0) {
                // Determine the number of triangle faces to be created per voxel cell.
                size_t trianglesPerCell = 2;
                if(interpolateColors() && gridObj->gridType() == VoxelGrid::GridType::CellData) {
                    if(colorArray || pseudoColorArray)
                        trianglesPerCell = 8;
                }

                DataOORef<TriangleMesh> mesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
                if(colorArray) {
                    if(interpolateColors()) mesh->setHasVertexColors(true);
                    else mesh->setHasFaceColors(true);
                }
                else if(pseudoColorArray) {
                    if(interpolateColors()) mesh->setHasVertexPseudoColors(true);
                    else mesh->setHasFacePseudoColors(true);
                }
                VoxelGrid::GridDimensions gridDims = gridObj->shape();
                std::array<bool, 3> pbcFlags = gridObj->domain()->pbcFlags();

                // Number of visible grid lines in each grid direction.
                std::array<int, 3> numLines;
                for(size_t dim = 0; dim < 3; dim++)
                    numLines[dim] = std::max(2, (int)gridDims[dim] + (gridObj->gridType() == VoxelGrid::GridType::CellData || pbcFlags[dim] ? 1 : 0));

                // Number of visible cells in each grid direction.
                std::array<int, 3> numCells;
                for(size_t dim = 0; dim < 3; dim++)
                    numCells[dim] = numLines[dim] - 1;

                // Create viewport picking object.
                pickInfo = OORef<VoxelGridPickInfo>::create(this, gridObj, numCells, trianglesPerCell);

                // Helper function that creates the mesh vertices and faces for one side of the grid volume.
                auto createFacesForSide = [&](size_t dim1, size_t dim2, size_t dim3, bool oppositeSide) {

                    // Number of grid lines in the two current directions:
                    int nlx = numLines[dim1];
                    int nly = numLines[dim2];

                    // Number of voxels in the two grid directions:
                    int nvx = numCells[dim1];
                    int nvy = numCells[dim2];

                    // Edge vectors of one voxel face:
                    Vector3 dx = gridObj->domain()->cellMatrix().column(dim1) / nvx;
                    Vector3 dy = gridObj->domain()->cellMatrix().column(dim2) / nvy;

                    // Will store the xyz voxel grid coordinates.
                    // The coordinate in the 3rd direction is a constant, which is precomputed here.
                    size_t coords[3];
                    coords[dim3] = oppositeSide ? (gridDims[dim3] - 1) : 0;
                    // Voxel grid coordinate on the opposite side of the domain:
                    size_t coords_wrap[3];
                    coords_wrap[dim3] = oppositeSide ? 0 : (gridDims[dim3] - 1);

                    if(gridObj->gridType() == VoxelGrid::GridType::PointData && interpolateColors() && pbcFlags[dim3])
                        coords[dim3] = coords_wrap[dim3] = 0;

                    // The origin of the grid face in world space.
                    Point3 origin = Point3::Origin() + gridObj->domain()->cellMatrix().translation();
                    if(oppositeSide) origin += gridObj->domain()->cellMatrix().column(dim3);

                    auto baseVertexCount = mesh->vertexCount();
                    auto baseFaceCount = mesh->faceCount();

                    if(!interpolateColors() || gridObj->gridType() == VoxelGrid::GridType::PointData || (!colorArray && !pseudoColorArray)) {
                        OVITO_ASSERT(trianglesPerCell == 2);

                        // Create two triangles per voxel face.
                        mesh->setVertexCount(baseVertexCount + nlx * nly);
                        mesh->setFaceCount(baseFaceCount + 2 * nvx * nvy);

                        // Create vertices.
                        auto vertex = mesh->vertices().begin() + baseVertexCount;
                        ColorAG* vertexColor = mesh->hasVertexColors() ? mesh->vertexColors().data() + baseVertexCount : nullptr;
                        FloatType* vertexPseudoColor = mesh->hasVertexPseudoColors() ? mesh->vertexPseudoColors().data() + baseVertexCount : nullptr;
                        for(int iy = 0; iy < nly; iy++) {
                            for(int ix = 0; ix < nlx; ix++) {
                                *vertex++ = origin + (ix * dx) + (iy * dy);
                                if(vertexColor || vertexPseudoColor) {
                                    coords[dim1] = ix;
                                    coords[dim2] = iy;
                                    if(coords[dim1] >= gridDims[dim1]) {
                                        if(pbcFlags[dim1]) coords[dim1] = 0;
                                        else coords[dim1] = gridDims[dim1]-1;
                                    }
                                    if(coords[dim2] >= gridDims[dim2]) {
                                        if(pbcFlags[dim2]) coords[dim2] = 0;
                                        else coords[dim2] = gridDims[dim2]-1;
                                    }
                                    if(vertexColor) {
                                        const ColorG& c = colorArray[gridObj->voxelIndex(coords[0], coords[1], coords[2])];
                                        *vertexColor++ = ColorAG(c, alpha);
                                    }
                                    else {
                                        *vertexPseudoColor++ = pseudoColorArray.get<FloatType>(gridObj->voxelIndex(coords[0], coords[1], coords[2]), pseudoColorPropertyComponent);
                                    }
                                }
                            }
                        }
                        OVITO_ASSERT(vertex == mesh->vertices().end());

                        // Create triangles.
                        auto face = mesh->faces().begin() + baseFaceCount;
                        ColorAG* faceColor = mesh->hasFaceColors() ? mesh->faceColors().data() + baseFaceCount : nullptr;
                        FloatType* facePseudoColor = mesh->hasFacePseudoColors() ? mesh->facePseudoColors().data() + baseFaceCount : nullptr;
                        for(int iy = 0; iy < nvy; iy++) {
                            for(int ix = 0; ix < nvx; ix++) {
                                face->setVertices(baseVertexCount + iy * nlx + ix, baseVertexCount + iy * nlx + ix + 1, baseVertexCount + (iy+1) * nlx + ix + 1);
                                face->setEdgeVisibility(true, true, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * nlx + ix, baseVertexCount + (iy+1) * nlx + ix + 1, baseVertexCount + (iy+1) * nlx + ix);
                                face->setEdgeVisibility(false, true, true);
                                ++face;
                                if(faceColor) {
                                    coords[dim1] = ix;
                                    coords[dim2] = iy;
                                    const ColorG& c = colorArray[gridObj->voxelIndex(coords[0], coords[1], coords[2])];
                                    *faceColor++ = ColorAG(c, alpha);
                                    *faceColor++ = ColorAG(c, alpha);
                                }
                                if(facePseudoColor) {
                                    coords[dim1] = ix;
                                    coords[dim2] = iy;
                                    FloatType c = pseudoColorArray.get<FloatType>(gridObj->voxelIndex(coords[0], coords[1], coords[2]), pseudoColorPropertyComponent);
                                    *facePseudoColor++ = c;
                                    *facePseudoColor++ = c;
                                }
                            }
                        }
                        OVITO_ASSERT(face == mesh->faces().end());
                    }
                    else if(pseudoColorArray) {
                        OVITO_ASSERT(trianglesPerCell == 8);
                        int verts_per_voxel = 4;
                        int verts_per_row = verts_per_voxel * nvx + 2;

                        // Generate 8 triangles per voxel cell face.
                        mesh->setVertexCount(baseVertexCount + verts_per_row * nvy + nvx * 2 + 1);
                        mesh->setFaceCount(baseFaceCount + trianglesPerCell * nvx * nvy);

                        // Create vertices.
                        auto vertex = mesh->vertices().begin() + baseVertexCount;
                        for(int iy = 0; iy < nly; iy++) {
                            for(int ix = 0; ix < nlx; ix++) {
                                // Create four vertices per voxel face.
                                Point3 corner = origin + (ix * dx) + (iy * dy);
                                *vertex++ = corner;
                                if(ix < nvx)
                                    *vertex++ = corner + FloatType(0.5) * dx;
                                if(iy < nvy)
                                    *vertex++ = corner + FloatType(0.5) * dy;
                                if(ix < nvx && iy < nvy)
                                    *vertex++ = corner + FloatType(0.5) * (dx + dy);
                            }
                        }
                        OVITO_ASSERT(vertex == mesh->vertices().end());

                        // Compute pseudo-color of vertices located in the center of voxel faces.
                        FloatType* vertexColor = mesh->vertexPseudoColors().data() + baseVertexCount;
                        for(int iy = 0; iy < nvy; iy++, vertexColor += 2) {
                            for(int ix = 0; ix < nvx; ix++, vertexColor += 4) {
                                coords[dim1] = ix;
                                coords[dim2] = iy;
                                FloatType c1 = pseudoColorArray.get<FloatType>(gridObj->voxelIndex(coords[0], coords[1], coords[2]), pseudoColorPropertyComponent);
                                if(pbcFlags[dim3]) {
                                    // Blend two colors if the grid is periodic.
                                    coords_wrap[dim1] = ix;
                                    coords_wrap[dim2] = iy;
                                    FloatType c2 = pseudoColorArray.get<FloatType>(gridObj->voxelIndex(coords_wrap[0], coords_wrap[1], coords_wrap[2]), pseudoColorPropertyComponent);
                                    vertexColor[3] = FloatType(0.5) * (c1 + c2);
                                }
                                else {
                                    vertexColor[3] = c1;
                                }
                            }
                        }

                        // Compute color of vertices located on the horizontal grid lines of the voxel grid.
                        vertexColor = mesh->vertexPseudoColors().data() + baseVertexCount;
                        if(!pbcFlags[dim2]) {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[ix * verts_per_voxel + 1] = vertexColor[ix * verts_per_voxel + 3];
                        }
                        else {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[ix * verts_per_voxel + 3] + vertexColor[(nvy - 1) * verts_per_row + ix * verts_per_voxel + 3]);
                        }
                        for(int iy = 1; iy < nvy; iy++) {
                            for(int ix = 0; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[(iy-1) * verts_per_row + ix * verts_per_voxel + 3]);
                            }
                        }
                        if(!pbcFlags[dim2]) {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[nvy * verts_per_row + ix * 2 + 1] = vertexColor[(nvy - 1) * verts_per_row + ix * verts_per_voxel + 3];
                        }
                        else {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[nvy * verts_per_row + ix * 2 + 1] = vertexColor[ix * verts_per_voxel + 1];
                        }

                        // Compute color of vertices located on the vertical grid lines of the voxel grid.
                        if(!pbcFlags[dim1]) {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + 2] = vertexColor[iy * verts_per_row + 3];
                        }
                        else {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 3] + vertexColor[(nvx - 1) * verts_per_voxel + iy * verts_per_row + 3]);
                        }
                        for(int iy = 0; iy < nvy; iy++) {
                            for(int ix = 1; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 3]);
                            }
                        }
                        if(!pbcFlags[dim1]) {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 3];
                        }
                        else {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + 2];
                        }

                        // Compute color of vertices located on the grid line intersections.
                        for(int iy = 0; iy < nvy; iy++) {
                            if(!pbcFlags[dim1])
                                vertexColor[iy * verts_per_row] = vertexColor[iy * verts_per_row + 1];
                            else
                                vertexColor[iy * verts_per_row] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 1] + vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 1]);
                            for(int ix = 1; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 1]);
                            }
                            if(!pbcFlags[dim1])
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel] = vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 1];
                            else
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel] = vertexColor[iy * verts_per_row];
                        }
                        if(!pbcFlags[dim1])
                            vertexColor[nvy * verts_per_row] = vertexColor[nvy * verts_per_row + 1];
                        else
                            vertexColor[nvy * verts_per_row] = FloatType(0.5) * (vertexColor[nvy * verts_per_row + 1] + vertexColor[nvy * verts_per_row + (nvx - 1) * 2 + 1]);
                        for(int ix = 1; ix < nvx; ix++) {
                            vertexColor[nvy * verts_per_row + ix * 2] = FloatType(0.5) * (vertexColor[nvy * verts_per_row + ix * 2 + 1] + vertexColor[nvy * verts_per_row + (ix - 1) * 2 + 1]);
                        }
                        if(!pbcFlags[dim1])
                            vertexColor[nvy * verts_per_row + nvx * 2] = vertexColor[nvy * verts_per_row + (nvx - 1) * 2 + 1];
                        else
                            vertexColor[nvy * verts_per_row + nvx * 2] = vertexColor[nvy * verts_per_row];

                        // Create triangles.
                        auto face = mesh->faces().begin() + baseFaceCount;
                        for(int iy = 0; iy < nvy; iy++) {
                            for(int ix = 0; ix < nvx; ix++) {
                                bool is_x_border = (ix == nvx - 1);
                                bool is_y_border = (iy == nvy - 1);
                                int centerVertex = baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 3;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel, baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 1, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 1, baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel, baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel + (is_x_border ? 1 : 2), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel + (is_x_border ? 1 : 2), baseVertexCount + (iy+1) * verts_per_row + (ix+1) * (is_y_border ? 2 : verts_per_voxel), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + (ix+1) * (is_y_border ? 2 : verts_per_voxel), baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel) + 1, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel) + 1, baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel), baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 2, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 2, baseVertexCount + iy * verts_per_row + ix * verts_per_voxel, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                            }
                        }
                        OVITO_ASSERT(face == mesh->faces().end());
                    }
                    else {
                        OVITO_ASSERT(trianglesPerCell == 8);
                        int verts_per_voxel = 4;
                        int verts_per_row = verts_per_voxel * nvx + 2;

                        // Generate 8 triangles per voxel cell face.
                        mesh->setVertexCount(baseVertexCount + verts_per_row * nvy + nvx * 2 + 1);
                        mesh->setFaceCount(baseFaceCount + trianglesPerCell * nvx * nvy);

                        // Create vertices.
                        auto vertex = mesh->vertices().begin() + baseVertexCount;
                        for(int iy = 0; iy < nly; iy++) {
                            for(int ix = 0; ix < nlx; ix++) {
                                // Create four vertices per voxel face.
                                Point3 corner = origin + (ix * dx) + (iy * dy);
                                *vertex++ = corner;
                                if(ix < nvx)
                                    *vertex++ = corner + FloatType(0.5) * dx;
                                if(iy < nvy)
                                    *vertex++ = corner + FloatType(0.5) * dy;
                                if(ix < nvx && iy < nvy)
                                    *vertex++ = corner + FloatType(0.5) * (dx + dy);
                            }
                        }
                        OVITO_ASSERT(vertex == mesh->vertices().end());

                        // Compute color of vertices located in the center of voxel faces.
                        auto* vertexColor = mesh->vertexColors().data() + baseVertexCount;
                        for(int iy = 0; iy < nvy; iy++, vertexColor += 2) {
                            for(int ix = 0; ix < nvx; ix++, vertexColor += 4) {
                                coords[dim1] = ix;
                                coords[dim2] = iy;
                                const ColorG& c1 = colorArray[gridObj->voxelIndex(coords[0], coords[1], coords[2])];
                                if(pbcFlags[dim3]) {
                                    // Blend two colors if the grid is periodic.
                                    coords_wrap[dim1] = ix;
                                    coords_wrap[dim2] = iy;
                                    const auto& c2 = colorArray[gridObj->voxelIndex(coords_wrap[0], coords_wrap[1], coords_wrap[2])];
                                    vertexColor[3] = ColorAG(GraphicsFloatType(0.5) * (c1 + c2), alpha);
                                }
                                else {
                                    vertexColor[3] = ColorAG(c1, alpha);
                                }
                            }
                        }

                        // Compute color of vertices located on the horizontal grid lines of the voxel grid.
                        vertexColor = mesh->vertexColors().data() + baseVertexCount;
                        if(!pbcFlags[dim2]) {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[ix * verts_per_voxel + 1] = vertexColor[ix * verts_per_voxel + 3];
                        }
                        else {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[ix * verts_per_voxel + 1] = GraphicsFloatType(0.5) * (vertexColor[ix * verts_per_voxel + 3] + vertexColor[(nvy - 1) * verts_per_row + ix * verts_per_voxel + 3]);
                        }
                        for(int iy = 1; iy < nvy; iy++) {
                            for(int ix = 0; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] = GraphicsFloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[(iy-1) * verts_per_row + ix * verts_per_voxel + 3]);
                            }
                        }
                        if(!pbcFlags[dim2]) {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[nvy * verts_per_row + ix * 2 + 1] = vertexColor[(nvy - 1) * verts_per_row + ix * verts_per_voxel + 3];
                        }
                        else {
                            for(int ix = 0; ix < nvx; ix++)
                                vertexColor[nvy * verts_per_row + ix * 2 + 1] = vertexColor[ix * verts_per_voxel + 1];
                        }

                        // Compute color of vertices located on the vertical grid lines of the voxel grid.
                        if(!pbcFlags[dim1]) {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + 2] = vertexColor[iy * verts_per_row + 3];
                        }
                        else {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + 2] = GraphicsFloatType(0.5) * (vertexColor[iy * verts_per_row + 3] + vertexColor[(nvx - 1) * verts_per_voxel + iy * verts_per_row + 3]);
                        }
                        for(int iy = 0; iy < nvy; iy++) {
                            for(int ix = 1; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel + 2] = GraphicsFloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 3]);
                            }
                        }
                        if(!pbcFlags[dim1]) {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 3];
                        }
                        else {
                            for(int iy = 0; iy < nvy; iy++)
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + 2];
                        }

                        // Compute color of vertices located on the grid line intersections.
                        for(int iy = 0; iy < nvy; iy++) {
                            if(!pbcFlags[dim1])
                                vertexColor[iy * verts_per_row] = vertexColor[iy * verts_per_row + 1];
                            else
                                vertexColor[iy * verts_per_row] = GraphicsFloatType(0.5) * (vertexColor[iy * verts_per_row + 1] + vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 1]);
                            for(int ix = 1; ix < nvx; ix++) {
                                vertexColor[iy * verts_per_row + ix * verts_per_voxel] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 1]);
                            }
                            if(!pbcFlags[dim1])
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel] = vertexColor[iy * verts_per_row + (nvx - 1) * verts_per_voxel + 1];
                            else
                                vertexColor[iy * verts_per_row + nvx * verts_per_voxel] = vertexColor[iy * verts_per_row];
                        }
                        if(!pbcFlags[dim1])
                            vertexColor[nvy * verts_per_row] = vertexColor[nvy * verts_per_row + 1];
                        else
                            vertexColor[nvy * verts_per_row] = GraphicsFloatType(0.5) * (vertexColor[nvy * verts_per_row + 1] + vertexColor[nvy * verts_per_row + (nvx - 1) * 2 + 1]);
                        for(int ix = 1; ix < nvx; ix++) {
                            vertexColor[nvy * verts_per_row + ix * 2] = GraphicsFloatType(0.5) * (vertexColor[nvy * verts_per_row + ix * 2 + 1] + vertexColor[nvy * verts_per_row + (ix - 1) * 2 + 1]);
                        }
                        if(!pbcFlags[dim1])
                            vertexColor[nvy * verts_per_row + nvx * 2] = vertexColor[nvy * verts_per_row + (nvx - 1) * 2 + 1];
                        else
                            vertexColor[nvy * verts_per_row + nvx * 2] = vertexColor[nvy * verts_per_row];

                        // Create triangles.
                        auto face = mesh->faces().begin() + baseFaceCount;
                        for(int iy = 0; iy < nvy; iy++) {
                            for(int ix = 0; ix < nvx; ix++) {
                                bool is_x_border = (ix == nvx - 1);
                                bool is_y_border = (iy == nvy - 1);
                                int centerVertex = baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 3;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel, baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 1, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 1, baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel, baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel + (is_x_border ? 1 : 2), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + (ix+1) * verts_per_voxel + (is_x_border ? 1 : 2), baseVertexCount + (iy+1) * verts_per_row + (ix+1) * (is_y_border ? 2 : verts_per_voxel), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + (ix+1) * (is_y_border ? 2 : verts_per_voxel), baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel) + 1, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel) + 1, baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel), centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + (iy+1) * verts_per_row + ix * (is_y_border ? 2 : verts_per_voxel), baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 2, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                                face->setVertices(baseVertexCount + iy * verts_per_row + ix * verts_per_voxel + 2, baseVertexCount + iy * verts_per_row + ix * verts_per_voxel, centerVertex);
                                face->setEdgeVisibility(true, false, false);
                                ++face;
                            }
                        }
                        OVITO_ASSERT(face == mesh->faces().end());
                    }
                };

                createFacesForSide(0, 1, 2, false);
                if(!gridObj->domain()->is2D()) {
                    createFacesForSide(0, 1, 2, true);
                    createFacesForSide(1, 2, 0, false);
                    createFacesForSide(1, 2, 0, true);
                    createFacesForSide(2, 0, 1, false);
                    createFacesForSide(2, 0, 1, true);
                }
                volumeFaces.setMesh(std::move(mesh));
                volumeFaces.setUniformColor(ColorA(1,1,1,alpha));
                volumeFaces.setEmphasizeEdges(highlightGridLines());
                volumeFaces.setCullFaces(false);

                AffineTransformation cellMatrix = gridObj->domain()->cellMatrix();
                if(gridObj->domain()->is2D())
                    cellMatrix.column(2).setZero();
                boundingBox = Box3(Point3(0), Point3(1)).transformed(cellMatrix);
            }
        });

    if(volumeFaces.mesh()) {

        // Update the color mapping.
        auto coloredVolumeFaces = std::make_unique<MeshPrimitive>(volumeFaces);
        coloredVolumeFaces->setPseudoColorMapping(colorMapping()->pseudoColorMapping());

        // Add the mesh to the frame graph.
        frameGraph.addCommandGroup(FrameGraph::SceneLayer).addPrimitive(std::move(coloredVolumeFaces), sceneNode->getWorldTransform(frameGraph.time()), boundingBox, sceneNode, pickInfo);
    }

    return status;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString VoxelGridPickInfo::infoString(const Pipeline* pipeline, uint32_t subobjectId)
{
    QString str = voxelGrid()->objectTitle();

    if(voxelGrid()->domain()) {

        auto locateFaceOnSide = [&](size_t dim1, size_t dim2, size_t dim3, bool oppositeSide) -> std::optional<std::array<size_t, 3>> {
            const VoxelGrid::GridDimensions& gridDims = voxelGrid()->shape();
            size_t ntri = _numCells[dim1] * _numCells[dim2] * _trianglesPerCell;
            if(subobjectId < ntri) {
                std::array<size_t, 3> coords;
                coords[dim1] = (subobjectId / _trianglesPerCell) % _numCells[dim1];
                coords[dim2] = (subobjectId / _trianglesPerCell) / _numCells[dim1];
                coords[dim3] = oppositeSide ? (gridDims[dim3] - 1) : 0;
                return coords;
            }
            subobjectId -= ntri;
            return std::nullopt;
        };

        // Determine the grid cell the mouse cursor is pointing at.
        auto coords = locateFaceOnSide(0, 1, 2, false);
        if(!coords && !voxelGrid()->domain()->is2D()) {
            coords = locateFaceOnSide(0, 1, 2, true);
            if(!coords) coords = locateFaceOnSide(1, 2, 0, false);
            if(!coords) coords = locateFaceOnSide(1, 2, 0, true);
            if(!coords) coords = locateFaceOnSide(2, 0, 1, false);
            if(!coords) coords = locateFaceOnSide(2, 0, 1, true);
        }
        OVITO_ASSERT(coords);

        // Retrieve the property values of the grid cell.
        if(coords) {
            if(!str.isEmpty()) str += QStringLiteral("<sep>");
            str += voxelGrid()->elementInfoString(voxelGrid()->voxelIndex((*coords)[0], (*coords)[1], (*coords)[2]));
        }
    }

    return str;
}

}   // End of namespace
