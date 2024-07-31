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

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMeshTopology.h>
#include <ovito/mesh/surface/SurfaceMeshVertices.h>
#include <ovito/mesh/surface/SurfaceMeshFaces.h>
#include <ovito/mesh/surface/SurfaceMeshRegions.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * Utility class that provides efficient read-only access to the data of an existing surface mesh object.
 */
class OVITO_MESH_EXPORT SurfaceMeshReadAccess
{
public:

    // Indexing data types:
    using size_type = SurfaceMesh::size_type;
    using vertex_index = SurfaceMesh::vertex_index;
    using edge_index = SurfaceMesh::edge_index;
    using face_index = SurfaceMesh::face_index;
    using region_index = SurfaceMesh::region_index;

    /// Special constant used to indicate an invalid list index (-1).
    constexpr static size_type InvalidIndex = SurfaceMesh::InvalidIndex;

    /// Constructor that takes an existing SurfaceMesh object.
    explicit SurfaceMeshReadAccess(const SurfaceMesh* mesh = nullptr);

    /// Indicates whether this accessor contains a valid surface mesh object.
    explicit operator bool() const noexcept { return (bool)_mesh; }

    /// Returns the topology of the surface mesh.
    const SurfaceMeshTopology* topology() const { return _topology; }

    /// Returns the number of vertices in the mesh.
    size_type vertexCount() const {  return static_cast<size_type>(topology()->vertexCount()); }

    /// Returns the number of faces in the mesh.
    size_type faceCount() const { return static_cast<size_type>(topology()->faceCount()); }

    /// Returns the number of half-edges in the mesh.
    size_type edgeCount() const { return topology()->edgeCount(); }

    /// Returns the number of spatial regions defined for the mesh.
    size_type regionCount() const { return static_cast<size_type>(regions()->elementCount()); }

    /// Returns an iterator range over all vertices of the mesh topology.
    auto verticesRange() const { return boost::counting_range<size_type>(0, vertexCount()); }

    /// Returns an iterator range over all faces of the mesh topology.
    auto facesRange() const { return boost::counting_range<size_type>(0, faceCount()); }

    /// Returns an iterator range over all half-edges of the mesh topology.
    auto edgesRange() const { return boost::counting_range<size_type>(0, edgeCount()); }

    /// Returns an iterator range over all regions of the mesh.
    auto regionsRange() const { return boost::counting_range<size_type>(0, regionCount()); }

    /// Returns the index of the space-filling spatial region.
    region_index spaceFillingRegion() const { return mesh()->spaceFillingRegion(); }

    /// Returns the first edge from a vertex' list of outgoing half-edges.
    edge_index firstVertexEdge(vertex_index vertex) const { return topology()->firstVertexEdge(vertex); }

    /// Returns the half-edge following the given half-edge in the linked list of half-edges of a vertex.
    edge_index nextVertexEdge(edge_index edge) const { return topology()->nextVertexEdge(edge); }

    /// Returns the first half-edge from the linked-list of half-edges of a face.
    edge_index firstFaceEdge(face_index face) const { return topology()->firstFaceEdge(face); }

    /// Returns the list of first half-edges for each face.
    const std::vector<edge_index>& firstFaceEdges() const { return topology()->firstFaceEdges(); }

    /// Returns the opposite face of a face.
    face_index oppositeFace(face_index face) const { return topology()->oppositeFace(face); };

    /// Determines whether the given face is linked to an opposite face.
    bool hasOppositeFace(face_index face) const { return topology()->hasOppositeFace(face); };

    /// Returns the next half-edge following the given half-edge in the linked-list of half-edges of a face.
    edge_index nextFaceEdge(edge_index edge) const { return topology()->nextFaceEdge(edge); }

    /// Returns the previous half-edge preceding the given edge in the linked-list of half-edges of a face.
    edge_index prevFaceEdge(edge_index edge) const { return topology()->prevFaceEdge(edge); }

    /// Returns the first vertex from the contour of a face.
    vertex_index firstFaceVertex(face_index face) const { return topology()->firstFaceVertex(face); }

    /// Returns the second vertex from the contour of a face.
    vertex_index secondFaceVertex(face_index face) const { return topology()->secondFaceVertex(face); }

    /// Returns the third vertex from the contour of a face.
    vertex_index thirdFaceVertex(face_index face) const { return topology()->thirdFaceVertex(face); }

    /// Returns the second half-edge (following the first half-edge) from the linked-list of half-edges of a face.
    edge_index secondFaceEdge(face_index face) const { return topology()->secondFaceEdge(face); }

    /// Returns the vertex the given half-edge is originating from.
    vertex_index vertex1(edge_index edge) const { return topology()->vertex1(edge); }

    /// Returns the vertex the given half-edge is leading to.
    vertex_index vertex2(edge_index edge) const { return topology()->vertex2(edge); }

    /// Returns the face which is adjacent to the given half-edge.
    face_index adjacentFace(edge_index edge) const { return topology()->adjacentFace(edge); }

    /// Returns the opposite half-edge of the given edge.
    edge_index oppositeEdge(edge_index edge) const { return topology()->oppositeEdge(edge); }

    /// Returns whether the given half-edge has an opposite half-edge.
    bool hasOppositeEdge(edge_index edge) const { return topology()->hasOppositeEdge(edge); }

    /// Counts the number of outgoing half-edges adjacent to the given mesh vertex.
    size_type vertexEdgeCount(vertex_index vertex) const { return topology()->vertexEdgeCount(vertex); }

    /// Searches the half-edges of a face for one connecting the two given vertices.
    edge_index findEdge(face_index face, vertex_index v1, vertex_index v2) const { return topology()->findEdge(face, v1, v2); }

    /// Returns the next incident manifold when going around the given half-edge.
    edge_index nextManifoldEdge(edge_index edge) const { return topology()->nextManifoldEdge(edge); };

    /// Determines the number of manifolds adjacent to a half-edge.
    int countManifolds(edge_index edge) const { return topology()->countManifolds(edge); }

    /// Tests if two faces connect the same sequence of vertices in reverse order.
    bool areOppositeFaces(face_index face1, face_index face2) const { return topology()->areOppositeFaces(face1, face2); }

    /// Returns the simulation domain the surface mesh is embedded in.
    const SimulationCell* domain() const { return _domain; }

    /// Returns whether the mesh's domain has periodic boundary conditions applied in the given direction.
    bool hasPbc(size_t dim) const { return domain() ? domain()->hasPbc(dim) : false; }

    /// Wraps a vector at periodic boundaries of the simulation cell.
    Vector3 wrapVector(const Vector3& v) const {
        return domain() ? domain()->wrapVector(v) : v;
    }

    /// Wraps a point at periodic boundaries of the simulation cell.
    Point3 wrapPoint(const Point3& p) const {
        return domain() ? domain()->wrapPoint(p) : p;
    }

    /// Returns the vector corresponding to an half-edge of the surface mesh.
    Vector3 edgeVector(edge_index edge, const BufferReadAccess<Point3>& vertexPositions) const {
        Vector3 delta = vertexPositions[vertex2(edge)] - vertexPositions[vertex1(edge)];
        return domain() ? domain()->wrapVector(delta) : delta;
    }

    /// Determines which spatial region contains the given point in space.
    /// Returns no result if the point is exactly on a region boundary.
    std::optional<std::pair<region_index, FloatType>> locatePoint(const Point3& location, FloatType epsilon = FLOATTYPE_EPSILON, const boost::dynamic_bitset<>& faceSubset = boost::dynamic_bitset<>()) const;

    /// Returns one of the standard vertex properties (or null if the property is not defined).
    const Property* vertexProperty(SurfaceMeshVertices::Type ptype) const {
        return vertices()->getProperty(ptype);
    }

    /// Returns one of the standard vertex properties (throws exception if the property is not defined).
    const Property* expectVertexProperty(SurfaceMeshVertices::Type ptype) const {
        return vertices()->expectProperty(ptype);
    }

    /// Returns a user vertex property (or null if the property is not defined).
    const Property* vertexProperty(const QString& name) const {
        return vertices()->getProperty(name);
    }

    /// Returns one of the standard face properties (or null if the property is not defined).
    const Property* faceProperty(SurfaceMeshFaces::Type ptype) const {
        return faces()->getProperty(ptype);
    }

    /// Returns one of the standard face properties (throws exception if the property is not defined).
    const Property* expectFaceProperty(SurfaceMeshFaces::Type ptype) const {
        return faces()->expectProperty(ptype);
    }

    /// Returns a user face property (or null if the property is not defined).
    const Property* faceProperty(const QString& name) const {
        return faces()->getProperty(name);
    }

    /// Returns one of the standard region properties (or null if the property is not defined).
    const Property* regionProperty(SurfaceMeshRegions::Type ptype) const {
        return regions()->getProperty(ptype);
    }

    /// Returns one of the standard region properties (throws exception if the property is not defined).
    const Property* expectRegionProperty(SurfaceMeshRegions::Type ptype) const {
        return regions()->expectProperty(ptype);
    }

    /// Returns a user region property (or null if the property is not defined).
    const Property* regionProperty(const QString& name) const {
        return regions()->getProperty(name);
    }

    /// Triangulates the polygonal faces of this mesh and outputs the results as a TriangleMesh.
    void convertToTriMesh(TriangleMesh& outputMesh, bool smoothShading, const boost::dynamic_bitset<>& faceSubset = boost::dynamic_bitset<>{}, std::vector<size_t>* originalFaceMap = nullptr, bool autoGenerateOppositeFaces = false) const;

    /// Computes the unit normal vector of a mesh face.
    Vector3 computeFaceNormal(face_index face, const BufferReadAccess<Point3>& vertexPositions) const;

    /// Returns the surface mesh object managed by this class.
    const SurfaceMesh* mesh() const { return _mesh; }

    /// Returns the vertex property container of the surface mesh.
    const SurfaceMeshVertices* vertices() const {
        OVITO_ASSERT(_vertices);
        return _vertices;
    }

    /// Returns the face property container of the surface mesh.
    const SurfaceMeshFaces* faces() const {
        OVITO_ASSERT(_faces);
        return _faces;
    }

    /// Returns the region property container of the surface mesh.
    const SurfaceMeshRegions* regions() const {
        OVITO_ASSERT(_regions);
        return _regions;
    }

protected:

    const SurfaceMesh* _mesh; ///< The surface mesh data object managed by this class.
    const SurfaceMeshTopology* _topology; ///< The topology of the surface mesh.
    const SurfaceMeshVertices* _vertices; ///< The vertex properties of the surface mesh.
    const SurfaceMeshFaces* _faces; ///< The face properties of the surface mesh.
    const SurfaceMeshRegions* _regions; ///< The region properties of the surface mesh.
    const SimulationCell* _domain; ///< The simulation cell object of the surface mesh.
};

}   // End of namespace
