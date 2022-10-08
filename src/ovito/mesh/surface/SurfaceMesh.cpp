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

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include "SurfaceMesh.h"
#include "SurfaceMeshVis.h"
#include "SurfaceMeshAccess.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(SurfaceMesh);
DEFINE_PROPERTY_FIELD(SurfaceMesh, spaceFillingRegion);
DEFINE_REFERENCE_FIELD(SurfaceMesh, topology);
DEFINE_REFERENCE_FIELD(SurfaceMesh, vertices);
DEFINE_REFERENCE_FIELD(SurfaceMesh, faces);
DEFINE_REFERENCE_FIELD(SurfaceMesh, regions);
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, topology, "Topology");
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, vertices, "Vertices");
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, faces, "Faces");
SET_PROPERTY_FIELD_LABEL(SurfaceMesh, regions, "Regions");

constexpr SurfaceMesh::size_type SurfaceMesh::InvalidIndex;

/******************************************************************************
* Constructs an empty surface mesh object.
******************************************************************************/
SurfaceMesh::SurfaceMesh(ObjectCreationParams params, const QString& title) : PeriodicDomainDataObject(params, title),
	_spaceFillingRegion(SurfaceMesh::InvalidIndex)
{
	if(params.createVisElement()) {
		// Attach a visualization element for rendering the surface mesh.
		setVisElement(OORef<SurfaceMeshVis>::create(params));
	}

	if(params.createSubObjects()) {
		// Create the sub-object for storing the mesh topology.
		setTopology(DataOORef<SurfaceMeshTopology>::create(params));

		// Create the sub-object for storing the vertex properties.
		setVertices(DataOORef<SurfaceMeshVertices>::create(params));

		// Create the sub-object for storing the face properties.
		setFaces(DataOORef<SurfaceMeshFaces>::create(params));

		// Create the sub-object for storing the region properties.
		setRegions(DataOORef<SurfaceMeshRegions>::create(params));
	}
}

/******************************************************************************
* Checks if the surface mesh is valid and all vertex and face properties
* are consistent with the topology of the mesh. If this is not the case,
* the method throws an exception.
******************************************************************************/
void SurfaceMesh::verifyMeshIntegrity() const
{
	OVITO_CHECK_OBJECT_POINTER(topology());
	if(!topology())
		throwException(tr("Surface mesh has no topology object attached."));

	OVITO_CHECK_OBJECT_POINTER(vertices());
	if(!vertices())
		throwException(tr("Surface mesh has no vertex properties container attached."));
	OVITO_CHECK_OBJECT_POINTER(vertices()->getProperty(SurfaceMeshVertices::PositionProperty));
	if(!vertices()->getProperty(SurfaceMeshVertices::PositionProperty))
		throwException(tr("Invalid data structure. Surface mesh is missing the position vertex property."));
	OVITO_ASSERT(topology()->vertexCount() == vertices()->elementCount());
	if(topology()->vertexCount() != vertices()->elementCount())
		throwException(tr("Length of vertex property arrays of surface mesh do not match number of vertices in the mesh topology."));

	OVITO_CHECK_OBJECT_POINTER(faces());
	if(!faces())
		throwException(tr("Surface mesh has no face properties container attached."));
	OVITO_ASSERT(faces()->properties().empty() || topology()->faceCount() == faces()->elementCount());
	if(!faces()->properties().empty() && topology()->faceCount() != faces()->elementCount())
		throwException(tr("Length of face property arrays of surface mesh do not match number of faces in the mesh topology."));

	OVITO_CHECK_OBJECT_POINTER(regions());
	if(!regions())
		throwException(tr("Surface mesh has no region properties container attached."));

	OVITO_ASSERT(spaceFillingRegion() == InvalidIndex || spaceFillingRegion() >= 0);
	if(spaceFillingRegion() != InvalidIndex && spaceFillingRegion() < 0)
		throwException(tr("Space filling region ID set for surface mesh must not be negative."));

	vertices()->verifyIntegrity();
	faces()->verifyIntegrity();
	regions()->verifyIntegrity();
}

/******************************************************************************
* Determines which spatial region contains the given point in space.
* Returns -1 if the point is exactly on a region boundary.
******************************************************************************/
std::optional<std::pair<SurfaceMesh::region_index, FloatType>> SurfaceMesh::locatePoint(const Point3& location, FloatType epsilon) const
{
	verifyMeshIntegrity();
	return SurfaceMeshAccess(this).locatePoint(location, epsilon);
}

}	// End of namespace
