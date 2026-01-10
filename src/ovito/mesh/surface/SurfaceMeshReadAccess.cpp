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
#include <ovito/mesh/util/polytess/glu.h>
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
* Produces a TriangleMesh from the SurfaceMesh by triangulating the
* polygonal faces and computing averaged vertex normals if requested.
* The method returns false to indicate that triangulation failed for one or more faces.
******************************************************************************/
bool SurfaceMeshReadAccess::convertToTriMesh(TriangleMesh& outputMesh, bool smoothShading, const boost::dynamic_bitset<>& faceSubset, std::vector<size_t>* originalFaceMap, bool autoGenerateOppositeFaces) const
{
    OVITO_ASSERT(this_task::get());

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

    // Pre-allocate space for original face map.
    if(originalFaceMap)
        originalFaceMap->reserve(originalFaceMap->size() + faceCount * (autoGenerateOppositeFaces ? 2 : 1));

    // Transfer faces from surface mesh to the output triangle mesh and triangulate them if necessary.
    bool triangulationSuccessful = true;
    if(!hasNonConvexFaces())
        triangulateConvexFaces(outputMesh, baseVertexCount, faceSubset, originalFaceMap, autoGenerateOppositeFaces);
    else
        triangulationSuccessful = triangulateNonConvexFaces(outputMesh, baseVertexCount, faceSubset, originalFaceMap, autoGenerateOppositeFaces);

    if(smoothShading) {
        // Compute mesh face normals.
        std::vector<Vector3G> faceNormals(faceCount);
        auto faceNormal = faceNormals.begin();
        for(face_index face = 0; face < faceCount; face++, ++faceNormal) {
            if(!faceSubset.empty() && !faceSubset[face])
                faceNormal->setZero();
            else
                *faceNormal = computeFaceUnitNormal(face, vertexPositions).toDataType<GraphicsFloatType>();
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

        // Helper method that calculates the mean normal at a SurfaceMesh vertex.
        // The method takes an half-edge incident on the vertex as starting point (instead of the vertex itself),
        // because the method only considers incident faces belonging to one manifold (the vertex can potentially be part of several manifolds).
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

    return triangulationSuccessful;
}

/******************************************************************************
* Helper method that triangulates all faces of the surface mesh, assuming they
* are all convex, and adds them to the output triangle mesh.
******************************************************************************/
void SurfaceMeshReadAccess::triangulateConvexFaces(TriangleMesh& outputMesh, int baseVertexIndex, const boost::dynamic_bitset<>& faceSubset, std::vector<size_t>* originalFaceMap, bool autoGenerateOppositeFaces) const
{
    const size_type faceCount = this->faceCount();
    for(face_index face = 0; face < faceCount; face++) {
        // Skip faces that are not part of the specified optional subset.
        if(!faceSubset.empty() && !faceSubset[face])
            continue;

        // Determine whether opposite triangles should be created for the current source face.
        bool createOppositeFace = autoGenerateOppositeFaces && (!hasOppositeFace(face) || (!faceSubset.empty() && !faceSubset[oppositeFace(face)]));

        // Go around the edges of the face to triangulate the polygon using a triangle fan, assuming it is convex.
        edge_index faceEdge = firstFaceEdge(face);
        vertex_index baseVertex = vertex2(faceEdge);
        edge_index edge1 = nextFaceEdge(faceEdge);
        edge_index edge2 = nextFaceEdge(edge1);
        while(edge2 != faceEdge) {
            TriMeshFace& outputFace = outputMesh.addFace();
            outputFace.setVertices(baseVertex + baseVertexIndex, vertex2(edge1) + baseVertexIndex, vertex2(edge2) + baseVertexIndex);
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
}

/******************************************************************************
* Helper method that triangulates all faces of the surface mesh, using a
* method that can handle non-convex polygons, and adds them to the output
* triangle mesh.
******************************************************************************/
bool SurfaceMeshReadAccess::triangulateNonConvexFaces(TriangleMesh& outputMesh, int baseVertexIndex, const boost::dynamic_bitset<>& faceSubset, std::vector<size_t>* originalFaceMap, bool autoGenerateOppositeFaces) const
{
    const size_type faceCount = this->faceCount();

    // Need access to the vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));

    // Set up GLU tessellator.
    std::unique_ptr<GLUtesselator, decltype(&gluDeleteTess)> tess(gluNewTess(), &gluDeleteTess);
    gluTessProperty(tess.get(), GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);

    struct GLUCallbackHelper {
        TriangleMesh& outputMesh;
        std::vector<size_t>* originalFaceMap;
        face_index currentSourceFace;
        int localFaceVertexIndex = 0;
        bool generateOppositeFace = false;
        bool edgeFlag = false;
        bool triangulationFailed = false;
    } callbackData{outputMesh, originalFaceMap};

    // Register an error handler callback.
    gluTessCallback(tess.get(), GLU_TESS_ERROR_DATA, (_GLUfuncptr)(void(*)(int, void*))[](int errnum, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        if(errnum == GLU_TESS_NEED_COMBINE_CALLBACK)
            helper->triangulationFailed = true;
        else
            qWarning() << "WARNING: Could not tessellate surface mesh face - error code" << errnum;
    });

    // Register the begin callback, which is called at the beginning of each new primitive.
    gluTessCallback(tess.get(), GLU_TESS_BEGIN, (_GLUfuncptr)(void(*)(int))[](int type) {
        OVITO_ASSERT(type == GL_TRIANGLES); // Must always be invidiual triangles because we have set a GLU_TESS_EDGE_FLAG callback.
    });

    // Register the edge flag callback, which is called to indicate whether the next edge is visible or not.
    gluTessCallback(tess.get(), GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr)(void(*)(bool, void*))[](bool flag, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        helper->edgeFlag = flag;
    });

    // Register the vertex callback, which is called for each vertex of the tessellated output.
    gluTessCallback(tess.get(), GLU_TESS_VERTEX_DATA, (_GLUfuncptr)(void(*)(void*,void*))[](void* vertex_data, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        intptr_t vindex = reinterpret_cast<intptr_t>(vertex_data);

        // Start a new triangle in the output mesh if necessary.
        if(helper->localFaceVertexIndex == 0) {
            helper->outputMesh.addFace();
            // Record original face index for the triangle being created.
            if(helper->originalFaceMap)
                helper->originalFaceMap->push_back(helper->currentSourceFace);
        }

        // Set the current face's next vertex
        TriMeshFace& outputFace = helper->outputMesh.faces().back();
        if(!helper->edgeFlag)
            outputFace.setEdgeHidden(helper->localFaceVertexIndex);
        outputFace.setVertex(helper->localFaceVertexIndex++, vindex);

        // Reset local vertex index after completing a triangle.
        if(helper->localFaceVertexIndex == 3) {
            helper->localFaceVertexIndex = 0;

            // Create opposite face if requested.
            if(helper->generateOppositeFace) {
                TriMeshFace& oppositeFace = helper->outputMesh.addFace();
                const TriMeshFace& thisFace = helper->outputMesh.face(helper->outputMesh.faceCount() - 2);
                oppositeFace.setVertices(thisFace.vertex(2), thisFace.vertex(1), thisFace.vertex(0));
                oppositeFace.setEdgeVisibility(thisFace.edgeVisible(1), thisFace.edgeVisible(0), thisFace.edgeVisible(2));
                if(helper->originalFaceMap)
                    helper->originalFaceMap->push_back(helper->currentSourceFace);
            }
        }
    });

    for(face_index face = 0; face < faceCount; face++) {
        // Skip faces that are not part of the specified optional subset.
        if(!faceSubset.empty() && !faceSubset[face])
            continue;

        // Determine whether opposite triangles should be created for the current face.
        callbackData.generateOppositeFace = autoGenerateOppositeFaces && (!hasOppositeFace(face) || (!faceSubset.empty() && !faceSubset[oppositeFace(face)]));

        // Compute face normal.
        Vector3 faceNormal = computeFaceNormal(face, vertexPositions);
        if(faceNormal.isZero())
            continue;

        // First, determine whether the face can be correctly triangulated using a simple triangle fan
        // Only if this fails, we invoke the more expensive GLU tessellator for general polygons.
        //
        // Go around the edges of the face to triangulate the general polygon using a triangle fan.
        // The face is convex if all triangles have normals that consistently point
        // into the same direction as the face normal.
        const edge_index ffe = firstFaceEdge(face);
        edge_index edge1 = nextFaceEdge(ffe);
        edge_index edge2 = nextFaceEdge(edge1);
        if(edge1 == ffe || edge2 == ffe) {
            // Degenerate face with less than three vertices - skip it.
            continue;
        }
        Point3 base = vertexPositions[vertex2(ffe)];
        Vector3 e1 = wrapVector(vertexPositions[vertex2(edge1)] - base);
        bool isSimplePolygon = true;
        while(edge2 != ffe) {
            Vector3 e2 = wrapVector(vertexPositions[vertex2(edge2)] - base);
            Vector3 n = e1.cross(e2);
            if(n.dot(faceNormal) < 0) {
                isSimplePolygon = false;
                break;
            }
            e1 = e2;
            edge1 = edge2;
            edge2 = nextFaceEdge(edge2);
        }

        if(isSimplePolygon) {
            // Go around the edges of the face to triangulate the polygon using a triangle fan, assuming it is convex.
            vertex_index baseVertex = vertex2(ffe);
            edge_index edge1 = nextFaceEdge(ffe);
            edge_index edge2 = nextFaceEdge(edge1);
            while(edge2 != ffe) {
                TriMeshFace& outputFace = outputMesh.addFace();
                outputFace.setVertices(baseVertex + baseVertexIndex, vertex2(edge1) + baseVertexIndex, vertex2(edge2) + baseVertexIndex);
                outputFace.setEdgeVisibility(edge1 == nextFaceEdge(ffe), true, false);
                if(originalFaceMap)
                    originalFaceMap->push_back(face);
                edge1 = edge2;
                edge2 = nextFaceEdge(edge2);
                if(edge2 == ffe)
                    outputFace.setEdgeVisible(2);
                if(callbackData.generateOppositeFace) {
                    TriMeshFace& oppositeFace = outputMesh.addFace();
                    const TriMeshFace& thisFace = outputMesh.face(outputMesh.faceCount()-2);
                    oppositeFace.setVertices(thisFace.vertex(2), thisFace.vertex(1), thisFace.vertex(0));
                    oppositeFace.setEdgeVisibility(thisFace.edgeVisible(1), thisFace.edgeVisible(0), thisFace.edgeVisible(2));
                    if(originalFaceMap)
                        originalFaceMap->push_back(face);
                }
            }
        }
        else {
            // Start GLU general polygon tessellation.
            callbackData.currentSourceFace = face;
            callbackData.localFaceVertexIndex = 0;
            gluTessBeginPolygon(tess.get(), &callbackData);
            gluTessBeginContour(tess.get());

            // Go around the edges of the face to generate an unwrapped version of the general polygon,
            // which can be passed to the GLU tessellator.
            edge_index edge = ffe;
            Point3 coord = vertexPositions[vertex1(edge)];
            do {
                intptr_t vindex = static_cast<intptr_t>(vertex1(edge));
                Point_3<double> vertexCoord = coord.toDataType<double>();
                gluTessVertex(tess.get(), vertexCoord.data(), reinterpret_cast<void*>(vindex + baseVertexIndex));
                coord += edgeVector(edge, vertexPositions); // For unwrapping the polygon in case of periodic boundaries.
                edge = nextFaceEdge(edge);
            }
            while(edge != ffe);

            // Let GLU tessellate the polygonal face.
            gluTessEndContour(tess.get());
            gluTessEndPolygon(tess.get());
        }
    }

    return !callbackData.triangulationFailed;
}

/******************************************************************************
* Computes the normal vector of a mesh face.
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
        Vector3 n = e1.cross(e2);
        // Reverse the contribution of back-facing triangles to get a reasonable normal
        // for self-intersecting or non-convex polygons.
        if(n.dot(faceNormal) >= 0)
            faceNormal += n;
        else
            faceNormal -= n;
        e1 = e2;
        edge1 = edge2;
        edge2 = nextFaceEdge(edge2);
    }

    return faceNormal;
}

}   // End of namespace
