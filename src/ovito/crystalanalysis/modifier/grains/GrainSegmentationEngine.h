////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//  Copyright 2020 Peter Mahler Larsen
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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/modifier/analysis/ptm/PTMAlgorithm.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/utilities/DisjointSet.h>
#include "GrainSegmentationModifier.h"

#include <boost/intrusive/rbtree_algorithms.hpp>
#include <unordered_set>

namespace Ovito {

union NodeUnion
{
    size_t opposite;
    size_t size;
};

struct HalfEdge
{
    HalfEdge *parent_, *left_, *right_;
    int      color_;

    //other members
    NodeUnion data;
    FloatType weight;
};

//Define rbtree_node_traits
struct my_rbtree_node_traits
{
    typedef HalfEdge                                   node;
    typedef HalfEdge *                                 node_ptr;
    typedef const HalfEdge *                           const_node_ptr;
    typedef int                                        color;
    static node_ptr get_parent(const_node_ptr n)       {  return n->parent_;   }
    static void set_parent(node_ptr n, node_ptr parent){  n->parent_ = parent; }
    static node_ptr get_left(const_node_ptr n)         {  return n->left_;     }
    static void set_left(node_ptr n, node_ptr left)    {  n->left_ = left;     }
    static node_ptr get_right(const_node_ptr n)        {  return n->right_;    }
    static void set_right(node_ptr n, node_ptr right)  {  n->right_ = right;   }
    static color get_color(const_node_ptr n)           {  return n->color_;    }
    static void set_color(node_ptr n, color c)         {  n->color_ = c;       }
    static color black()                               {  return color(0);     }
    static color red()                                 {  return color(1);     }
};

struct node_ptr_compare
{
    bool operator()(const HalfEdge *a, const HalfEdge *b)
    {return a->data.opposite < b->data.opposite;}
};

typedef boost::intrusive::rbtree_algorithms<my_rbtree_node_traits> algo;

static HalfEdge* find(HalfEdge* header, size_t index) {
    HalfEdge key;
    key.data.opposite = index;
    HalfEdge *result = algo::find(header, &key, node_ptr_compare());
    return result == header ? nullptr : result;
}

static void insert_halfedge(HalfEdge* header, HalfEdge* edge) {
    algo::insert_equal_upper_bound(header, edge, node_ptr_compare());
    header->data.size++;
}

/*
 * Computation engine of the GrainSegmentationModifier, which decomposes a polycrystalline microstructure into individual grains.
 */
class GrainSegmentationEngine1
{
public:

    class Graph
    {
    public:
        std::vector<FloatType> wnode;
        std::vector<HalfEdge> header;
        std::vector<HalfEdge> edge_buffer;
        size_t edge_count;
        std::unordered_set<size_t> active_nodes;

        Graph(size_t num_nodes, size_t num_edges) {
            edge_count = 0;
            wnode.resize(num_nodes);
            std::ranges::fill(wnode, 0);

            header.resize(num_nodes);
            for (size_t i=0;i<num_nodes;i++) {
                algo::init_header(&header[i]);
                header[i].data.size = 0;
            }

            edge_buffer.resize(2 * num_edges);
        }

        size_t num_nodes() const {
            return active_nodes.size();
        }

        size_t next_node() const {
            return *active_nodes.begin();
        }

        std::tuple<FloatType, size_t> nearest_neighbor(size_t a) const {
            FloatType dmin = std::numeric_limits<FloatType>::max();
            size_t vmin = std::numeric_limits<size_t>::max();

            HalfEdge *edge = header[a].left_;
            for (size_t it=0;it<header[a].data.size;it++) {
                size_t v = edge->data.opposite;
                FloatType weight = edge->weight;
                edge = algo::next_node(edge);

                OVITO_ASSERT(v != a); // Graph has self loops.
                if(v == a)
                    throw Exception("Graph has self loops");

                FloatType d = wnode[v] / weight;
                OVITO_ASSERT(!std::isnan(d));

                if (d < dmin) {
                    dmin = d;
                    vmin = v;
                }
                else if (d == dmin) {
                    vmin = std::min(vmin, v);
                }
            }

            Q_DECL_UNUSED FloatType check = dmin * wnode[a];
            OVITO_ASSERT(!std::isnan(check));

            return std::make_tuple(dmin * wnode[a], vmin);
        }

        void add_edge(size_t u, size_t v, FloatType w) {
            size_t nodes[2] = {u, v};

            for (auto index: nodes) {
                if (header[index].data.size == 0) {
                    active_nodes.insert(index);
                }

                wnode[index] += w;
            }

            HalfEdge* edge = &edge_buffer[edge_count++];
            edge->data.opposite = v;
            edge->weight = w;
            insert_halfedge(&header[u], edge);

            edge = &edge_buffer[edge_count++];
            edge->data.opposite = u;
            edge->weight = w;
            insert_halfedge(&header[v], edge);
        }

        void remove_node(size_t u) {
            active_nodes.erase(u);
        }

        size_t contract_edge(size_t a, size_t b) {
            if (header[b].data.size > header[a].data.size) {
                std::swap(a, b);
            }

            algo::unlink(find(&header[b], a));
            algo::unlink(find(&header[a], b));
            header[a].data.size--;
            header[b].data.size--;

            HalfEdge *edge = header[b].left_;
            while (header[b].data.size != 0) {
                size_t v = edge->data.opposite;
                FloatType w = edge->weight;
                auto next = algo::next_node(edge);
                algo::unlink(edge);
                header[b].data.size--;

                HalfEdge* opposite_edge = find(&header[v], b);
                algo::unlink(opposite_edge);
                header[v].data.size--;

                // Now add edge weights like this:
                // (adj[a])[v] += w;
                // (adj[v])[a] += w;
                HalfEdge *temp = find(&header[a], v);
                if (temp != nullptr) {
                    temp->weight += w;
                    find(&header[v], a)->weight += w;
                }
                else {
                    edge->data.opposite = v;
                    edge->weight = w;
                    insert_halfedge(&header[a], edge);

                    opposite_edge->data.opposite = a;
                    opposite_edge->weight = w;
                    insert_halfedge(&header[v], opposite_edge);
                }

                edge = next;
            }

            remove_node(b);
            wnode[a] += wnode[b];
            return a;
        }
    };

    /// The maximum number of neighbor atoms taken into account for orphan atom adoption.
    static constexpr int MAX_DISORDERED_NEIGHBORS = 8;

    /// Represents a single bond connecting two neighboring lattice atoms.
    struct NeighborBond {
        size_t a;
        size_t b;
        FloatType disorientation;
        FloatType length;
    };

    struct DendrogramNode {

        DendrogramNode() = default;
        DendrogramNode(size_t _a, size_t _b, FloatType _distance, FloatType _disorientation, size_t _size, Quaternion _orientation)
            : a(_a), b(_b), distance(_distance), disorientation(_disorientation), size(_size), orientation(_orientation) {}

        size_t a = 0;
        size_t b = 0;
        FloatType distance = std::numeric_limits<FloatType>::lowest();
        FloatType disorientation = std::numeric_limits<FloatType>::lowest();
        size_t size = 0;
        FloatType merge_size = 0;
        Quaternion orientation;
    };

    class InterfaceHandler
    {
    public:
        InterfaceHandler(BufferReadAccess<PTMAlgorithm::StructureType> structuresArray) {

            // Count structure types
            int structureCounts[PTMAlgorithm::NUM_STRUCTURE_TYPES] = {0};
            for (auto structureType: structuresArray) {
                structureCounts[(int)structureType]++;
            }

            parent_fcc = structureCounts[(size_t)PTMAlgorithm::FCC] >= structureCounts[(size_t)PTMAlgorithm::HCP];
            parent_dcub = structureCounts[(size_t)PTMAlgorithm::CUBIC_DIAMOND] >= structureCounts[(size_t)PTMAlgorithm::HEX_DIAMOND];

            // Set structure targets (i.e. which way a structure will flip)
            if (parent_fcc) {
                target[(size_t)PTMAlgorithm::HCP] = PTMAlgorithm::FCC;
            }
            else {
                target[(size_t)PTMAlgorithm::FCC] = PTMAlgorithm::HCP;
            }

            if (parent_dcub) {
                target[(size_t)PTMAlgorithm::HEX_DIAMOND] = PTMAlgorithm::CUBIC_DIAMOND;
            }
            else {
                target[(size_t)PTMAlgorithm::CUBIC_DIAMOND] = PTMAlgorithm::HEX_DIAMOND;
            }
        }

        PTMAlgorithm::StructureType parent_phase(PTMAlgorithm::StructureType defectStructureType) {
            return target[(size_t)defectStructureType];
        }

        bool reorder_bond(NeighborBond& bond, std::vector<PTMAlgorithm::StructureType>& _adjustedStructureTypes) {
            auto a = bond.a;
            auto b = bond.b;
            auto sa = _adjustedStructureTypes[a];
            auto sb = _adjustedStructureTypes[b];

            // We want ordering of (a, b) to be (parent phase, defect phase)
            bool flipped = false;
            if (sa == PTMAlgorithm::FCC && sb == PTMAlgorithm::HCP) {
                flipped |= !parent_fcc;
            }
            else if (sa == PTMAlgorithm::HCP && sb == PTMAlgorithm::FCC) {
                flipped |= parent_fcc;
            }
            else if (sa == PTMAlgorithm::CUBIC_DIAMOND && sb == PTMAlgorithm::HEX_DIAMOND) {
                flipped |= !parent_dcub;
            }
            else if (sa == PTMAlgorithm::HEX_DIAMOND && sb == PTMAlgorithm::CUBIC_DIAMOND) {
                flipped |= parent_dcub;
            }
            else {
                return false;
            }

            if (flipped) {
                std::swap(a, b);
            }

            bond.a = a;
            bond.b = b;
            return true;
        }

    private:
        bool parent_fcc;
        bool parent_dcub;
        PTMAlgorithm::StructureType target[PTMAlgorithm::NUM_STRUCTURE_TYPES];
    };

    /// Constructor.
    GrainSegmentationEngine1(
            ConstPropertyPtr positions,
            ConstPropertyPtr structureProperty,
            ConstPropertyPtr orientationProperty,
            ConstPropertyPtr correspondenceProperty,
            const SimulationCell* simCell,
            GrainSegmentationModifier::MergeAlgorithm algorithmType,
            bool handleCoherentInterfaces,
            bool outputBonds);

    /// Performs the computation.
    void perform();

    /// Injects the computed results into the data pipeline.
    void applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, BondsVis* bondsVis) const;

    /// Returns the property storage that contains the input particle positions.
    const ConstPropertyPtr& positions() const { return _positions; }

    /// Returns the simulation cell data.
    const SimulationCellData& cell() const { return _simCell; }

    // Returns the merge distances for the scatter plot
    const PropertyPtr& mergeDistance() const { return _mergeDistance; }

    // Returns the merge sizes for the scatter plot
    const PropertyPtr& mergeSize() const { return _mergeSize; }

    // Returns the log merge distances for the scatter plot
    const PropertyPtr& logMergeDistance() const { return _logMergeDistance; }

    // Returns the log merge sizes for the scatter plot
    const PropertyPtr& logMergeSize() const { return _logMergeSize; }

    /// Returns the per-particle structure types.
    const ConstPropertyPtr& structureTypes() const { return _structureTypes; }

    /// Returns the per-particle lattice orientations.
    const ConstPropertyPtr& orientations() const { return _orientations; }

    /// Returns the per-particle template correspondences.
    const ConstPropertyPtr& correspondences() const { return _correspondences; }

    /// Returns the adaptively determined merge threshold.
    FloatType suggestedMergingThreshold() const { return _suggestedMergingThreshold; }

private:

    /// Returns the list of bonds connecting neighboring lattice atoms.
    std::vector<NeighborBond>& neighborBonds() { return _neighborBonds; }

    /// Returns the list of bonds connecting neighboring lattice atoms.
    const std::vector<NeighborBond>& neighborBonds() const { return _neighborBonds; }

    /// Creates neighbor bonds from stored PTM data.
    void createNeighborBonds(TaskProgress& progress);

    /// Rotates hexagonal atoms (HCP and hex-diamond) to an equivalent cubic orientation.
    void rotateInterfaceAtoms(TaskProgress& progress);

    /// Calculates the disorientation angle for each graph edge (i.e. bond).
    void computeDisorientationAngles(TaskProgress& progress);

    /// Builds grains by iterative region merging.
    void determineMergeSequence(TaskProgress& progress);

    /// Computes the disorientation angle between two crystal clusters of the given lattice type.
    /// Furthermore, the function computes the weighted average of the two cluster orientations.
    /// The norm of the two input quaternions and the output quaternion represents the size of the clusters.
    static FloatType calculate_disorientation(int structureType, Quaternion& qa, const Quaternion& qb);

    // Algorithm types:
    void minimum_spanning_tree_clustering(std::vector<Quaternion>& qsum, DisjointSet& uf, TaskProgress& progress);
    void node_pair_sampling_clustering(Graph& graph, std::vector<Quaternion>& qsum, TaskProgress& progress);

    // Selects a threshold for Node Pair Sampling algorithm
    FloatType calculate_threshold_suggestion();

    // Determines if a bond is crystalline
    bool isCrystallineBond(const NeighborBond& bond) const
    {
        auto a = _adjustedStructureTypes[bond.a];
        auto b = _adjustedStructureTypes[bond.b];

        if (a == PTMAlgorithm::OTHER) return false;
        if (b == PTMAlgorithm::OTHER) return false;
        if (a == b) return true;
        if (!_handleBoundaries) return false;

        if (a == PTMAlgorithm::FCC && b == PTMAlgorithm::HCP) return true;
        if (a == PTMAlgorithm::HCP && b == PTMAlgorithm::FCC) return true;
        if (a == PTMAlgorithm::CUBIC_DIAMOND && b == PTMAlgorithm::HEX_DIAMOND) return true;
        if (a == PTMAlgorithm::HEX_DIAMOND && b == PTMAlgorithm::CUBIC_DIAMOND) return true;
        return false;
    }

    bool interface_cubic_hex(NeighborBond& bond, InterfaceHandler& interfaceHandler,
                             Quaternion& output);

    // Converts a disorientation to an edge weight for Node Pair Sampling algorithm
    static FloatType calculateGraphWeight(FloatType disorientation) {
        // This is a workaround for an issue in GrainSegmentationEngine1::node_pair_sampling_clustering(),
        // which can get stuck in an infinite loop for pathological inputs, e.g. an ideal HCP crystal.
        if(disorientation < 1e-5)
            disorientation = 0;

        // This is fairly arbitrary but it works well.
        return std::exp(-FloatType(1)/3 * disorientation * disorientation);
    }

    // TODO: remove this and replace with a lambda function if possible
    struct PriorityQueueCompare
    {
        bool operator()(const NeighborBond &a, const NeighborBond &b) const {return a.disorientation > b.disorientation;}
    };

private:

    const size_t _minPlotSize = 20;

    // A hardcoded cutoff, in degrees, used for skipping low-weight edges in Node Pair Sampling mode
    static constexpr FloatType _misorientationThreshold = 4.0;

    // The linkage criterion used in the merge algorithm
    GrainSegmentationModifier::MergeAlgorithm _algorithmType;

    // The type of stacking fault handling
    bool _handleBoundaries;

    /// Controls the output of neighbor bonds to the data pipeline for visualization purposes.
    bool _outputBondsToPipeline;

    /// The number of input particles.
    size_t _numParticles;

    /// The coordinates of the input particles.
    ConstPropertyPtr _positions;

    /// The simulation cell geometry.
    const SimulationCellData _simCell;

    // The merge distances
    PropertyPtr _mergeDistance;

    // The merge sizes
    PropertyPtr _mergeSize;

    // The log merge distances
    PropertyPtr _logMergeDistance;

    // The merge sizes
    PropertyPtr _logMergeSize;

    /// The per-particle structure types.
    ConstPropertyPtr _structureTypes;

    /// The per-particle lattice orientations.
    ConstPropertyPtr _orientations;

    /// The per-particle structure types, adjusted for stacking fault handling.
    std::vector<PTMAlgorithm::StructureType> _adjustedStructureTypes;

    /// The per-particle lattice orientations.
    std::vector<Quaternion> _adjustedOrientations;

    /// The per-particle template correspondences.
    ConstPropertyPtr _correspondences;

    /// The bonds connecting neighboring lattice atoms.
    std::vector<NeighborBond> _neighborBonds;

    // Dendrogram as list of cluster merges.
    std::vector<DendrogramNode> _dendrogram;

    /// The adaptively computed merge threshold.
    FloatType _suggestedMergingThreshold = 0;

    friend class GrainSegmentationEngine2;
};

/*
 * Computation engine of the GrainSegmentationModifier, which decomposes a polycrystalline microstructure into individual grains.
 */
class GrainSegmentationEngine2
{
public:

    /// Constructor.
    GrainSegmentationEngine2(
            std::shared_ptr<const GrainSegmentationEngine1> engine1,
            FloatType mergingThreshold,
            bool adoptOrphanAtoms,
            size_t minGrainAtomCount,
            bool colorParticlesByGrain) :
        _engine1(std::move(engine1)),
        _numParticles(_engine1->_numParticles),
        _mergingThreshold(mergingThreshold),
        _adoptOrphanAtoms(adoptOrphanAtoms),
        _minGrainAtomCount(minGrainAtomCount),
        _colorParticlesByGrain(colorParticlesByGrain),
        _atomClusters(Particles::OOClass().createUserProperty(DataBuffer::Initialized, _numParticles, Property::Int64, 1, QStringLiteral("Grain"))) {}

    /// Performs the computation.
    void perform();

    /// Injects the computed results into the data pipeline.
    void applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, BondsVis* bondsVis) const;

    /// Returns the array storing the cluster ID of each particle.
    const PropertyPtr& atomClusters() const { return _atomClusters; }

    struct PQNode {
        qlonglong cluster;
        size_t particleIndex;
        FloatType length;
    };

private:

    /// Merges any orphan atoms into the closest cluster.
    void mergeOrphanAtoms(TaskProgress& progress);

    struct PQCompareLength
    {
        bool operator()(const PQNode &a, const PQNode &b) const {return a.length > b.length;}
    };

private:

    /// Pointer to the first algorithm stage.
    std::shared_ptr<const GrainSegmentationEngine1> _engine1;

    /// The number of input particles.
    size_t _numParticles;

    /// The particle to cluster assignment.
    PropertyPtr _atomClusters;

    /// Counts the number of clusters
    size_t _numClusters = 1;

    // The output list of grain IDs.
    PropertyPtr _grainIds;

    // The output list of grain sizes.
    PropertyPtr _grainSizes;

    /// The output list of per-grain structure types.
    PropertyPtr _grainStructureTypes;

    /// The output list of colors assigned to grains.
    PropertyPtr _grainColors;

    /// The output list of mean grain orientations.
    PropertyPtr _grainOrientations;

    /// The user-defined merge threshold.
    FloatType _mergingThreshold;

    /// The minimum number of atoms a grain must have.
    size_t _minGrainAtomCount;

    /// Controls the adoption of orphan atoms after the grains have been formed.
    bool _adoptOrphanAtoms;

    bool _colorParticlesByGrain;
};

}   // End of namespace
