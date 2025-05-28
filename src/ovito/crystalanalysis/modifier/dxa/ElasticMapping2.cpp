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
#include "ElasticMapping2.h"
#include "CrystalPathFinder.h"

namespace Ovito {

/******************************************************************************
* Uses the alpha shape criterion to classify tetrahedra as filled or not.
* In addition, creates a lookup map that allows to retrieve the primary Delaunay
* cell belonging to a given triangular facet formed by three atoms.
******************************************************************************/
void ElasticMapping2::classifyTetrahedra(FloatType alpha, TaskProgress& progress)
{
    // A helper method that decides which tetrahedra are considered filled and which are not.
    auto isInteriorTetrahedron = [&](DelaunayTessellation::CellHandle cell) -> bool {
        if(tessellation().isGhostCell(cell))
            return false; // Skip ghost cells.
        if(auto alphaTestResult = tessellation().alphaTest(cell, alpha)) {
            return *alphaTestResult;
        }
        else {
            // If the alpha test is inconclusive (which may happen if the element is a sliver tetrahedron),
            // then we check the surrounding tetrahedra. Only if all four neighbors are classified as filled or inconclusive,
            // then we accept the sliver tetrahedron as filled too.
            for(int f = 0; f < 4; f++) {
                DelaunayTessellation::CellHandle adjacentCell = tessellation().mirrorFacet(cell, f).first;
                if(!tessellation().isFiniteCell(adjacentCell))
                    return false;
                auto adjacentAlphaTestResult = tessellation().alphaTest(adjacentCell, alpha);
                if(adjacentAlphaTestResult.has_value() && !adjacentAlphaTestResult.value())
                    return false;
            }
            return true;
        }
    };

    _filledCells.resize(tessellation().numberOfTetrahedra());
    _primaryFacetLookupMap.reserve(tessellation().numberOfPrimaryTetrahedra() * 4);

    progress.setMaximum(tessellation().numberOfTetrahedra());
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        // Update progress indicator.
        progress.setValueIntermittent(cell);

        // Classify tetrahedron as filled or not.
        if(isInteriorTetrahedron(cell)) {
            _filledCells[cell] = true;
        }
        else {
            _filledCells[cell] = false;
            continue; // Skip empty cells.
        }

        // Get the four Delaunay vertices of the current cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);

        // Get the four particles that form the vertices of the current Delaunay cell.
        std::array<AtomIndex, 4> cellAtoms;
        for(int v = 0; v < 4; v++)
            cellAtoms[v] = tessellation().inputPointIndex(cellVertices[v]);

        // Loop over the 4 facets of the cell.
        for(int f = 0; f < 4; f++) {
            // Get the 3 vertices of the facet.
            std::array<AtomIndex, 3> facetVertices = {
                cellAtoms[DelaunayTessellation::cellFacetVertexIndex(f, 0)],
                cellAtoms[DelaunayTessellation::cellFacetVertexIndex(f, 1)],
                cellAtoms[DelaunayTessellation::cellFacetVertexIndex(f, 2)]
            };

            // Bring vertices into a well-defined order, which can be used as lookup key.
            reorderFacetVertices(facetVertices);

            // Add facet and its adjacent cell to the lookup map.
            _primaryFacetLookupMap.emplace(facetVertices, std::make_pair(cell, f));
        }
    }
}

/******************************************************************************
* Builds the list of edges in the tetrahedral tessellation.
******************************************************************************/
void ElasticMapping2::generateTessellationEdges(TaskProgress& progress)
{
    progress.setMaximum(tessellation().numberOfPrimaryTetrahedra());

    // Generate list of tessellation edges.
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {

        // Skip invalid cells (those not connecting four physical atoms), ghost cells, and empty cells.
        if(!isFilledCell(cell))
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
                // Insert into the linked list of edges outbound on vertex 1 and inbound on vertex 2.
                edge->nextOutboundEdge = std::exchange(_atomOutboundEdges[atom1], edge);
                edge->nextInboundEdge = std::exchange(_atomInboundEdges[atom2], edge);
            }
        }
    }
}

/******************************************************************************
* Assigns each tessellation vertex to a cluster.
******************************************************************************/
void ElasticMapping2::assignAtomsToClusters(TaskProgress& progress)
{
    // The total length of this task is unknown.
    progress.setMaximum(0);

    // Assign each atom to some crystal cluster, which will be used for expressing
    // reference vectors assigned to the edges incident on that atom.

    // List of atoms that have not been assigned to a cluster yet.
    const AtomIndex atomCount = structureAnalysis().atomCount();
    std::vector<AtomIndex> unassignedAtoms;
    unassignedAtoms.reserve(atomCount);

    // First, apply most obvious choice:
    // If an atom has been assigned to a cluster by the structural analysis,
    // then we simply adopt this assignment.
    for(AtomIndex atomIndex = 0; atomIndex < atomCount; atomIndex++) {
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
        AtomIndex remainingAtomCount = 0;
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
* Guesses a lattice vector for each edge of the tessellation from the
* results of the atomistic crystal structure analysis.
******************************************************************************/
void ElasticMapping2::assignIdealVectorsToEdges(int crystalPathSteps, TaskProgress& progress)
{
    // Create a path finder, which can determine the ideal lattice vector
    // connecting a given pair of atoms (which don't have to be nearest neighbors).
    CrystalPathFinder pathFinder(_structureAnalysis, crystalPathSteps);

    // Iterate over all edges of the tessellation.
    _edgePool.visitAll(progress, [&](TessellationEdge* edge) {
        OVITO_ASSERT(!edge->transition);
        this_task::throwIfCanceled();

        Cluster* cluster1 = clusterOfAtom(edge->atom1);
        Cluster* cluster2 = clusterOfAtom(edge->atom2);
        OVITO_ASSERT(cluster1 && cluster2);
        if(cluster1->id == 0 || cluster2->id == 0)
            return; // One of the atoms is not part of any crystal cluster.

        // Assign a cluster transition to the edge.
        // The two Delaunay vertices may be part of two disconnected components of the cluster graph,
        // in which case the transition is not defined.
        edge->transition = clusterGraph()->determineClusterTransition(cluster1, cluster2);
        if(edge->transition) {
            // Determine the ideal vector connecting the two atoms.
            std::optional<ClusterVector> idealVector = pathFinder.findPath(edge->atom1, edge->atom2);

            // Translate vector to the frame of the atom cluster.
            if(idealVector.has_value() && idealVector->transformToCluster(cluster1, *clusterGraph())) {
                // Assign cluster vector to the edge.
                edge->vector = idealVector->localVec();
                edge->hasEdgeVector = true;
            }
        }
    });
}

/******************************************************************************
* Narrows down the bad tessellation region by complementing the lattice vectors
* of unassigned Delaunay edges.
******************************************************************************/
bool ElasticMapping2::complementEdgeVectors()
{
    std::deque<std::tuple<DelaunayTessellation::CellHandle, int, int>> queue;
    size_t numAssignedEdges = 0;

    auto processNextEdgeFromQueue = [&]() {
        const DelaunayTessellation::CellHandle startCell = std::get<0>(queue.front());
        const auto atom1 = std::get<1>(queue.front());
        const auto atom2 = std::get<2>(queue.front());
        queue.pop_front();
        constexpr int tab_next_around_edge[4][4] = {
                {5, 2, 3, 1},
                {3, 5, 0, 2},
                {1, 3, 5, 0},
                {2, 0, 1, 5}};
        DelaunayTessellation::CellHandle cell = startCell;
        do {
            // Find original edge vertices in current cell.
            int localVertex1 = tessellation().findInputPointInCell(cell, atom1);
            int localVertex2 = tessellation().findInputPointInCell(cell, atom2);
            OVITO_ASSERT(localVertex1 >= 0 && localVertex1 < 4);
            OVITO_ASSERT(localVertex2 >= 0 && localVertex2 < 4);
            // The current facet we are at.
            int facet;
            std::tie(cell, facet) = tessellation().mirrorFacet(cell, tab_next_around_edge[localVertex1][localVertex2]);

            // Map the current cell to its primary image.
            if(tessellation().isGhostCell(cell)) {

                // Get the 3 vertices of the facet.
                std::array<AtomIndex, 3> vertices;
                for(int i = 0; i < 3; i++)
                    vertices[i] = static_cast<AtomIndex>(tessellation().inputPointIndex(tessellation().cellVertex(cell, DelaunayTessellation::cellFacetVertexIndex(facet, i))));

                // Bring vertices into a well-defined order, which can be used as lookup key.
                reorderFacetVertices(vertices);

                // Perform lookup.
                auto iter = _primaryFacetLookupMap.find(vertices);
                if(iter != _primaryFacetLookupMap.end()) {
                    // Found the primary image of the facet.
                    std::tie(cell, facet) = iter->second;
                }
                else {
                    // The primary image is not defined, so we cannot process this cell.
                    continue;
                }
            }

            // Skip exterior cells.
            if(!isFilledCell(cell))
                continue;

            // Get the three oriented edges of the current facet.
            std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cell, facet);
            if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
                continue; // Skip if one of the edges is missing.

            // Maintain correct order of edges in the facet in order to circulate around the same edge.
            while(facetEdges[2].atom2() != atom1) {
                std::rotate(facetEdges.begin(), facetEdges.begin() + 1, facetEdges.end());
            }
            OVITO_ASSERT(facetEdges[2].hasEdgeVector());

            if(!facetEdges[0].hasEdgeVector() && !facetEdges[0].isBlocked() && facetEdges[1].hasEdgeVector()) {
                // Infer edge vector from sum of the other two edges to form a facet with a zero Burgers vector.
                facetEdges[0].setEdgeVector(-facetEdges[0].transition()->reverseTransform(facetEdges[1].vector()) - facetEdges[2].transition()->transform(facetEdges[2].vector()));
                queue.push_back({cell, facetEdges[0].atom1(), facetEdges[0].atom2()});
                numAssignedEdges++;
            }
        }
        while(cell != startCell);
        this_task::throwIfCanceled();
    };

    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        // Skip invalid cells (those not connecting four physical atoms), ghost cells, and empty cells.
        if(!isFilledCell(cell))
            continue;

        // Get the six oriented edges of the Delaunay cell.
        std::array<OrientedEdge, 6> cellEdges = getOrientedEdges(cell);

        // Skip this cell if all six edges are good.
        if(std::ranges::all_of(cellEdges, [](const OrientedEdge& edge) { return edge && edge.hasEdgeVector(); }))
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
                if(!facetEdges[0].hasEdgeVector() && !facetEdges[0].isBlocked() && facetEdges[1].hasEdgeVector()) {
                    // Only process this edge of the current triangle facet if:
                    //   1. the two other edges have valid vectors OR
                    //   2. it ends at a Delaunay vertex who's adjacent edges are ALL without a valid vector.
                    if(facetEdges[2].hasEdgeVector()) {
                        // Infer edge vector from sum of the other two edges to form a facet with a zero Burgers vector.
                        facetEdges[0].setEdgeVector(-facetEdges[0].transition()->reverseTransform(facetEdges[1].vector()) - facetEdges[2].transition()->transform(facetEdges[2].vector()));
                    }
                    else {
                        bool skipEdge = false;
                        for(TessellationEdge* e = _atomOutboundEdges[facetEdges[0].atom1()]; e != nullptr && !skipEdge; e = e->nextOutboundEdge)
                            skipEdge |= e->hasEdgeVector;
                        for(TessellationEdge* e = _atomInboundEdges[facetEdges[0].atom1()]; e != nullptr && !skipEdge; e = e->nextInboundEdge)
                            skipEdge |= e->hasEdgeVector;
                        if(!skipEdge) {
                            // If the third edge doesn't have an edge vector yet, arbitrarily assume it is zero.
                            facetEdges[0].setEdgeVector(-facetEdges[0].transition()->reverseTransform(facetEdges[1].vector()));
                        }
                    }

                    if(facetEdges[0].hasEdgeVector()) {
                        // Push the edge into the queue for further processing.
                        queue.push_back({cell, facetEdges[0].atom1(), facetEdges[0].atom2()});
                        numAssignedEdges++;
                        do {
                            processNextEdgeFromQueue();
                        }
                        while(!queue.empty());
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
* Determines whether the tetrahedron formed by the six edges is dislocation-free, i.e. whether
* their assigned lattice vectors are all compatible.
******************************************************************************/
bool ElasticMapping2::isElasticMappingCompatible(const std::array<OrientedEdge, 6>& cellEdges) const
{
    // Perform the Burgers circuit test on each of the four facets of the tetrahedron.
    for(int facet = 0; facet < 4; facet++) {
        std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cellEdges, facet);
        if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
            return false; // Skip if one of the edges is missing.

        OVITO_ASSERT(facetEdges[0].atom2() == facetEdges[1].atom1());
        OVITO_ASSERT(facetEdges[1].atom2() == facetEdges[2].atom1());
        OVITO_ASSERT(facetEdges[2].atom2() == facetEdges[0].atom1());

        ClusterTransition* t0 = facetEdges[0].transition();
        ClusterTransition* t1 = facetEdges[1].transition();
        ClusterTransition* t2 = facetEdges[2].transition();
        if(!t0 || !t1 || !t2) {
            // Skip if one of the edges has no assigned cluster transition.
            return false;
        }

        // Perform Burgers circuit test on the face.
        // Calculation is performed in the frame of the first vertex' lattice cluster.
        Vector3 burgersVector = facetEdges[0].vector() + t0->reverseTransform(facetEdges[1].vector()) + t2->transform(facetEdges[2].vector());
        if(!burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON)) {
            return false;
        }

        // Perform disclination test on the face.
        if(!t0->isSelfTransition() || !t1->isSelfTransition() || !t2->isSelfTransition()) {
            Matrix3 frankRotation = t2->tm * t1->tm * t0->tm;
            if(!frankRotation.equals(Matrix3::Identity(), CA_TRANSITION_MATRIX_EPSILON)) {
                return false;
            }
        }
    }

    return true;
}

/******************************************************************************
* Extracts the Delaunay edges with no lattice vector from the elastic mapping.
* Note: This method is only used for debugging purposes.
******************************************************************************/
void ElasticMapping2::extractUnassignedEdges(TaskProgress& progress, PropertyFactory<Point3>& edgePosition1Access,
                                 PropertyFactory<Point3>& edgePosition2Access, PropertyFactory<int64_t*>& edgeAtomAccess, PropertyFactory<int>& edgeStageAccess,
                                 int stage)
{
    // Iterate over all edges of the tessellation.
    BufferReadAccess<Point3> positions(structureAnalysis().positions());
    _edgePool.visitAll(progress, [&](TessellationEdge* edge) {
        if(edge->transition && !edge->hasEdgeVector) {
            edgePosition1Access.push_back(positions[edge->atom1]);
            edgePosition2Access.push_back(positions[edge->atom2]);
            edgeAtomAccess.push_back(std::array<int64_t,2>{{(int64_t)edge->atom1, (int64_t)edge->atom2}});
            edgeStageAccess.push_back(stage);
        }
    });
}

}   // End of namespace
