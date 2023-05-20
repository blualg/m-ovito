////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "SurfaceMeshReplicateModifierDelegate.h"

namespace Ovito::Mesh {

IMPLEMENT_OVITO_CLASS(SurfaceMeshReplicateModifierDelegate);

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus SurfaceMeshReplicateModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    ReplicateModifier* mod = static_object_cast<ReplicateModifier>(request.modifier());

    std::array<int,3> nPBC;
    nPBC[0] = std::max(mod->numImagesX(),1);
    nPBC[1] = std::max(mod->numImagesY(),1);
    nPBC[2] = std::max(mod->numImagesZ(),1);

    size_t numCopies = nPBC[0] * nPBC[1] * nPBC[2];
    if(numCopies <= 1)
        return PipelineStatus::Success;

    Box3I newImages = mod->replicaRange();

    for(const DataObject* obj : state.data()->objects()) {
        if(const SurfaceMesh* existingSurface = dynamic_object_cast<SurfaceMesh>(obj)) {
            // For replication, a domain is always required.
            if(!existingSurface->domain())
                continue;

            // The simulation cell must not be degenerate.
            AffineTransformation simCell = existingSurface->domain()->cellMatrix();
            auto pbcFlags = existingSurface->domain()->pbcFlags();
            AffineTransformation inverseSimCell;
            if(!simCell.inverse(inverseSimCell))
                continue;

            // Make sure input mesh data structure is in a good state.
            existingSurface->verifyMeshIntegrity();

            // Create a mutable of the input mesh.
            SurfaceMesh* newSurface = state.makeMutable(existingSurface);

            // Create a copy of the mesh topology.
            SurfaceMeshTopology* topology = newSurface->makeTopologyMutable();

            // Create a copy of the vertices sub-object.
            SurfaceMeshVertices* newVertices = newSurface->makeVerticesMutable();

            // Extend the property arrays.
            size_t oldVertexCount = newVertices->elementCount();
            size_t newVertexCount = oldVertexCount * numCopies;
            newVertices->replicate(numCopies);

            // Shift vertex positions by the periodicity vector.
            BufferAccess<Point3> positionProperty = newVertices->expectMutableProperty(SurfaceMeshVertices::PositionProperty);
            Point3* p = positionProperty.begin();
            for(int imageX = newImages.minc.x(); imageX <= newImages.maxc.x(); imageX++) {
                for(int imageY = newImages.minc.y(); imageY <= newImages.maxc.y(); imageY++) {
                    for(int imageZ = newImages.minc.z(); imageZ <= newImages.maxc.z(); imageZ++) {
                        const Vector3 imageDelta = simCell * Vector3(imageX, imageY, imageZ);
                        for(size_t i = 0; i < oldVertexCount; i++)
                            *p++ += imageDelta;
                    }
                }
            }
            positionProperty.reset();

            // Create a copy of the faces sub-object.
            SurfaceMeshFaces* newFaces = newSurface->makeFacesMutable();

            // Replicate all face properties
            size_t oldFaceCount = newFaces->elementCount();
            size_t newFaceCount = oldFaceCount * numCopies;
            newFaces->replicate(numCopies);

            // Add right number of new vertices to the topology.
            for(size_t i = oldVertexCount; i < newVertexCount; i++) {
                topology->createVertex();
            }

            // Replicate topology faces.
            std::vector<SurfaceMesh::vertex_index> newFaceVertices;
            for(int imageX = 0; imageX < nPBC[0]; imageX++) {
                for(int imageY = 0; imageY < nPBC[1]; imageY++) {
                    for(int imageZ = 0; imageZ < nPBC[2]; imageZ++) {
                        if(imageX == 0 && imageY == 0 && imageZ == 0) continue;
                        size_t imageIndexShift = (imageX * nPBC[1] * nPBC[2]) + (imageY * nPBC[2]) + imageZ;
                        // Copy faces.
                        for(SurfaceMesh::face_index face = 0; face < oldFaceCount; face++) {
                            newFaceVertices.clear();
                            SurfaceMesh::edge_index edge = topology->firstFaceEdge(face);
                            do {
                                SurfaceMesh::vertex_index newVertexIndex = topology->vertex1(edge) + imageIndexShift * oldVertexCount;
                                newFaceVertices.push_back(newVertexIndex);
                                edge = topology->nextFaceEdge(edge);
                            }
                            while(edge != topology->firstFaceEdge(face));
                            topology->createFaceAndEdges(newFaceVertices.begin(), newFaceVertices.end());
                        }
                        // Copy face connectivity.
                        for(SurfaceMesh::face_index oldFace = 0; oldFace < oldFaceCount; oldFace++) {
                            SurfaceMesh::face_index newFace = oldFace + imageIndexShift * oldFaceCount;
                            SurfaceMesh::edge_index oldEdge = topology->firstFaceEdge(oldFace);
                            SurfaceMesh::edge_index newEdge = topology->firstFaceEdge(newFace);
                            do {
                                if(topology->hasOppositeEdge(oldEdge)) {
                                    SurfaceMesh::face_index adjacentFaceIndex = topology->adjacentFace(topology->oppositeEdge(oldEdge));
                                    adjacentFaceIndex += imageIndexShift * oldFaceCount;
                                    SurfaceMesh::edge_index newOppositeEdge = topology->findEdge(adjacentFaceIndex, topology->vertex2(newEdge), topology->vertex1(newEdge));
                                    OVITO_ASSERT(newOppositeEdge != SurfaceMesh::InvalidIndex);
                                    if(!topology->hasOppositeEdge(newEdge)) {
                                        topology->linkOppositeEdges(newEdge, newOppositeEdge);
                                    }
                                    else {
                                        OVITO_ASSERT(topology->oppositeEdge(newEdge) == newOppositeEdge);
                                    }
                                }
                                if(topology->nextManifoldEdge(oldEdge) != SurfaceMesh::InvalidIndex) {
                                    SurfaceMesh::face_index nextManifoldFaceIndex = topology->adjacentFace(topology->nextManifoldEdge(oldEdge));
                                    nextManifoldFaceIndex += imageIndexShift * oldFaceCount;
                                    SurfaceMesh::edge_index newManifoldEdge = topology->findEdge(nextManifoldFaceIndex, topology->vertex1(newEdge), topology->vertex2(newEdge));
                                    OVITO_ASSERT(newManifoldEdge != SurfaceMesh::InvalidIndex);
                                    topology->setNextManifoldEdge(newEdge, newManifoldEdge);
                                }
                                oldEdge = topology->nextFaceEdge(oldEdge);
                                newEdge = topology->nextFaceEdge(newEdge);
                            }
                            while(oldEdge != topology->firstFaceEdge(oldFace));

                            // Link opposite faces.
                            SurfaceMesh::face_index oldOppositeFace = topology->oppositeFace(oldFace);
                            if(oldOppositeFace != SurfaceMesh::InvalidIndex) {
                                SurfaceMesh::face_index newOppositeFace = oldOppositeFace + imageIndexShift * oldFaceCount;
                                topology->linkOppositeFaces(newFace, newOppositeFace);
                            }
                        }
                    }
                }
            }
            OVITO_ASSERT(topology->faceCount() == newFaceCount);

            if(pbcFlags[0] || pbcFlags[1] || pbcFlags[2]) {
                BufferAccess<const Point3> vertexCoords = newVertices->getProperty(SurfaceMeshVertices::PositionProperty);
                // Unwrap faces that crossed a periodic boundary in the original cell.
                for(SurfaceMesh::face_index face = 0; face < newFaceCount; face++) {
                    SurfaceMesh::edge_index edge = topology->firstFaceEdge(face);
                    SurfaceMesh::vertex_index v1 = topology->vertex1(edge);
                    SurfaceMesh::vertex_index v1wrapped = v1 % oldVertexCount;
                    Vector3I imageShift = Vector3I::Zero();
                    do {
                        SurfaceMesh::vertex_index v2 = topology->vertex2(edge);
                        SurfaceMesh::vertex_index v2wrapped = v2 % oldVertexCount;
                        Vector3 delta = inverseSimCell * (vertexCoords[v2wrapped] - vertexCoords[v1wrapped]);
                        for(size_t dim = 0; dim < 3; dim++) {
                            if(pbcFlags[dim])
                                imageShift[dim] -= (int)std::floor(delta[dim] + FloatType(0.5));
                        }
                        if(imageShift != Vector3I::Zero()) {
                            size_t imageIndex = v2 / oldVertexCount;
                            Point3I image(imageIndex / nPBC[1] / nPBC[2], (imageIndex / nPBC[2]) % nPBC[1], imageIndex % nPBC[2]);
                            Point3I newImage(SimulationCellObject::modulo(image[0] + imageShift[0], nPBC[0]),
                                            SimulationCellObject::modulo(image[1] + imageShift[1], nPBC[1]),
                                            SimulationCellObject::modulo(image[2] + imageShift[2], nPBC[2]));
                            size_t newImageIndex = (newImage[0] * nPBC[1] * nPBC[2]) + (newImage[1] * nPBC[2]) + newImage[2];
                            SurfaceMesh::vertex_index new_v2 = v2wrapped + newImageIndex * oldVertexCount;
                            topology->transferFaceBoundaryToVertex(edge, new_v2);
                        }
                        v1 = v2;
                        v1wrapped = v2wrapped;
                        edge = topology->nextFaceEdge(edge);
                    }
                    while(edge != topology->firstFaceEdge(face));
                }

                // Since faces that cross a periodic boundary can end up in different images,
                // we now need to "repair" the face connectivity.
                for(SurfaceMesh::face_index face = 0; face < newFaceCount; face++) {
                    SurfaceMesh::edge_index edge = topology->firstFaceEdge(face);
                    do {
                        if(topology->hasOppositeEdge(edge) && topology->vertex2(topology->oppositeEdge(edge)) != topology->vertex1(edge)) {
                            SurfaceMesh::face_index adjacentFaceIndex = topology->adjacentFace(topology->oppositeEdge(edge)) % oldFaceCount;
                            topology->setOppositeEdge(edge, SurfaceMesh::InvalidIndex);
                            for(size_t i = 0; i < numCopies; i++) {
                                SurfaceMesh::edge_index newOppositeEdge = topology->findEdge(adjacentFaceIndex + i * oldFaceCount, topology->vertex2(edge), topology->vertex1(edge));
                                if(newOppositeEdge != SurfaceMesh::InvalidIndex) {
                                    topology->setOppositeEdge(edge, newOppositeEdge);
                                    break;
                                }
                            }
                            OVITO_ASSERT(topology->hasOppositeEdge(edge));
                            OVITO_ASSERT(topology->vertex2(topology->oppositeEdge(edge)) == topology->vertex1(edge));
                        }
                        if(topology->nextManifoldEdge(edge) != SurfaceMesh::InvalidIndex && topology->vertex2(topology->nextManifoldEdge(edge)) != topology->vertex2(edge)) {
                            SurfaceMesh::face_index nextManifoldFaceIndex = topology->adjacentFace(topology->nextManifoldEdge(edge)) % oldFaceCount;
                            topology->setNextManifoldEdge(edge, SurfaceMesh::InvalidIndex);
                            for(size_t i = 0; i < numCopies; i++) {
                                SurfaceMesh::edge_index newNextManifoldEdge = topology->findEdge(nextManifoldFaceIndex + i * oldFaceCount, topology->vertex1(edge), topology->vertex2(edge));
                                if(newNextManifoldEdge != SurfaceMesh::InvalidIndex) {
                                    topology->setNextManifoldEdge(edge, newNextManifoldEdge);
                                    break;
                                }
                            }
                            OVITO_ASSERT(topology->nextManifoldEdge(edge)!= SurfaceMesh::InvalidIndex);
                            OVITO_ASSERT(topology->vertex1(topology->nextManifoldEdge(edge)) == topology->vertex1(edge));
                            OVITO_ASSERT(topology->vertex2(topology->nextManifoldEdge(edge)) == topology->vertex2(edge));
                        }
                        edge = topology->nextFaceEdge(edge);
                    }
                    while(edge != topology->firstFaceEdge(face));
                }
            }

#ifdef OVITO_DEBUG
            // Verify that the connection between pairs of opposite faces is correct.
            for(SurfaceMesh::face_index face = 0; face < newFaceCount; face++) {
                if(!topology->hasOppositeFace(face)) continue;
                SurfaceMesh::edge_index edge = topology->firstFaceEdge(face);
                do {
                    OVITO_ASSERT(topology->findEdge(topology->oppositeFace(face), topology->vertex2(edge), topology->vertex1(edge)) != SurfaceMesh::InvalidIndex);
                    edge = topology->nextFaceEdge(edge);
                }
                while(edge != topology->firstFaceEdge(face));
            }
#endif

            // Extend the periodic domain the surface is embedded in.
            simCell.translation() += (FloatType)newImages.minc.x() * simCell.column(0);
            simCell.translation() += (FloatType)newImages.minc.y() * simCell.column(1);
            simCell.translation() += (FloatType)newImages.minc.z() * simCell.column(2);
            simCell.column(0) *= (newImages.sizeX() + 1);
            simCell.column(1) *= (newImages.sizeY() + 1);
            simCell.column(2) *= (newImages.sizeZ() + 1);
            newSurface->mutableDomain()->setCellMatrix(simCell);
        }
    }

    return PipelineStatus::Success;
}

}   // End of namespace
