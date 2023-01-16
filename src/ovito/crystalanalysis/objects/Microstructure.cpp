////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include "Microstructure.h"

namespace Ovito::CrystalAnalysis {

IMPLEMENT_OVITO_CLASS(Microstructure);

/******************************************************************************
* Constructor.
******************************************************************************/
Microstructure::Microstructure(ObjectCreationParams params) : SurfaceMesh(params)
{
    if(params.createSubObjects()) {
        makeFacesMutable()->createProperty(SurfaceMeshFaces::RegionProperty);
        makeFacesMutable()->createProperty(SurfaceMeshFaces::BurgersVectorProperty);
        makeFacesMutable()->createProperty(SurfaceMeshFaces::FaceTypeProperty);
        makeFacesMutable()->createProperty(SurfaceMeshFaces::CrystallographicNormalProperty);
        makeRegionsMutable()->createProperty(SurfaceMeshRegions::PhaseProperty);
    }
}

/******************************************************************************
* Constructor that adopts the data from the given pipeline data object into
* this structure.
******************************************************************************/
MicrostructureAccess::MicrostructureAccess(const Microstructure* mo) : SurfaceMeshAccess(mo)
{
}

/******************************************************************************
* Create a dislocation line segment between two nodal points.
******************************************************************************/
MicrostructureAccess::edge_index MicrostructureAccess::createDislocationSegment(vertex_index vertex1, vertex_index vertex2, const Vector3& burgersVector, region_index region)
{
    face_index face1 = createFace({vertex1, vertex2}, region, DISLOCATION,  burgersVector, Vector3::Zero());
    face_index face2 = createFace({vertex2, vertex1}, region, DISLOCATION, -burgersVector, Vector3::Zero());
    // Note: We are intentionally linking only one pair of opposite half-edges here.
    // The other two face edges remain without an opposite edge partner
    // to mark them as as virtual dislocation segments, which exist only to close the face boundaries.
    linkOppositeEdges(firstFaceEdge(face1), firstFaceEdge(face2));
    linkOppositeFaces(face1, face2);
    return firstFaceEdge(face1);
}

/******************************************************************************
* Merges virtual dislocation faces to build continuous lines from individual
* dislocation segments.
******************************************************************************/
void MicrostructureAccess::makeContinuousDislocationLines()
{
    size_t joined = 0;

    // Process each vertex in the microstructure.
    auto vertexCount = this->vertexCount();
    for(vertex_index vertex = 0; vertex < vertexCount; vertex++) {

        // Specifically look for 2-nodes which are part of continuous dislocation lines.
        int armCount = 0;
        edge_index arms[3];
        for(edge_index e = firstVertexEdge(vertex); e != SurfaceMeshAccess::InvalidIndex; e = nextVertexEdge(e)) {
            if(isPhysicalDislocationEdge(e)) {
                arms[armCount++] = e;
                if(armCount == 3) break;
            }
        }
        if(armCount == 2) {
            edge_index arm1 = arms[0];
            edge_index arm2 = arms[1];

            // The segments of a continuous dislocation line must be embedded in the same crystallite.
            if(edgeRegion(arm1) == edgeRegion(arm2)) {

                // Verify that Burgers vector conservation is fulfilled at the 2-node.
//                if(!(burgersVector(adjacentFace(arm1)).equals(-burgersVector(adjacentFace(arm2)))))
//                    continue;
                OVITO_ASSERT(burgersVector(adjacentFace(arm1)).equals(-burgersVector(adjacentFace(arm2))));

                // These conditions must always be fulfilled:
                OVITO_ASSERT(vertex2(prevFaceEdge(arm1)) == vertex);
                OVITO_ASSERT(vertex2(prevFaceEdge(arm2)) == vertex);
                OVITO_ASSERT(adjacentFace(oppositeEdge(arm1)) == oppositeFace(adjacentFace(arm1)));
                OVITO_ASSERT(adjacentFace(oppositeEdge(arm2)) == oppositeFace(adjacentFace(arm2)));
                OVITO_ASSERT(vertex1(prevFaceEdge(arm1)) == vertex2(nextFaceEdge(oppositeEdge(arm1))));
                OVITO_ASSERT(vertex1(prevFaceEdge(arm2)) == vertex2(nextFaceEdge(oppositeEdge(arm2))));

                // Test if the two pairs of virtual faces have already been joined.
                if(adjacentFace(arm1) != adjacentFace(oppositeEdge(arm2))) {

                    edge_index virtualArm1 = nextFaceEdge(oppositeEdge(arm1));
                    edge_index virtualArm2 = nextFaceEdge(oppositeEdge(arm2));

                    // Rewire first edge sequence at the vertex.
                    SurfaceMeshTopology* topo = mutableTopology();
                    topo->setNextFaceEdge(prevFaceEdge(arm1), virtualArm2);
                    topo->setPrevFaceEdge(virtualArm2, prevFaceEdge(arm1));
                    topo->setPrevFaceEdge(arm1, oppositeEdge(arm2));
                    topo->setNextFaceEdge(oppositeEdge(arm2), arm1);

                    // Rewire second edge sequence at the vertex.
                    topo->setNextFaceEdge(prevFaceEdge(arm2), virtualArm1);
                    topo->setPrevFaceEdge(virtualArm1, prevFaceEdge(arm2));
                    topo->setPrevFaceEdge(arm2, oppositeEdge(arm1));
                    topo->setNextFaceEdge(oppositeEdge(arm1), arm2);

                    face_index delFace1 = adjacentFace(oppositeEdge(arm2));
                    face_index delFace2 = adjacentFace(arm2);
                    face_index keepFace1 = adjacentFace(arm1);
                    face_index keepFace2 = adjacentFace(oppositeEdge(arm1));
                    OVITO_ASSERT(oppositeFace(delFace1) == delFace2);
                    OVITO_ASSERT(burgersVector(delFace1).equals(-burgersVector(delFace2)));
                    OVITO_ASSERT(oppositeFace(keepFace1) == keepFace2);
                    OVITO_ASSERT(burgersVector(keepFace1).equals(-burgersVector(keepFace2)));

                    // Make sure the first edge of a face is always the one at the beginning of the
                    // corresponding continuous dislocation line.
                    topo->setFirstFaceEdge(keepFace1, firstFaceEdge(adjacentFace(virtualArm2)));

                    // Transfer edges of the faces that are going to be removed to the remaining faces.
                    for(edge_index currentEdge = virtualArm2; currentEdge != arm1; currentEdge = nextFaceEdge(currentEdge)) {
                        topo->setAdjacentFace(currentEdge, keepFace1);
                    }
                    for(edge_index currentEdge = arm2; currentEdge != virtualArm1; currentEdge = nextFaceEdge(currentEdge)) {
                        topo->setAdjacentFace(currentEdge, keepFace2);
                    }

                    // Delete one pair of faces from the mesh.
                    topo->setFirstFaceEdge(delFace1, SurfaceMeshAccess::InvalidIndex);
                    topo->setFirstFaceEdge(delFace2, SurfaceMeshAccess::InvalidIndex);
                    topo->unlinkFromOppositeFace(delFace1);

                    // Make sure we delete the faces in an ordered fashion, starting from the back.
                    if(delFace1 < delFace2) std::swap(delFace1, delFace2);
                    deleteFace(delFace1);
                    deleteFace(delFace2);

                    joined++;
                }
            }
        }
    }
}

}   // End of namespace
