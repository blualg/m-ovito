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
                if(structureAnalysis().cell().isWrappedVector(p2 - p1))
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

    // Iterate over all edges of the tessellation.
    _edgePool.visitAll(progress, [&](TessellationEdge* edge) {
        OVITO_ASSERT(!edge->transition);

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
            edge->vector = idealVector->localVec();
            edge->transition = transition;
        }
    });
}

/******************************************************************************
* Narrows down the bad tessellation region by complementing the lattice vectors of unassigned Delaunay edges.
******************************************************************************/
bool ElasticMapping::complementEdgeVectors()
{
    std::deque<std::tuple<DelaunayTessellation::CellHandle, std::array<OrientedEdge, 3>>> queue;
    size_t numAssignedEdges = 0;

    auto pushEdgeToQueue = [&](DelaunayTessellation::CellHandle cell, std::array<OrientedEdge, 3> facetEdges) {
        OVITO_ASSERT(facetEdges[0].atom2() == facetEdges[1].atom1());
        OVITO_ASSERT(facetEdges[1].atom2() == facetEdges[2].atom1());
        OVITO_ASSERT(facetEdges[2].atom2() == facetEdges[0].atom1());
        if(facetEdges[0].hasEdgeVector() || !facetEdges[1].hasEdgeVector())
            return;
        if(facetEdges[2].hasEdgeVector()) {
            // Infer edge vector from sum of the other two edges.
            facetEdges[0].setEdgeVector((-facetEdges[2]).concatenate(-facetEdges[1], clusterGraph()));
        }
        else {
            // Set edge vector to the reverse of the second edge.
            ClusterTransition* transition = clusterGraph()->determineClusterTransition(clusterOfAtom(facetEdges[0].atom1()), clusterOfAtom(facetEdges[0].atom2()));
            if(!transition)
                return; // No cluster transition defined for this edge.
            facetEdges[0].setEdgeVector(transition->reverseTransform((-facetEdges[1]).vector()), transition);
            OVITO_ASSERT(facetEdges[0].concatenate(facetEdges[1], clusterGraph()).first.isZero(CA_LATTICE_VECTOR_EPSILON));
        }
        OVITO_ASSERT(facetEdges[0].transition()->cluster1 == clusterOfAtom(facetEdges[0].atom1()));
        OVITO_ASSERT(facetEdges[0].transition()->cluster2 == clusterOfAtom(facetEdges[0].atom2()));
        numAssignedEdges++;
        queue.push_back({cell, facetEdges});
    };

    auto processNextEdgeFromQueue = [&]() {
        auto [cell, edges] = queue.front();
        queue.pop_front();
        OVITO_ASSERT(edges[0].hasEdgeVector() && edges[1].hasEdgeVector());
        // The two atoms that form the edge we will circulate around.
        const size_t atom1 = edges[0].atom1();
        const size_t atom2 = edges[0].atom2();
        OVITO_ASSERT(atom1 != atom2);
        OVITO_ASSERT(edges[0].atom2() == edges[1].atom1());
        OVITO_ASSERT(edges[1].atom2() == edges[2].atom1());
        OVITO_ASSERT(edges[2].atom2() == edges[0].atom1());
        const DelaunayTessellation::CellHandle startCell = cell;
        constexpr int tab_next_around_edge[4][4] = {
                {5, 2, 3, 1},
                {3, 5, 0, 2},
                {1, 3, 5, 0},
                {2, 0, 1, 5}};
        do {
            // Find original edge vertices in current cell.
            int localVertex1 = tessellation().findInputPointInCell(cell, atom1);
            int localVertex2 = tessellation().findInputPointInCell(cell, atom2);
            OVITO_ASSERT(localVertex1 >= 0 && localVertex1 < 4);
            OVITO_ASSERT(localVertex2 >= 0 && localVertex2 < 4);
            // The current facet we are at.
            int facet = tab_next_around_edge[localVertex1][localVertex2];

            // Get the six oriented edges of the current Delaunay cell.
            std::array<OrientedEdge, 6> cellEdges = getOrientedEdges(cell);
            // Get the three oriented edges of the current facet.
            std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cellEdges, facet);
            if(facetEdges[0] && facetEdges[1] && facetEdges[2]) {
                OVITO_ASSERT(facetEdges[0].atom2() == facetEdges[1].atom1());
                OVITO_ASSERT(facetEdges[1].atom2() == facetEdges[2].atom1());
                OVITO_ASSERT(facetEdges[2].atom2() == facetEdges[0].atom1());
                while(facetEdges[1].atom1() != atom1) {
                    std::rotate(facetEdges.begin(), facetEdges.begin() + 1, facetEdges.end());
                }
                OVITO_ASSERT(facetEdges[1].atom1() == atom1 && facetEdges[1].atom2() == atom2);
                OVITO_ASSERT(facetEdges[1].undirectedEdge() == edges[0].undirectedEdge());
                if(facetEdges[2].hasEdgeVector()) {
                    std::array<OrientedEdge, 3> reversedFacetEdges = { -facetEdges[0], -facetEdges[2], -facetEdges[1] };
                    pushEdgeToQueue(tessellation().cellAdjacent(cell, facet), reversedFacetEdges);
                }
            }
            // Circulate around edge.
            cell = tessellation().cellAdjacent(cell, facet);
        }
        while(cell != startCell);
    };

    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        if(tessellation().isGhostCell(cell))
            continue;

        // Get the six oriented edges of the Delaunay cell.
        std::array<OrientedEdge, 6> cellEdges = getOrientedEdges(cell);

        // Skip this cell if all six edges are good.
        if(std::ranges::all_of(cellEdges, [](const OrientedEdge& edge) { return edge.hasEdgeVector(); }))
            continue;

        // Consider each of the four facets of the tetrahedron.
        for(int facet = 0; facet < 4; facet++) {
            std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cellEdges, facet);
            if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
                continue; // Skip if one of the edges is missing.
            OVITO_ASSERT(facetEdges[0].atom2() == facetEdges[1].atom1());
            OVITO_ASSERT(facetEdges[1].atom2() == facetEdges[2].atom1());
            OVITO_ASSERT(facetEdges[2].atom2() == facetEdges[0].atom1());

            // Consider each of the three edges of the facet.
            for(int edge = 0; edge < 3; edge++) {
                if(!facetEdges[0].hasEdgeVector() && facetEdges[1].hasEdgeVector()) {
                    bool skipEdge = !facetEdges[2].hasEdgeVector();
                    if(skipEdge) {
                        skipEdge = false;
                        for(TessellationEdge* e = _atomOutboundEdges[facetEdges[0].atom1()]; e != nullptr && !skipEdge; e = e->nextOutboundEdge)
                            skipEdge |= (e->transition != nullptr);
                        for(TessellationEdge* e = _atomInboundEdges[facetEdges[0].atom1()]; e != nullptr && !skipEdge; e = e->nextInboundEdge)
                            skipEdge |= (e->transition != nullptr);
                    }
                    if(!skipEdge) {
                        pushEdgeToQueue(cell, facetEdges);
                        while(!queue.empty()) {
                            processNextEdgeFromQueue();
                        }
                    }
                }
                std::rotate(facetEdges.begin(), facetEdges.begin() + 1, facetEdges.end());
            }
        }
    }
    qInfo() << "Assigned" << numAssignedEdges << "new edges";
    return numAssignedEdges != 0;
}

/******************************************************************************
* Determines whether the elastic mapping from the physical configuration
* of the crystal to the imaginary, stress-free configuration is compatible
* within the given tessellation cell. Returns false if the mapping is incompatible
* or cannot be determined at all.
******************************************************************************/
bool ElasticMapping::isElasticMappingCompatible(const std::array<OrientedEdge, 6>& cellEdges) const
{
    // Perform the Burgers circuit test on each of the four facets of the tetrahedron.
    for(int facet = 0; facet < 4; facet++) {
        std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cellEdges, facet);
        if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
            return false; // Skip if one of the edges is missing.

        OVITO_ASSERT(facetEdges[0].atom2() == facetEdges[1].atom1());
        OVITO_ASSERT(facetEdges[1].atom2() == facetEdges[2].atom1());
        OVITO_ASSERT(facetEdges[2].atom2() == facetEdges[0].atom1());

        ClusterTransition* t1 = facetEdges[0].transition();
        ClusterTransition* t2 = facetEdges[1].transition();
        ClusterTransition* t3 = facetEdges[2].transition();
        if(!t1 || !t2 || !t3) {
            // Skip if one of the edges has no assigned cluster vector.
            return false;
        }

        // Perform Burgers circuit test on the face.
        // Calculation is performed in the frame of the first vertex' lattice cluster.
        Vector3 burgersVector = facetEdges[0].vector() + t1->reverseTransform(facetEdges[1].vector()) + t3->transform(facetEdges[2].vector());
        if(!burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON)) {
            return false;
        }

        // Perform disclination test on the face.
        if(!t1->isSelfTransition() || !t2->isSelfTransition() || !t3->isSelfTransition()) {
            Matrix3 frankRotation = t3->tm * t2->tm * t1->tm;
            if(!frankRotation.equals(Matrix3::Identity(), CA_TRANSITION_MATRIX_EPSILON)) {
                return false;
            }
        }
    }

    return true;
}

/******************************************************************************
 * Extracts the Delaunay edges with no lattice vector from the elastic mapping.
 ******************************************************************************/
void ElasticMapping::extractUnassignedEdges(TaskProgress& progress, PropertyFactory<Point3>& edgePosition1Access,
                                 PropertyFactory<Point3>& edgePosition2Access, PropertyFactory<int64_t*>& edgeAtomAccess, PropertyFactory<int>& edgeStageAccess,
                                 int stage)
{
    // Iterate over all edges of the tessellation.
    size_t count = 0;
    BufferReadAccess<Point3> positions(structureAnalysis().positions());
    _edgePool.visitAll(progress, [&](TessellationEdge* edge) {
        if(!edge->transition) {
            edgePosition1Access.push_back(positions[edge->atom1]);
            edgePosition2Access.push_back(positions[edge->atom2]);
            edgeAtomAccess.push_back(std::array<int64_t,2>{{(int64_t)edge->atom1, (int64_t)edge->atom2}});
            edgeStageAccess.push_back(stage);
            count++;
        }
    });
}

}   // End of namespace
