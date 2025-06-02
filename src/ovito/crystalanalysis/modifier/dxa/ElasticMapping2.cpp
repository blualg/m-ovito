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
    // A helper method that decides using the alpha-shape criterion which tetrahedra are filled and which are not.
    auto isInteriorTetrahedron = [&](DelaunayTessellation::CellHandle cell) -> bool {
        if(tessellation().isGhostCell(cell))
            return false; // Skip ghost cells.

        // Check if the tetrahedron is filled using the alpha-shape criterion.
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

    progress.setMaximum(tessellation().numberOfTetrahedra());
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        // Update progress indicator.
        progress.setValueIntermittent(cell);

        // Classify the tetrahedron as filled or empty using the alpha-shape criterion.
        if(isInteriorTetrahedron(cell)) {
            _filledCells[cell] = true;
        }
        else {
            _filledCells[cell] = false;
            continue; // Skip empty cells.
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

        // Get the four particles that form the vertices of the current Delaunay cell.
        std::array<AtomIndex, 4> cellAtoms;
        for(int v = 0; v < 4; v++)
            cellAtoms[v] = tessellation().inputPointIndex(cellVertices[v]);

        // Create edge data structure for each of the six edges of the cell.
        for(int edgeIndex = 0; edgeIndex < 6; edgeIndex++) {
            // Get the two atoms connected by this edge.
            AtomIndex atom1 = cellAtoms[CellEdgeVertices[edgeIndex][0]];
            AtomIndex atom2 = cellAtoms[CellEdgeVertices[edgeIndex][1]];
            if(atom1 == atom2)
                continue;
            // Check if the edge already exists in the list of edges.
            if(!findEdge(atom1, atom2)) {
                // Register a new edge.
                TessellationEdge* edge = _edgePool.construct(atom1, atom2, cell);
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
        if(cluster->structure == StructureAnalysis::LATTICE_OTHER)
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
            OVITO_ASSERT(assignedCluster->structure == StructureAnalysis::LATTICE_OTHER);

            // Check if the atom is connected to a cluster by an outbound edge.
            for(TessellationEdge* e = _atomOutboundEdges[atomIndex]; e != nullptr; e = e->nextOutboundEdge) {
                OVITO_ASSERT(e->atom1 == atomIndex);
                if(Cluster* cluster2 = clusterOfAtom(e->atom2); cluster2->structure != StructureAnalysis::LATTICE_OTHER) {
                    assignedCluster = cluster2;
                    break;
                }
            }
            if(assignedCluster->structure != StructureAnalysis::LATTICE_OTHER)
                continue;

            // Check if the atom is connected to a cluster by an inbound edge.
            for(TessellationEdge* e = _atomInboundEdges[atomIndex]; e != nullptr; e = e->nextInboundEdge) {
                OVITO_ASSERT(e->atom2 == atomIndex);
                if(Cluster* cluster1 = clusterOfAtom(e->atom1); cluster1->structure != StructureAnalysis::LATTICE_OTHER) {
                    assignedCluster = cluster1;
                    break;
                }
            }
            if(assignedCluster->structure != StructureAnalysis::LATTICE_OTHER)
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
        if(cluster1->structure == StructureAnalysis::LATTICE_OTHER || cluster2->structure == StructureAnalysis::LATTICE_OTHER)
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
            else {
                // The ideal vector could not be determined, so add this edge to the list of still unassigned edges.
                // We'll try to complement the vector later.
                _unassignedEdges.push_back(edge);
            }
        }
    });
}

/******************************************************************************
* Builds the lookup map that allows to retrieve the primary Delaunay
* cell belonging to a given triangular facet formed by three atoms.
******************************************************************************/
void ElasticMapping2::buildFacetLookupMap(TaskProgress& progress)
{
    _primaryFacetLookupMap.reserve(_unassignedEdges.size() * 6);

    progress.setMaximum(tessellation().numberOfTetrahedra());
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        if(!isFilledCell(cell))
            continue; // Skip empty cells.

        // Update progress indicator.
        progress.setValueIntermittent(cell);

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

            // Get the three edges of the facet.
            TessellationEdge* e1 = findEdge(facetVertices[0], facetVertices[1]);
            TessellationEdge* e2 = findEdge(facetVertices[1], facetVertices[2]);
            TessellationEdge* e3 = findEdge(facetVertices[2], facetVertices[0]);
            if(!e1 || !e2 || !e3)
                continue;
            if(e1->hasEdgeVector && e2->hasEdgeVector && e3->hasEdgeVector)
                continue;

            // Bring vertices into a well-defined order, which can be used as lookup key.
            reorderFacetVertices(facetVertices);

            // Add facet and its adjacent cell to the lookup map.
            _primaryFacetLookupMap.emplace(facetVertices, std::make_pair(cell, f));
        }
    }
}

/******************************************************************************
* Narrows down the bad tessellation region by complementing the lattice vectors
* of unassigned Delaunay edges.
******************************************************************************/
bool ElasticMapping2::complementEdgeVectors(TaskProgress& progress)
{
    // Counts the total number of newly assigned edges.
    size_t numAssignedEdges = 0;

    // Lookup table for traversing the incident cells around an edge.
    constexpr int tab_next_around_edge[4][4] = {
            {5, 2, 3, 1},
            {3, 5, 0, 2},
            {1, 3, 5, 0},
            {2, 0, 1, 5}};

    auto visitEdgeIncidentFacets = [&](TessellationEdge* edge, auto&& visitor) {
        OVITO_ASSERT(edge->transition);
        auto atom1 = edge->atom1;
        auto atom2 = edge->atom2;
        DelaunayTessellation::CellHandle startCell = edge->adjacentCell;
        DelaunayTessellation::CellHandle cell = startCell;
        do {
            // Find the two edge vertices in the current cell.
            int localVertex1 = tessellation().findInputPointInCell(cell, atom1);
            int localVertex2 = tessellation().findInputPointInCell(cell, atom2);
            OVITO_ASSERT(localVertex1 >= 0 && localVertex1 < 4 && localVertex2 >= 0 && localVertex2 < 4);
            // The current facet we are at.
            int facet;
            std::tie(cell, facet) = tessellation().mirrorFacet(cell, tab_next_around_edge[localVertex1][localVertex2]);

            // If the current cell is a ghost cell, switch over to its primary image.
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

            // Call the visitor function for the current facet.
            visitor(cell, facet);
        }
        while(cell != startCell);
    };

    // Maximum number of unique Burgers vector to consider when
    // analyzing the elementary dislocation loop around a Delaunay edge.
    static constexpr int MaxUniqueBurgersVectorsPerEdge = 8;

    // A utilities class that maintains a list of unique edge vectors and their counts.
    // This is used to find the most frequent edge vector among the incident tetrahedra of an edge.
    class UniqueVectorsList {
    public:
        void add(const Cluster::VecType& item) {
            for(int i = 0; i < numItems; i++) {
                if(items[i].equals(item, CA_LATTICE_VECTOR_EPSILON)) {
                    itemCounts[i]++;
                    return; // Item already exists, increment its count.
                }
            }
            if(numItems < MaxUniqueBurgersVectorsPerEdge) {
                itemCounts[numItems] = 1; // New item, initialize its count.
                items[numItems++] = item;
            }
        }
        bool empty() const {
            return numItems == 0;
        }
        const Cluster::VecType& mostFrequent() const {
            OVITO_ASSERT(!empty());
            int maxCount = 0;
            int maxIndex = 0;
            for(int i = 0; i < numItems; i++) {
                if(itemCounts[i] > maxCount) {
                    maxCount = itemCounts[i];
                    maxIndex = i;
                }
            }
            return items[maxIndex];
        }
    private:
        Cluster::VecType items[MaxUniqueBurgersVectorsPerEdge];
        int itemCounts[MaxUniqueBurgersVectorsPerEdge]; //< Counts how many times each item was added.
        int numItems = 0;
    };

    // Helper method that checks if a vertex is fully surrounded by unassigned edges.
    auto isVertexFullyIsolated = [&](AtomIndex atom) -> bool {
        for(TessellationEdge* e = _atomOutboundEdges[atom]; e != nullptr; e = e->nextOutboundEdge)
            if(e->hasEdgeVector) return false;
        for(TessellationEdge* e = _atomInboundEdges[atom]; e != nullptr; e = e->nextInboundEdge)
            if(e->hasEdgeVector) return false;
        return true;
    };

    // Queue of newly assigned edges to be processed.
    std::deque<TessellationEdge*> queue;

    // Visit all edges of the tessellation that are not yet assigned a lattice vector.
    size_t numProcessedEdges = 0;
    for(TessellationEdge* edge : _unassignedEdges) {
        OVITO_ASSERT(edge->transition);
        if(edge->hasEdgeVector)
            continue; // Skip edge if it already has a lattice vector assigned or is blocked.
        numProcessedEdges++;

        this_task::throwIfCanceled();

        OVITO_ASSERT(queue.empty());
        queue.push_back(edge);
        do {
            // Handle the next edge from the processing queue.
            TessellationEdge* edge = queue.front();
            queue.pop_front();

            // Check if the same edge has already been processed in an earlier iteration.
            // The queue may contain the same edge multiple times if it was added by different incident facets.
            if(edge->hasEdgeVector)
                continue;

            // Circulate around the edge in the tessellation and collect edge vector candidates from incident facets
            // such that the sum of the edge vectors in the facet becomes zero.
            // Implementing a voting scheme to find the most frequent edge vector among the candidates,
            // in order to find a choice that eliminates the most number of segments from the dislocation loop around the current edge.
            UniqueVectorsList edgeVectorCandidates;
            OrientedEdge secondaryCandidate;
            visitEdgeIncidentFacets(edge, [&](DelaunayTessellation::CellHandle cell, int facet) {
                // Get the three oriented edges of the current facet.
                std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cell, facet);

                // Maintain correct order of edges in the facet in order to circulate around the same edge.
                while(facetEdges[0].atom2() != edge->atom1) {
                    std::rotate(facetEdges.begin(), facetEdges.begin() + 1, facetEdges.end());
                }
                OVITO_ASSERT(facetEdges[0].atom1() == edge->atom2 && facetEdges[0].atom2() == edge->atom1);
                OVITO_ASSERT(!facetEdges[0].hasEdgeVector() && !facetEdges[0].isBlocked());

                // Infer edge vector from sum of the other two edges to form a facet with a zero Burgers vector.
                if(facetEdges[1].hasEdgeVector() && facetEdges[2].hasEdgeVector())
                    edgeVectorCandidates.add(-facetEdges[0].transition()->reverseTransform(facetEdges[1].vector()) - facetEdges[2].transition()->transform(facetEdges[2].vector()));
                else if(facetEdges[1].hasEdgeVector()) {
                    secondaryCandidate = facetEdges[1];
                    if(facetEdges[2].undirectedEdge()->transition)
                       queue.push_back(facetEdges[2].undirectedEdge());
                }
                else if(facetEdges[2].hasEdgeVector()) {
                    secondaryCandidate = -facetEdges[2];
                    if(facetEdges[1].undirectedEdge()->transition)
                        queue.push_back(facetEdges[1].undirectedEdge());
                }
            });

            if(!edgeVectorCandidates.empty()) {
                // Assign the most frequent vector to the edge from the candidates found in the incident cells.
                edge->vector = -edge->transition->reverseTransform(edgeVectorCandidates.mostFrequent());
            }
            else if(secondaryCandidate) {
                OVITO_ASSERT(secondaryCandidate.atom1() == edge->atom1 || secondaryCandidate.atom1() == edge->atom2);
                // If no edge vector could be inferred from two incident edges,
                // consider choosing the edge vector on the basis of just one incident edge with a known edge vector.
                // Check if the other vertex is fully isolated, i.e., has no incident edges with a valid edge vector.
                if(isVertexFullyIsolated(secondaryCandidate.atom1() == edge->atom1 ? edge->atom2 : edge->atom1)) {
                    if(secondaryCandidate.atom1() == edge->atom1)
                        edge->vector = secondaryCandidate.vector();
                    else
                        edge->vector = edge->transition->reverseTransform(secondaryCandidate.vector());
                }
                else {
                    queue.clear();
                    continue; // Leave edge unassigned if the opposite vertex is not isolated.
                }
            }
            else {
                queue.clear();
                break; // No incident cells with valid edge vectors found, skip this edge.
            }
            edge->hasEdgeVector = true;
            numAssignedEdges++;
        }
        while(!queue.empty());
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
        Cluster::VecType burgersVector = facetEdges[0].vector() + t0->reverseTransform(facetEdges[1].vector()) + t2->transform(facetEdges[2].vector());
        if(!burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON)) {
            return false;
        }

        // Perform disclination test on the face.
        if(!t0->isSelfTransition() || !t1->isSelfTransition() || !t2->isSelfTransition()) {
            Cluster::MatType frankRotation = t2->tm * t1->tm * t0->tm;
            if(!frankRotation.equals(Cluster::MatType::Identity(), CA_TRANSITION_MATRIX_EPSILON)) {
                return false;
            }
        }
    }

    return true;
}

/******************************************************************************
* Extracts the dislocation lines segments.
* Invokes the callback function for each segment found.
******************************************************************************/
void ElasticMapping2::extractDislocationSegments(TaskProgress& progress, const std::function<void(DelaunayTessellation::CellHandle, int, Cluster*, const Vector3F&)>& callback) const
{
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        // Skip empty cells.
        if(!isFilledCell(cell))
            continue;

        this_task::throwIfCanceled();

        // Get the six oriented edges of the Delaunay cell.
        const std::array<ElasticMapping2::OrientedEdge, 6> cellEdges = getOrientedEdges(cell);

        // Perform Burgers circuit test on each of the four facets of the tetrahedron.
        for(int facet = 0; facet < 4; facet++) {

            // Get the three oriented edges of the current facet.
            std::array<OrientedEdge, 3> facetEdges = getFacetCircuitEdges(cell, facet);
            if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
                continue; // Skip if one of the edges is missing.
            if(facetEdges[0].isBlocked() || facetEdges[1].isBlocked() || facetEdges[2].isBlocked())
                continue; // Skip if one of the edges is blocked.

            // Perform Burgers circuit test on the face.
            Cluster::VecType burgersVector = Vector3F::Zero();
            Cluster* cluster = clusterOfAtom(facetEdges[0].atom1());
            if(facetEdges[0].hasEdgeVector())
                burgersVector += facetEdges[0].vector();
            if(facetEdges[1].hasEdgeVector()) {
                if(facetEdges[0].hasEdgeVector())
                    burgersVector += facetEdges[0].transition()->reverseTransform(facetEdges[1].vector());
                else if(facetEdges[2].hasEdgeVector())
                    burgersVector += facetEdges[2].transition()->transform(facetEdges[1].transition()->transform(facetEdges[1].vector()));
                else
                    burgersVector = facetEdges[1].vector();
            }
            if(facetEdges[2].hasEdgeVector())
                burgersVector += facetEdges[2].transition()->transform(facetEdges[2].vector());
            if(burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON))
                continue; // Burgers circuit test passed.

            // Perform disclination test on the face.
            if(facetEdges[0].hasEdgeVector() && facetEdges[1].hasEdgeVector() && facetEdges[2].hasEdgeVector()) {
                ClusterTransition* t1 = facetEdges[0].transition();
                ClusterTransition* t2 = facetEdges[1].transition();
                ClusterTransition* t3 = facetEdges[2].transition();
                if(!t1->isSelfTransition() || !t2->isSelfTransition() || !t3->isSelfTransition()) {
                    Cluster::MatType frankRotation = t3->tm * t2->tm * t1->tm;
                    if(!frankRotation.equals(Matrix3F::Identity(), CA_TRANSITION_MATRIX_EPSILON))
                        continue; // Disclination test failed.
                }
            }

            // Invoke the callback function with the found segment.
            callback(cell, facet, cluster, burgersVector);
        }
    }
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
    for(TessellationEdge* edge : _unassignedEdges) {
        if(edge->transition && !edge->hasEdgeVector) {
            edgePosition1Access.push_back(positions[edge->atom1]);
            edgePosition2Access.push_back(positions[edge->atom2]);
            edgeAtomAccess.push_back(std::array<int64_t,2>{{(int64_t)edge->atom1, (int64_t)edge->atom2}});
            edgeStageAccess.push_back(stage);
        }
    }
}

}   // End of namespace
