////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/core/utilities/MemoryPool.h>
#include "InterfaceMesh.h"
#include "DelaunayTessellationSpatialQuery.h"

namespace Ovito {

/**
 * This is the central class for dislocation line tracing.
 */
class DislocationTracer
{
public:
    /// Constructor.
    DislocationTracer(InterfaceMesh& mesh, int maxTrialCircuitSize, int maxCircuitElongation, DislocationNetwork* network,
                      bool markCoreAtoms)
        : _mesh(mesh),
          _network(network),
          _clusterGraph(network->clusterGraph()),
          _maxBurgersCircuitSize(maxTrialCircuitSize),
          _maxExtendedBurgersCircuitSize(maxTrialCircuitSize + maxCircuitElongation),
          _markCoreAtoms(markCoreAtoms)
    {
    }

    /// Returns the interface mesh that separates the crystal defects from the perfect regions.
    const InterfaceMesh& mesh() const { return _mesh; }

    /// Returns the interface mesh that separates the crystal defects from the perfect regions.
    InterfaceMesh& mesh() { return _mesh; }

    /// Returns a reference to the cluster graph.
    const ClusterGraph* clusterGraph() { return _clusterGraph; }

    /// Returns the extracted network of dislocation segments.
    DislocationNetwork* network() { return _network; }

    /// Returns the simulation cell.
    const SimulationCell* cell() const { return mesh().domain(); }

    /// Performs a dislocation search on the interface mesh by generating
    /// trial Burgers circuits. Identified dislocation segments are converted to
    /// a continuous line representation
    void traceDislocationSegments();

    /// After dislocation segments have been extracted, this method trims
    /// dangling lines and finds the optimal cluster to express each segment's
    /// Burgers vector.
    void finishDislocationSegments(int crystalStructure);

    /// Returns the list of nodes that are not part of a junction.
    const std::vector<DislocationNode*>& danglingNodes() const { return _danglingNodes; }

    /// Returns the the dislocation line end that got associated with the given Delaunay cell.
    /// This method is part of the dislocation core atom identification algorithm.
    std::pair<DislocationNode*, bool> getDislocationNodeForDelaunayCell(size_t cell) const {
        OVITO_ASSERT(_markCoreAtoms);

        auto cellIdx = _mesh.tessellation().getUserField(cell);
        OVITO_ASSERT(cellIdx == -1 || (size_t)cellIdx < _cellDataForCoreAtomIdentification.size());

        return (cellIdx != -1) ? _cellDataForCoreAtomIdentification[cellIdx] : std::make_pair(nullptr, false);
    }

private:
    BurgersCircuit* allocateCircuit();
    void discardCircuit(BurgersCircuit* circuit);
    void findPrimarySegments(int maxBurgersCircuitSize);
    bool createBurgersCircuit(InterfaceMesh::Edge* edge, int maxBurgersCircuitSize);
    void createAndTraceSegment(const ClusterVector& burgersVector, BurgersCircuit* forwardCircuit, int maxCircuitLength);
    bool intersectsOtherCircuits(BurgersCircuit* circuit);
    BurgersCircuit* buildReverseCircuit(BurgersCircuit* forwardCircuit);
    void traceSegment(DislocationSegment& segment, DislocationNode& node, int maxCircuitLength, bool isPrimarySegment);
    bool tryRemoveTwoCircuitEdges(InterfaceMesh::Edge*& edge0, InterfaceMesh::Edge*& edge1, InterfaceMesh::Edge*& edge2);
    bool tryRemoveThreeCircuitEdges(InterfaceMesh::Edge*& edge0, InterfaceMesh::Edge*& edge1, InterfaceMesh::Edge*& edge2,
                                    bool isPrimarySegment);
    bool tryRemoveOneCircuitEdge(InterfaceMesh::Edge*& edge0, InterfaceMesh::Edge*& edge1, InterfaceMesh::Edge*& edge2,
                                 bool isPrimarySegment);
    bool trySweepTwoFacets(InterfaceMesh::Edge*& edge0, InterfaceMesh::Edge*& edge1, InterfaceMesh::Edge*& edge2, bool isPrimarySegment);
    bool tryInsertOneCircuitEdge(InterfaceMesh::Edge*& edge0, InterfaceMesh::Edge*& edge1, bool isPrimarySegment);
    void appendLinePoint(DislocationNode& node);
    void circuitCircuitIntersection(InterfaceMesh::Edge* circuitAEdge1, InterfaceMesh::Edge* circuitAEdge2,
                                    InterfaceMesh::Edge* circuitBEdge1, InterfaceMesh::Edge* circuitBEdge2, int& goingOutside,
                                    int& goingInside);
    size_t joinSegments(int maxCircuitLength);
    void createSecondarySegment(InterfaceMesh::Edge* firstEdge, BurgersCircuit* outerCircuit, int maxCircuitLength);

    /// Calculates the shift vector that must be subtracted from point B to bring it close to point A such that
    /// the vector (B-A) is not a wrapped vector.
    Vector3 calculateShiftVector(const Point3& a, const Point3& b) const
    {
        if(cell()) {
            Vector3 d = cell()->absoluteToReduced(b - a);
            d.x() = cell()->hasPbc(0) ? std::floor(d.x() + FloatType(0.5)) : FloatType(0);
            d.y() = cell()->hasPbc(1) ? std::floor(d.y() + FloatType(0.5)) : FloatType(0);
            d.z() = cell()->hasPbc(2) ? std::floor(d.z() + FloatType(0.5)) : FloatType(0);
            return cell()->reducedToAbsolute(d);
        }
        else {
            return b - a;
        }
    }

private:
    /// The interface mesh that separates the crystal defects from the perfect regions.
    InterfaceMesh& _mesh;

    /// The extracted network of dislocation segments.
    DislocationNetwork* _network;

    /// The cluster graph.
    const ClusterGraph* _clusterGraph;

    /// The maximum length (number of edges) for Burgers circuits during the first tracing phase.
    int _maxBurgersCircuitSize;

    /// The maximum length (number of edges) for Burgers circuits during the second tracing phase.
    int _maxExtendedBurgersCircuitSize;

    /// Used to allocate memory for BurgersCircuit instances.
    MemoryPool<BurgersCircuit> _circuitPool;

    /// List of nodes that do not form a junction.
    std::vector<DislocationNode*> _danglingNodes;

    /// Stores a pointer to the last allocated circuit which has been discarded.
    /// It can be recycled to serve the next allocation request.
    BurgersCircuit* _unusedCircuit = nullptr;

    /// Random number generator for the circuit sweeping algorithm, initialized with a fixed seed for reproducibility.
    std::mt19937 _rng{1};

    /// Controls the identification of dislocation core atoms.
    bool _markCoreAtoms;

    /// Spatial query class used to find Delaunay tetrahedra based on their bounding box.
    /// Used in the dislocation core atom identification algorithm.
    std::optional<DelaunayTessellationSpatialQuery> _spatialQuery;

    /// Output vector for tetrahedra obtained from the spatial query class.
    /// Vector variable is being recycled to avoid memory allocation overhead.
    std::vector<DelaunayTessellationSpatialQuery::bBox> _ranges;

    /// List of triangles of the currently moved circuit cap polygon, which is used for dislocation core atom identification.
    /// Vector variable is being recycled to avoid memory allocation overhead.
    std::vector<std::array<Point3, 3>> _triangles;

    /// Stores auxiliary information per "defective" Delaunay cell for dislocation core atom identification:
    ///   1. The dislocation line end that got associated with the cell.
    ///   2. A flag indicating whether that dislocation line end got extended already to detect dislocation junctions.
    std::vector<std::pair<DislocationNode*, bool>> _cellDataForCoreAtomIdentification;
};

}  // namespace Ovito
