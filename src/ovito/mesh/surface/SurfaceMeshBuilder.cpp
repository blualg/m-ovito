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

#include <ovito/mesh/Mesh.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "SurfaceMeshBuilder.h"

namespace Ovito::Mesh {

/******************************************************************************
* Constructor that takes an existing SurfaceMesh object.
******************************************************************************/
SurfaceMeshBuilder::SurfaceMeshBuilder(SurfaceMesh* mesh) : SurfaceMeshReadAccess(mesh)
{
    OVITO_ASSERT(!mesh || mesh->isSafeToModify());
}

#ifdef OVITO_DEBUG
/******************************************************************************
* Destructor.
******************************************************************************/
SurfaceMeshBuilder::~SurfaceMeshBuilder()
{
    if(mesh())
        mesh()->verifyMeshIntegrity();
}
#endif

/******************************************************************************
* Resets the surface mesh structure by discarding all existing vertices, faces and regions.
******************************************************************************/
void SurfaceMeshBuilder::clearMesh()
{
    mutableVertices()->setElementCount(0);
    mutableFaces()->setElementCount(0);
    mutableRegions()->setElementCount(0);
    mutableTopology()->clear();
    mutableMesh()->setSpaceFillingRegion(InvalidIndex);
    OVITO_ASSERT(vertexCount() == 0);
    OVITO_ASSERT(faceCount() == 0);
    OVITO_ASSERT(regionCount() == 0);
}

/******************************************************************************
* Deletes all regions from the mesh for which the bit in the given mask array is set.
* This method assumes that the deleted regions are not referenced by any other part of the mesh.
******************************************************************************/
void SurfaceMeshBuilder::deleteRegions(const boost::dynamic_bitset<>& mask)
{
    OVITO_ASSERT(mask.size() == regionCount());

    // Update the region property of faces.
    if(BufferWriteAccess<int32_t, access_mode::read_write> faceRegions = mutableFaceProperty(SurfaceMeshFaces::RegionProperty)) {
        // Build a mapping from old region indices to new indices.
        std::vector<region_index> remapping(regionCount());
        size_type newRegionCount = 0;
        for(region_index region = 0; region < regionCount(); region++) {
            if(mask.test(region))
                remapping[region] = InvalidIndex;
            else
                remapping[region] = newRegionCount++;
        }
        for(auto& fr : faceRegions) {
            if(fr >= 0 && fr < regionCount())
                fr = remapping[fr];
        }
    }

    // Filter and condense the region property arrays.
    mutableRegions()->PropertyContainer::deleteElements(mask);
}

/******************************************************************************
* Deletes all faces from the mesh for which the bit in the given mask array is set.
* Holes in the mesh will be left behind at the location of the deleted faces.
* The half-edges of the faces are also disconnected from their respective opposite half-edges and deleted by this method.
******************************************************************************/
void SurfaceMeshBuilder::deleteFaces(const boost::dynamic_bitset<>& mask)
{
    OVITO_ASSERT(mask.size() == faceCount());

    // Filter and condense the face property arrays.
    mutableFaces()->PropertyContainer::deleteElements(mask);

    // Update the mesh topology.
    mutableTopology()->deleteFaces(mask);

    OVITO_ASSERT(topology()->faceCount() == faces()->elementCount());
}

/******************************************************************************
* Duplicates any vertices that are shared by more than one manifold.
* The method may only be called on a closed mesh.
* Returns the number of vertices that were duplicated by the method.
******************************************************************************/
SurfaceMesh::size_type SurfaceMeshBuilder::makeManifold()
{
    VertexGrower vertexGrower(*this);

    size_type numSharedVertices = 0;
    size_type oldVertexCount = vertexCount();

    // Stack of edges of the current manifold still to be visited.
    QVarLengthArray<edge_index, 16> edgesToVisit;

    // Edges that have been marked as visited.
    boost::dynamic_bitset<> visitedEdges(edgeCount());

    for(vertex_index vertex = 0; vertex < oldVertexCount; vertex++) {
        // Count the number of half-edges incident on the current vertex.
        size_type numVertexEdges = vertexEdgeCount(vertex);
        OVITO_ASSERT(numVertexEdges >= 2);

        edge_index firstEdge = firstVertexEdge(vertex);
        size_type numManifoldEdges = 0;

        // Initialize the stack of edges to be visited.
        visitedEdges.set(firstEdge);
        edgesToVisit.push_back(firstEdge);
        do {
            // Take the next edge from the stack.
            edge_index currentEdge = edgesToVisit.back();
            edgesToVisit.pop_back();

            // Verify integrity of mesh structure.
            OVITO_ASSERT(currentEdge != InvalidIndex); // Mesh must be closed.
            OVITO_ASSERT(adjacentFace(currentEdge) != InvalidIndex); // Every edge must be connected to a face.
            OVITO_ASSERT(prevFaceEdge(currentEdge) != InvalidIndex); // Every edge must be preceded by another edge along the same face.
            OVITO_ASSERT(vertex1(currentEdge) == vertex);   // Edge must be incident on the current vertex.

            // Count the current edge.
            numManifoldEdges++;

            // Visit all manifolds that share the current edge.
            edge_index edge = nextManifoldEdge(currentEdge);
            while(edge != InvalidIndex) {
                if(!visitedEdges.test(edge)) {
                    // Put the next edge onto the stack.
                    visitedEdges.set(edge);
                    edgesToVisit.push_back(edge);
                }
                edge = nextManifoldEdge(edge);
                if(edge == currentEdge) break;
            }

            // Go in positive direction around the vertex, facet by facet.
            edge_index nextManifoldEdge = oppositeEdge(prevFaceEdge(currentEdge));
            OVITO_ASSERT(nextManifoldEdge != InvalidIndex);
            if(!visitedEdges.test(nextManifoldEdge)) {
                // Put the next edge in the current manifold onto the stack.
                visitedEdges.set(nextManifoldEdge);
                edgesToVisit.push_back(nextManifoldEdge);
            }
        }
        while(!edgesToVisit.empty());

        // If the number of edges in the first manifold is equal to the total number of edges
        // incident on the vertex, then the vertex is not part of separate manifolds and we are done.
        if(numManifoldEdges == numVertexEdges)
            continue;
        OVITO_ASSERT(numManifoldEdges < numVertexEdges);

        // Now identify the other manifolds and create a vertex copy for each.
        do {
            // Create a second vertex that will receive the edges not visited yet.
            // Copy all properties of the original vertex to its duplicate.
            vertex_index newVertex = vertexGrower.copyVertex(vertex);

            // Iterate over the edges of the vertex until we find the next one that
            // hasn't been visited yet. This edge will by used to start the new manifold.
            for(firstEdge = firstVertexEdge(vertex); firstEdge != InvalidIndex; firstEdge = nextVertexEdge(firstEdge)) {
                if(!visitedEdges.test(firstEdge))
                    break;
            }
            OVITO_ASSERT(firstEdge != InvalidIndex);

            // Initialize the stack of edges to be visited.
            visitedEdges.set(firstEdge);
            edgesToVisit.push_back(firstEdge);
            do {
                // Take the next edge from the stack.
                edge_index currentEdge = edgesToVisit.back();
                edgesToVisit.pop_back();

                // Verify integrity of mesh structure.
                OVITO_ASSERT(currentEdge != InvalidIndex); // Mesh must be closed.
                OVITO_ASSERT(adjacentFace(currentEdge) != InvalidIndex); // Every edge must be connected to a face.
                OVITO_ASSERT(prevFaceEdge(currentEdge) != InvalidIndex); // Every edge must be preceded by another edge along the same face.

                // Transfer current edge to new vertex.
                OVITO_ASSERT(firstVertexEdge(vertex) != currentEdge);
                mutableTopology()->transferEdgeToVertex(currentEdge, vertex, newVertex);

                // Count the current edge.
                numManifoldEdges++;

                // Visit all manifolds that share the current edge.
                edge_index edge = nextManifoldEdge(currentEdge);
                while(edge != InvalidIndex) {
                    if(!visitedEdges.test(edge)) {
                        // Put the next edge onto the stack.
                        visitedEdges.set(edge);
                        edgesToVisit.push_back(edge);
                    }
                    edge = nextManifoldEdge(edge);
                    if(edge == currentEdge) break;
                }

                // Go in positive direction around the vertex, facet by facet.
                edge_index nextManifoldEdge = oppositeEdge(prevFaceEdge(currentEdge));
                OVITO_ASSERT(nextManifoldEdge != InvalidIndex);
                if(!visitedEdges.test(nextManifoldEdge)) {
                    // Put the next edge in the current manifold onto the stack.
                    visitedEdges.set(nextManifoldEdge);
                    edgesToVisit.push_back(nextManifoldEdge);
                }
            }
            while(!edgesToVisit.empty());
        }
        while(numManifoldEdges != numVertexEdges);

        numSharedVertices++;
    }

    return numSharedVertices;
}

/******************************************************************************
* Joins pairs of triangular faces to form quadrilateral faces.
******************************************************************************/
void SurfaceMeshBuilder::makeQuadrilateralFaces()
{
    // Get access to the vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));

    FaceGrower faceGrower(*this);

    // Visit each triangular face and its adjacent faces.
    for(face_index face = 0; face < faceCount(); ) {

        // Determine the longest edge of the current face and check if it is a triangle.
        // Find the longest edge of the three edges.
        edge_index faceEdge = firstFaceEdge(face);
        edge_index edge = faceEdge;
        int edgeCount = 0;
        edge_index longestEdge;
        FloatType longestEdgeLengthSq = 0;
        do {
            edgeCount++;
            FloatType edgeLengthSq = edgeVector(edge, vertexPositions).squaredLength();
            if(edgeLengthSq >= longestEdgeLengthSq) {
                longestEdgeLengthSq = edgeLengthSq;
                longestEdge = edge;
            }
            edge = nextFaceEdge(edge);
        }
        while(edge != faceEdge);

        // Skip face if it is not a triangle.
        if(edgeCount != 3) {
            face++;
            continue;
        }
        face_index nextFace = face + 1;

        // Check if the adjacent face exists and is also a triangle.
        edge = longestEdge;
        edge_index opp_edge = oppositeEdge(edge);
        if(opp_edge != InvalidIndex) {
            face_index adj_face = adjacentFace(opp_edge);
            if(adj_face > face && topology()->countFaceEdges(adj_face) == 3) {

                // Eliminate this half-edge pair and join the two faces.
                SurfaceMeshTopology* topo = mutableTopology();
                for(edge_index currentEdge = nextFaceEdge(edge); currentEdge != edge; currentEdge = nextFaceEdge(currentEdge)) {
                    OVITO_ASSERT(topo->adjacentFace(currentEdge) == face);
                    topo->setAdjacentFace(currentEdge, adj_face);
                }
                topo->setFirstFaceEdge(adj_face, topo->nextFaceEdge(opp_edge));
                topo->setFirstFaceEdge(face, edge);
                topo->setNextFaceEdge(topo->prevFaceEdge(edge), topo->nextFaceEdge(opp_edge));
                topo->setPrevFaceEdge(topo->nextFaceEdge(opp_edge), topo->prevFaceEdge(edge));
                topo->setNextFaceEdge(topo->prevFaceEdge(opp_edge), topo->nextFaceEdge(edge));
                topo->setPrevFaceEdge(topo->nextFaceEdge(edge), topo->prevFaceEdge(opp_edge));
                topo->setNextFaceEdge(edge, opp_edge);
                topo->setNextFaceEdge(opp_edge, edge);
                topo->setPrevFaceEdge(edge, opp_edge);
                topo->setPrevFaceEdge(opp_edge, edge);
                topo->setAdjacentFace(opp_edge, face);
                OVITO_ASSERT(adjacentFace(edge) == face);
                OVITO_ASSERT(topo->countFaceEdges(face) == 2);
                faceGrower.deleteFace(face);
                nextFace = face;
            }
        }
        face = nextFace;
    }
}

/******************************************************************************
* Deletes all vertices from the mesh which are not connected to any half-edge.
******************************************************************************/
void SurfaceMeshBuilder::deleteIsolatedVertices()
{
    VertexGrower vertexGrower(*this);
    for(vertex_index vertex = vertexCount() - 1; vertex >= 0; vertex--) {
        if(firstVertexEdge(vertex) == InvalidIndex) {
            vertexGrower.deleteVertex(vertex);
        }
    }
}

/******************************************************************************
* Fairs a closed triangle mesh.
******************************************************************************/
bool SurfaceMeshBuilder::smoothMesh(int numIterations, ProgressingTask& task, FloatType k_PB, FloatType lambda)
{
    // This is the implementation of the mesh smoothing algorithm:
    //
    // Gabriel Taubin
    // A Signal Processing Approach To Fair Surface Design
    // In SIGGRAPH 95 Conference Proceedings, pages 351-358 (1995)

    // Performs one iteration of the smoothing algorithm.
    auto smoothMeshIteration = [this](FloatType prefactor) {

        BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));

        // Compute displacement for each vertex.
        std::vector<Vector3> displacements(vertexCount());
        parallelFor(vertexCount(), [&](vertex_index vertex) {
            Vector3 d = Vector3::Zero();

            // Go in positive direction around vertex, facet by facet.
            edge_index currentEdge = firstVertexEdge(vertex);
            if(currentEdge != InvalidIndex) {
                int numManifoldEdges = 0;
                do {
                    OVITO_ASSERT(currentEdge != InvalidIndex);
                    OVITO_ASSERT(adjacentFace(currentEdge) != InvalidIndex);
                    d += edgeVector(currentEdge, vertexPositions);
                    numManifoldEdges++;
                    currentEdge = oppositeEdge(prevFaceEdge(currentEdge));
                }
                while(currentEdge != firstVertexEdge(vertex));
                d *= (prefactor / numManifoldEdges);
            }

            displacements[vertex] = d;
        });
        vertexPositions.reset();

        // Apply computed displacements.
        auto d = displacements.cbegin();
        for(Point3& vertex : BufferWriteAccess<Point3, access_mode::read_write>(mutableVertexProperty(SurfaceMeshVertices::PositionProperty)))
            vertex += *d++;
    };

    FloatType mu = FloatType(1) / (k_PB - FloatType(1)/lambda);
    task.setProgressMaximum(numIterations);

    for(int iteration = 0; iteration < numIterations; iteration++) {
        if(!task.setProgressValue(iteration))
            return false;
        smoothMeshIteration(lambda);
        smoothMeshIteration(mu);
    }

    return !task.isCanceled();
}

/******************************************************************************
* Splits a face along the edge given by two vertices of the face.
******************************************************************************/
SurfaceMesh::edge_index SurfaceMeshBuilder::splitFace(edge_index edge1, edge_index edge2, FaceGrower& faceGrower)
{
    OVITO_ASSERT(adjacentFace(edge1) == adjacentFace(edge2));
    OVITO_ASSERT(nextFaceEdge(edge1) != edge2);
    OVITO_ASSERT(prevFaceEdge(edge1) != edge2);
    OVITO_ASSERT(!hasOppositeFace(adjacentFace(edge1)));
    OVITO_ASSERT(faces()->properties().empty()); // Per-face properties are not yet supported by this implementation.

    SurfaceMeshTopology* topo = _mutableTopology;

    face_index old_f = adjacentFace(edge1);
    face_index new_f = faceGrower.copyFace(old_f);

    vertex_index v1 = vertex2(edge1);
    vertex_index v2 = vertex2(edge2);
    edge_index edge1_successor = nextFaceEdge(edge1);
    edge_index edge2_successor = nextFaceEdge(edge2);

    // Create the new pair of half-edges.
    edge_index new_e = topo->createEdge(v1, v2, old_f, edge1);
    edge_index new_oe = createOppositeEdge(new_e, new_f);

    // Rewire edge sequence of the primary face.
    OVITO_ASSERT(prevFaceEdge(new_e) == edge1);
    OVITO_ASSERT(nextFaceEdge(edge1) == new_e);
    topo->setNextFaceEdge(new_e, edge2_successor);
    topo->setPrevFaceEdge(edge2_successor, new_e);

    // Rewire edge sequence of the secondary face.
    topo->setNextFaceEdge(edge2, new_oe);
    topo->setPrevFaceEdge(new_oe, edge2);
    topo->setNextFaceEdge(new_oe, edge1_successor);
    topo->setPrevFaceEdge(edge1_successor, new_oe);

    // Connect the edges with the newly created secondary face.
    edge_index e = edge1_successor;
    do {
        topo->setAdjacentFace(e, new_f);
        e = nextFaceEdge(e);
    }
    while(e != new_oe);
    OVITO_ASSERT(adjacentFace(edge2) == new_f);
    OVITO_ASSERT(adjacentFace(new_oe) == new_f);

    // Make the newly created edge the leading edge of the original face.
    topo->setFirstFaceEdge(old_f, new_e);

    return new_e;
}

/******************************************************************************
* Constructs the convex hull from a set of points and adds the resulting
* polyhedron to the mesh.
******************************************************************************/
void SurfaceMeshBuilder::constructConvexHull(std::vector<Point3> vecs, SurfaceMesh::region_index region, FloatType epsilon)
{
    if(vecs.size() < 4)
        return; // Convex hull requires at least 4 input points.

    // Keep track of how many faces and vertices we started with.
    // We won't touch the existing mesh faces and vertices.
    auto originalFaceCount = faceCount();
    auto originalVertexCount = vertexCount();

    // Determine which points are used to build the initial tetrahedron.
    // Make sure they are not co-planar and the tetrahedron is not degenerate.
    size_t tetrahedraCorners[4];
    tetrahedraCorners[0] = 0;
    Matrix3 m;

    // Find optimal second point.
    FloatType maxVal = epsilon;
    for(size_t i = 1; i < vecs.size(); i++) {
        m.column(0) = vecs[i] - vecs[0];
        FloatType distSq = m.column(0).squaredLength();
        if(distSq > maxVal) {
            maxVal = distSq;
            tetrahedraCorners[1] = i;
        }
    }
    // Convex hull is degenerate if all input points are identitical.
    if(maxVal <= epsilon)
        return;
    m.column(0) = vecs[tetrahedraCorners[1]] - vecs[0];

    // Find optimal third point.
    maxVal = epsilon;
    for(size_t i = 1; i < vecs.size(); i++) {
        if(i == tetrahedraCorners[1])
            continue;
        m.column(1) = vecs[i] - vecs[0];
        FloatType areaSq = m.column(0).cross(m.column(1)).squaredLength();
        if(areaSq > maxVal) {
            maxVal = areaSq;
            tetrahedraCorners[2] = i;
        }
    }
    // Convex hull is degnerate if all input points are co-linear.
    if(maxVal <= epsilon)
        return;
    m.column(1) = vecs[tetrahedraCorners[2]] - vecs[0];

    // Find optimal fourth point.
    maxVal = epsilon;
    bool flipTet;
    for(size_t i = 1; i < vecs.size(); i++) {
        if(i == tetrahedraCorners[1] || i == tetrahedraCorners[2])
            continue;
        m.column(2) = vecs[i] - vecs[0];
        FloatType vol = m.determinant();
        if(vol > maxVal) {
            maxVal = vol;
            flipTet = false;
            tetrahedraCorners[3] = i;
        }
        else if(-vol > maxVal) {
            maxVal = -vol;
            flipTet = true;
            tetrahedraCorners[3] = i;
        }
    }
    // Convex hull is degnerate if all input points are co-planar.
    if(maxVal <= epsilon)
        return;

    // For adding new vertices and faces to the mesh.
    VertexGrower vertexGrower(*this);
    FaceGrower faceGrower(*this);

    // Create the initial tetrahedron.
    vertex_index tetverts[4];
    for(size_t i = 0; i < 4; i++) {
        tetverts[i] = vertexGrower.createVertex(vecs[tetrahedraCorners[i]]);
    }
    if(flipTet)
        std::swap(tetverts[0], tetverts[1]);
    faceGrower.createFace({tetverts[0], tetverts[1], tetverts[3]}, region);
    faceGrower.createFace({tetverts[2], tetverts[0], tetverts[3]}, region);
    faceGrower.createFace({tetverts[0], tetverts[2], tetverts[1]}, region);
    faceGrower.createFace({tetverts[1], tetverts[2], tetverts[3]}, region);
    // Connect opposite half-edges to link the four faces together.
    for(size_t i = 0; i < 4; i++)
        mutableTopology()->connectOppositeHalfedgesAtVertex(tetverts[i]);

    if(vecs.size() == 4)
        return; // If the input point set consists only of 4 points, then we are done after constructing the initial tetrahedron.

    // Remove 4 points of initial tetrahedron from input list.
    std::sort(std::begin(tetrahedraCorners), std::end(tetrahedraCorners), std::greater<>());
    OVITO_ASSERT(tetrahedraCorners[0] > tetrahedraCorners[1]);
    for(size_t i = 0; i < 4; i++)
        vecs[tetrahedraCorners[i]] = vecs[vecs.size()-i-1];
    vecs.erase(vecs.end() - 4, vecs.end());

    // Simplified Quick-hull algorithm.
    while(!vecs.empty()) {
        // Find the point on the positive side of a face and furthest away from it.
        // Also remove points from list which are on the negative side of all faces.
        auto furthestPoint = vecs.rend();
        FloatType furthestPointDistance = 0;
        size_t remainingPointCount = vecs.size();
        for(auto p = vecs.rbegin(); p != vecs.rend(); ++p) {
            bool insideHull = true;
            for(auto faceIndex = originalFaceCount; faceIndex < faceCount(); faceIndex++) {
                auto v0 = firstFaceVertex(faceIndex);
                auto v1 = secondFaceVertex(faceIndex);
                auto v2 = thirdFaceVertex(faceIndex);
                Plane3 plane(vertexGrower.vertexPosition(v0), vertexGrower.vertexPosition(v1), vertexGrower.vertexPosition(v2), true);
                FloatType signedDistance = plane.pointDistance(*p);
                if(signedDistance > epsilon) {
                    insideHull = false;
                    if(signedDistance > furthestPointDistance) {
                        furthestPointDistance = signedDistance;
                        furthestPoint = p;
                    }
                }
            }
            // When point is inside the hull, remove it from the input list.
            if(insideHull) {
                if(furthestPoint == vecs.rend() - remainingPointCount)
                    furthestPoint = p;
                remainingPointCount--;
                *p = vecs[remainingPointCount];
            }
        }
        if(!remainingPointCount)
            break;
        OVITO_ASSERT(furthestPointDistance > 0 && furthestPoint != vecs.rend());

        // Kill all faces of the polyhedron that can be seen from the selected point.
        for(auto face = originalFaceCount; face < faceCount(); face++) {
            auto v0 = firstFaceVertex(face);
            auto v1 = secondFaceVertex(face);
            auto v2 = thirdFaceVertex(face);
            Plane3 plane(vertexGrower.vertexPosition(v0), vertexGrower.vertexPosition(v1), vertexGrower.vertexPosition(v2), true);
            if(plane.pointDistance(*furthestPoint) > epsilon) {
                faceGrower.deleteFace(face);
                face--;
            }
        }

        // Find an edge that borders the newly created hole in the mesh.
        edge_index firstBorderEdge = InvalidIndex;
        for(auto face = originalFaceCount; face < faceCount() && firstBorderEdge == InvalidIndex; face++) {
            edge_index e = firstFaceEdge(face);
            OVITO_ASSERT(e != InvalidIndex);
            do {
                if(!hasOppositeEdge(e)) {
                    firstBorderEdge = e;
                    break;
                }
                e = nextFaceEdge(e);
            }
            while(e != firstFaceEdge(face));
        }
        OVITO_ASSERT(firstBorderEdge != InvalidIndex); // If this assert fails, then there was no hole in the mesh.

        // Create new faces that connects the edges at the horizon (i.e. the border of the hole) with
        // the selected vertex.
        vertex_index vertex = vertexGrower.createVertex(*furthestPoint);
        edge_index borderEdge = firstBorderEdge;
        face_index previousFace = InvalidIndex;
        face_index firstFace = InvalidIndex;
        face_index newFace;
        do {
            newFace = faceGrower.createFace({ vertex2(borderEdge), vertex1(borderEdge), vertex }, region);
            linkOppositeEdges(firstFaceEdge(newFace), borderEdge);
            if(borderEdge == firstBorderEdge)
                firstFace = newFace;
            else
                linkOppositeEdges(secondFaceEdge(newFace), prevFaceEdge(firstFaceEdge(previousFace)));
            previousFace = newFace;
            // Proceed to next edge along the hole's border.
            for(;;) {
                borderEdge = nextFaceEdge(borderEdge);
                if(!hasOppositeEdge(borderEdge) || borderEdge == firstBorderEdge)
                    break;
                borderEdge = oppositeEdge(borderEdge);
            }
        }
        while(borderEdge != firstBorderEdge);
        OVITO_ASSERT(firstFace != newFace);
        linkOppositeEdges(secondFaceEdge(firstFace), prevFaceEdge(firstFaceEdge(newFace)));

        // Remove selected point from the input list as well.
        remainingPointCount--;
        *furthestPoint = vecs[remainingPointCount];
        vecs.resize(remainingPointCount);
    }

    // Delete interior vertices from the mesh that are no longer attached to any of the faces.
    for(auto vertex = originalVertexCount; vertex < vertexCount(); vertex++) {
        if(firstVertexEdge(vertex) == InvalidIndex) {
            // Delete the vertex from the mesh topology.
            vertexGrower.deleteVertex(vertex);
            // Adjust index to point to next vertex in the mesh after loop incrementation.
            vertex--;
        }
    }
}

/******************************************************************************
* Joins adjacent faces that are coplanar.
******************************************************************************/
void SurfaceMeshBuilder::joinCoplanarFaces(FloatType thresholdAngle)
{
    FloatType dotThreshold = std::cos(thresholdAngle);

    // Compute mesh face normals.
    std::vector<Vector3> faceNormals(faceCount());
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));
    for(face_index face : facesRange()) {
        faceNormals[face] = computeFaceNormal(face, vertexPositions);
    }

    // Visit each face and its adjacent faces.
    std::optional<FaceGrower> faceGrower;
    for(face_index face = 0; face < faceCount(); ) {
        face_index nextFace = face + 1;
        const Vector3& normal1 = faceNormals[face];
        edge_index faceEdge = firstFaceEdge(face);
        edge_index edge = faceEdge;
        do {
            edge_index opp_edge = oppositeEdge(edge);
            if(opp_edge != InvalidIndex) {
                face_index adj_face = adjacentFace(opp_edge);
                OVITO_ASSERT(adj_face >= 0 && adj_face < faceNormals.size());
                if(adj_face > face) {

                    // Check if current face and its current neighbor are coplanar.
                    const Vector3& normal2 = faceNormals[adj_face];
                    if(normal1.dot(normal2) > dotThreshold) {
                        // Eliminate this half-edge pair and join the two faces.
                        SurfaceMeshTopology* topo = mutableTopology();
                        for(edge_index currentEdge = nextFaceEdge(edge); currentEdge != edge; currentEdge = nextFaceEdge(currentEdge)) {
                            OVITO_ASSERT(adjacentFace(currentEdge) == face);
                            topo->setAdjacentFace(currentEdge, adj_face);
                        }
                        topo->setFirstFaceEdge(adj_face, nextFaceEdge(opp_edge));
                        topo->setFirstFaceEdge(face, edge);
                        topo->setNextFaceEdge(prevFaceEdge(edge), nextFaceEdge(opp_edge));
                        topo->setPrevFaceEdge(nextFaceEdge(opp_edge), prevFaceEdge(edge));
                        topo->setNextFaceEdge(prevFaceEdge(opp_edge), nextFaceEdge(edge));
                        topo->setPrevFaceEdge(nextFaceEdge(edge), prevFaceEdge(opp_edge));
                        topo->setNextFaceEdge(edge, opp_edge);
                        topo->setNextFaceEdge(opp_edge, edge);
                        topo->setPrevFaceEdge(edge, opp_edge);
                        topo->setPrevFaceEdge(opp_edge, edge);
                        topo->setAdjacentFace(opp_edge, face);
                        OVITO_ASSERT(adjacentFace(edge) == face);
                        OVITO_ASSERT(topo->countFaceEdges(face) == 2);
                        faceNormals[face] = faceNormals[faceCount() - 1];
                        if(!faceGrower)
                            faceGrower.emplace(*this);
                        faceGrower->deleteFace(face);
                        nextFace = face;
                        break;
                    }
                }
            }
            edge = nextFaceEdge(edge);
        }
        while(edge != faceEdge);
        face = nextFace;
    }
}

}   // End of namespace
