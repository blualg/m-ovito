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
#include <ovito/mesh/surface/SurfaceMeshTopology.h>
#include <ovito/mesh/surface/SurfaceMeshVertices.h>
#include <ovito/mesh/surface/SurfaceMeshFaces.h>
#include <ovito/mesh/surface/SurfaceMeshRegions.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainerAccess.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/data/DataObjectAccess.h>

namespace Ovito::Mesh {

/**
 * Utility class that provides efficient access to the data of a surface mesh object.
 */
class OVITO_MESH_EXPORT SurfaceMeshAccess
{
public:

    // Indexing data types:
    using size_type = SurfaceMesh::size_type;
    using vertex_index = SurfaceMesh::vertex_index;
    using edge_index = SurfaceMesh::edge_index;
    using face_index = SurfaceMesh::face_index;
    using region_index = SurfaceMesh::region_index;

    // Property container access types:
    using VerticesAccess = PropertyContainerAccess<SurfaceMeshVertices::PositionProperty>;
    using FacesAccess = PropertyContainerAccess<SurfaceMeshFaces::RegionProperty, SurfaceMeshFaces::BurgersVectorProperty, SurfaceMeshFaces::FaceTypeProperty, SurfaceMeshFaces::CrystallographicNormalProperty>;
    using RegionsAccess = PropertyContainerAccess<SurfaceMeshRegions::PhaseProperty, SurfaceMeshRegions::VolumeProperty, SurfaceMeshRegions::SurfaceAreaProperty>;

    /// Special constant used to indicate an invalid list index (-1).
    constexpr static size_type InvalidIndex = SurfaceMesh::InvalidIndex;

    /// Constructor that takes an existing SurfaceMesh object.
    explicit SurfaceMeshAccess(const SurfaceMesh* mesh = nullptr);

    /// Destructor. Makes sure we don't leave the modified surface mesh in an inconsistent state.
    ~SurfaceMeshAccess() { reset(); }

	/// Indicates whether this accessor contains a valid surface mesh object. 
	explicit operator bool() const noexcept { return (bool)_mesh; }

    /// Releases the current mesh from this accessor and loads a new one.
    OORef<const SurfaceMesh> reset(const SurfaceMesh* newMesh = nullptr) noexcept;

	/// Releases the SurfaceMesh after it was modified.
	OORef<const SurfaceMesh> take() noexcept { return reset(); }

    /// Exchanges the contents of this data structure with another structure.
    void swap(SurfaceMeshAccess& other) noexcept {
        _mesh.swap(other._mesh);
        _topology.swap(other._topology);
        _vertices.swap(other._vertices);
        _faces.swap(other._faces);
        _regions.swap(other._regions);
    }

    /// Returns the topology of the surface mesh.
    const SurfaceMeshTopology* topology() const { return _topology; }

    /// Returns the number of vertices in the mesh.
    size_type vertexCount() const { 
        OVITO_ASSERT(topology()->vertexCount() == vertices().elementCount());
        return static_cast<size_type>(vertices().elementCount());
    }

    /// Returns the number of faces in the mesh.
    size_type faceCount() const { 
        OVITO_ASSERT(topology()->faceCount() == faces().elementCount()); 
        return static_cast<size_type>(faces().elementCount()); 
    }

    /// Returns the number of half-edges in the mesh.
    size_type edgeCount() const { return topology()->edgeCount(); }

    /// Returns the number of spatial regions defined for the mesh.
    size_type regionCount() const { return static_cast<size_type>(regions().elementCount()); }

    /// Returns the index of the space-filling spatial region.
    region_index spaceFillingRegion() const { return mesh()->spaceFillingRegion(); }

    /// Sets the index of the space-filling spatial region.
    void setSpaceFillingRegion(region_index region) { mutableMesh()->setSpaceFillingRegion(region); }

    /// Returns whether the "Region" face property is defined in this mesh.
    bool hasFaceRegions() const { return faces().hasProperty<SurfaceMeshFaces::RegionProperty>(); }

    /// Returns the spatial region which the given face belongs to.
    region_index faceRegion(face_index face) const {
        OVITO_ASSERT(face >= 0 && face < faceCount());
        return faces().getPropertyValue<SurfaceMeshFaces::RegionProperty, region_index>(face);
    }

    /// Sets the cluster a dislocation/slip face is embedded in.
    void setFaceRegion(face_index face, region_index region) { 
        OVITO_ASSERT(face >= 0 && face < faceCount()); 
        mutableFaces().setPropertyValue<SurfaceMeshFaces::RegionProperty, region_index>(face, region);
    }

    /// Returns a mutable range over the 'Region' property values of the mesh faces.
    auto mutableFaceRegions() { return mutableFaces().mutablePropertyRange<SurfaceMeshFaces::RegionProperty, region_index>(); }

    /// Returns the spatial region which the given mesh edge belongs to.
    region_index edgeRegion(edge_index edge) const { return faceRegion(adjacentFace(edge)); }

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

    /// Sets what is the next incident manifold when going around the given half-edge.
    void setNextManifoldEdge(edge_index edge, edge_index nextEdge) {
        mutableTopology()->setNextManifoldEdge(edge, nextEdge);
    }

    /// Determines the number of manifolds adjacent to a half-edge.
    int countManifolds(edge_index edge) const { return topology()->countManifolds(edge); }

    /// Resets the surface mesh structure by discarding all existing vertices, faces and regions.
    void clearMesh() {
        mutableVertices().truncateElements(vertexCount());
        mutableFaces().truncateElements(faceCount());
        mutableRegions().truncateElements(regionCount());
        mutableTopology()->clear();
        mutableMesh()->setSpaceFillingRegion(InvalidIndex);
        OVITO_ASSERT(vertexCount() == 0);
        OVITO_ASSERT(faceCount() == 0);
        OVITO_ASSERT(regionCount() == 0);
    }

    /// Returns the position of the i-th mesh vertex.
    const Point3& vertexPosition(vertex_index vertex) const {
        OVITO_ASSERT(vertex >= 0 && vertex < vertexCount());
        return vertices().getPropertyValue<SurfaceMeshVertices::PositionProperty, Point3>(vertex);
    }

    /// Sets the position of the i-th mesh vertex.
    void setVertexPosition(vertex_index vertex, const Point3& coords) {
        OVITO_ASSERT(vertex >= 0 && vertex < vertexCount());
        mutableVertices().setPropertyValue<SurfaceMeshVertices::PositionProperty, Point3>(vertex, coords);
    }

    /// Returns a read-only range over the 'Position' property values of the mesh vertices.
    auto vertexPositions() const { return vertices().propertyRange<SurfaceMeshVertices::PositionProperty, Point3>(); }    

    /// Returns a mutable range over the 'Position' property values of the mesh vertices.
    auto mutableVertexPositions() { return mutableVertices().mutablePropertyRange<SurfaceMeshVertices::PositionProperty, Point3>(); }    

    /// Creates a specified number of new vertices in the mesh without initializing their positions.
    /// Returns the index of first newly created vertex.
    vertex_index createVertices(size_type count) {
        // Update the mesh topology.
        size_type vidx = mutableTopology()->createVertices(count);
        // Grow the vertex property arrays.
        mutableVertices().growElements(count);
        OVITO_ASSERT(vertexCount() == vidx + count);
        return vidx;
    }

    /// Creates several new vertices and initializes their coordinates.
    template<typename CoordinatesIterator>
    vertex_index createVertices(CoordinatesIterator begin, CoordinatesIterator end) {
        auto nverts = std::distance(begin, end);
        vertex_index startIndex = createVertices(nverts);
	    std::copy(std::move(begin), std::move(end), std::next(std::begin(mutableVertexPositions()), startIndex));
        return startIndex;
    }

    /// Creates a new vertex at the given coordinates.
    vertex_index createVertex(const Point3& pos) {
        vertex_index vidx = createVertices(1);
        mutableVertices().setPropertyValue<SurfaceMeshVertices::PositionProperty, Point3>(vidx, pos);
        return vidx;
    }

    /// Deletes a vertex from the mesh.
    /// This method assumes that the vertex is not connected to any part of the mesh.
    void deleteVertex(vertex_index vertex) {
        if(vertex < vertexCount() - 1) {
            // Move the last vertex to the index of the vertex being deleted.
            mutableVertices().moveElement(vertexCount() - 1, vertex);
        }
        // Truncate the vertex property arrays by one element.
        mutableVertices().truncateElements(1);
        // Update mesh topology.
        mutableTopology()->deleteVertex(vertex);
    }

    /// Creates a new face, and optionally also the half-edges surrounding it.
    /// Returns the index of the new face.
    face_index createFace(std::initializer_list<vertex_index> vertices, region_index faceRegion = InvalidIndex) {
        return createFace(vertices.begin(), vertices.end(), faceRegion);
    }

    /// Creates a new face, and optionally also the half-edges surrounding it.
    /// Returns the index of the new face.
    template<typename VertexIterator>
    face_index createFace(VertexIterator begin, VertexIterator end, region_index faceRegion = InvalidIndex) {
        // Update the mesh topology.
        face_index fidx = (begin == end) ? mutableTopology()->createFace() : mutableTopology()->createFaceAndEdges(begin, end);
        // Grow the face property arrays.
        mutableFaces().growElements(1);
        mutableFaces().setOptionalPropertyValue<SurfaceMeshFaces::RegionProperty,int>(fidx, faceRegion);
        return fidx;
    }

    /// Splits a face along the edge given by the second vertices of two of its border edges.
    edge_index splitFace(edge_index edge1, edge_index edge2);

    /// Deletes a face from the mesh.
    /// A hole in the mesh will be left behind at the location of the deleted face.
    /// The half-edges of the face are also disconnected from their respective opposite half-edges and deleted by this method.
    void deleteFace(face_index face) {
        if(face < faceCount() - 1) {
            // Move the last face to the index of the face being deleted.
            mutableFaces().moveElement(faceCount() - 1, face);
        }
        // Truncate the face property arrays by one element.
        mutableFaces().truncateElements(1);
        // Update mesh topology.
        mutableTopology()->deleteFace(face);
    }

    /// Deletes all faces from the mesh for which the bit in the given mask array is set.
    /// Holes in the mesh will be left behind at the location of the deleted faces.
    /// The half-edges of the faces are also disconnected from their respective opposite half-edges and deleted by this method.
    void deleteFaces(const boost::dynamic_bitset<>& mask) {
        OVITO_ASSERT(mask.size() == faceCount());
        // Filter and condense the face property arrays.
        mutableFaces().filterResize(mask);
        // Update the mesh topology.
        mutableTopology()->deleteFaces(mask);
    }

    /// Creates a new half-edge between two vertices and adjacent to the given face.
    /// Returns the index of the new half-edge.
    edge_index createEdge(vertex_index vertex1, vertex_index vertex2, face_index face) {
        return mutableTopology()->createEdge(vertex1, vertex2, face);
    }

    /// Creates a new half-edge connecting the two vertices of an existing edge in reverse direction
    /// and which is adjacent to the given face. Returns the index of the new half-edge.
    edge_index createOppositeEdge(edge_index edge, face_index face) {
        return mutableTopology()->createOppositeEdge(edge, face);
    }

    /// Inserts a new vertex in the middle of an existing edge.
    vertex_index splitEdge(edge_index edge, const Point3& pos) {
        vertex_index new_v = createVertex(pos);
        mutableTopology()->splitEdge(edge, new_v);
        return new_v;
    }

    /// Defines a new spatial region.
    region_index createRegion(int phase = 0, FloatType volume = 0, FloatType surfaceArea = 0) {
        // Grow the region property arrays.
        region_index ridx = mutableRegions().growElements(1);
        mutableRegions().setOptionalPropertyValue<SurfaceMeshRegions::PhaseProperty,int>(ridx, phase);
        mutableRegions().setOptionalPropertyValue<SurfaceMeshRegions::VolumeProperty,FloatType>(ridx, volume);
        mutableRegions().setOptionalPropertyValue<SurfaceMeshRegions::SurfaceAreaProperty,FloatType>(ridx, surfaceArea);
        return ridx;
    }

    /// Defines an array of new spatial regions.
    region_index createRegions(size_type numRegions) {
        // Grow the region property arrays.
        region_index ridx = mutableRegions().growElements(numRegions);
        OVITO_ASSERT(regionCount() == ridx + numRegions);
        return ridx;
    }

    /// Deletes a region from the mesh.
    /// This method assumes that the region is not referenced by any other part of the mesh.
    void deleteRegion(region_index region) {
        OVITO_ASSERT(region >= 0 && region < regionCount());
        OVITO_ASSERT(std::none_of(topology()->begin_faces(), topology()->end_faces(), [&](auto face) { return this->faceRegion(face) == region; } ));
        if(region < regionCount() - 1) {
            // Move the last region to the index of the region being deleted.
            mutableRegions().moveElement(regionCount() - 1, region);
            // Update the faces that belong to the moved region.
            if(hasFaceRegions())
                boost::range::replace(mutableFaceRegions(), regionCount() - 1, region);
        }
        // Truncate the region property arrays.
        mutableRegions().truncateElements(1);
    }

    /// Deletes all region from the mesh for which the bit in the given mask array is set.
    /// This method assumes that the deleted regions are not referenced by any other part of the mesh.
    void deleteRegions(const boost::dynamic_bitset<>& mask) {
        OVITO_ASSERT(mask.size() == regionCount());

        // Update the region property of faces.
        if(hasFaceRegions()) {
            // Build a mapping from old region indices to new indices. 
            std::vector<region_index> remapping(regionCount());
            size_type newRegionCount = 0;
            for(region_index region = 0; region < regionCount(); region++) {
                if(mask.test(region))
                    remapping[region] = InvalidIndex;
                else
                    remapping[region] = newRegionCount++;
            }
            for(auto& fr : mutableFaceRegions()) {
                if(fr >= 0 && fr < regionCount())
                    fr = remapping[fr];
            }
        }

        // Filter and condense the region property arrays.
        mutableRegions().filterResize(mask);
    }

    /// Returns the volume of the i-th region.
    FloatType regionVolume(region_index region) const {
        OVITO_ASSERT(regions().hasProperty<SurfaceMeshRegions::VolumeProperty>());
        OVITO_ASSERT(region >= 0 && region < regionCount());
        return regions().getPropertyValue<SurfaceMeshRegions::VolumeProperty,FloatType>(region);
    }

    /// Sets the stored volume of the i-th region.
    void setRegionVolume(region_index region, FloatType vol) {
        OVITO_ASSERT(regions().hasProperty<SurfaceMeshRegions::VolumeProperty>());
        OVITO_ASSERT(region >= 0 && region < regionCount());
        mutableRegions().setPropertyValue<SurfaceMeshRegions::VolumeProperty,FloatType>(region, vol);
    }

    /// Sets the stored total surface area of the i-th region.
    void setRegionSurfaceArea(region_index region, FloatType area) {
        OVITO_ASSERT(regions().hasProperty<SurfaceMeshRegions::SurfaceAreaProperty>());
        OVITO_ASSERT(region >= 0 && region < regionCount());
        mutableRegions().setPropertyValue<SurfaceMeshRegions::SurfaceAreaProperty,FloatType>(region, area);
    }

    /// Returns the total surface area of the i-th region.
    FloatType regionSurfaceArea(region_index region) const {
        OVITO_ASSERT(regions().hasProperty<SurfaceMeshRegions::SurfaceAreaProperty>());
        OVITO_ASSERT(region >= 0 && region < regionCount());
        return regions().getPropertyValue<SurfaceMeshRegions::SurfaceAreaProperty,FloatType>(region);
    }

	/// Returns the phase ID of the given region.
	int regionPhase(region_index region) const {
        OVITO_ASSERT(regions().hasProperty<SurfaceMeshRegions::PhaseProperty>());
		OVITO_ASSERT(region >= 0 && region < regionCount());
		return regions().getPropertyValue<SurfaceMeshRegions::PhaseProperty, int>(region);
	}

    /// Links two opposite half-edges together.
    void linkOppositeEdges(edge_index edge1, edge_index edge2) {
        mutableTopology()->linkOppositeEdges(edge1, edge2);
    }

    /// Links two opposite faces together.
    void linkOppositeFaces(face_index face1, face_index face2) {
        mutableTopology()->linkOppositeFaces(face1, face2);
    }

    /// Transfers a segment of a face boundary, formed by the given edge and its successor edge,
    /// to a different vertex.
    void transferFaceBoundaryToVertex(edge_index edge, vertex_index newVertex) {
        mutableTopology()->transferFaceBoundaryToVertex(edge, newVertex);
    }

    /// Transforms all vertices of the mesh with the given affine transformation matrix.
    void transformVertices(const AffineTransformation tm) {
        for(Point3& p : mutableVertexPositions())
            p = tm * p;
    }

    /// Returns the simulation box the surface mesh is embedded in.
    const SimulationCellObject* cell() const { return _mesh->domain(); }

    /// Replaces the simulation box.
    void setCell(const SimulationCellObject* cell) { mutableMesh()->setDomain(cell); } 

	/// Returns whether the mesh's domain has periodic boundary conditions applied in the given direction.
	bool hasPbc(size_t dim) const { return cell() ? cell()->hasPbc(dim) : false; }

    /// Wraps a vector at periodic boundaries of the simulation cell.
    Vector3 wrapVector(const Vector3& v) const {
        return cell() ? cell()->wrapVector(v) : v;
    }

    /// Wraps a point at periodic boundaries of the simulation cell.
    Point3 wrapPoint(const Point3& p) const {
        return cell() ? cell()->wrapPoint(p) : p;
    }

    /// Returns the vector corresponding to an half-edge of the surface mesh.
    Vector3 edgeVector(edge_index edge) const {
        Vector3 delta = vertexPosition(vertex2(edge)) - vertexPosition(vertex1(edge));
        return cell() ? cell()->wrapVector(delta) : delta;
    }

    /// Flips the orientation of all faces in the mesh.
    void flipFaces() {
        mutableTopology()->flipFaces();
    }

    /// Tries to wire each half-edge with its opposite (reverse) half-edge.
    /// Returns true if every half-edge has an opposite half-edge, i.e. if the mesh
    /// is closed after this method returns.
    bool connectOppositeHalfedges() {
        return mutableTopology()->connectOppositeHalfedges();
    }

    /// Duplicates any vertices that are shared by more than one manifold.
    /// The method may only be called on a closed mesh.
    /// Returns the number of vertices that were duplicated by the method.
    size_type makeManifold() {
        return mutableTopology()->makeManifold([this](vertex_index copiedVertex) {
            // Duplicate the property data of the copied vertex.
            vertex_index newVertex = mutableVertices().growElements(1);
            mutableVertices().copyElement(copiedVertex, newVertex);
        });
    }

	/// Fairs the surface mesh.
	bool smoothMesh(int numIterations, ProgressingTask& task, FloatType k_PB = FloatType(0.1), FloatType lambda = FloatType(0.5));

	/// Determines which spatial region contains the given point in space.
	/// Returns no result if the point is exactly on a region boundary.
	std::optional<std::pair<region_index, FloatType>> locatePoint(const Point3& location, FloatType epsilon = FLOATTYPE_EPSILON, const boost::dynamic_bitset<>& faceSubset = boost::dynamic_bitset<>()) const;

    /// Returns one of the standard vertex properties (or null if the property is not defined).
    const PropertyObject* vertexProperty(SurfaceMeshVertices::Type ptype) const {
        return _vertices.getProperty(ptype);
    }

    /// Returns one of the standard vertex properties (or null if the property is not defined).
    PropertyObject* mutableVertexProperty(SurfaceMeshVertices::Type ptype) {
        return mutableVertices().getMutableProperty(ptype);
    }

    /// Returns a user vertex property (or null if the property is not defined).
    PropertyObject* mutableVertexProperty(const QString& name) {
        return mutableVertices().getMutableProperty(name);
    }

    /// Returns one of the standard face properties (or null if the property is not defined).
    const PropertyObject* faceProperty(SurfaceMeshFaces::Type ptype) const {
        return _faces.getProperty(ptype);
    }

    /// Returns a user face property (or null if the property is not defined).
    const PropertyObject* faceProperty(const QString& name) const {
        return _faces.getProperty(name);
    }

    /// Returns one of the standard face properties (or null if the property is not defined).
    PropertyObject* mutableFaceProperty(SurfaceMeshFaces::Type ptype) {
        return mutableFaces().getMutableProperty(ptype);
    }

    /// Returns a user face property (or null if the property is not defined).
    PropertyObject* mutableFaceProperty(const QString& name) {
        return mutableFaces().getMutableProperty(name);
    }

    /// Returns one of the standard region properties (or null if the property is not defined).
    const PropertyObject* regionProperty(SurfaceMeshRegions::Type ptype) const {
        return _regions.getProperty(ptype);
    }

    /// Adds a new standard vertex property to the mesh.
    PropertyObject* createVertexProperty(SurfaceMeshVertices::Type ptype, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags) {
        return mutableVertices().createProperty(ptype, flags);
    }

	/// Add a new user-defined vertex property to the mesh.
	PropertyObject* createVertexProperty(const QString& name, int dataType, size_t componentCount = 1, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags, QStringList componentNames = QStringList()) {
        return mutableVertices().createProperty(name, dataType, componentCount, flags, std::move(componentNames));
	}

    /// Attaches an existing property object to the vertices of the mesh.
    void addVertexProperty(const PropertyObject* property) {
        mutableVertices().addProperty(property);
    }

    /// Deletes one of properties associated with the mesh vertices. 
    void removeVertexProperty(const PropertyObject* property) {
        mutableVertices().removeProperty(property);
    }

    /// Adds a new standard face property to the mesh.
    PropertyObject* createFaceProperty(SurfaceMeshFaces::Type ptype, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags) {
        return mutableFaces().createProperty(ptype, flags);
    }

	/// Add a new user-defined face property to the mesh.
	PropertyObject* createFaceProperty(const QString& name, int dataType, size_t componentCount = 1, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags, QStringList componentNames = QStringList()) {
        return mutableFaces().createProperty(name, dataType, componentCount, flags, std::move(componentNames));
	}

    /// Attaches an existing property object to the faces of the mesh.
    void addFaceProperty(const PropertyObject* property) {
        mutableFaces().addProperty(property);
    }

    /// Deletes one of properties associated with the mesh faces. 
    void removeFaceProperty(const PropertyObject* property) {
        mutableFaces().removeProperty(property);
    }

    /// Adds a new standard region property to the mesh.
    PropertyObject* createRegionProperty(SurfaceMeshRegions::Type ptype, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags) {
        return mutableRegions().createProperty(ptype, flags);
    }

	/// Add a new user-defined region property to the mesh.
	PropertyObject* createRegionProperty(const QString& name, int dataType, size_t componentCount = 1, DataBuffer::InitializationFlags flags = DataBuffer::NoFlags, QStringList componentNames = QStringList()) {
        return mutableRegions().createProperty(name, dataType, componentCount, flags, std::move(componentNames));
	}

    /// Attaches an existing property object to the regions of the mesh.
    void addRegionProperty(const PropertyObject* property) {
        mutableRegions().addProperty(property);
    }

    /// Deletes one of the standard properties associated with the mesh regions. 
    void removeRegionProperty(SurfaceMeshRegions::Type ptype) {
        if(const PropertyObject* property = regions().getProperty(ptype))
            mutableRegions().removeProperty(property);
    }

    /// Deletes one of properties associated with the mesh regions. 
    void removeRegionProperty(const PropertyObject* property) {
        mutableRegions().removeProperty(property);
    }

    /// Constructs the convex hull from a set of points and adds the resulting polyhedron to the mesh.
    void constructConvexHull(std::vector<Point3> vecs, FloatType epsilon = FLOATTYPE_EPSILON);

    /// Joins adjacent faces that are coplanar.
    void joinCoplanarFaces(FloatType thresholdAngle = qDegreesToRadians(0.01));

    /// Joins pairs of triangular faces to form quadrilateral faces.
    void makeQuadrilateralFaces();

    /// Deletes all vertices from the mesh which are not connected to any half-edge.
    void deleteIsolatedVertices();

    /// Triangulates the polygonal faces of this mesh and outputs the results as a TriMeshObject.
    void convertToTriMesh(TriMeshObject& outputMesh, bool smoothShading, const boost::dynamic_bitset<>& faceSubset = boost::dynamic_bitset<>{}, std::vector<size_t>* originalFaceMap = nullptr, bool autoGenerateOppositeFaces = false) const;

    /// Computes the unit normal vector of a mesh face.
    Vector3 computeFaceNormal(face_index face) const;

protected:

    /// Returns the surface mesh object managed by this class.
    const SurfaceMesh* mesh() const { return _mesh; }

    /// Returns the vertex property container of the surface mesh.
    const VerticesAccess& vertices() const { return _vertices; }

    /// Returns the face property container of the surface mesh.
    const FacesAccess& faces() const { return _faces; }

    /// Returns the region property container of the surface mesh.
    const RegionsAccess& regions() const { return _regions; }

    /// Makes sure the surface mesh managed by this class is safe to modify.
    /// Automatically creates a copy of the surface mesh object if necessary.
    SurfaceMesh* mutableMesh() { return _mesh.makeMutable(); }

    /// Returns the topology of the surface mesh that is ready for being modified.
    SurfaceMeshTopology* mutableTopology() { return _topology.makeMutable(); }

    /// Returns the vertex property container of the mutable surface mesh.
    VerticesAccess& mutableVertices() { return _vertices; }

    /// Returns the face property container of the mutable surface mesh.
    FacesAccess& mutableFaces() { return _faces; }

    /// Returns the regions property container of the mutable surface mesh.
    RegionsAccess& mutableRegions() { return _regions; }

private:

    DataObjectAccess<OORef, SurfaceMesh> _mesh;                   ///< The surface mesh data object managed by this class.
    DataObjectAccess<OORef, SurfaceMeshTopology> _topology;       ///< The topology of the surface mesh.
    VerticesAccess _vertices;   ///< Provides access to the vertex property container of the surface mesh.
    FacesAccess _faces;         ///< Provides access to the face property container of the surface mesh.
    RegionsAccess _regions;     ///< Provides access to the region property container of the surface mesh.
};

}	// End of namespace
