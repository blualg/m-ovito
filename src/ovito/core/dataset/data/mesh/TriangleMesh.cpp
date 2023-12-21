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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/utilities/io/CompressedTextWriter.h>
#include "TriangleMesh.h"
#include "TriangleMeshVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TriangleMesh);

/******************************************************************************
* Constructor.
******************************************************************************/
TriangleMesh::TriangleMesh(ObjectInitializationFlags flags) : DataObject(flags)
{
    if(!flags.testAnyFlags(ObjectInitializationFlags(DontInitializeObject) | ObjectInitializationFlags(DontCreateVisElement))) {
        setVisElement(OORef<TriangleMeshVis>::create(flags));
    }
}

/******************************************************************************
* Creates a copy of a topology structure.
******************************************************************************/
OORef<RefTarget> TriangleMesh::clone(bool deepCopy, CloneHelper& cloneHelper) const
{
    // Let the base class create an instance of this class.
    OORef<TriangleMesh> clone = static_object_cast<TriangleMesh>(DataObject::clone(deepCopy, cloneHelper));

    // Copy internal data.
    clone->_boundingBox = _boundingBox;
    clone->_vertices = _vertices;
    clone->_hasVertexColors = _hasVertexColors;
    clone->_vertexColors = _vertexColors;
    clone->_hasVertexPseudoColors = _hasVertexPseudoColors;
    clone->_vertexPseudoColors = _vertexPseudoColors;
    clone->_hasFaceColors = _hasFaceColors;
    clone->_faceColors = _faceColors;
    clone->_hasFacePseudoColors = _hasFacePseudoColors;
    clone->_facePseudoColors = _facePseudoColors;
    clone->_faces = _faces;
    clone->_hasNormals = _hasNormals;
    clone->_normals = _normals;

    return clone;
}

/******************************************************************************
* Clears all vertices and faces.
******************************************************************************/
void TriangleMesh::clear()
{
    _vertices.clear();
    _faces.clear();
    _vertexColors.clear();
    _vertexPseudoColors.clear();
    _faceColors.clear();
    _facePseudoColors.clear();
    _boundingBox.setEmpty();
    _hasVertexColors = false;
    _hasVertexPseudoColors = false;
    _hasFaceColors = false;
    _hasFacePseudoColors = false;
    _hasNormals = false;
}

/******************************************************************************
* Sets the number of vertices in this mesh.
******************************************************************************/
void TriangleMesh::setVertexCount(int n)
{
    _vertices.resize(n);
    if(_hasVertexColors)
        _vertexColors.resize(n);
    if(_hasVertexPseudoColors)
        _vertexPseudoColors.resize(n);
}

/******************************************************************************
* Sets the number of faces in this mesh.
******************************************************************************/
void TriangleMesh::setFaceCount(int n)
{
    _faces.resize(n);
    if(_hasFaceColors)
        _faceColors.resize(n);
    if(_hasFacePseudoColors)
        _facePseudoColors.resize(n);
    if(_hasNormals)
        _normals.resize(n * 3);
}

/******************************************************************************
* Adds a new triangle face and returns a reference to it.
* The new face is NOT initialized by this method.
******************************************************************************/
TriMeshFace& TriangleMesh::addFace()
{
    setFaceCount(faceCount() + 1);
    return _faces.back();
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void TriangleMesh::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    DataObject::saveToStream(stream, excludeRecomputableData);

    stream.beginChunk(0x01);
    stream.beginChunk(0x04);

    // Save vertices.
    stream << _vertices;

    // Save per-vertex RGBA colors.
    stream << _hasVertexColors;
    stream << _vertexColors;

    // Note: Current file format does not store pseudo-color values.
    // This may be added in the future, when there is a usecase for it.
    OVITO_ASSERT(!_hasVertexPseudoColors);
    OVITO_ASSERT(!_hasFacePseudoColors);

    // Save per-face colors.
    stream << _hasFaceColors;
    stream << _faceColors;

    // Save normals (three per face).
    stream << _hasNormals;
    stream << _normals;

    // Save faces.
    stream << (int)faceCount();
    for(auto face = faces().constBegin(); face != faces().constEnd(); ++face) {
        stream << face->_flags;
        stream << face->_vertices[0];
        stream << face->_vertices[1];
        stream << face->_vertices[2];
        stream << face->_smoothingGroups;
        stream << face->_materialIndex;
    }

    stream.endChunk();
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void TriangleMesh::loadFromStream(ObjectLoadStream& stream)
{
    DataObject::loadFromStream(stream);

    // Reset mesh.
    clear();

    if(stream.expectChunkRange(0x00, 0x01) != 0) {
        int formatVersion = stream.expectChunkRange(0x00, 0x04);

        // Load vertices.
        stream >> _vertices;

        // Load per-vertex RGBA colors.
        stream >> _hasVertexColors;
        stream >> _vertexColors;
        OVITO_ASSERT(_vertexColors.size() == _vertices.size() || !_hasVertexColors);

        if(formatVersion >= 2) {
            // Load per-face RGBA colors.
            stream >> _hasFaceColors;
            stream >> _faceColors;
        }

        if(formatVersion >= 3) {
            // Load normals (three per face).
            stream >> _hasNormals;
            stream >> _normals;
        }

        // Load faces.
        int nFaces;
        stream >> nFaces;
        _faces.resize(nFaces);
        for(TriMeshFace& face : faces()) {
            stream >> face._flags;
            stream >> face._vertices[0];
            stream >> face._vertices[1];
            stream >> face._vertices[2];
            stream >> face._smoothingGroups;
            stream >> face._materialIndex;
        }

        stream.closeChunk();
    }
    stream.closeChunk();
}


/******************************************************************************
* Flip the orientation of all faces.
******************************************************************************/
void TriangleMesh::flipFaces()
{
    for(TriMeshFace& face : faces()) {
        face.setVertices(face.vertex(2), face.vertex(1), face.vertex(0));
        face.setEdgeVisibility(face.edgeVisible(1), face.edgeVisible(0), face.edgeVisible(2));
    }
    if(hasNormals()) {
        // Negate normal vectors and swap normals of first and third face vertex.
        for(auto n = _normals.begin(); n != _normals.end(); ) {
            auto& n1 = *n++;
            *n = -(*n);
            ++n;
            auto temp = -(*n);
            *n = -n1;
            n1 = temp;
            ++n;
        }
    }
}

/******************************************************************************
* Performs a ray intersection calculation.
******************************************************************************/
bool TriangleMesh::intersectRay(const Ray3& ray, FloatType& t, Vector3& normal, int& faceIndex, bool backfaceCull) const
{
    FloatType bestT = FLOATTYPE_MAX;
    int index = 0;
    for(auto face = faces().constBegin(); face != faces().constEnd(); ++face) {

        Point3 v0 = vertex(face->vertex(0));
        Vector3 e1 = vertex(face->vertex(1)) - v0;
        Vector3 e2 = vertex(face->vertex(2)) - v0;

        Vector3 h = ray.dir.cross(e2);
        FloatType a = e1.dot(h);

        if(std::fabs(a) < FLOATTYPE_EPSILON)
            continue;

        FloatType f = 1 / a;
        Vector3 s = ray.base - v0;
        FloatType u = f * s.dot(h);

        if(u < FloatType(0) || u > FloatType(1))
            continue;

        Vector3 q = s.cross(e1);
        FloatType v = f * ray.dir.dot(q);

        if(v < FloatType(0) || u + v > FloatType(1))
            continue;

        FloatType tt = f * e2.dot(q);

        if(tt < FLOATTYPE_EPSILON)
            continue;

        if(tt >= bestT)
            continue;

        // Compute face normal.
        Vector3 faceNormal = e1.cross(e2);
        if(faceNormal.isZero(FLOATTYPE_EPSILON)) continue;

        // Do backface culling.
        if(backfaceCull && faceNormal.dot(ray.dir) >= 0)
            continue;

        bestT = tt;
        normal = faceNormal;
        faceIndex = (face - faces().constBegin());
    }

    if(bestT != FLOATTYPE_MAX) {
        t = bestT;
        return true;
    }

    return false;
}

/******************************************************************************
* Exports the triangle mesh to a VTK file.
******************************************************************************/
void TriangleMesh::saveToVTK(CompressedTextWriter& stream) const
{
    stream << "# vtk DataFile Version 3.0\n";
    stream << "# Triangle mesh\n";
    stream << "ASCII\n";
    stream << "DATASET UNSTRUCTURED_GRID\n";
    stream << "POINTS " << vertexCount() << " double\n";
    for(const Point3& p : vertices())
        stream << p.x() << " " << p.y() << " " << p.z() << "\n";
    stream << "\nCELLS " << faceCount() << " " << (faceCount() * 4) << "\n";
    for(const TriMeshFace& f : faces()) {
        stream << "3";
        for(size_t i = 0; i < 3; i++)
            stream << " " << f.vertex(i);
        stream << "\n";
    }
    stream << "\nCELL_TYPES " << faceCount() << "\n";
    for(size_t i = 0; i < faceCount(); i++)
        stream << "5\n";    // Triangle
}

/******************************************************************************
* Exports the triangle mesh to a Wavefront .obj file.
******************************************************************************/
void TriangleMesh::saveToOBJ(CompressedTextWriter& stream) const
{
    stream << "# Wavefront OBJ file written by OVITO\n";
    stream << "# List of geometric vertices:\n";
    for(const Point3& p : vertices())
        stream << "v " << p.x() << " " << p.y() << " " << p.z() << "\n";
    stream << "# List of faces:\n";
    for(const TriMeshFace& f : faces()) {
        stream << "f ";
        for(size_t i = 0; i < 3; i++)
            stream << " " << (f.vertex(i)+1);
        stream << "\n";
    }
}

/******************************************************************************
* Clips the mesh at the given plane.
******************************************************************************/
void TriangleMesh::clipAtPlane(const Plane3& plane)
{
    OORef<TriangleMesh> clippedMesh = OORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    clippedMesh->setHasVertexColors(hasVertexColors());
    clippedMesh->setHasVertexPseudoColors(hasVertexPseudoColors());
    clippedMesh->setHasFaceColors(hasFaceColors());
    clippedMesh->setHasFacePseudoColors(hasFacePseudoColors());

    // Clip vertices.
    std::vector<int> existingVertexMapping(vertexCount(), -1);
    for(int vindex = 0; vindex < vertexCount(); vindex++) {
        if(plane.classifyPoint(vertex(vindex)) != 1) {
            existingVertexMapping[vindex] = clippedMesh->addVertex(vertex(vindex));
            if(hasVertexColors())
                clippedMesh->vertexColors().back() = vertexColor(vindex);
            if(hasVertexPseudoColors())
                clippedMesh->vertexPseudoColors().back() = vertexPseudoColor(vindex);
        }
    }

    // Clip edges.
    clippedMesh->setHasNormals(hasNormals());
    std::map<std::pair<int,int>, std::pair<int,FloatType>> newVertexMapping;
    for(const TriMeshFace& face : faces()) {
        for(int v = 0; v < 3; v++) {
            auto vindices = std::make_pair(face.vertex(v), face.vertex((v+1)%3));
            if(vindices.first > vindices.second) std::swap(vindices.first, vindices.second);
            const Point3& v1 = vertex(vindices.first);
            const Point3& v2 = vertex(vindices.second);
            // Check if edge intersects plane.
            FloatType z1 = plane.pointDistance(v1);
            FloatType z2 = plane.pointDistance(v2);
            if((z1 < FLOATTYPE_EPSILON && z2 > FLOATTYPE_EPSILON) || (z2 < FLOATTYPE_EPSILON && z1 > FLOATTYPE_EPSILON)) {
                if(newVertexMapping.find(vindices) == newVertexMapping.end()) {
                    FloatType t = z1 / (z1 - z2);
                    Point3 intersection = v1 + (v2 - v1) * t;
                    newVertexMapping.emplace(vindices, std::make_pair(clippedMesh->addVertex(intersection), t));
                    if(hasVertexColors()) {
                        const auto& color1 = vertexColor(vindices.first);
                        const auto& color2 = vertexColor(vindices.second);
                        auto& newColor = clippedMesh->vertexColors().back();
                        newColor.r() = color1.r() + (color2.r() - color1.r()) * static_cast<GraphicsFloatType>(t);
                        newColor.g() = color1.g() + (color2.g() - color1.g()) * static_cast<GraphicsFloatType>(t);
                        newColor.b() = color1.b() + (color2.b() - color1.b()) * static_cast<GraphicsFloatType>(t);
                        newColor.a() = color1.a() + (color2.a() - color1.a()) * static_cast<GraphicsFloatType>(t);
                    }
                    if(hasVertexPseudoColors()) {
                        FloatType pseudoColor1 = vertexPseudoColor(vindices.first);
                        FloatType pseudoColor2 = vertexPseudoColor(vindices.second);
                        clippedMesh->vertexPseudoColors().back() = pseudoColor1 + (pseudoColor2 - pseudoColor1) * t;
                    }
                }
            }
        }
    }

    // Clip faces.
    int faceIndex = 0;
    for(const TriMeshFace& face : faces()) {
        for(int v0 = 0; v0 < 3; v0++) {
            Point3 current_pos = vertex(face.vertex(v0));
            int current_classification = plane.classifyPoint(current_pos);
            if(current_classification == -1) {
                int newface[4];
                Vector3G newface_normals[4];
                bool newface_edge_visibility[4];
                int vout = 0;
                int next_classification;
                for(int v = v0; v < v0 + 3; v++, current_classification = next_classification) {
                    int vwrapped = v % 3;
                    next_classification = plane.classifyPoint(vertex(face.vertex((v+1)%3)));
                    if((next_classification <= 0 && current_classification <= 0) || (next_classification == 1 && current_classification == 0)) {
                        OVITO_ASSERT(existingVertexMapping[face.vertex(vwrapped)] >= 0);
                        OVITO_ASSERT(vout <= 3);
                        if(hasNormals())
                            newface_normals[vout] = faceVertexNormal(faceIndex, vwrapped);
                        newface_edge_visibility[vout] = face.edgeVisible(vwrapped);
                        newface[vout++] = existingVertexMapping[face.vertex(vwrapped)];
                    }
                    else if((current_classification == +1 && next_classification == -1) || (current_classification == -1 && next_classification == +1)) {
                        auto vindices = std::make_pair(face.vertex(vwrapped), face.vertex((v+1)%3));
                        if(vindices.first > vindices.second) std::swap(vindices.first, vindices.second);
                        auto ve = newVertexMapping.find(vindices);
                        OVITO_ASSERT(ve != newVertexMapping.end());
                        newface_edge_visibility[vout] = face.edgeVisible(vwrapped);
                        if(current_classification == -1) {
                            OVITO_ASSERT(vout <= 3);
                            if(hasNormals())
                                newface_normals[vout] = faceVertexNormal(faceIndex, vwrapped);
                            newface[vout++] = existingVertexMapping[face.vertex(vwrapped)];
                            newface_edge_visibility[vout] = false;
                        }
                        OVITO_ASSERT(vout <= 3);
                        if(hasNormals()) {
                            FloatType t = ve->second.second;
                            if(vindices.first != face.vertex(vwrapped)) t = FloatType(1)-t;
                            newface_normals[vout] = faceVertexNormal(faceIndex, v%3) * t + faceVertexNormal(faceIndex, (v+1)%3) * (FloatType(1) - t);
                            newface_normals[vout].normalizeSafely();
                        }
                        newface[vout++] = ve->second.first;
                    }
                }
                if(vout >= 3) {
                    OVITO_ASSERT(newface[0] >= 0 && newface[0] < clippedMesh->vertexCount());
                    OVITO_ASSERT(newface[1] >= 0 && newface[1] < clippedMesh->vertexCount());
                    OVITO_ASSERT(newface[2] >= 0 && newface[2] < clippedMesh->vertexCount());
                    TriMeshFace& face1 = clippedMesh->addFace();
                    face1.setVertices(newface[0], newface[1], newface[2]);
                    face1.setSmoothingGroups(face.smoothingGroups());
                    face1.setMaterialIndex(face.materialIndex());
                    if(hasNormals()) {
                        auto n = clippedMesh->normals().end() - 3;
                        *n++ = newface_normals[0];
                        *n++ = newface_normals[1];
                        *n = newface_normals[2];
                    }
                    if(hasFaceColors()) {
                        clippedMesh->faceColors().back() = faceColor(faceIndex);
                    }
                    if(hasFacePseudoColors()) {
                        clippedMesh->facePseudoColors().back() = facePseudoColor(faceIndex);
                    }
                    if(vout == 4) {
                        OVITO_ASSERT(newface[3] >= 0 && newface[3] < clippedMesh->vertexCount());
                        OVITO_ASSERT(newface[3] != newface[0]);
                        face1.setEdgeVisibility(newface_edge_visibility[0], newface_edge_visibility[1], false);
                        TriMeshFace& face2 = clippedMesh->addFace();
                        face2.setVertices(newface[0], newface[2], newface[3]);
                        face2.setSmoothingGroups(face.smoothingGroups());
                        face2.setMaterialIndex(face.materialIndex());
                        face2.setEdgeVisibility(false, newface_edge_visibility[2], newface_edge_visibility[3]);
                        if(hasNormals()) {
                            auto n = clippedMesh->normals().end() - 3;
                            *n++ = newface_normals[0];
                            *n++ = newface_normals[2];
                            *n = newface_normals[3];
                        }
                        if(hasFaceColors()) {
                            clippedMesh->faceColors().back() = faceColor(faceIndex);
                        }
                        if(hasFacePseudoColors()) {
                            clippedMesh->facePseudoColors().back() = facePseudoColor(faceIndex);
                        }
                    }
                    else {
                        face1.setEdgeVisibility(newface_edge_visibility[0], newface_edge_visibility[1], newface_edge_visibility[2]);
                    }
                }
                break;
            }
        }
        faceIndex++;
    }

    this->swap(*clippedMesh);
}

/******************************************************************************
* Determines the visibility of face edges depending on the angle between the normals of adjacent faces.
******************************************************************************/
void TriangleMesh::determineEdgeVisibility(FloatType thresholdAngle)
{
    FloatType dotThreshold = std::cos(thresholdAngle);

    // Build map of face edges and their adjacent faces.
    std::map<std::pair<int,int>,int> edgeMap;
    int faceIndex = 0;
    for(TriMeshFace& face : faces()) {
        for(size_t e = 0; e < 3; e++) {
            int v1 = face.vertex(e);
            int v2 = face.vertex((e+1)%3);
            if(v2 > v1)
                edgeMap.emplace(std::make_pair(v1,v2), faceIndex);
        }
        face.setEdgeVisibility(true, true, true);
        faceIndex++;
    }

    // Helper function that computes the normal of a triangle face.
    auto computeFaceNormal = [this](const TriMeshFace& face) {
        const Point3& p0 = vertex(face.vertex(0));
        Vector3 d1 = vertex(face.vertex(1)) - p0;
        Vector3 d2 = vertex(face.vertex(2)) - p0;
        return d2.cross(d1).safelyNormalized();
    };

    // Visit all face edges again.
    for(TriMeshFace& face : faces()) {
        for(size_t e = 0; e < 3; e++) {
            int v1 = face.vertex(e);
            int v2 = face.vertex((e+1)%3);
            if(v2 < v1) {
                // Look up the adjacent face for the current edge.
                if(auto iter = edgeMap.find(std::make_pair(v2,v1)); iter != edgeMap.end()) {
                    TriMeshFace& adjacentFace = this->face(iter->second);
                    // Always retain edges between two faces with different colors or not belonging to the same smoothing group.
                    if(adjacentFace.materialIndex() != face.materialIndex())
                        continue;
                    Vector3 normal1 = computeFaceNormal(face);
                    // Look up the opposite edge.
                    for(size_t e2 = 0; e2 < 3; e2++) {
                        if(adjacentFace.vertex(e2) == v2 && adjacentFace.vertex((e2+1)%3) == v1) {
                            Vector3 normal2 = computeFaceNormal(adjacentFace);
                            if(normal1.dot(normal2) > dotThreshold) {
                                face.setEdgeHidden(e);
                                adjacentFace.setEdgeHidden(e2);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

/******************************************************************************
* Identifies duplicate vertices and merges them into a single vertex shared
* by multiple faces.
******************************************************************************/
void TriangleMesh::removeDuplicateVertices(FloatType epsilon)
{
    std::vector<int> remapping(vertexCount(), -1);

    int vcount = vertexCount();
    auto p1 = vertices().cbegin();
    for(int v1 = 0; v1 < vcount; v1++, ++p1) {
        if(remapping[v1] != -1) continue;
        auto p2 = p1 + 1;
        for(int v2 = v1 + 1; v2 < vcount; v2++, ++p2) {
            if(p1->equals(*p2, epsilon)) {
                remapping[v2] = v1;
            }
        }
    }

    int newIndex = 0;
    auto pold = vertices().begin();
    auto pnew = vertices().begin();
    for(int& rm : remapping) {
        if(rm == -1) {
            *pnew++ = *pold++;
            rm = newIndex++;
        }
        else {
            rm = remapping[rm];
            ++pold;
        }
    }

    for(TriMeshFace& face : faces()) {
        for(size_t v = 0; v < 3; v++) {
            face.setVertex(v, remapping[face.vertex(v)]);
        }
    }

    setVertexCount(newIndex);
    invalidateVertices();
}

/******************************************************************************
* Creates a triangulated unit sphere model by subdividing a icosahedron.
* The resolution parameter controls the number of subdivision iterations and determines the
* resulting vertices/faces of the mesh.
******************************************************************************/
void TriangleMesh::createIcosphere(int resolution)
{
    OVITO_ASSERT(resolution >= 0);

    constexpr FloatType X = 0.525731112119133606;
    constexpr FloatType Z = 0.850650808352039932;
    constexpr FloatType N = 0.0;

    static const Point3 vertices[] = {
        {-X,N,Z}, {X,N,Z}, {-X,N,-Z}, {X,N,-Z},
        {N,Z,X}, {N,Z,-X}, {N,-Z,X}, {N,-Z,-X},
        {Z,X,N}, {-Z,X, N}, {Z,-X,N}, {-Z,-X, N}
    };

    static const std::array<int,3> triangles[] = {
        {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
        {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
        {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
        {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
    };

    clear();
    setVertexCount(sizeof(vertices) / sizeof(vertices[0]));
    setFaceCount(sizeof(triangles) / sizeof(triangles[0]));
    boost::range::copy(vertices, this->vertices().begin());
    for(int i = 0; i < faceCount(); i++)
        face(i).setVertices(triangles[i][2], triangles[i][1], triangles[i][0]);

    using Lookup = std::map<std::pair<int, int>, int>;

    for(int i = 0; i < resolution; i++) {
        Lookup lookup;
        QVector<TriMeshFace> newFaces(faceCount() * 4);
        auto newFace = newFaces.begin();
        for(TriMeshFace& face : faces()) {
            std::array<int, 3> mid;
            for(int edge = 0; edge < 3; edge++) {
                int first = face.vertex(edge);
                int second = face.vertex((edge+1)%3);

                Lookup::key_type key(first, second);
                if(key.first > key.second)
                    std::swap(key.first, key.second);

                auto inserted = lookup.insert({key, vertexCount()});
                if(inserted.second) {
                    const Vector3& edge0 = vertex(first) - Point3::Origin();
                    const Vector3& edge1 = vertex(second) - Point3::Origin();
                    Point3 point = Point3::Origin() + (edge0 + edge1).normalized();
                    addVertex(point);
                }

                mid[edge] =  inserted.first->second;
            }
            (*newFace++).setVertices(face.vertex(0), mid[0], mid[2]);
            (*newFace++).setVertices(face.vertex(1), mid[1], mid[0]);
            (*newFace++).setVertices(face.vertex(2), mid[2], mid[1]);
            (*newFace++).setVertices(mid[0], mid[1], mid[2]);
        }
        _faces = std::move(newFaces);
    }
}

/******************************************************************************
* Creates a unit superellipsoid.
******************************************************************************/
void TriangleMesh::createSuperellipsoid(int resolutionU, int resolutionV, FloatType epsilon1, FloatType epsilon2)
{
    OVITO_ASSERT(resolutionU > 0);
    OVITO_ASSERT(resolutionV > 0);

    clear();
    setHasNormals(true);
    std::vector<Vector3G> vertexNormals;

    // Add top vertex.
    auto v0 = addVertex(Point3(0, 0, 1));
    vertexNormals.push_back(Vector3G(0, 0, 1));

    // Generate UV vertices.
    for(int i = 0; i < resolutionV - 1; i++) {
        auto phi = FLOATTYPE_PI * FloatType(i + 1) / resolutionV;
        for(int j = 0; j < resolutionU; j++) {
            auto theta = 2 * FLOATTYPE_PI * FloatType(j) / resolutionU;
            auto sin_phi = std::sin(phi);
            auto cos_phi = std::cos(phi);
            auto sin_theta = std::sin(theta);
            auto cos_theta = std::cos(theta);
            auto x = std::copysign(FloatType(1), sin_phi) * std::pow(std::abs(sin_phi), epsilon2) * std::copysign(FloatType(1), cos_theta) * std::pow(std::abs(cos_theta), epsilon1);
            auto y = std::copysign(FloatType(1), sin_phi) * std::pow(std::abs(sin_phi), epsilon2) * std::copysign(FloatType(1), sin_theta) * std::pow(std::abs(sin_theta), epsilon1);
            auto z = std::copysign(FloatType(1), cos_phi) * std::pow(std::abs(cos_phi), epsilon2);
            addVertex(Point3(x, y, z));
            auto nx = std::copysign(FloatType(1), sin_phi) * std::pow(std::abs(sin_phi), 2 - epsilon2) * std::copysign(FloatType(1), cos_theta) * std::pow(std::abs(cos_theta), 2 - epsilon1);
            auto ny = std::copysign(FloatType(1), sin_phi) * std::pow(std::abs(sin_phi), 2 - epsilon2) * std::copysign(FloatType(1), sin_theta) * std::pow(std::abs(sin_theta), 2 - epsilon1);
            auto nz = std::copysign(FloatType(1), cos_phi) * std::pow(std::abs(cos_phi), 2 - epsilon2);
            vertexNormals.push_back(Vector3G(nx, ny, nz).normalized());
        }
    }

    // Add bottom vertex.
    auto v1 = addVertex(Point3(0, 0, -1));
    vertexNormals.push_back(Vector3G(0, 0, -1));

    // Add top / bottom triangles.
    for(int i = 0; i < resolutionU; i++) {
        auto i0 = i + 1;
        auto i1 = (i + 1) % resolutionU + 1;
        TriMeshFace& f1 = addFace();
        f1.setVertices(v0, i0, i1);
        i0 = i + resolutionU * (resolutionV - 2) + 1;
        i1 = (i + 1) % resolutionU + resolutionU * (resolutionV - 2) + 1;
        TriMeshFace& f2 = addFace();
        f2.setVertices(v1, i1, i0);
    }

    // Add quads per stack / slice.
    for(int j = 0; j < resolutionV - 2; j++) {
        auto j0 = j * resolutionU + 1;
        auto j1 = (j + 1) * resolutionU + 1;
        for(int i = 0; i < resolutionU; i++) {
            auto i0 = j0 + i;
            auto i1 = j0 + (i + 1) % resolutionU;
            auto i2 = j1 + (i + 1) % resolutionU;
            auto i3 = j1 + i;
            TriMeshFace& f1 = addFace();
            f1.setVertices(i0, i2, i1);
            f1.setEdgeVisibility(false, true, true);
            TriMeshFace& f2 = addFace();
            f2.setVertices(i0, i3, i2);
            f2.setEdgeVisibility(true, true, false);
        }
    }

    auto n = normals().begin();
    for(const TriMeshFace& face : faces()) {
        for(int v = 0; v < 3; v++) {
            *n++ = vertexNormals[face.vertex(v)];
        }
    }
}

/******************************************************************************
* Creates an axis-aligned box geometry.
******************************************************************************/
void TriangleMesh::createBox(const Box3& box)
{
    clear();

    // Create a box with 8 vertices and 12 triangular faces.
    setVertexCount(8);
    setFaceCount(12);

    // Add vertices.
    vertex(0) = box.minc;
    vertex(1) = Point3(box.minc.x(), box.minc.y(), box.maxc.z());
    vertex(2) = Point3(box.minc.x(), box.maxc.y(), box.minc.z());
    vertex(3) = Point3(box.minc.x(), box.maxc.y(), box.maxc.z());
    vertex(4) = Point3(box.maxc.x(), box.minc.y(), box.minc.z());
    vertex(5) = Point3(box.maxc.x(), box.minc.y(), box.maxc.z());
    vertex(6) = Point3(box.maxc.x(), box.maxc.y(), box.minc.z());
    vertex(7) = box.maxc;

    // Add faces.
    TriMeshFace& f1 = face(0);
    f1.setVertices(0, 1, 2);
    TriMeshFace& f2 = face(1);
    f2.setVertices(1, 3, 2);
    TriMeshFace& f3 = face(2);
    f3.setVertices(4, 6, 5);
    TriMeshFace& f4 = face(3);
    f4.setVertices(5, 6, 7);
    TriMeshFace& f5 = face(4);
    f5.setVertices(0, 4, 1);
    TriMeshFace& f6 = face(5);
    f6.setVertices(1, 4, 5);
    TriMeshFace& f7 = face(6);
    f7.setVertices(2, 3, 6);
    TriMeshFace& f8 = face(7);
    f8.setVertices(3, 7, 6);
    TriMeshFace& f9 = face(8);
    f9.setVertices(0, 2, 4);
    TriMeshFace& f10 = face(9);
    f10.setVertices(2, 6, 4);
    TriMeshFace& f11 = face(10);
    f11.setVertices(1, 5, 3);
    TriMeshFace& f12 = face(11);
    f12.setVertices(3, 5, 7);

    invalidateVertices();
}

/******************************************************************************
* Determines whether the mesh forms a closed manifold, i.e. each triangle has
* three adjacent triangles with correct orientation.
******************************************************************************/
bool TriangleMesh::isClosed() const
{
    // All face edges (pairs of vertex indices):
    std::set<std::pair<int,int>> edges;
    for(const TriMeshFace& face : faces()) {
        for(size_t e = 0; e < 3; e++) {
            int v1 = face.vertex(e);
            int v2 = face.vertex((e+1) % 3);
            if(edges.insert(std::make_pair(v1, v2)).second == false)
                return false; // Two half-edges connecting the same vertices v1 and v2.
        }
    }

    // Check if each edge has an opposite partner.
    for(auto [v1,v2] : edges) {
        if(edges.find(std::make_pair(v2,v1)) == edges.end())
            return false;
    }

    return true;
}

}   // End of namespace
