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

#pragma once


#include <ovito/mesh/Mesh.h>
#include <ovito/mesh/surface/SurfaceMeshTopology.h>
#include <ovito/mesh/surface/SurfaceMeshVertices.h>
#include <ovito/mesh/surface/SurfaceMeshFaces.h>
#include <ovito/mesh/surface/SurfaceMeshRegions.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshReadAccess.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>

namespace Ovito::Mesh {

/**
 * Utility class that provides efficient mutable access to the data of a surface mesh object.
 */
class OVITO_MESH_EXPORT SurfaceMeshBuilder : public SurfaceMeshReadAccess
{
public:

    /// Utility class that supports in efficiently and incrementally adding and removing vertices to the surface mesh.
    class OVITO_MESH_EXPORT VertexGrower : public PropertyContainer::Grower
    {
    public:

        VertexGrower(SurfaceMeshBuilder& builder) :
            PropertyContainer::Grower(builder.mutableVertices()),
            _topo(builder.mutableTopology()),
            _vertexPositions(mutableProperty(SurfaceMeshVertices::PositionProperty)) {}

        void reset() {
            PropertyContainer::Grower::commit();
            _topo = nullptr;
            _vertexPositions.reset();
        }

        /// Adds a new vertex to the mesh and initializes its coordinates ("Position" vertex property).
        vertex_index createVertex(const Point3& p) {
            OVITO_ASSERT(_topo);
            vertex_index vertex = _topo->createVertex();
            grow(1, SurfaceMeshVertices::PositionProperty);
            _vertexPositions.updateDataStorageAddress();
            _vertexPositions[vertex] = p;
            return vertex;
        }

        /// Creates a copy of an existing vertex (including all its properties).
        vertex_index copyVertex(vertex_index existingVertex) {
            OVITO_ASSERT(_topo);
            vertex_index vertex = _topo->createVertex();
            grow(1, SurfaceMeshVertices::PositionProperty);
            _vertexPositions.updateDataStorageAddress();
            moveElement(existingVertex, vertex, SurfaceMeshVertices::PositionProperty);
            return vertex;
        }

        /// Deletes one vertex from the mesh.
        void deleteVertex(vertex_index vertex) {
            OVITO_ASSERT(_topo);
            // Move the last vertex to the index of the vertex being deleted.
            moveElement(_topo->vertexCount() - 1, vertex, SurfaceMeshVertices::PositionProperty);
            // Truncate the vertex property arrays by one element.
            truncate(1, SurfaceMeshVertices::PositionProperty);
            // Update mesh topology.
            _topo->deleteVertex(vertex);
        }

        /// Returns the coordinates of the i-th mesh vertex.
        const Point3& vertexPosition(vertex_index i) const { return _vertexPositions[i]; }

    private:
        SurfaceMeshTopology* _topo;
        BufferAccess<Point3> _vertexPositions;
    };

    /// Utility class that supports in efficiently and incrementally adding and removing faces to the surface mesh.
    class OVITO_MESH_EXPORT FaceGrower : public PropertyContainer::Grower
    {
    public:

        FaceGrower(SurfaceMeshBuilder& builder) :
            PropertyContainer::Grower(builder.mutableFaces()),
            _topo(builder.mutableTopology()),
            _faceRegions(mutableProperty(SurfaceMeshFaces::RegionProperty)) {}

        void reset() {
            PropertyContainer::Grower::commit();
            _topo = nullptr;
            _faceRegions.reset();
        }

        /// Adds a new face to the mesh.
        template<typename VertexIterator>
        face_index createFace(VertexIterator begin, VertexIterator end, region_index region = InvalidIndex) {
            OVITO_ASSERT(_topo);
            face_index face = _topo->createFaceAndEdges(std::forward<VertexIterator>(begin), std::forward<VertexIterator>(end));
            grow(1, SurfaceMeshFaces::RegionProperty);
            if(_faceRegions) {
                _faceRegions.updateDataStorageAddress();
                _faceRegions[face] = region;
            }
            else OVITO_ASSERT(region == InvalidIndex);
            return face;
        }

        /// Adds a new face to the mesh without creating any half-edges.
        face_index createFace(region_index region = InvalidIndex) {
            OVITO_ASSERT(_topo);
            face_index face = _topo->createFace();
            grow(1, SurfaceMeshFaces::RegionProperty);
            if(_faceRegions) {
                _faceRegions.updateDataStorageAddress();
                _faceRegions[face] = region;
            }
            else OVITO_ASSERT(region == InvalidIndex);
            return face;
        }

        /// Adds a new face to the mesh.
        face_index createFace(std::initializer_list<vertex_index> range, region_index region = InvalidIndex) {
            return createFace(std::begin(range), std::end(range), region);
        }

        /// Creates a copy of an existing face including all its properties.
        face_index copyFace(face_index existingFace) {
            OVITO_ASSERT(_topo);
            face_index face = _topo->createFace();
            grow(1, SurfaceMeshFaces::RegionProperty);
            if(_faceRegions)
                _faceRegions.updateDataStorageAddress();
            moveElement(existingFace, face, SurfaceMeshFaces::RegionProperty);
            return face;
        }

        /// Deletes one face from the mesh.
        void deleteFace(face_index face) {
            OVITO_ASSERT(_topo);
            // Move the last face from the array into the free slot.
            moveElement(_topo->faceCount() - 1, face, SurfaceMeshFaces::RegionProperty);
            // Truncate the face property arrays by one element.
            truncate(1, SurfaceMeshFaces::RegionProperty);
            // Update mesh topology.
            _topo->deleteFace(face);
        }

        /// Returns the region the i-th mesh face belongs to.
        region_index faceRegion(face_index i) const { return _faceRegions[i]; }

        /// Provides direct access to the per-face region information.
        const BufferAccess<region_index>& faceRegions() const { return _faceRegions; }

        /// Provides direct access to the per-face region information.
        BufferAccess<region_index>& faceRegions() { return _faceRegions; }

    private:
        SurfaceMeshTopology* _topo;
        BufferAccess<region_index> _faceRegions;
    };

public:

    /// Constructor that takes an existing SurfaceMesh object.
    explicit SurfaceMeshBuilder(SurfaceMesh* mesh);

#ifdef OVITO_DEBUG
    /// Destructor.
    ~SurfaceMeshBuilder();
#endif

    /// Resets the surface mesh structure by discarding all existing vertices, faces and regions.
    void clearMesh();

    /// Returns the mutable surface mesh object.
    SurfaceMesh* mutableMesh() { return const_cast<SurfaceMesh*>(mesh()); }

    /// Sets the index of the space-filling spatial region.
    void setSpaceFillingRegion(region_index region) { mutableMesh()->setSpaceFillingRegion(region); }

    /// Replaces the simulation box of the mesh.
    void setDomain(const SimulationCellObject* domain) {
        mutableMesh()->setDomain(domain);
        _domain = domain;
    }

    /// Returns the surface mesh topology after making sure it is mutable.
    SurfaceMeshTopology* mutableTopology() {
        if(!_mutableTopology) {
            _mutableTopology = mutableMesh()->makeMutable(topology());
            SurfaceMeshReadAccess::_topology = _mutableTopology;
        }
        return _mutableTopology;
    }

    /// Returns the surface mesh vertex property container after making sure it is mutable.
    SurfaceMeshVertices* mutableVertices() {
        if(!_mutableVertices) {
            _mutableVertices = mutableMesh()->makeMutable(vertices());
            SurfaceMeshReadAccess::_vertices = _mutableVertices;
        }
        return _mutableVertices;
    }

    /// Returns the surface mesh face property container after making sure it is mutable.
    SurfaceMeshFaces* mutableFaces() {
        if(!_mutableFaces) {
            _mutableFaces = mutableMesh()->makeMutable(faces());
            SurfaceMeshReadAccess::_faces = _mutableFaces;
        }
        return _mutableFaces;
    }

    /// Returns the surface mesh region property container after making sure it is mutable.
    SurfaceMeshRegions* mutableRegions() {
        if(!_mutableRegions) {
            _mutableRegions = mutableMesh()->makeMutable(regions());
            SurfaceMeshReadAccess::_regions = _mutableRegions;
        }
        return _mutableRegions;
    }

    /// Creates a specified number of new vertices in the mesh without initializing their positions.
    /// Returns the index of first newly created vertex.
    vertex_index createVertices(size_type count) {
        OVITO_ASSERT(vertices()->elementCount() == vertexCount());
        // Update the mesh topology.
        size_type vidx = mutableTopology()->createVertices(count);
        // Grow the vertex property arrays.
        mutableVertices()->setElementCount(vertexCount());
        OVITO_ASSERT(vertexCount() == vidx + count);
        return vidx;
    }

    /// Creates several new vertices and initializes their positions.
    template<typename CoordinatesRange>
    vertex_index createVerticesRange(CoordinatesRange coordRange) {
        auto nverts = std::distance(std::begin(coordRange), std::end(coordRange));
        vertex_index startIndex = createVertices(nverts);
        BufferAccess<Point3> vertexPositions(mutableVertexProperty(SurfaceMeshVertices::PositionProperty));
        boost::copy(std::forward<CoordinatesRange>(coordRange), std::next(vertexPositions.begin(), startIndex));
        return startIndex;
    }

    /// Returns one of the standard vertex properties (or null if the property is not defined).
    PropertyObject* mutableVertexProperty(SurfaceMeshVertices::Type ptype) {
        return mutableVertices()->getMutableProperty(ptype);
    }

    /// Returns one of the standard face properties (or null if the property is not defined).
    PropertyObject* mutableFaceProperty(SurfaceMeshFaces::Type ptype) {
        return mutableFaces()->getMutableProperty(ptype);
    }

    /// Returns one of the standard region properties (or null if the property is not defined).
    PropertyObject* mutableRegionProperty(SurfaceMeshRegions::Type ptype) {
        return mutableRegions()->getMutableProperty(ptype);
    }

    /// Returns a user vertex property (or null if the property is not defined).
    PropertyObject* mutableVertexProperty(const QString& name) {
        return mutableVertices()->getMutableProperty(name);
    }

    /// Returns a user face property (or null if the property is not defined).
    PropertyObject* mutableFaceProperty(const QString& name) {
        return mutableFaces()->getMutableProperty(name);
    }

    /// Attaches an existing property object to the vertices of the mesh.
    void addVertexProperty(const PropertyObject* property) {
        OVITO_ASSERT(property->type() == 0 || vertices()->getProperty(property->type()) == nullptr);
        mutableVertices()->addProperty(property);
    }

    /// Attaches an existing property object to the faces of the mesh.
    void addFaceProperty(const PropertyObject* property) {
        OVITO_ASSERT(property->type() == 0 || faces()->getProperty(property->type()) == nullptr);
        mutableFaces()->addProperty(property);
    }

    /// Attaches an existing property object to the regions of the mesh.
    void addRegionProperty(const PropertyObject* property) {
        OVITO_ASSERT(property->type() == 0 || regions()->getProperty(property->type()) == nullptr);
        mutableRegions()->addProperty(property);
    }

    /// Adds a new standard face property to the mesh.
    PropertyObject* createVertexProperty(DataBuffer::BufferInitialization init, SurfaceMeshVertices::Type ptype) {
        return mutableVertices()->createProperty(init, ptype);
    }

    /// Adds a new standard face property to the mesh.
    PropertyObject* createFaceProperty(DataBuffer::BufferInitialization init, SurfaceMeshFaces::Type ptype) {
        return mutableFaces()->createProperty(init, ptype);
    }

    /// Adds a new standard region property to the mesh.
    PropertyObject* createRegionProperty(DataBuffer::BufferInitialization init, SurfaceMeshRegions::Type ptype) {
        return mutableRegions()->createProperty(init, ptype);
    }

    /// Add a new user-defined vertex property to the mesh.
    PropertyObject* createVertexProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) {
        return mutableVertices()->createProperty(init, name, dataType, componentCount, std::move(componentNames));
    }

    /// Add a new user-defined face property to the mesh.
    PropertyObject* createFaceProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) {
        return mutableFaces()->createProperty(init, name, dataType, componentCount, std::move(componentNames));
    }

    /// Add a new user-defined region property to the mesh.
    PropertyObject* createRegionProperty(DataBuffer::BufferInitialization init, const QString& name, int dataType, size_t componentCount = 1, QStringList componentNames = QStringList()) {
        return mutableRegions()->createProperty(init, name, dataType, componentCount, std::move(componentNames));
    }

    /// Deletes one of the standard properties associated with the mesh regions.
    void removeRegionProperty(SurfaceMeshRegions::Type ptype) {
        if(const PropertyObject* property = regions()->getProperty(ptype))
            mutableRegions()->removeProperty(property);
    }

    /// Tries to wire each half-edge with its opposite (reverse) half-edge.
    /// Returns true if every half-edge has an opposite half-edge, i.e. if the mesh
    /// is closed after this method returns.
    bool connectOppositeHalfedges() { return mutableTopology()->connectOppositeHalfedges(); }

    /// Links two opposite faces together.
    void linkOppositeFaces(face_index face1, face_index face2) { mutableTopology()->linkOppositeFaces(face1, face2); }

    /// Links two opposite half-edges together.
    void linkOppositeEdges(edge_index edge1, edge_index edge2) { mutableTopology()->linkOppositeEdges(edge1, edge2); }

    /// Sets what is the next incident manifold when going around the given half-edge.
    void setNextManifoldEdge(edge_index edge, edge_index nextEdge) { mutableTopology()->setNextManifoldEdge(edge, nextEdge); }

    /// Transfers a segment of a face boundary, formed by the given edge and its successor edge,
    /// to a different vertex.
    void transferFaceBoundaryToVertex(edge_index edge, vertex_index newVertex) { mutableTopology()->transferFaceBoundaryToVertex(edge, newVertex); }

    /// Creates a new half-edge connecting the two vertices of an existing edge in reverse direction
    /// and which is adjacent to the given face. Returns the index of the new half-edge.
    edge_index createOppositeEdge(edge_index edge, face_index face) { return mutableTopology()->createOppositeEdge(edge, face); }

    /// Deletes all faces from the mesh for which the bit in the given mask array is set.
    /// Holes in the mesh will be left behind at the location of the deleted faces.
    /// The half-edges of the faces are also disconnected from their respective opposite half-edges and deleted by this method.
    void deleteFaces(const boost::dynamic_bitset<>& mask);

    /// Deletes all regions from the mesh for which the bit in the given mask array is set.
    /// This method assumes that the deleted regions are not referenced by any other part of the mesh.
    void deleteRegions(const boost::dynamic_bitset<>& mask);

    /// Joins pairs of triangular faces to form quadrilateral faces.
    void makeQuadrilateralFaces();

    /// Deletes all vertices from the mesh which are not connected to any half-edge.
    void deleteIsolatedVertices();

    /// Transforms all vertices of the mesh with the given affine transformation matrix.
    void transformVertices(const AffineTransformation tm) {
        for(Point3& p : BufferAccess<Point3>(mutableVertexProperty(SurfaceMeshVertices::PositionProperty)))
            p = tm * p;
    }

    /// Flips the orientation of all faces in the mesh.
    void flipFaces() { mutableTopology()->flipFaces(); }

    /// Fairs the surface mesh.
    bool smoothMesh(int numIterations, ProgressingTask& task, FloatType k_PB = FloatType(0.1), FloatType lambda = FloatType(0.5));

    /// Splits a face along the edge given by the second vertices of two of its border edges.
    edge_index splitFace(edge_index edge1, edge_index edge2, FaceGrower& faceGrower);

    /// Constructs the convex hull from a set of points and adds the resulting polyhedron to the mesh.
    void constructConvexHull(std::vector<Point3> vecs, SurfaceMesh::region_index region = InvalidIndex, FloatType epsilon = FLOATTYPE_EPSILON);

    /// Joins adjacent faces that are coplanar.
    void joinCoplanarFaces(FloatType thresholdAngle = qDegreesToRadians(0.01));

    /// Duplicates any vertices that are shared by more than one manifold.
    /// The method may only be called on a closed mesh.
    /// Returns the number of vertices that were duplicated by the method.
    size_type makeManifold();

    /// Inserts a new vertex in the middle of an existing edge.
    vertex_index splitEdge(edge_index edge, const Point3& pos, VertexGrower& grower) {
        vertex_index new_v = grower.createVertex(pos);
        _mutableTopology->splitEdge(edge, new_v);
        return new_v;
    }

private:

    SurfaceMeshTopology* _mutableTopology = nullptr; ///< The topology of the surface mesh after it was made mutable.
    SurfaceMeshVertices* _mutableVertices = nullptr; ///< The vertex property container of the surface mesh after it was made mutable.
    SurfaceMeshFaces* _mutableFaces = nullptr; ///< The face property container of the surface mesh after it was made mutable.
    SurfaceMeshRegions* _mutableRegions = nullptr; ///< The region property container of the surface mesh after it was made mutable.
};

}   // End of namespace
