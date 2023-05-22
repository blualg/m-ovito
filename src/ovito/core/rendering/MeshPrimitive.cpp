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
#include <ovito/core/dataset/data/BufferAccess.h>
#include "MeshPrimitive.h"

namespace Ovito {

/******************************************************************************
* Indicates whether the mesh is fully opaque (no semi-transparent colors).
******************************************************************************/
bool MeshPrimitive::isFullyOpaque() const
{
    if(_isMeshFullyOpaque.has_value() == false) {
        if(!_mesh)
            _isMeshFullyOpaque = true;
        else if(_perInstanceColors)
            _isMeshFullyOpaque =
                (_perInstanceColors->dataType() == DataBuffer::Float32)
                ? boost::algorithm::none_of(BufferAccess<const ColorAT<float>>(_perInstanceColors), [](const auto& c) { return c.a() != 1.0f; })
                : boost::algorithm::none_of(BufferAccess<const ColorAT<double>>(_perInstanceColors), [](const auto& c) { return c.a() != 1.0; });
        else if(mesh()->hasVertexColors())
            _isMeshFullyOpaque = (uniformColor().a() >= FloatType(1)) && boost::algorithm::none_of(mesh()->vertexColors(), [](const ColorAG& c) { return c.a() != GraphicsFloatType(1); });
        else if(mesh()->hasVertexPseudoColors())
            _isMeshFullyOpaque = (uniformColor().a() >= FloatType(1));
        else if(mesh()->hasFaceColors())
            _isMeshFullyOpaque = (uniformColor().a() >= FloatType(1)) && boost::algorithm::none_of(mesh()->faceColors(), [](const ColorAG& c) { return c.a() != GraphicsFloatType(1); });
        else if(mesh()->hasFacePseudoColors())
            _isMeshFullyOpaque = (uniformColor().a() >= FloatType(1));
        else if(!materialColors().empty())
            _isMeshFullyOpaque = boost::algorithm::none_of(materialColors(), [](const ColorA& c) { return c.a() != FloatType(1); });
        else
            _isMeshFullyOpaque = (uniformColor().a() >= FloatType(1));
    }
    return *_isMeshFullyOpaque;
}

/******************************************************************************
* Generates a list of renderable triangles. Each triangle consists of three vertices.
******************************************************************************/
void MeshPrimitive::generateRenderableVertices(RenderVertex* renderableVertices, bool highlightSelectedFaces, bool enablePseudoColorMapping) const
{
    if(!mesh())
        return;

    const QVector<Point3>& vertices = mesh()->vertices();
    const ColorAG* vertexColors = mesh()->hasVertexColors() ? mesh()->vertexColors().constData() : nullptr;
    const ColorAG* faceColors = mesh()->hasFaceColors() ? mesh()->faceColors().constData() : nullptr;
    const FloatType* vertexPseudoColors = (enablePseudoColorMapping && mesh()->hasVertexPseudoColors()) ? mesh()->vertexPseudoColors().constData() : nullptr;
    const FloatType* facePseudoColors = (enablePseudoColorMapping && mesh()->hasFacePseudoColors()) ? mesh()->facePseudoColors().constData() : nullptr;
    ColorAT<float> defaultVertexColor = uniformColor().toDataType<float>();

    auto rv = renderableVertices;

    if(!mesh()->hasNormals()) {
        quint32 allMask = 0;

        // Compute face normals.
        std::vector<Vector_3<float>> faceNormals(faceCount());
        auto faceNormal = faceNormals.begin();
        for(const auto& face : mesh()->faces()) {
            const Point3& p0 = vertices[face.vertex(0)];
            Vector3 d1 = vertices[face.vertex(1)] - p0;
            Vector3 d2 = vertices[face.vertex(2)] - p0;
            *faceNormal = d1.cross(d2).toDataType<float>();
            if(*faceNormal != Vector_3<float>::Zero()) {
                allMask |= face.smoothingGroups();
            }
            ++faceNormal;
        }

        // Initialize render vertices.
        faceNormal = faceNormals.begin();
        for(const auto& face : mesh()->faces()) {
            // Initialize render vertices for this face.
            for(size_t v = 0; v < 3; v++, rv++) {
                if(face.smoothingGroups())
                    rv->normal = Vector_3<float>::Zero();
                else
                    rv->normal = *faceNormal;
                rv->position = vertices[face.vertex(v)].toDataType<float>();
                if(vertexColors) {
                    rv->color = vertexColors[face.vertex(v)].toDataType<float>();
                    if(defaultVertexColor.a() != 1) rv->color.a() = defaultVertexColor.a();
                }
                else if(vertexPseudoColors) {
                    rv->color.r() = vertexPseudoColors[face.vertex(v)];
                    rv->color.g() = 0;
                    rv->color.b() = 0;
                    rv->color.a() = defaultVertexColor.a();
                }
                else if(faceColors) {
                    rv->color = faceColors->toDataType<float>();
                    if(defaultVertexColor.a() != 1) rv->color.a() = defaultVertexColor.a();
                }
                else if(facePseudoColors) {
                    rv->color.r() = *facePseudoColors;
                    rv->color.g() = 0;
                    rv->color.b() = 0;
                    rv->color.a() = defaultVertexColor.a();
                }
                else if(face.materialIndex() < materialColors().size() && face.materialIndex() >= 0) {
                    rv->color = materialColors()[face.materialIndex()].toDataType<float>();
                }
                else {
                    rv->color = defaultVertexColor;
                }

                // Override color of faces that are selected.
                if(highlightSelectedFaces && face.isSelected()) {
                    if(!enablePseudoColorMapping)
                        rv->color = faceSelectionColor().toDataType<float>();
                    else
                        rv->color.g() = 1.0f; // Non-zero green-component marks selected faces in pseudo-color mode.
                }
            }
            ++faceNormal;
            if(faceColors)
                ++faceColors;
            if(facePseudoColors)
                ++facePseudoColors;
        }
        OVITO_ASSERT(rv == renderableVertices + 3*faceCount());

        if(allMask) {
            std::vector<Vector_3<float>> groupVertexNormals(vertexCount());
            for(int group = 0; group < OVITO_MAX_NUM_SMOOTHING_GROUPS; group++) {
                quint32 groupMask = quint32(1) << group;
                if((allMask & groupMask) == 0)
                    continue;   // Group is not used.

                // Reset work arrays.
                std::fill(groupVertexNormals.begin(), groupVertexNormals.end(), Vector_3<float>::Zero());

                // Compute vertex normals at original vertices for current smoothing group.
                faceNormal = faceNormals.begin();
                for(const auto& face : mesh()->faces()) {
                    // Skip faces that do not belong to the current smoothing group.
                    if(face.smoothingGroups() & groupMask) {
                        // Add face's normal to vertex normals.
                        for(size_t fv = 0; fv < 3; fv++)
                            groupVertexNormals[face.vertex(fv)] += *faceNormal;
                    }
                    ++faceNormal;
                }

                // Transfer vertex normals from original vertices to render vertices.
                rv = renderableVertices;
                for(const auto& face : mesh()->faces()) {
                    if(face.smoothingGroups() & groupMask) {
                        for(size_t fv = 0; fv < 3; fv++, ++rv)
                            rv->normal += groupVertexNormals[face.vertex(fv)];
                    }
                    else rv += 3;
                }
            }
        }
    }
    else {
        // Use normals stored in the mesh.
        const Vector3G* faceNormal = mesh()->normals().constData();
        for(const auto& face : mesh()->faces()) {
            // Initialize render vertices for this face.
            for(size_t v = 0; v < 3; v++, rv++) {
                rv->normal = (*faceNormal++).toDataType<float>();
                rv->position = vertices[face.vertex(v)].toDataType<float>();
                if(vertexColors) {
                    rv->color = vertexColors[face.vertex(v)].toDataType<float>();
                    if(defaultVertexColor.a() != 1) rv->color.a() = defaultVertexColor.a();
                }
                else if(vertexPseudoColors) {
                    rv->color.r() = vertexPseudoColors[face.vertex(v)];
                    rv->color.g() = 0;
                    rv->color.b() = 0;
                    rv->color.a() = defaultVertexColor.a();
                }
                else if(faceColors) {
                    rv->color = faceColors->toDataType<float>();
                    if(defaultVertexColor.a() != 1) rv->color.a() = defaultVertexColor.a();
                }
                else if(facePseudoColors) {
                    rv->color.r() = *facePseudoColors;
                    rv->color.g() = 0;
                    rv->color.b() = 0;
                    rv->color.a() = defaultVertexColor.a();
                }
                else if(face.materialIndex() >= 0 && face.materialIndex() < materialColors().size()) {
                    rv->color = materialColors()[face.materialIndex()].toDataType<float>();
                }
                else {
                    rv->color = defaultVertexColor;
                }

                // Override color of faces that are selected.
                if(highlightSelectedFaces && face.isSelected()) {
                    if(!enablePseudoColorMapping)
                        rv->color = faceSelectionColor().toDataType<float>();
                    else
                        rv->color.g() = 1.0f; // Non-zero green-component marks selected faces in pseudo-color mode.
                }
            }
            if(faceColors)
                ++faceColors;
            if(facePseudoColors)
                ++facePseudoColors;
        }
    }
    OVITO_ASSERT(rv == renderableVertices + 3*faceCount());
}

/******************************************************************************
* Generates a list of vertices for rendering the wireframe as individual line segments.
******************************************************************************/
ConstDataBufferPtr MeshPrimitive::generateWireframeLines() const
{
    OVITO_ASSERT(mesh());

    // Count how many polygon edge are in the mesh.
    size_t numVisibleEdges = 0;
    for(const TriMeshFace& face : mesh()->faces()) {
        for(size_t e = 0; e < 3; e++)
            if(face.edgeVisible(e)) numVisibleEdges++;
    }

    // Allocate storage buffer for line elements.
    BufferAccessAndRef<Point3G> lines = DataBufferPtr::create(numVisibleEdges * 2, DataBuffer::FloatGraphics, 3);

    // Generate line elements.
    const auto& vertices = mesh()->vertices();
    auto* outVert = lines.begin();
    for(const TriMeshFace& face : mesh()->faces()) {
        for(size_t e = 0; e < 3; e++) {
            if(face.edgeVisible(e)) {
                *outVert++ = vertices[face.vertex(e)].toDataType<GraphicsFloatType>();
                *outVert++ = vertices[face.vertex((e+1)%3)].toDataType<GraphicsFloatType>();
            }
        }
    }
    OVITO_ASSERT(outVert == lines.end());

    return lines.take();
}

}   // End of namespace
