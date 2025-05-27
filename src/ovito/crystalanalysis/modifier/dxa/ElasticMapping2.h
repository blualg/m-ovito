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
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/core/utilities/MemoryPool.h>
#include "StructureAnalysis.h"

namespace Ovito {

/**
 * Computes the elastic mapping from the physical configuration to a stress-free reference state.
 */
class ElasticMapping2
{
private:

    /// Data type used for atom indices.
    using AtomIndex = int;

    /// Data structure associated with an edge of the tessellation.
    struct TessellationEdge {

        /// Constructor.
        TessellationEdge(AtomIndex atom1, AtomIndex atom2) : atom1(atom1), atom2(atom2) {}

        /// The index of the atom this edge is originating from.
        AtomIndex atom1;

        /// The index of the atom this edge is going to.
        AtomIndex atom2;

        /// The vector corresponding to this edge in the stress-free reference configuration.
        Vector3 vector = Vector3::Zero();

        /// The crystal cluster transition when going from atom1 to atom2.
        ClusterTransition* transition = nullptr;

        /// Indicates that this edge has been assigned a valid cluster vector.
        bool hasClusterVector = false;

        /// The next edge in the linked list of edges leaving atom 1.
        TessellationEdge* nextOutboundEdge;

        /// The next edge in the linked list of edges arriving at atom 2.
        TessellationEdge* nextInboundEdge;
    };

public:

    class OrientedEdge
    {
    public:
        /// Default constructor.
        OrientedEdge() = default;

        /// Constructor.
        OrientedEdge(TessellationEdge* edge, bool isFlipped) : _edge(edge), _isFlipped(isFlipped) {}

        /// Returns the atom index of the first atom connected by this edge.
        AtomIndex atom1() const { OVITO_ASSERT(_edge != nullptr); return _isFlipped ? _edge->atom2 : _edge->atom1; }

        /// Returns the atom index of the second atom connected by this edge.
        AtomIndex atom2() const { OVITO_ASSERT(_edge != nullptr); return _isFlipped ? _edge->atom1 : _edge->atom2; }

        /// Returns the cluster vector assigned to this edge, possibly flipped.
        Vector3 vector() const {
            OVITO_ASSERT(_edge != nullptr);
            return _isFlipped ? -_edge->vector : _edge->vector;
        }

        /// Returns the cluster transition associated with this edge, possibly flipped.
        ClusterTransition* transition() const {
            OVITO_ASSERT(_edge != nullptr);
            ClusterTransition* t = _edge->transition;
            return (_isFlipped && t) ? t->reverse : t;
        }

        /// Returns true if this edge has an assigned cluster vector.
        bool hasEdgeVector() const {
            OVITO_ASSERT(_edge != nullptr);
            OVITO_ASSERT(!_edge->hasClusterVector || _edge->transition != nullptr);
            return _edge->hasClusterVector;
        }

        /// Sets the edge vector assigned to this edge, possibly flipped.
        void setEdgeVector(const Vector3& v) {
            OVITO_ASSERT(_edge != nullptr);
            OVITO_ASSERT(!hasEdgeVector());
            OVITO_ASSERT(!isBlocked());
            if(!_isFlipped) {
                _edge->vector = v;
            }
            else {
                _edge->vector = transition()->transform(-v);
            }
            _edge->hasClusterVector = true;
        }

        /// Sets the edge vector assigned to this edge, possibly flipped.
        void setEdgeVectorLegacy(const std::pair<Vector3, ClusterTransition*>& edgeVectorAndTransition) {
            _edge->transition = edgeVectorAndTransition.second;
            setEdgeVector(edgeVectorAndTransition.first);
        }

        /// Returns true if this edge is valid, i.e. if it is associated with a tessellation edge.
        operator bool() const { return _edge != nullptr; }

        /// Returns the underlying tessellation edge.
        TessellationEdge* undirectedEdge() const { return _edge; }

        /// Returns true if this edge is blocked because it does not connect two related crystal clusters.
        bool isBlocked() const {
            OVITO_ASSERT(_edge != nullptr);
            return _edge->transition == nullptr;
        }

        /// Returns the inverse edge.
        OrientedEdge operator-() const {
            return OrientedEdge(_edge, !_isFlipped);
        }

        /// Computes the sum of this edge vector and another edge vector.
        std::pair<Vector3, ClusterTransition*> concatenate(const OrientedEdge& other, ClusterGraph* clusterGraph) const {
            OVITO_ASSERT(hasEdgeVector() && other.hasEdgeVector());
            return { vector() + transition()->reverseTransform(other.vector()), clusterGraph->concatenateClusterTransitions(transition(), other.transition()) };
        }

        /// Checks if this edge connects two given atoms.
        bool connectsAtoms(AtomIndex atomIndex1, AtomIndex atomIndex2) const {
            return (_edge != nullptr) && ((atom1() == atomIndex1 && atom2() == atomIndex2) ||
                                          (atom1() == atomIndex2 && atom2() == atomIndex1));
        }

    private:
        TessellationEdge* _edge = nullptr; ///< Pointer to the underlying undirected tessellation edge.
        bool _isFlipped = false; // Indicates whether the edge is oriented from atom1 to atom2 (false) or the other way around (true).
    };

    /// Pairs of cell vertices that form the six edges of a tetrahedron.
    static constexpr int CellEdgeVertices[6][2] = {{0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}};

    /// Constructor.
    ElasticMapping2(StructureAnalysis& structureAnalysis, DelaunayTessellation& tessellation) :
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

    /// Uses the alpha shape criterion to classify tetrahedra as filled or not.
    /// In addition, creates a lookup map that allows to retrieve the primary Delaunay
    /// cell belonging to a given triangular facet formed by three atoms.
    void classifyTetrahedra(FloatType alpha, TaskProgress& progress);

    /// Builds an explicit list of all Delaunay edges.
    void generateTessellationEdges(TaskProgress& progress);

    /// Assigns each atom to a crystal cluster.
    void assignAtomsToClusters(TaskProgress& progress);

    /// Determines the ideal vector corresponding to each edge of the tessellation.
    void assignIdealVectorsToEdges(int crystalPathSteps, TaskProgress& progress);

    /// Narrows down the bad tessellation region by complementing the lattice vectors of unassigned Delaunay edges.
    bool complementEdgeVectors();

    /// Returns the cluster to which an atom has been assigned (may be nullptr).
    Cluster* clusterOfAtom(AtomIndex atomIndex) const {
        OVITO_ASSERT(atomIndex < _atomClusters.size());
        return _atomClusters[atomIndex];
    }

    /// Looks up the edge connecting two atoms and returns it as an oriented edge.
    /// If the edge is not found, returns an invalid OrientedEdge.
    OrientedEdge getOrientedEdge(AtomIndex atomIndex1, AtomIndex atomIndex2) const {
        TessellationEdge* edge = findEdge(atomIndex1, atomIndex2);
        if(!edge)
            return OrientedEdge(nullptr, false);
        return OrientedEdge(edge, edge->atom1 != atomIndex1);
    }

    /// Looks up one of the six local edges of the given Delaunay cell and returns it as an oriented edge.
    OrientedEdge getOrientedEdge(const std::array<DelaunayTessellation::VertexHandle, 4>& cellVertices, int localEdgeIndex) const {
        AtomIndex atom1 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[localEdgeIndex][0]]);
        AtomIndex atom2 = tessellation().inputPointIndex(cellVertices[CellEdgeVertices[localEdgeIndex][1]]);
        return getOrientedEdge(atom1, atom2);
    }

    /// Returns the six oriented edges of a Delaunay cell.
    std::array<OrientedEdge, 6> getOrientedEdges(DelaunayTessellation::CellHandle cell) const {
        // Get the four vertices of the current Delaunay cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);
        // Retrieve the six edges of the tetrahedron.
        std::array<OrientedEdge, 6> edges;
        for(int i = 0; i < 6; i++)
            edges[i] = getOrientedEdge(cellVertices, i);
        return edges;
    }

    /// Returns the three oriented edges of a facet of a Delaunay cell.
    static std::array<OrientedEdge, 3> getFacetCircuitEdges(const std::array<OrientedEdge, 6>& cellEdges, int facetIndex) {
        switch(facetIndex) {
            case 0: return { cellEdges[3], cellEdges[5], -cellEdges[4] };
            case 1: return { cellEdges[2], -cellEdges[5], -cellEdges[1] };
            case 2: return { cellEdges[0], cellEdges[4], -cellEdges[2] };
            case 3: return { cellEdges[1], -cellEdges[3], -cellEdges[0] };
            default: OVITO_ASSERT(false); return {};
        }
    }

    /// Returns the three oriented edges of a facet of a Delaunay cell.
    std::array<OrientedEdge, 3> getFacetCircuitEdges(DelaunayTessellation::CellHandle cell, int facetIndex) const {
        // Get the four vertices of the current Delaunay cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);
        switch(facetIndex) {
            case 0: return { getOrientedEdge(cellVertices, 3), getOrientedEdge(cellVertices, 5), -getOrientedEdge(cellVertices, 4) };
            case 1: return { getOrientedEdge(cellVertices, 2), -getOrientedEdge(cellVertices, 5), -getOrientedEdge(cellVertices, 1) };
            case 2: return { getOrientedEdge(cellVertices, 0), getOrientedEdge(cellVertices, 4), -getOrientedEdge(cellVertices, 2) };
            case 3: return { getOrientedEdge(cellVertices, 1), -getOrientedEdge(cellVertices, 3), -getOrientedEdge(cellVertices, 0) };
            default: OVITO_ASSERT(false); return {};
        }
    }

    /// Determines whether the elastic mapping from the physical configuration
    /// of the crystal to the imaginary, stress-free configuration is compatible
    /// within the given Delaunay cell. Returns false if the mapping is incompatible
    /// or cannot be determined.
    bool isElasticMappingCompatible(DelaunayTessellation::CellHandle cell) const {
        return isElasticMappingCompatible(getOrientedEdges(cell));
    }

    /// Determines whether the elastic mapping from the physical configuration
    /// of the crystal to the imaginary, stress-free configuration is compatible
    /// within the given Delaunay cell. Returns false if the mapping is incompatible
    /// or cannot be determined.
    bool isElasticMappingCompatible(const std::array<OrientedEdge, 6>& cellEdges) const;

    /// Extracts the Delaunay edges with no lattice vector from the elastic mapping.
    void extractUnassignedEdges(TaskProgress& progress, PropertyFactory<Point3>& edgePosition1Access,
                                 PropertyFactory<Point3>& edgePosition2Access, PropertyFactory<int64_t*>& edgeAtomAccess, PropertyFactory<int>& edgeStageAccess,
                                 int stage);

    /// Returns for a Delaunay cell whether it's a filled tetrahedron or not.
    bool isFilledCell(DelaunayTessellation::CellHandle cell) const {
        return _filledCells[cell];
    }

private:

    /// Looks up the tessellation edge connecting two atoms.
    /// Returns nullptr if the two atoms are not connected by a Delaunay edge.
    TessellationEdge* findEdge(AtomIndex atomIndex1, AtomIndex atomIndex2) const {
        OVITO_ASSERT(atomIndex1 < _atomOutboundEdges.size());
        OVITO_ASSERT(atomIndex2 < _atomOutboundEdges.size());
        for(TessellationEdge* e = _atomOutboundEdges[atomIndex1]; e != nullptr; e = e->nextOutboundEdge)
            if(e->atom2 == atomIndex2) return e;
        for(TessellationEdge* e = _atomInboundEdges[atomIndex1]; e != nullptr; e = e->nextInboundEdge)
            if(e->atom1 == atomIndex2) return e;
        return nullptr;
    }

    /// Shifts the order of face vertices so that the smallest index is at the front.
    static void reorderFaceVertices(std::array<AtomIndex, 3>& atomIndices) {
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WASM)
        std::rotate(std::begin(atomIndices), std::min_element(std::begin(atomIndices), std::end(atomIndices)), std::end(atomIndices));
#else
        // Workaround for compiler bug in Xcode 10.0. Clang hangs when compiling the code above with -O2/-O3 flag.
        auto min_index = std::distance(atomIndices.begin(), std::min_element(atomIndices.begin(), atomIndices.end()));
        std::rotate(atomIndices.begin(), atomIndices.begin() + min_index, atomIndices.end());
#endif
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

    /// Stores for each Delaunay cell whether it's a filled tetrahedron or not.
    std::vector<bool> _filledCells;

    /// This map allows looking up the primary image of a tetrahedral facet.
    std::map<std::array<AtomIndex, 3>, DelaunayTessellation::Facet> _primaryFacetLookupMap;
};

}   // End of namespace
