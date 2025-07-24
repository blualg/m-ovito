////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "SurfaceMeshReadAccess.h"
#include "SurfaceMesh.h"

namespace Ovito {

constexpr SurfaceMeshReadAccess::size_type SurfaceMeshReadAccess::InvalidIndex;

/******************************************************************************
* Constructor that takes an existing SurfaceMesh object.
******************************************************************************/
SurfaceMeshReadAccess::SurfaceMeshReadAccess(const SurfaceMesh* mesh) :
    _mesh(mesh),
    _topology(mesh ? mesh->topology() : nullptr),
    _vertices(mesh ? mesh->vertices() : nullptr),
    _faces(mesh ? mesh->faces() : nullptr),
    _regions(mesh ? mesh->regions() : nullptr),
    _domain(mesh ? mesh->domain() : nullptr)
{
}

/******************************************************************************
* Determines which spatial region contains the given point in space.
*
* Algorithm:
*
* J. Andreas Baerentzen and Henrik Aanaes:
* Signed Distance Computation Using the Angle Weighted Pseudonormal
* IEEE Transactions on Visualization and Computer Graphics 11 (2005), Page 243
******************************************************************************/
std::optional<std::pair<SurfaceMeshReadAccess::region_index, FloatType>> SurfaceMeshReadAccess::locatePoint(const Point3& location, FloatType epsilon, const boost::dynamic_bitset<>& faceSubset) const
{
    // Get access to the vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));
    // Get access to the face regions.
    BufferReadAccess<int32_t> faceRegions(faceProperty(SurfaceMeshFaces::RegionProperty));

    // Determine which vertex is closest to the query point.
    FloatType closestDistanceSq = FLOATTYPE_MAX;
    vertex_index closestVertex = InvalidIndex;
    Vector3 closestNormal, closestVector;
    region_index closestRegion = spaceFillingRegion();
    size_type vcount = vertexCount();
    for(vertex_index vindex = 0; vindex < vcount; vindex++) {
        // Compute distance from query point to vertex.
        const Point3& vertexPos = vertexPositions[vindex];
        Vector3 r = wrapVector(vertexPos - location);
        FloatType distSq = r.squaredLength();
        if(distSq < closestDistanceSq) {
            // Compute pseudo-normal at the vertex.
            // Note that a vertex may have multiple pseudo-normals if it is part of multiple manifolds.
            // If the manifold is two-sided, we need to compute the normal belonging to each manifold and use the one that is facing
            // away from the query point (if any).
            Vector3 pseudoNormal = Vector3::Zero();
            edge_index firstEdge = firstVertexEdge(vindex);
            QVarLengthArray<edge_index, 16> visitedEdges;
            for(;;) {
                // Skip edges that are not adjacent to a visible face.
                if(!faceSubset.empty()) {
                    while(firstEdge != InvalidIndex && !faceSubset[adjacentFace(firstEdge)])
                        firstEdge = nextVertexEdge(firstEdge);
                }
                if(firstEdge == InvalidIndex) break;

                if(std::find(visitedEdges.cbegin(), visitedEdges.cend(), firstEdge) == visitedEdges.cend()) {
                    // Compute vertex pseudo-normal by averaging the normal vectors of adjacent faces.
                    edge_index edge = firstEdge;
                    Vector3 edge1v = wrapVector(vertexPositions[vertex2(edge)] - vertexPos);
                    edge1v.normalizeSafely();
                    do {
                        visitedEdges.push_back(edge);
                        if(!hasOppositeEdge(edge))
                            throw Exception("Point location query requires a surface mesh that is closed.");
                        edge_index nextEdge = nextFaceEdge(oppositeEdge(edge));
                        OVITO_ASSERT(vertex1(nextEdge) == vindex);
                        Vector3 edge2v = wrapVector(vertexPositions[vertex2(nextEdge)] - vertexPos);
                        edge2v.normalizeSafely();
                        FloatType angle = std::acos(edge1v.dot(edge2v));
                        Vector3 faceNormal = edge2v.cross(edge1v);
                        if(faceNormal != Vector3::Zero())
                            pseudoNormal += faceNormal.normalized() * angle;
                        edge = nextEdge;
                        edge1v = edge2v;
                    }
                    while(edge != firstEdge);
                    closestRegion = faceRegions ? faceRegions[adjacentFace(firstEdge)] : 0;

                    // We can stop if the manifold is two-sided and the pseudo-normal is facing away from query point.
                    if(pseudoNormal.dot(r) > -epsilon || !hasOppositeFace(adjacentFace(edge)))
                        break;
                    pseudoNormal.setZero();
                }

                // Continue with next edge that is adjacent to the vertex.
                firstEdge = nextVertexEdge(firstEdge);
            }

            if(!pseudoNormal.isZero()) {
                closestDistanceSq = distSq;
                closestVertex = vindex;
                closestVector = r;
                closestNormal = pseudoNormal;
            }
        }
    }

    // If the surface is degenerate, any point is inside the space-filling region.
    if(closestVertex == InvalidIndex) return std::make_pair(spaceFillingRegion(), std::sqrt(closestDistanceSq));

    // Check if any edge is closer to the test point than the closest vertex.
    size_type edgeCount = this->edgeCount();
    for(edge_index edge = 0; edge < edgeCount; edge++) {
        if(!faceSubset.empty() && !faceSubset[adjacentFace(edge)]) continue;
        if(!hasOppositeEdge(edge))
            throw Exception("Point location query requires a surface mesh that is closed.");
        const Point3& p1 = vertexPositions[vertex1(edge)];
        const Point3& p2 = vertexPositions[vertex2(edge)];
        Vector3 edgeDir = wrapVector(p2 - p1);
        Vector3 r = wrapVector(p1 - location);
        FloatType edgeLength = edgeDir.length();
        if(edgeLength <= FLOATTYPE_EPSILON) continue;
        edgeDir /= edgeLength;
        FloatType d = -edgeDir.dot(r);
        if(d <= 0 || d >= edgeLength) continue;
        Vector3 c = r + edgeDir * d;
        FloatType distSq = c.squaredLength();
        if(distSq < closestDistanceSq) {

            // Compute pseudo normal of edge by averaging the normal vectors of the two adjacent faces.
            const Point3& p1a = vertexPositions[vertex2(nextFaceEdge(edge))];
            const Point3& p1b = vertexPositions[vertex2(nextFaceEdge(oppositeEdge(edge)))];
            Vector3 e1 = wrapVector(p1a - p1);
            Vector3 e2 = wrapVector(p1b - p1);
            Vector3 pseudoNormal = edgeDir.cross(e1).safelyNormalized() + e2.cross(edgeDir).safelyNormalized();

            // In case the manifold is two-sided, skip edge if pseudo-normal is facing toward the query point.
            if(pseudoNormal.dot(c) > -epsilon || !hasOppositeFace(adjacentFace(edge))) {
                closestDistanceSq = distSq;
                closestVertex = InvalidIndex;
                closestVector = c;
                closestNormal = pseudoNormal;
                closestRegion = faceRegions ? faceRegions[adjacentFace(edge)] : 0;
            }
        }
    }

    // Check if any facet is closer to the test point than the closest vertex and the closest edge.
    size_type faceCount = this->faceCount();
    for(face_index face = 0; face < faceCount; face++) {
        if(!faceSubset.empty() && !faceSubset[face]) continue;
        edge_index firstEdge = firstFaceEdge(face);
        vertex_index firstVertex = vertex1(firstEdge);
        const Point3& p1 = vertexPositions[firstVertex];
        Vector3 r = wrapVector(p1 - location);
        edge_index edge2 = nextFaceEdge(firstEdge);
        while(vertex2(edge2) != firstVertex) {
            const Point3& p2 = vertexPositions[vertex1(edge2)];
            const Point3& p3 = vertexPositions[vertex2(edge2)];
            Vector3 edgeVectors[3];
            edgeVectors[0] = wrapVector(p2 - p1);
            edgeVectors[1] = wrapVector(p3 - p2);
            edgeVectors[2] = -edgeVectors[1] - edgeVectors[0];

            // Compute face normal.
            Vector3 normal = edgeVectors[0].cross(edgeVectors[1]);

            // Determine whether the projection of the query point is inside the face's boundaries.
            bool isInsideTriangle = true;
            Vector3 vertexVector = r;
            for(size_t v = 0; v < 3; v++) {
                if(vertexVector.dot(normal.cross(edgeVectors[v])) >= FLOATTYPE_EPSILON) {
                    isInsideTriangle = false;
                    break;
                }
                vertexVector += edgeVectors[v];
            }

            if(isInsideTriangle) {
                FloatType normalLengthSq = normal.squaredLength();
                if(std::abs(normalLengthSq) > FLOATTYPE_EPSILON) {
                    normal /= sqrt(normalLengthSq);
                    FloatType planeDist = normal.dot(r);
                    // In case the manifold is two-sided, skip face if it is facing toward the query point.
                    if(planeDist > -epsilon || !hasOppositeFace(face)) {
                        if(planeDist * planeDist < closestDistanceSq) {
                            closestDistanceSq = planeDist * planeDist;
                            closestVector = normal * planeDist;
                            closestVertex = InvalidIndex;
                            closestNormal = normal;
                            closestRegion = faceRegions ? faceRegions[face] : 0;
                        }
                    }
                }
            }
            edge2 = nextFaceEdge(edge2);
        }
    }

    FloatType dot = closestNormal.dot(closestVector);
    if(dot >= epsilon) return std::make_pair(closestRegion, std::sqrt(closestDistanceSq));
    if(dot <= -epsilon) return std::make_pair(spaceFillingRegion(), std::sqrt(closestDistanceSq));
    return {};
}

/******************************************************************************
* Triangulates the polygonal faces of this mesh and outputs the results as a TriMesh object.
******************************************************************************/
void SurfaceMeshReadAccess::convertToTriMesh(TriangleMesh& outputMesh, bool smoothShading, const boost::dynamic_bitset<>& faceSubset, std::vector<size_t>* originalFaceMap, bool autoGenerateOppositeFaces) const
{
    size_type faceCount = this->faceCount();
    OVITO_ASSERT(faceSubset.empty() || faceSubset.size() == faceCount);

    // Get access to the vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));

    // Create output vertices.
    auto baseVertexCount = outputMesh.vertexCount();
    auto baseFaceCount = outputMesh.faceCount();
    outputMesh.setVertexCount(baseVertexCount + vertexCount());
    vertex_index vidx = 0;
    for(auto p = outputMesh.vertices().begin() + baseVertexCount; p != outputMesh.vertices().end(); ++p)
        *p = vertexPositions[vidx++];

    // Transfer faces from surface mesh to output triangle mesh.
    for(face_index face = 0; face < faceCount; face++) {
        if(!faceSubset.empty() && !faceSubset[face])
            continue;

        // Determine whether opposite triangles should be created for the current source face.
        bool createOppositeFace = autoGenerateOppositeFaces && (!hasOppositeFace(face) || (!faceSubset.empty() && !faceSubset[oppositeFace(face)]));

        // Go around the edges of the face to triangulate the polygon (assuming it is convex).
        edge_index faceEdge = firstFaceEdge(face);
        vertex_index baseVertex = vertex2(faceEdge);
        edge_index edge1 = nextFaceEdge(faceEdge);
        edge_index edge2 = nextFaceEdge(edge1);
        while(edge2 != faceEdge) {
            TriMeshFace& outputFace = outputMesh.addFace();
            outputFace.setVertices(baseVertex + baseVertexCount, vertex2(edge1) + baseVertexCount, vertex2(edge2) + baseVertexCount);
            outputFace.setEdgeVisibility(edge1 == nextFaceEdge(faceEdge), true, false);
            if(originalFaceMap)
                originalFaceMap->push_back(face);
            edge1 = edge2;
            edge2 = nextFaceEdge(edge2);
            if(edge2 == faceEdge)
                outputFace.setEdgeVisible(2);
            if(createOppositeFace) {
                TriMeshFace& oppositeFace = outputMesh.addFace();
                const TriMeshFace& thisFace = outputMesh.face(outputMesh.faceCount()-2);
                oppositeFace.setVertices(thisFace.vertex(2), thisFace.vertex(1), thisFace.vertex(0));
                oppositeFace.setEdgeVisibility(thisFace.edgeVisible(1), thisFace.edgeVisible(0), thisFace.edgeVisible(2));
                if(originalFaceMap)
                    originalFaceMap->push_back(face);
            }
        }
    }

    if(smoothShading) {
        // Compute mesh face normals.
        std::vector<Vector3G> faceNormals(faceCount);
        auto faceNormal = faceNormals.begin();
        for(face_index face = 0; face < faceCount; face++, ++faceNormal) {
            if(!faceSubset.empty() && !faceSubset[face])
                faceNormal->setZero();
            else
                *faceNormal = computeFaceNormal(face, vertexPositions).toDataType<GraphicsFloatType>();
        }

        // Smooth normals.
        std::vector<Vector3G> newFaceNormals(faceCount);
        auto oldFaceNormal = faceNormals.begin();
        auto newFaceNormal = newFaceNormals.begin();
        for(face_index face = 0; face < faceCount; face++, ++oldFaceNormal, ++newFaceNormal) {
            *newFaceNormal = *oldFaceNormal;
            if(!faceSubset.empty() && !faceSubset[face]) continue;

            edge_index faceEdge = firstFaceEdge(face);
            edge_index edge = faceEdge;
            do {
                edge_index oe = oppositeEdge(edge);
                if(oe != InvalidIndex) {
                    *newFaceNormal += faceNormals[adjacentFace(oe)];
                }
                edge = nextFaceEdge(edge);
            }
            while(edge != faceEdge);

            newFaceNormal->normalizeSafely();
        }
        faceNormals = std::move(newFaceNormals);
        newFaceNormals.clear();
        newFaceNormals.shrink_to_fit();

        // Helper method that calculates the mean normal at a surface mesh vertex.
        // The method takes an half-edge incident on the vertex as input (instead of the vertex itself),
        // because the method will only take into account incident faces belonging to one manifold.
        auto calculateNormalAtVertex = [&](edge_index startEdge) {
            Vector3G normal = Vector3G::Zero();
            edge_index edge = startEdge;
            do {
                normal += faceNormals[adjacentFace(edge)];
                edge = oppositeEdge(nextFaceEdge(edge));
                if(edge == InvalidIndex) break;
            }
            while(edge != startEdge);
            if(edge == InvalidIndex) {
                edge = oppositeEdge(startEdge);
                while(edge != InvalidIndex) {
                    normal += faceNormals[adjacentFace(edge)];
                    edge = oppositeEdge(prevFaceEdge(edge));
                }
            }
            return normal;
        };

        // Compute normal at each face vertex of the output mesh.
        outputMesh.setHasNormals(true);
        auto outputNormal = outputMesh.normals().begin() + (baseFaceCount * 3);
        for(face_index face = 0; face < faceCount; face++) {
            if(!faceSubset.empty() && !faceSubset[face]) continue;

            bool createOppositeFace = autoGenerateOppositeFaces && (!hasOppositeFace(face) || (!faceSubset.empty() && !faceSubset[oppositeFace(face)])) ;

            // Go around the edges of the face.
            edge_index faceEdge = firstFaceEdge(face);
            edge_index edge1 = nextFaceEdge(faceEdge);
            edge_index edge2 = nextFaceEdge(edge1);
            Vector3G baseNormal = calculateNormalAtVertex(faceEdge);
            Vector3G normal1 = calculateNormalAtVertex(edge1);
            while(edge2 != faceEdge) {
                Vector3G normal2 = calculateNormalAtVertex(edge2);
                *outputNormal++ = baseNormal;
                *outputNormal++ = normal1;
                *outputNormal++ = normal2;
                if(createOppositeFace) {
                    *outputNormal++ = -normal2;
                    *outputNormal++ = -normal1;
                    *outputNormal++ = -baseNormal;
                }
                normal1 = normal2;
                edge2 = nextFaceEdge(edge2);
            }
        }
        OVITO_ASSERT(outputNormal == outputMesh.normals().end());
    }
}

/******************************************************************************
* Computes the unit normal vector of a mesh face.
******************************************************************************/
Vector3 SurfaceMeshReadAccess::computeFaceNormal(face_index face, const BufferReadAccess<Point3>& vertexPositions) const
{
    Vector3 faceNormal = Vector3::Zero();

    // Go around the edges of the face to triangulate the general polygon.
    edge_index faceEdge = firstFaceEdge(face);
    edge_index edge1 = nextFaceEdge(faceEdge);
    edge_index edge2 = nextFaceEdge(edge1);
    Point3 base = vertexPositions[vertex2(faceEdge)];
    Vector3 e1 = wrapVector(vertexPositions[vertex2(edge1)] - base);
    while(edge2 != faceEdge) {
        Vector3 e2 = wrapVector(vertexPositions[vertex2(edge2)] - base);
        faceNormal += e1.cross(e2);
        e1 = e2;
        edge1 = edge2;
        edge2 = nextFaceEdge(edge2);
    }

    return faceNormal.safelyNormalized();
}

}   // End of namespace
