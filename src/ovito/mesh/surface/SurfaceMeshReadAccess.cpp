////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
                            throw Exception(QStringLiteral("Point location query requires a surface mesh that is closed."));
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
            throw Exception(QStringLiteral("Point location query requires a surface mesh that is closed."));
        const Point3& p1 = vertexPositions[vertex1(edge)];
        const Point3& p2 = vertexPositions[vertex2(edge)];
        Vector3 edgeDir = wrapVector(p2 - p1);
        Vector3 r = wrapVector(p1 - location);
        FloatType edgeLength = edgeDir.length();
        if(edgeLength <= Ovito::epsilon_v<FloatType>) continue;
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
                if(vertexVector.dot(normal.cross(edgeVectors[v])) >= Ovito::epsilon_v<FloatType>) {
                    isInsideTriangle = false;
                    break;
                }
                vertexVector += edgeVectors[v];
            }

            if(isInsideTriangle) {
                FloatType normalLengthSq = normal.squaredLength();
                if(std::abs(normalLengthSq) > Ovito::epsilon_v<FloatType>) {
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
bool SurfaceMeshReadAccess::convertToTriMesh(TriangleMesh& outputMesh, const boost::dynamic_bitset<>& faceSubset, std::vector<size_t>* originalFaceMap, bool autoGenerateOppositeFaces) const
{
    OVITO_ASSERT(this_task::get());

    size_type faceCount = this->faceCount();
    OVITO_ASSERT(faceSubset.empty() || faceSubset.size() == faceCount);
    OVITO_ASSERT(outputMesh.faceCount() == 0);
    OVITO_ASSERT(outputMesh.vertexCount() == 0 || outputMesh.vertexCount() == vertexCount());

    // Get access to the input vertex coordinates.
    BufferReadAccess<Point3> vertexPositions(expectVertexProperty(SurfaceMeshVertices::PositionProperty));

    // Create output vertices and copy coordinates.
    outputMesh.setVertexCount(vertexCount());
    vertex_index vidx = 0;
    for(auto p = outputMesh.vertices().begin(); p != outputMesh.vertices().end(); ++p)
        *p = vertexPositions[vidx++];

    // Pre-allocate space for triangles in the output mesh.
    // Assume one triangle per input face as a lower bound.
    int outputFaceCountEstimate = (faceSubset.empty() ? faceCount : faceSubset.count()) * (autoGenerateOppositeFaces ? 2 : 1);
    outputMesh.reserveFaces(outputFaceCountEstimate);

    // Pre-allocate space for original face map.
    if(originalFaceMap)
        originalFaceMap->reserve(originalFaceMap->size() + outputFaceCountEstimate);

    // Transfer faces from surface mesh to the output triangle mesh and triangulate them if necessary.

    // Precompute SurfaceMesh face normals.
    std::vector<Vector3G> faceNormals(faceCount);
    if(outputMesh.hasNormals()) {
        parallelFor(faceCount, 1024, TaskProgress::Ignore, [&](face_index face) {
            faceNormals[face] = computeFaceUnitNormal(face, vertexPositions).toDataType<GraphicsFloatType>();
        });
        // Additionally smooth face normals by taking average of adjacent face normals.
        std::vector<Vector3G> newFaceNormals(faceCount);
        parallelFor(faceCount, 1024, TaskProgress::Ignore, [&](face_index face) {
            Vector3G& avgNormal = newFaceNormals[face];
            avgNormal = faceNormals[face];
            if(!faceSubset.empty() && !faceSubset[face]) {
                avgNormal.normalizeSafely();
                return;
            }

            edge_index faceEdge = firstFaceEdge(face);
            edge_index edge = faceEdge;
            do {
                edge_index oe = oppositeEdge(edge);
                if(oe != InvalidIndex) {
                    avgNormal += faceNormals[adjacentFace(oe)];
                }
                edge = nextFaceEdge(edge);
            }
            while(edge != faceEdge);

            avgNormal.normalizeSafely();
        });
        faceNormals.swap(newFaceNormals);
    }

    // Set up GLU tessellator.
    std::unique_ptr<GLUtesselator, decltype(&gluDeleteTess)> tess(gluNewTess(), &gluDeleteTess);
    gluTessProperty(tess.get(), GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);
    gluTessProperty(tess.get(), GLU_TESS_TOLERANCE, 1e-8);

    struct GLUCallbackHelper {
        const SurfaceMeshReadAccess* self;
        TriangleMesh& outputMesh;
        std::vector<size_t>* originalFaceMap;
        std::vector<Vector3G>& faceNormals;
        face_index currentSourceFace;
        TriMeshFace* currentOutputFace;
        Vector3G* currentFaceVertexNormal = nullptr;
        int localFaceVertexIndex = 0;
        bool generateOppositeFace = false;
        bool edgeFlag = false;
        bool triangulationFailed = false;

        // Helper method that calculates the mean normal at a SurfaceMesh vertex.
        Vector3G computeNormalAtVertex(edge_index startEdge) const {
            Vector3G normal = Vector3G::Zero();
            if(!outputMesh.hasNormals())
                return normal; // Skip computation if normals are not requested.
            edge_index edge = startEdge;
            do {
                normal += faceNormals[self->adjacentFace(edge)];
                edge = self->oppositeEdge(self->nextFaceEdge(edge));
                if(edge == InvalidIndex) break;
            }
            while(edge != startEdge);
            if(edge == InvalidIndex) {
                edge = self->oppositeEdge(startEdge);
                while(edge != InvalidIndex) {
                    normal += faceNormals[self->adjacentFace(edge)];
                    edge = self->oppositeEdge(self->prevFaceEdge(edge));
                }
            }
            return normal;
        }

    } callbackData{this, outputMesh, originalFaceMap, faceNormals};

    // Register an error handler callback.
    gluTessCallback(tess.get(), GLU_TESS_ERROR_DATA, (_GLUfuncptr)(void(*)(int, void*))[](int errnum, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        if(errnum != GLU_TESS_NEED_COMBINE_CALLBACK)
            qWarning() << "WARNING: Could not tessellate surface mesh face - error code" << errnum;
        helper->triangulationFailed = true;
    });

    // Register the begin callback, which is called at the beginning of each new primitive.
    gluTessCallback(tess.get(), GLU_TESS_BEGIN, (_GLUfuncptr)(void(*)(int))[](int type) {
        OVITO_ASSERT(type == GL_TRIANGLES); // Must always be individual triangles because we have set a GLU_TESS_EDGE_FLAG callback.
    });

    // Register the edge flag callback, which is called to indicate whether the next edge is visible or not.
    gluTessCallback(tess.get(), GLU_TESS_EDGE_FLAG_DATA, (_GLUfuncptr)(void(*)(bool, void*))[](bool flag, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        helper->edgeFlag = flag;
    });

#if 0
    // Register the combine callback, which is called to create new vertices.
    gluTessCallback(tess.get(), GLU_TESS_COMBINE_DATA, (_GLUfuncptr)(void(*)(double*, void**, float*, void**, void*))[](double coords[3], void* vertex_data[4], float weight[4], void** outData, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        qDebug() << "Tessellator combine callback called to create new vertex at" << coords[0] << coords[1] << coords[2];

        std::array<int, 4> contributingVertexIndices{{
            static_cast<int>(reinterpret_cast<intptr_t>(vertex_data[0])),
            static_cast<int>(reinterpret_cast<intptr_t>(vertex_data[1])),
            static_cast<int>(reinterpret_cast<intptr_t>(vertex_data[2])),
            static_cast<int>(reinterpret_cast<intptr_t>(vertex_data[3]))
        }};
        for(int& idx : contributingVertexIndices) {
            if(idx < 0)
                idx = -idx; // New vertex created during this tessellation run.
            else
                idx = helper->self->vertex2(idx) + helper->baseVertexIndex; // Original vertex from input mesh.
        }
        int newVertexIndex = helper->outputMesh.addVertex(Point3(static_cast<FloatType>(coords[0]), static_cast<FloatType>(coords[1]), static_cast<FloatType>(coords[2])));
        *outData = reinterpret_cast<void*>(static_cast<intptr_t>(-newVertexIndex));
    });
#endif

    // Register the vertex callback, which is called for each vertex of the tessellated output.
    gluTessCallback(tess.get(), GLU_TESS_VERTEX_DATA, (_GLUfuncptr)(void(*)(void*,void*))[](void* vertex_data, void* polygon_data) {
        GLUCallbackHelper* helper = static_cast<GLUCallbackHelper*>(polygon_data);
        intptr_t edge_index = reinterpret_cast<intptr_t>(vertex_data);
        OVITO_ASSERT(edge_index < 0 || edge_index < helper->self->edgeCount());

        // Start a new triangle in the output mesh if necessary.
        if(helper->localFaceVertexIndex == 0) {
            auto newFaceIndex = helper->outputMesh.addFaces(helper->generateOppositeFace ? 2 : 1);
            helper->currentOutputFace = &helper->outputMesh.face(newFaceIndex);
            // Record original face index for the triangle being created.
            if(helper->originalFaceMap) {
                if(!helper->generateOppositeFace)
                    helper->originalFaceMap->push_back(helper->currentSourceFace);
                else
                    helper->originalFaceMap->insert(helper->originalFaceMap->end(), { (size_t)helper->currentSourceFace, (size_t)helper->currentSourceFace });
            }
            if(helper->outputMesh.hasNormals())
                helper->currentFaceVertexNormal = &helper->outputMesh.faceVertexNormal(newFaceIndex, 0);
        }

        // Set the current triangle's next vertex.
        TriMeshFace& outputFace = helper->outputMesh.faces().back();
        if(!helper->edgeFlag)
            outputFace.setEdgeHidden(helper->localFaceVertexIndex);
        int triangleMeshVertexIndex = (edge_index < 0) ? -static_cast<int>(edge_index) : (helper->self->vertex2(edge_index));
        outputFace.setVertex(helper->localFaceVertexIndex++, triangleMeshVertexIndex);
        if(helper->currentFaceVertexNormal) {
            if(edge_index >= 0)
                *helper->currentFaceVertexNormal++ = helper->computeNormalAtVertex(edge_index);
            else
                *helper->currentFaceVertexNormal++ = Vector3G::Zero(); // New vertex created during tessellation - normal cannot be computed.
        }

        // Reset local vertex index after completing a triangle.
        if(helper->localFaceVertexIndex == 3) {
            helper->localFaceVertexIndex = 0;

            // Create opposite face if requested.
            if(helper->generateOppositeFace) {
                TriMeshFace& oppositeFace = helper->currentOutputFace[1];
                const TriMeshFace& thisFace = helper->currentOutputFace[0];
                oppositeFace.setVertices(thisFace.vertex(2), thisFace.vertex(1), thisFace.vertex(0));
                oppositeFace.setEdgeVisibility(thisFace.edgeVisible(1), thisFace.edgeVisible(0), thisFace.edgeVisible(2));
                if(helper->currentFaceVertexNormal) {
                    helper->currentFaceVertexNormal[0] = -helper->currentFaceVertexNormal[-1];
                    helper->currentFaceVertexNormal[1] = -helper->currentFaceVertexNormal[-2];
                    helper->currentFaceVertexNormal[2] = -helper->currentFaceVertexNormal[-3];
                }
            }
        }
    });

    for(face_index face = 0; face < faceCount; face++) {
        // Skip faces that are not part of the specified optional subset.
        if(!faceSubset.empty() && !faceSubset[face])
            continue;

        // Determine whether opposite triangles should be created for the current source face.
        // This depends on whether the auto-generation flag is enabled and whether the face
        // has an explicit opposite face already.
        callbackData.generateOppositeFace = false;
        if(autoGenerateOppositeFaces) {
            face_index opposite = oppositeFace(face);
            if(opposite == InvalidIndex || (!faceSubset.empty() && !faceSubset[opposite]))
                callbackData.generateOppositeFace = true;
        }

        // First, determine whether the face can be correctly triangulated using a simple triangle fan
        // Only if this fails, we invoke the more expensive GLU tessellator for general polygons.
        //
        // Go around the edges of the face to triangulate the general polygon using a triangle fan.
        // The face is convex if all triangles of the fan have normals that consistently point
        // into the same direction.
        const edge_index ffe = firstFaceEdge(face);
        edge_index edge1 = nextFaceEdge(ffe);
        edge_index edge2 = nextFaceEdge(edge1);
        if(edge1 == ffe || edge2 == ffe) {
            // Degenerate face with less than three vertices - skip it.
            continue;
        }
        bool isSimplePolygon = true;
        if(nextFaceEdge(edge2) != ffe) { // Don't test triangle faces, because they are always convex.
            Point3 base = vertexPositions[vertex2(ffe)];
            Vector3G e1 = wrapVector(vertexPositions[vertex2(edge1)] - base).toDataType<GraphicsFloatType>();
            Vector3G referenceNormal = Vector3G::Zero();
            while(edge2 != ffe) {
                Vector3G e2 = wrapVector(vertexPositions[vertex2(edge2)] - base).toDataType<GraphicsFloatType>();
                Vector3G n = e1.cross(e2);
                auto nmag = n.length();
                if(nmag > 1e-6f) {
                    n /= nmag;
                    if(n.dot(referenceNormal) < -0.9f) {
                        isSimplePolygon = false;
                        break;
                    }
                    referenceNormal = n;
                }
                e1 = e2;
                edge1 = edge2;
                edge2 = nextFaceEdge(edge2);
            }
        }

        if(isSimplePolygon) {
            // Go around the edges of the face to triangulate the polygon using a simple triangle fan.
            vertex_index baseVertex = vertex2(ffe);
            edge_index edge1 = nextFaceEdge(ffe);
            edge_index edge2 = nextFaceEdge(edge1);
            const Vector3G baseVertexNormal = callbackData.computeNormalAtVertex(ffe);
            Vector3G vertex1Normal = callbackData.computeNormalAtVertex(edge1);
            while(edge2 != ffe) {
                int newFaceIndex = outputMesh.addFaces(callbackData.generateOppositeFace ? 2 : 1);
                TriMeshFace& outputFace = outputMesh.face(newFaceIndex);
                outputFace.setVertices(baseVertex, vertex2(edge1), vertex2(edge2));
                outputFace.setEdgeVisibility(edge1 == nextFaceEdge(ffe), true, false);
                Vector3G vertex2Normal = callbackData.computeNormalAtVertex(edge2);
                if(outputMesh.hasNormals()) {
                    outputMesh.setFaceVertexNormal(newFaceIndex, 0, baseVertexNormal);
                    outputMesh.setFaceVertexNormal(newFaceIndex, 1, vertex1Normal);
                    outputMesh.setFaceVertexNormal(newFaceIndex, 2, vertex2Normal);
                }
                if(originalFaceMap)
                    originalFaceMap->push_back(face);
                edge1 = edge2;
                edge2 = nextFaceEdge(edge2);
                vertex1Normal = vertex2Normal;
                if(edge2 == ffe)
                    outputFace.setEdgeVisible(2);

                // Create backside triangle with opposite winding order and normal vector if requested.
                if(callbackData.generateOppositeFace) {
                    TriMeshFace& oppositeFace = outputMesh.face(newFaceIndex + 1);
                    oppositeFace.setVertices(outputFace.vertex(2), outputFace.vertex(1), outputFace.vertex(0));
                    oppositeFace.setEdgeVisibility(outputFace.edgeVisible(1), outputFace.edgeVisible(0), outputFace.edgeVisible(2));
                    if(outputMesh.hasNormals()) {
                        outputMesh.setFaceVertexNormal(newFaceIndex + 1, 0, -vertex2Normal);
                        outputMesh.setFaceVertexNormal(newFaceIndex + 1, 1, -vertex1Normal);
                        outputMesh.setFaceVertexNormal(newFaceIndex + 1, 2, -baseVertexNormal);
                    }
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
            Point3 coord = vertexPositions[vertex2(edge)];
            do {
                Point_3<double> vertexCoord = coord.toDataType<double>();
                gluTessVertex(tess.get(), vertexCoord.data(), reinterpret_cast<void*>(static_cast<intptr_t>(edge)));
                edge = nextFaceEdge(edge);
                coord += edgeVector(edge, vertexPositions); // For unwrapping the polygon in case of periodic boundaries.
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
        if(n.dot(faceNormal) >= -1e-8)
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
