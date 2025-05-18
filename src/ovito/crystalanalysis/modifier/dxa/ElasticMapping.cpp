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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "ElasticMapping.h"
#include "CrystalPathFinder.h"
#include "DislocationTracer.h"
#include "DislocationAnalysisEngine.h"

namespace Ovito {

/// Pairs of cell vertices that form the six edges of a tetrahedron.
static constexpr int CellEdgeVertices[6][2] = {{0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}};

/// Triplets of edges that form the Burgers circuits for each face of a tetrahedron.
static constexpr int CellEdgeCircuits[4][3] = {{0,4,2}, {1,5,2}, {0,3,1}, {3,5,4}};

/******************************************************************************
* Builds the list of edges in the tetrahedral tessellation.
******************************************************************************/
void ElasticMapping::generateTessellationEdges(TaskProgress& progress)
{
    progress.setMaximum(tessellation().numberOfPrimaryTetrahedra());

    // Generate list of tessellation edges.
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {

        // Skip invalid cells (those not connecting four physical atoms) and ghost cells.
        if(tessellation().isGhostCell(cell))
            continue;

        // Update progress indicator.
        progress.setValueIntermittent(tessellation().getCellIndex(cell));

        // Get the four Delaunay vertices of the current cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);

        // Create edge data structure for each of the six edges of the cell.
        for(int edgeIndex = 0; edgeIndex < 6; edgeIndex++) {
            // Get the two atoms connected by this edge.
            size_t atom1 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[edgeIndex][0]]);
            size_t atom2 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[edgeIndex][1]]);
            if(atom1 == atom2)
                continue;
            // Check if the edge already exists in the list of edges.
            if(!findEdge(atom1, atom2)) {
                // Avoid creating long edges that violate the minimum image convention, i.e.,
                // which span more than half the simulation cell.
                const Point3& p1 = tessellation().vertexPosition(cellVertices[CellEdgeVertices[edgeIndex][0]]);
                const Point3& p2 = tessellation().vertexPosition(cellVertices[CellEdgeVertices[edgeIndex][1]]);
                if(structureAnalysis().cell().isWrappedVector(p1 - p2))
                    continue;

                // Register a new edge.
                TessellationEdge* edge = _edgePool.construct(atom1, atom2);
                // Insert into the linked list of edges leaving vertex 1 and arriving at vertex 2.
                edge->nextOutboundEdge = std::exchange(_atomOutboundEdges[atom1], edge);
                edge->nextInboundEdge = std::exchange(_atomInboundEdges[atom2], edge);
            }
        }
    }
}

/******************************************************************************
* Assigns each tessellation vertex to a cluster.
******************************************************************************/
void ElasticMapping::assignAtomsToClusters(TaskProgress& progress)
{
    // The total length of this task is unknown.
    progress.setMaximum(0);

    // Assign each atom to some crystal cluster, which will be used for expressing
    // reference vectors assigned to the edges incident on that atom.

    // List of atoms that have not been assigned to a cluster yet.
    const size_t atomCount = structureAnalysis().atomCount();
    std::vector<size_t> unassignedAtoms;
    unassignedAtoms.reserve(atomCount);

    // First, apply most obvious choice:
    // If an atom has been assigned to a cluster by the structural analysis,
    // then we simply adopt this assignment.
    for(size_t atomIndex = 0; atomIndex < atomCount; atomIndex++) {
        Cluster* cluster = structureAnalysis().atomCluster(atomIndex);
        _atomClusters[atomIndex] = cluster;
        if(cluster->id == 0)
            unassignedAtoms.push_back(atomIndex);
    }
    this_task::throwIfCanceled();

    // Now try to assign a cluster to those atoms which have not been assigned yet.
    // This is performed by repeatedly copying the cluster assignment
    // from the already assigned atoms to their unassigned neighbors connected by Delaunay edges.
    while(!unassignedAtoms.empty()) {
        size_t remainingAtomCount = 0;
        for(auto atomIndex : unassignedAtoms) {
            Cluster*& assignedCluster = _atomClusters[atomIndex];
            OVITO_ASSERT(assignedCluster->id == 0);

            // Check if the atom is connected to a cluster by an outbound edge.
            for(TessellationEdge* e = _atomOutboundEdges[atomIndex]; e != nullptr; e = e->nextOutboundEdge) {
                OVITO_ASSERT(e->atom1 == atomIndex);
                if(Cluster* cluster2 = clusterOfAtom(e->atom2); cluster2->id != 0) {
                    assignedCluster = cluster2;
                    break;
                }
            }
            if(assignedCluster->id != 0)
                continue;

            // Check if the atom is connected to a cluster by an inbound edge.
            for(TessellationEdge* e = _atomInboundEdges[atomIndex]; e != nullptr; e = e->nextInboundEdge) {
                OVITO_ASSERT(e->atom2 == atomIndex);
                if(Cluster* cluster1 = clusterOfAtom(e->atom1); cluster1->id != 0) {
                    assignedCluster = cluster1;
                    break;
                }
            }
            if(assignedCluster->id != 0)
                continue;

            // The atom is not connected to a cluster by any edge. Keep it in the list of unassigned atoms.
            unassignedAtoms[remainingAtomCount++] = atomIndex;
        }
        this_task::throwIfCanceled();

        if(unassignedAtoms.size() == remainingAtomCount)
            break; // Could not make any further progress.

        // Reduce the list of unassigned atoms.
        unassignedAtoms.resize(remainingAtomCount);
    }
}

/******************************************************************************
* Determines the ideal vector corresponding to each edge of the tessellation.
******************************************************************************/
void ElasticMapping::assignIdealVectorsToEdges(int crystalPathSteps, TaskProgress& progress)
{
    // Create a path finder, which can determine the ideal lattice vector
    // connecting a given pair of atoms (which don't have to be nearest neighbors).
    CrystalPathFinder pathFinder(_structureAnalysis, crystalPathSteps);

    // Set up progress reporting.
    progress.setMaximum(_edgePool.count());
    size_t progressCounter = 0;

    // Iterate over all edges of the tessellation.
    _edgePool.visitAll([&](TessellationEdge* edge) {
        OVITO_ASSERT(!edge->vector.isValid());

        // Update progress indicator.
        progress.setValueIntermittent(progressCounter++);

        Cluster* cluster1 = clusterOfAtom(edge->atom1);
        Cluster* cluster2 = clusterOfAtom(edge->atom2);
        OVITO_ASSERT(cluster1 && cluster2);
        if(cluster1->id == 0 || cluster2->id == 0)
            return; // One of the atoms is not part of any crystal cluster.

        // Determine the ideal vector connecting the two atoms.
        std::optional<ClusterVector> idealVector = pathFinder.findPath(edge->atom1, edge->atom2);
        if(!idealVector.has_value())
            return;

        // Translate vector to the frame of the atom cluster.
        if(!idealVector->transformToCluster(cluster1, *clusterGraph()))
            return;

        // Assign the right cluster transition to the edge.
        // The two Delaunay vertices may be part of two disconnected components of the cluster graph,
        // in which case the transition is not defined.
        if(ClusterTransition* transition = clusterGraph()->determineClusterTransition(cluster1, cluster2)) {
            // Assign cluster vector to the edge.
            edge->vector = EdgeVector(idealVector->localVec(), transition);
        }
    });
}

/******************************************************************************
* Determines whether the elastic mapping from the physical configuration
* of the crystal to the imaginary, stress-free configuration is compatible
* within the given tessellation cell. Returns false if the mapping is incompatible
* or cannot be determined at all.
******************************************************************************/
bool ElasticMapping::isElasticMappingCompatible(DelaunayTessellation::CellHandle cell) const
{
    // Must be a valid tessellation cell to determine the mapping.
    if(!tessellation().isFiniteCell(cell))
        return false;

    // Get the four Delaunay vertices of the current cell.
    const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);

    // Retrieve the cluster vectors assigned to the six edges of the tetrahedron.
    EdgeVector edgeVectors[6];
    for(int edgeIndex = 0; edgeIndex < 6; edgeIndex++) {
        size_t atom1 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[edgeIndex][0]]);
        size_t atom2 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[edgeIndex][1]]);
        edgeVectors[edgeIndex] = getEdgeClusterVector(atom1, atom2);
        if(!edgeVectors[edgeIndex].isValid())
            return false; // Edge has no valid cluster vector.
    }

    // Perform the Burgers circuit test on each of the four faces of the tetrahedron.
    for(int face = 0; face < 4; face++) {
        const EdgeVector& e1 = edgeVectors[CellEdgeCircuits[face][0]];
        const EdgeVector& e2 = edgeVectors[CellEdgeCircuits[face][1]];
        const EdgeVector& e3 = edgeVectors[CellEdgeCircuits[face][2]];

        // Perform Burgers circuit test on the face.
        // Calculate b = e1 + e2 - e3.
        // Third edge must be flipped, because it's oriented in the opposite direction.
        Vector3 burgersVector = e1.vec() + e1.transition()->reverseTransform(e2.vec()) - e3.vec();
        if(!burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON))
            return false;

        // Perform disclination test on the face.
        ClusterTransition* t1 = e1.transition();
        ClusterTransition* t2 = e2.transition();
        ClusterTransition* t3 = e3.transition();
        if(!t1->isSelfTransition() || !t2->isSelfTransition() || !t3->isSelfTransition()) {
            Matrix3 frankRotation = t3->reverse->tm * t2->tm * t1->tm;
            if(!frankRotation.equals(Matrix3::Identity(), CA_TRANSITION_MATRIX_EPSILON))
                return false;
        }
    }

    return true;
}

}   // End of namespace
