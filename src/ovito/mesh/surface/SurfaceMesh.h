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

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/PeriodicDomainDataObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include "SurfaceMeshVertices.h"
#include "SurfaceMeshFaces.h"
#include "SurfaceMeshRegions.h"
#include "SurfaceMeshTopology.h"

namespace Ovito::Mesh {

/**
 * \brief A closed mesh representing a surface, i.e. a two-dimensional manifold.
 */
class OVITO_MESH_EXPORT SurfaceMesh : public PeriodicDomainDataObject
{
	OVITO_CLASS(SurfaceMesh)
	Q_CLASSINFO("DisplayName", "Surface mesh");

public:

    using size_type = SurfaceMeshTopology::size_type;
    using vertex_index = SurfaceMeshTopology::vertex_index;
    using edge_index = SurfaceMeshTopology::edge_index;
    using face_index = SurfaceMeshTopology::face_index;
    using region_index = int;

    /// Special value used to indicate an invalid list index.
    constexpr static size_type InvalidIndex = SurfaceMeshTopology::InvalidIndex;

	/// Constructor creating an empty SurfaceMesh object.
	Q_INVOKABLE SurfaceMesh(ObjectCreationParams params, const QString& title = QString());
	
	/// Makes sure that the data structures of the surface mesh are valid and all vertex and face properties
	/// are consistent with the topology of the mesh. If this is not the case, the method throws an exception.
	void verifyMeshIntegrity() const;

	/// Duplicates the SurfaceMeshTopology sub-object if it is shared with other surface meshes.
	/// After this method returns, the sub-object is exclusively owned by the container and
	/// can be safely modified without unwanted side effects.
	SurfaceMeshTopology* makeTopologyMutable() {
		OVITO_ASSERT(topology());
	    return makeMutable(topology());
	}

	/// Duplicates the SurfaceMeshVertices sub-object if it is shared with other surface meshes.
	/// After this method returns, the sub-object is exclusively owned by the container and
	/// can be safely modified without unwanted side effects.
	SurfaceMeshVertices* makeVerticesMutable() {
		OVITO_ASSERT(vertices());
	    return makeMutable(vertices());
	}

	/// Duplicates the SurfaceMeshFaces sub-object if it is shared with other surface meshes.
	/// After this method returns, the sub-object is exclusively owned by the container and
	/// can be safely modified without unwanted side effects.
	SurfaceMeshFaces* makeFacesMutable() {
		OVITO_ASSERT(faces());
	    return makeMutable(faces());
	}

	/// Duplicates the SurfaceMeshRegions sub-object if it is shared with other surface meshes.
	/// After this method returns, the sub-object is exclusively owned by the container and
	/// can be safely modified without unwanted side effects.
	SurfaceMeshRegions* makeRegionsMutable() {
		OVITO_ASSERT(regions());
	    return makeMutable(regions());
	}

	/// Determines which spatial region contains the given location in space.
	std::optional<std::pair<region_index, FloatType>> locatePoint(const Point3& location, FloatType epsilon = FLOATTYPE_EPSILON) const;

private:

	/// The data structure storing the topology of the surface mesh.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const SurfaceMeshTopology>, topology, setTopology);

	/// The container holding the mesh vertex properties.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const SurfaceMeshVertices>, vertices, setVertices);

	/// The container holding the mesh face properties.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const SurfaceMeshFaces>, faces, setFaces);

	/// The container holding the properties of the volumetric regions enclosed by the mesh.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const SurfaceMeshRegions>, regions, setRegions);

	/// If the mesh has zero faces and is embedded in a fully periodic domain,
	/// this indicates the volumetric region that fills the entire space.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(SurfaceMesh::region_index, spaceFillingRegion, setSpaceFillingRegion);
};

}	// End of namespace
