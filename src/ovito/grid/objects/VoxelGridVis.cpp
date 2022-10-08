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

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/data/mesh/TriMeshObject.h>
#include "VoxelGridVis.h"

namespace Ovito::Grid {

IMPLEMENT_OVITO_CLASS(VoxelGridVis);
DEFINE_REFERENCE_FIELD(VoxelGridVis, transparencyController);
DEFINE_PROPERTY_FIELD(VoxelGridVis, highlightGridLines);
DEFINE_PROPERTY_FIELD(VoxelGridVis, interpolateColors);
DEFINE_REFERENCE_FIELD(VoxelGridVis, colorMapping);
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, transparencyController, "Surface transparency");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, highlightGridLines, "Show grid lines");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, interpolateColors, "Color interpolation");
SET_PROPERTY_FIELD_LABEL(VoxelGridVis, colorMapping, "Color mapping");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(VoxelGridVis, transparencyController, PercentParameterUnit, 0, 1);

IMPLEMENT_OVITO_CLASS(VoxelGridPickInfo);

/******************************************************************************
* Constructor.
******************************************************************************/
VoxelGridVis::VoxelGridVis(ObjectCreationParams params) : DataVis(params),
	_highlightGridLines(true),
	_interpolateColors(false)
{
	if(params.createSubObjects()) {
		// Create animation controller for the transparency parameter.
		setTransparencyController(ControllerManager::createFloatController(dataset()));

		// Create a color mapping object for pseudo-color visualization of a grid property.
		setColorMapping(OORef<PropertyColorMapping>::create(params));
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
		setColorMapping(OORef<PropertyColorMapping>::create(dataset()));
	}
}

/******************************************************************************
* Computes the bounding box of the displayed data.
******************************************************************************/
Box3 VoxelGridVis::boundingBox(TimePoint time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, TimeInterval& validityInterval)
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
PipelineStatus VoxelGridVis::render(TimePoint time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode)
{
	PipelineStatus status;

	// Check if this is just the bounding box computation pass.
	if(renderer->isBoundingBoxPass()) {
		TimeInterval validityInterval;
		renderer->addToLocalBoundingBox(boundingBox(time, path, contextNode, flowState, validityInterval));
		return status;
	}

	// Get the grid object being rendered.
	const VoxelGrid* gridObj = path.lastAs<VoxelGrid>();
	if(!gridObj) return status;

	// Throws an exception if the input data structure is corrupt.
	gridObj->verifyIntegrity();

	// Look for 'Color' voxel property.
	const PropertyObject* colorProperty = gridObj->getProperty(VoxelGrid::ColorProperty);
	ConstPropertyAccess<Color> colorArray(colorProperty);

	// Look for selected pseudo-coloring property.
	const PropertyObject* pseudoColorProperty = nullptr;
	int pseudoColorPropertyComponent = 0;
	if(!colorProperty && colorMapping() && colorMapping()->sourceProperty()) {
		pseudoColorProperty = colorMapping()->sourceProperty().findInContainer(gridObj);
		if(!pseudoColorProperty) {
			status = PipelineStatus(PipelineStatus::Error, tr("The property with the name '%1' does not exist.").arg(colorMapping()->sourceProperty().name()));
		}
		else {
			if(colorMapping()->sourceProperty().vectorComponent() >= (int)pseudoColorProperty->componentCount()) {
				status = PipelineStatus(PipelineStatus::Error, tr("The vector component is out of range. The property '%1' has only %2 values per data element.").arg(colorMapping()->sourceProperty().name()).arg(pseudoColorProperty->componentCount()));
				pseudoColorProperty = nullptr;
			}
			pseudoColorPropertyComponent = std::max(0, colorMapping()->sourceProperty().vectorComponent());
		}
	}
	ConstPropertyAccess<void,true> pseudoColorArray(pseudoColorProperty);
	OVITO_ASSERT(!(colorArray && pseudoColorArray));

	// The key type used for caching the geometry primitive:
	using CacheKey = RendererResourceKey<struct VoxelGridSurface,
		ConstDataObjectRef,			// Voxel grid object
		ConstDataObjectRef,			// Color property
		ConstDataObjectRef,			// Pseudo-color property
		int,						// Pseudo-color vector component
		FloatType,					// Transparency
		bool,						// Grid line highlighting
		bool						// Interpolate colors
	>;

	// The values stored in the vis cache.
	struct CacheValue {
		MeshPrimitive volumeFaces;
        OORef<ObjectPickInfo> pickInfo;
	};

	// Determine the opacity value for rendering the mesh.
	FloatType transp = 0;
	TimeInterval iv;
	if(transparencyController()) {
		transp = transparencyController()->getFloatValue(time, iv);
		if(transp >= 1.0) return status;
	}
	FloatType alpha = FloatType(1) - transp;

	// Look up the rendering primitive in the vis cache.
	auto& primitives = dataset()->visCache().get<CacheValue>(CacheKey(
		gridObj, 
		colorProperty, 
		pseudoColorProperty, 
		pseudoColorPropertyComponent, 
		transp, 
		highlightGridLines(), 
		interpolateColors()));

	// Check if we already have valid rendering primitives that are up to date.
	if(!primitives.volumeFaces.mesh()) {
		// Determine the number of triangle faces to create per voxel cell.
		size_t trianglesPerCell = 2;
		if(interpolateColors()) {
			if(colorArray || pseudoColorArray)
				trianglesPerCell = 8;
		}
		primitives.pickInfo = new VoxelGridPickInfo(this, gridObj, trianglesPerCell);
		if(gridObj->domain()) {
			DataOORef<TriMeshObject> mesh = DataOORef<TriMeshObject>::create(dataset(), ObjectCreationParams::WithoutVisElement);
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

			// Helper function that creates the mesh vertices and faces for one side of the grid volume.
			auto createFacesForSide = [&](size_t dim1, size_t dim2, size_t dim3, bool oppositeSide) {

				// Number of grid lines between voxels:
				int nx = gridDims[dim1] + 1;
				int ny = gridDims[dim2] + 1;

				// Edge vectors of one voxel face:
				Vector3 dx = gridObj->domain()->cellMatrix().column(dim1) / gridDims[dim1];
				Vector3 dy = gridObj->domain()->cellMatrix().column(dim2) / gridDims[dim2];

				// The xyz voxel grid coordinates:
				size_t coords[3];
				coords[dim3] = oppositeSide ? (gridDims[dim3] - 1) : 0;
				size_t coords_wrap[3];
				coords_wrap[dim3] = oppositeSide ? 0 : (gridDims[dim3] - 1);

				// The origin of the grid face in world space.
				Point3 origin = Point3::Origin() + gridObj->domain()->cellMatrix().translation();
				if(oppositeSide) origin += gridObj->domain()->cellMatrix().column(dim3);

				auto baseVertexCount = mesh->vertexCount();
				auto baseFaceCount = mesh->faceCount();

				if(!interpolateColors() || (!colorArray && !pseudoColorArray)) {
					OVITO_ASSERT(trianglesPerCell == 2);

					// Create two triangles per voxel face. 
					mesh->setVertexCount(baseVertexCount + nx * ny);
					mesh->setFaceCount(baseFaceCount + 2 * (nx-1) * (ny-1));

					// Create vertices.
					auto vertex = mesh->vertices().begin() + baseVertexCount;
					for(int iy = 0; iy < ny; iy++) {
						for(int ix = 0; ix < nx; ix++) {
							*vertex++ = origin + (ix * dx) + (iy * dy);
						}
					}
					OVITO_ASSERT(vertex == mesh->vertices().end());

					// Create triangles.
					auto face = mesh->faces().begin() + baseFaceCount;
					ColorA* faceColor = colorArray ? mesh->faceColors().data() + baseFaceCount : nullptr;
					FloatType* facePseudoColor = pseudoColorArray ? mesh->facePseudoColors().data() + baseFaceCount : nullptr;
					for(int iy = 0; iy < ny - 1; iy++) {
						for(int ix = 0; ix < nx - 1; ix++) {
							face->setVertices(baseVertexCount + iy * nx + ix, baseVertexCount + iy * nx + ix+1, baseVertexCount + (iy+1) * nx + ix+1);
							face->setEdgeVisibility(true, true, false);
							++face;
							face->setVertices(baseVertexCount + iy * nx + ix, baseVertexCount + (iy+1) * nx + ix+1, baseVertexCount + (iy+1) * nx + ix);
							face->setEdgeVisibility(false, true, true);
							++face;
							if(faceColor) {
								coords[dim1] = ix;
								coords[dim2] = iy;
								const Color& c = colorArray[gridObj->voxelIndex(coords[0], coords[1], coords[2])];
								*faceColor++ = ColorA(c, alpha);
								*faceColor++ = ColorA(c, alpha);
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
					int verts_per_row = verts_per_voxel * (nx - 1) + 2;

					// Generate 8 triangles per voxel cell face.
					mesh->setVertexCount(baseVertexCount + verts_per_row * (ny-1) + (nx - 1) * 2 + 1);
					mesh->setFaceCount(baseFaceCount + 8 * (nx-1) * (ny-1));

					// Create vertices.
					auto vertex = mesh->vertices().begin() + baseVertexCount;
					for(int iy = 0; iy < ny; iy++) {
						for(int ix = 0; ix < nx; ix++) {
							// Create four vertices per voxel face.
							Point3 corner = origin + (ix * dx) + (iy * dy);
							*vertex++ = corner;
							if(ix < nx - 1)
								*vertex++ = corner + FloatType(0.5) * dx;
							if(iy < ny - 1)
								*vertex++ = corner + FloatType(0.5) * dy;
							if(ix < nx - 1 && iy < ny - 1)
								*vertex++ = corner + FloatType(0.5) * (dx + dy);
						}
					}
					OVITO_ASSERT(vertex == mesh->vertices().end());

					// Compute pseudo-color of vertices located in the center of voxel faces.
					FloatType* vertexColor = mesh->vertexPseudoColors().data() + baseVertexCount;
					for(int iy = 0; iy < ny - 1; iy++, vertexColor += 2) {
						for(int ix = 0; ix < nx - 1; ix++, vertexColor += 4) {
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
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[ix * verts_per_voxel + 1] = vertexColor[ix * verts_per_voxel + 3];
					}
					else {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[ix * verts_per_voxel + 3] + vertexColor[(ny - 2) * verts_per_row + ix * verts_per_voxel + 3]);
					}
					for(int iy = 1; iy < ny - 1; iy++) {
						for(int ix = 0; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[(iy-1) * verts_per_row + ix * verts_per_voxel + 3]);
						}
					}
					if(!pbcFlags[dim2]) {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] = vertexColor[(ny - 2) * verts_per_row + ix * verts_per_voxel + 3];
					}
					else {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] = vertexColor[ix * verts_per_voxel + 1];
					}

					// Compute color of vertices located on the vertical grid lines of the voxel grid.
					if(!pbcFlags[dim1]) {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + 2] = vertexColor[iy * verts_per_row + 3];
					}
					else {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 3] + vertexColor[(nx - 2) * verts_per_voxel + iy * verts_per_row + 3]);
					}
					for(int iy = 0; iy < ny - 1; iy++) {
						for(int ix = 1; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 3]);
						}
					}
					if(!pbcFlags[dim1]) {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 3];
					}
					else {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + 2];
					}

					// Compute color of vertices located on the grid line intersections.
					for(int iy = 0; iy < ny - 1; iy++) {
						if(!pbcFlags[dim1])
							vertexColor[iy * verts_per_row] = vertexColor[iy * verts_per_row + 1];
						else
							vertexColor[iy * verts_per_row] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 1] + vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 1]);
						for(int ix = 1; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 1]);
						}
						if(!pbcFlags[dim1])
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel] = vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 1];
						else
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel] = vertexColor[iy * verts_per_row];
					}
					if(!pbcFlags[dim1])
						vertexColor[(ny - 1) * verts_per_row] = vertexColor[(ny - 1) * verts_per_row + 1];
					else
						vertexColor[(ny - 1) * verts_per_row] = FloatType(0.5) * (vertexColor[(ny - 1) * verts_per_row + 1] + vertexColor[(ny - 1) * verts_per_row + (nx - 2) * 2 + 1]);
					for(int ix = 1; ix < nx - 1; ix++) {
						vertexColor[(ny - 1) * verts_per_row + ix * 2] = FloatType(0.5) * (vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] + vertexColor[(ny - 1) * verts_per_row + (ix - 1) * 2 + 1]);
					}
					if(!pbcFlags[dim1])
						vertexColor[(ny - 1) * verts_per_row + (nx - 1) * 2] = vertexColor[(ny - 1) * verts_per_row + (nx - 2) * 2 + 1];
					else
						vertexColor[(ny - 1) * verts_per_row + (nx - 1) * 2] = vertexColor[(ny - 1) * verts_per_row];

					// Create triangles.
					auto face = mesh->faces().begin() + baseFaceCount;
					for(int iy = 0; iy < ny - 1; iy++) {
						for(int ix = 0; ix < nx - 1; ix++) {
							bool is_x_border = (ix == nx - 2);
							bool is_y_border = (iy == ny - 2);
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
					int verts_per_row = verts_per_voxel * (nx - 1) + 2;

					// Generate 8 triangles per voxel cell face.
					mesh->setVertexCount(baseVertexCount + verts_per_row * (ny-1) + (nx - 1) * 2 + 1);
					mesh->setFaceCount(baseFaceCount + 8 * (nx-1) * (ny-1));

					// Create vertices.
					auto vertex = mesh->vertices().begin() + baseVertexCount;
					for(int iy = 0; iy < ny; iy++) {
						for(int ix = 0; ix < nx; ix++) {
							// Create four vertices per voxel face.
							Point3 corner = origin + (ix * dx) + (iy * dy);
							*vertex++ = corner;
							if(ix < nx - 1)
								*vertex++ = corner + FloatType(0.5) * dx;
							if(iy < ny - 1)
								*vertex++ = corner + FloatType(0.5) * dy;
							if(ix < nx - 1 && iy < ny - 1)
								*vertex++ = corner + FloatType(0.5) * (dx + dy);
						}
					}
					OVITO_ASSERT(vertex == mesh->vertices().end());

					// Compute color of vertices located in the center of voxel faces.
					ColorA* vertexColor = mesh->vertexColors().data() + baseVertexCount;
					for(int iy = 0; iy < ny - 1; iy++, vertexColor += 2) {
						for(int ix = 0; ix < nx - 1; ix++, vertexColor += 4) {
							coords[dim1] = ix;
							coords[dim2] = iy;
							const Color& c1 = colorArray[gridObj->voxelIndex(coords[0], coords[1], coords[2])];
							if(pbcFlags[dim3]) {
								// Blend two colors if the grid is periodic.
								coords_wrap[dim1] = ix;
								coords_wrap[dim2] = iy;
								const Color& c2 = colorArray[gridObj->voxelIndex(coords_wrap[0], coords_wrap[1], coords_wrap[2])];
								vertexColor[3] = ColorA(FloatType(0.5) * (c1 + c2), alpha);
							}
							else {
								vertexColor[3] = ColorA(c1, alpha);
							}
						}
					}

					// Compute color of vertices located on the horizontal grid lines of the voxel grid.
					vertexColor = mesh->vertexColors().data() + baseVertexCount;
					if(!pbcFlags[dim2]) {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[ix * verts_per_voxel + 1] = vertexColor[ix * verts_per_voxel + 3];
					}
					else {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[ix * verts_per_voxel + 3] + vertexColor[(ny - 2) * verts_per_row + ix * verts_per_voxel + 3]);
					}
					for(int iy = 1; iy < ny - 1; iy++) {
						for(int ix = 0; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[(iy-1) * verts_per_row + ix * verts_per_voxel + 3]);
						}
					}
					if(!pbcFlags[dim2]) {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] = vertexColor[(ny - 2) * verts_per_row + ix * verts_per_voxel + 3];
					}
					else {
						for(int ix = 0; ix < nx - 1; ix++)
							vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] = vertexColor[ix * verts_per_voxel + 1];
					}

					// Compute color of vertices located on the vertical grid lines of the voxel grid.
					if(!pbcFlags[dim1]) {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + 2] = vertexColor[iy * verts_per_row + 3];
					}
					else {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 3] + vertexColor[(nx - 2) * verts_per_voxel + iy * verts_per_row + 3]);
					}
					for(int iy = 0; iy < ny - 1; iy++) {
						for(int ix = 1; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel + 2] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 3] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 3]);
						}
					}
					if(!pbcFlags[dim1]) {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 3];
					}
					else {
						for(int iy = 0; iy < ny - 1; iy++)
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel + 1] = vertexColor[iy * verts_per_row + 2];
					}

					// Compute color of vertices located on the grid line intersections.
					for(int iy = 0; iy < ny - 1; iy++) {
						if(!pbcFlags[dim1])
							vertexColor[iy * verts_per_row] = vertexColor[iy * verts_per_row + 1];
						else
							vertexColor[iy * verts_per_row] = FloatType(0.5) * (vertexColor[iy * verts_per_row + 1] + vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 1]);
						for(int ix = 1; ix < nx - 1; ix++) {
							vertexColor[iy * verts_per_row + ix * verts_per_voxel] = FloatType(0.5) * (vertexColor[iy * verts_per_row + ix * verts_per_voxel + 1] + vertexColor[iy * verts_per_row + (ix-1) * verts_per_voxel + 1]);
						}
						if(!pbcFlags[dim1])
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel] = vertexColor[iy * verts_per_row + (nx - 2) * verts_per_voxel + 1];
						else
							vertexColor[iy * verts_per_row + (nx - 1) * verts_per_voxel] = vertexColor[iy * verts_per_row];
					}
					if(!pbcFlags[dim1])
						vertexColor[(ny - 1) * verts_per_row] = vertexColor[(ny - 1) * verts_per_row + 1];
					else
						vertexColor[(ny - 1) * verts_per_row] = FloatType(0.5) * (vertexColor[(ny - 1) * verts_per_row + 1] + vertexColor[(ny - 1) * verts_per_row + (nx - 2) * 2 + 1]);
					for(int ix = 1; ix < nx - 1; ix++) {
						vertexColor[(ny - 1) * verts_per_row + ix * 2] = FloatType(0.5) * (vertexColor[(ny - 1) * verts_per_row + ix * 2 + 1] + vertexColor[(ny - 1) * verts_per_row + (ix - 1) * 2 + 1]);
					}
					if(!pbcFlags[dim1])
						vertexColor[(ny - 1) * verts_per_row + (nx - 1) * 2] = vertexColor[(ny - 1) * verts_per_row + (nx - 2) * 2 + 1];
					else
						vertexColor[(ny - 1) * verts_per_row + (nx - 1) * 2] = vertexColor[(ny - 1) * verts_per_row];

					// Create triangles.
					auto face = mesh->faces().begin() + baseFaceCount;
					for(int iy = 0; iy < ny - 1; iy++) {
						for(int ix = 0; ix < nx - 1; ix++) {
							bool is_x_border = (ix == nx - 2);
							bool is_y_border = (iy == ny - 2);
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
			primitives.volumeFaces.setMesh(std::move(mesh));
			primitives.volumeFaces.setUniformColor(ColorA(1,1,1,alpha));
			primitives.volumeFaces.setEmphasizeEdges(highlightGridLines());
			primitives.volumeFaces.setCullFaces(false);
		}
	}

	// Update the color mapping.
	primitives.volumeFaces.setPseudoColorMapping(colorMapping()->pseudoColorMapping());

	renderer->beginPickObject(contextNode, primitives.pickInfo);
	renderer->renderMesh(primitives.volumeFaces);
	renderer->endPickObject();

	return status;
}

/******************************************************************************
* Returns a human-readable string describing the picked object,
* which will be displayed in the status bar by OVITO.
******************************************************************************/
QString VoxelGridPickInfo::infoString(PipelineSceneNode* objectNode, quint32 subobjectId)
{
    QString str = voxelGrid()->objectTitle();

	if(voxelGrid()->domain()) {

		auto locateFaceOnSide = [&](size_t dim1, size_t dim2, size_t dim3, bool oppositeSide) -> std::optional<std::array<size_t, 3>> {
			const VoxelGrid::GridDimensions& gridDims = voxelGrid()->shape();
			size_t ntri = gridDims[dim1] * gridDims[dim2] * _trianglesPerCell;
			if(subobjectId < ntri) {
				std::array<size_t, 3> coords;
				coords[dim1] = (subobjectId / _trianglesPerCell) % gridDims[dim1];
				coords[dim2] = (subobjectId / _trianglesPerCell) / gridDims[dim1];
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

}	// End of namespace
