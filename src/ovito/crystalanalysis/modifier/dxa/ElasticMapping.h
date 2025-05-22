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

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/Cluster.h>
#include <ovito/crystalanalysis/objects/ClusterGraph.h>
#include <ovito/crystalanalysis/objects/EdgeVector.h>
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/core/utilities/MemoryPool.h>
#include "StructureAnalysis.h"

namespace Ovito {

/**
 * Computes the elastic mapping from the physical configuration to a stress-free reference state.
 */
class ElasticMapping
{
private:

    /// Data structure associated with an edge of the tessellation.
    struct TessellationEdge {

        /// Constructor.
        TessellationEdge(size_t a1, size_t a2, const Vector3& delta) : atom1(a1), atom2(a2), physicalVector(delta) {}

        /// The index of the atom this edge is originating from.
        size_t atom1;

        /// The index of the atom this edge is going to.
        size_t atom2;

        /// The vector corresponding to this edge in the stress-free reference configuration.
        EdgeVector vector;

        /// The physical vector connecting the two atoms in the current configuration.
        Vector3 physicalVector;

        /// The next edge in the linked list of edges leaving atom 1.
        TessellationEdge* nextOutboundEdge;

        /// The next edge in the linked list of edges arriving at atom 2.
        TessellationEdge* nextInboundEdge;
    };

public:

    /// Pairs of cell vertices that form the six edges of a tetrahedron.
    static constexpr int CellEdgeVertices[6][2] = {{0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}};

    /// Triplets of edges that form the Burgers circuits for each face of a tetrahedron.
    static constexpr int CellEdgeCircuits[4][3] = {{3,5,4}, {1,5,2}, {0,4,2}, {0,3,1}};

    /// Constructor.
    ElasticMapping(StructureAnalysis& structureAnalysis, DelaunayTessellation& tessellation) :
        _structureAnalysis(structureAnalysis),
        _tessellation(tessellation),
        _clusterGraph(structureAnalysis.clusterGraph()),
        _atomOutboundEdges(structureAnalysis.atomCount(), nullptr),
        _atomInboundEdges(structureAnalysis.atomCount(), nullptr),
        _atomClusters(structureAnalysis.atomCount(), nullptr)
    {}

    /// Returns the structure analysis object.
    const StructureAnalysis& structureAnalysis() const { return _structureAnalysis; }

    /// Returns the underlying tessellation.
    DelaunayTessellation& tessellation() { return _tessellation; }

    /// Returns the underlying tessellation.
    const DelaunayTessellation& tessellation() const { return _tessellation; }

    /// Returns the cluster graph.
    const DataOORef<ClusterGraph>& clusterGraph() const { return _clusterGraph; }

    /// Builds an explicit list of all Delaunay edges.
    void generateTessellationEdges(TaskProgress& progress);

    /// Assigns each atom to a crystal cluster.
    void assignAtomsToClusters(TaskProgress& progress);

    /// Determines the ideal vector corresponding to each edge of the tessellation.
    void assignIdealVectorsToEdges(int crystalPathSteps, TaskProgress& progress);

    /// Guesses ideal vectors for those edges of the Delaunay tessellation,
    /// which are not yet assigned a vector.
    void complementUnassignedEdges(TaskProgress& progress);

    /// Determines whether the elastic mapping from the physical configuration
    /// of the crystal to the imaginary, stress-free configuration is compatible
    /// within the given Delaunay cell. Returns false if the mapping is incompatible
    /// or cannot be determined.
    bool isElasticMappingCompatible(DelaunayTessellation::CellHandle cell) const;

    /// Returns the cluster to which an atom has been assigned (may be nullptr).
    Cluster* clusterOfAtom(size_t atomIndex) const {
        OVITO_ASSERT(atomIndex < _atomClusters.size());
        return _atomClusters[atomIndex];
    }

    /// Returns the lattice vector assigned to a Delaunay edge.
    EdgeVector getEdgeClusterVector(size_t atomIndex1, size_t atomIndex2) const {
        TessellationEdge* edge = findEdge(atomIndex1, atomIndex2);
        if(!edge || !edge->vector.isValid())
            return { Vector3::Zero(), nullptr };
        return (edge->atom1 == atomIndex1) ? edge->vector : -edge->vector;
    }

    /// Returns the lattice vector assigned to an edge of a Delaunay cell.
    EdgeVector getEdgeClusterVector(const std::array<DelaunayTessellation::VertexHandle, 4>& cellVertices, int localEdgeIndex) const {
        size_t atom1 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[localEdgeIndex][0]]);
        size_t atom2 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[localEdgeIndex][1]]);
        return getEdgeClusterVector(atom1, atom2);
    }

private:

    /// Looks up the tessellation edge connecting two atoms.
    /// Returns nullptr if the two atoms are not connected by a Delaunay edge.
    TessellationEdge* findEdge(size_t atomIndex1, size_t atomIndex2) const {
        OVITO_ASSERT(atomIndex1 < _atomOutboundEdges.size());
        OVITO_ASSERT(atomIndex2 < _atomOutboundEdges.size());
        for(TessellationEdge* e = _atomOutboundEdges[atomIndex1]; e != nullptr; e = e->nextOutboundEdge)
            if(e->atom2 == atomIndex2) return e;
        for(TessellationEdge* e = _atomInboundEdges[atomIndex1]; e != nullptr; e = e->nextInboundEdge)
            if(e->atom1 == atomIndex2) return e;
        return nullptr;
    }

private:

    /// The structure analysis object.
    StructureAnalysis& _structureAnalysis;

    /// The underlying tessellation of the atomistic system.
    DelaunayTessellation& _tessellation;

    /// The cluster graph.
    DataOORef<ClusterGraph> _clusterGraph;

    /// Stores the head of the linked lists of outbound edges of each atom.
    std::vector<TessellationEdge*> _atomOutboundEdges;

    /// Stores the head of the linked lists of inbound edges of each atom.
    std::vector<TessellationEdge*> _atomInboundEdges;

    /// Memory pool for the creation of TessellationEdge structure instances.
    MemoryPool<TessellationEdge> _edgePool;

    /// Stores the cluster assigned to each atom.
    std::vector<Cluster*> _atomClusters;
};

}   // End of namespace
