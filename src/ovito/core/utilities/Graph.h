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

#include <ovito/core/Core.h>
#include <ovito/core/utilities/MemoryPool.h>
#include <vector>
#include <span>

namespace Ovito {

// Forward declaration
template<typename NodeData, typename EdgeData>
class SubGraph;

/**
 * \brief An undirected graph data structure with efficient edge lookup per node.
 *
 * This graph class provides:
 * - Fast O(1) node insertion
 * - Fast O(1) edge insertion (before finalization)
 * - Fast O(1) access to all edges of a given node (after finalization)
 *
 * The graph uses an offline construction model:
 * 1. Insert all nodes using addNode()
 * 2. Insert all edges using addEdge()
 * 3. Call finalize() to build the adjacency structures
 * 4. Query edges efficiently using getNodeEdges()
 *
 * \tparam NodeData Custom data type to store with each node
 * \tparam EdgeData Custom data type to store with each edge
 */
template<typename NodeData = void, typename EdgeData = void>
class Graph
{
public:
    /// Forward declarations
    struct Node;
    struct Edge;

    /// Type alias for node index
    using NodeIndex = size_t;

    /// Type alias for edge index
    using EdgeIndex = size_t;

    /**
     * \brief Represents a node in the graph.
     */
    struct Node {
        /// Index of this node
        NodeIndex index;

        /// Custom node data (if NodeData is not void)
        [[no_unique_address]] NodeData data;

        /// Range of edges in the finalized edge list (set during finalization)
        size_t edgeBegin = 0;
        size_t edgeEnd = 0;

        /// Constructor for Node with data
        explicit Node(NodeIndex idx, NodeData&& nodeData = NodeData{})
            requires(!std::is_void_v<NodeData>)
            : index(idx), data(std::move(nodeData))
        {
        }

        /// Constructor for Node without data
        explicit Node(NodeIndex idx)
            requires(std::is_void_v<NodeData>)
            : index(idx)
        {
        }
    };

    /**
     * \brief Represents an edge in the graph.
     */
    struct Edge {
        /// Source node index
        NodeIndex source;

        /// Target node index
        NodeIndex target;

        /// Custom edge data (if EdgeData is not void)
        [[no_unique_address]] EdgeData data;

        /// Constructor for Edge with data
        Edge(NodeIndex src, NodeIndex tgt, EdgeData&& edgeData = EdgeData{})
            requires(!std::is_void_v<EdgeData>)
            : source(src), target(tgt), data(std::move(edgeData))
        {
        }

        /// Constructor for Edge without data
        Edge(NodeIndex src, NodeIndex tgt)
            requires(std::is_void_v<EdgeData>)
            : source(src), target(tgt)
        {
        }
    };

    /**
     * \brief Constructor.
     */
    Graph() = default;

    /// Destructor
    ~Graph() = default;

    // Disable copy operations (use move instead)
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    // Enable move operations
    Graph(Graph&&) noexcept = default;
    Graph& operator=(Graph&&) noexcept = default;

    /**
     * \brief Adds a new node to the graph.
     * \param data Custom data to store with the node (if NodeData is not void)
     * \return The index of the newly created node, or the index of the existing node if one with the same data already exists
     *
     * If a node with the same data already exists, it will not be added again.
     * This method can only be called before finalize().
     */
    NodeIndex addNode(NodeData&& data = NodeData{})
        requires(!std::is_void_v<NodeData>)
    {
        OVITO_ASSERT(!_finalized);

        // Check if a node with the same data already exists
        for(const Node& node : _nodes) {
            if(node.data == data) {
                return node.index;  // Node with same data already exists
            }
        }

        NodeIndex idx = _nodes.size();
        _nodes.emplace_back(idx, std::move(data));
        return idx;
    }

    NodeIndex addNode()
        requires(std::is_void_v<NodeData>)
    {
        OVITO_ASSERT(!_finalized);

        NodeIndex idx = _nodes.size();
        _nodes.emplace_back(idx);
        return idx;
    }

    /**
     * \brief Adds a new edge to the graph.
     * \param source Index of the source node
     * \param target Index of the target node
     * \param data Custom data to store with the edge (if EdgeData is not void)
     * \return The index of the newly created edge, or the index of the existing edge if it already exists
     *
     * The edge direction will be normalized so that source < target.
     * Only add the edge once (not both directions).
     * The reverse edge will be created automatically during finalization.
     * If the edge already exists, it will not be added again.
     *
     * This method can only be called before finalize().
     */
    EdgeIndex addEdge(NodeIndex source, NodeIndex target, EdgeData&& data = EdgeData{})
        requires(!std::is_void_v<EdgeData>)
    {
        OVITO_ASSERT(!_finalized);
        OVITO_ASSERT(source < _nodes.size());
        OVITO_ASSERT(target < _nodes.size());
        OVITO_ASSERT(source != target);  // No self-loops

        // Normalize edge direction: always store with source < target
        if(source > target) {
            std::swap(source, target);
        }

        // Check if edge already exists
        for(size_t i = 0; i < _edges.size(); i++) {
            const Edge& e = _edges[i];
            if(e.source == source && e.target == target) {
                return i;  // Edge already exists, return its index
            }
        }

        EdgeIndex idx = _edges.size();
        _edges.emplace_back(source, target, std::move(data));
        return idx;
    }

    EdgeIndex addEdge(NodeIndex source, NodeIndex target)
        requires(std::is_void_v<EdgeData>)
    {
        OVITO_ASSERT(!_finalized);
        OVITO_ASSERT(source < _nodes.size());
        OVITO_ASSERT(target < _nodes.size());
        OVITO_ASSERT(source != target);  // No self-loops

        // Normalize edge direction: always store with source < target
        if(source > target) {
            std::swap(source, target);
        }

        // Check if edge already exists
        for(size_t i = 0; i < _edges.size(); i++) {
            const Edge& e = _edges[i];
            if(e.source == source && e.target == target) {
                return i;  // Edge already exists, return its index
            }
        }

        EdgeIndex idx = _edges.size();
        _edges.emplace_back(source, target);
        return idx;
    }

    /**
     * \brief Finalizes the graph and builds adjacency structures for fast edge queries.
     *
     * After calling this method:
     * - No more nodes or edges can be added
     * - getNodeEdges() can be called efficiently
     *
     * This creates reverse edges automatically for the undirected graph.
     *
     * Time complexity: O(V + E log E) where V is the number of nodes and E is the number of edges.
     */
    void finalize()
    {
        OVITO_ASSERT(!_finalized);

        // Create reverse edges for undirected graph
        size_t originalEdgeCount = _edges.size();
        _edges.reserve(originalEdgeCount * 2);
        for(size_t i = 0; i < originalEdgeCount; i++) {
            const Edge& e = _edges[i];
            if constexpr(!std::is_void_v<EdgeData>) {
                _edges.emplace_back(e.target, e.source, EdgeData(e.data));
            }
            else {
                _edges.emplace_back(e.target, e.source);
            }
        }

        // Sort edges by source node for fast lookup
        std::sort(_edges.begin(), _edges.end(), [](const Edge& a, const Edge& b) { return a.source < b.source; });

        // Build adjacency ranges for each node
        size_t edgeIdx = 0;
        for(Node& node : _nodes) {
            node.edgeBegin = edgeIdx;
            while(edgeIdx < _edges.size() && _edges[edgeIdx].source == node.index) {
                edgeIdx++;
            }
            node.edgeEnd = edgeIdx;
        }

        _finalized = true;
    }

    /**
     * \brief Returns all edges originating from the given node.
     * \param nodeIdx Index of the node
     * \return A span of edges originating from the node
     *
     * This method can only be called after finalize().
     * Time complexity: O(1)
     */
    [[nodiscard]] std::span<const Edge> getNodeEdges(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(_finalized);
        OVITO_ASSERT(nodeIdx < _nodes.size());
        const Node& node = _nodes[nodeIdx];
        return std::span<const Edge>(_edges.data() + node.edgeBegin, node.edgeEnd - node.edgeBegin);
    }

    /**
     * \brief Returns a mutable span of edges originating from the given node.
     * \param nodeIdx Index of the node
     * \return A span of edges originating from the node
     *
     * This method can only be called after finalize().
     * Time complexity: O(1)
     */
    [[nodiscard]] std::span<Edge> getNodeEdges(NodeIndex nodeIdx)
    {
        OVITO_ASSERT(_finalized);
        OVITO_ASSERT(nodeIdx < _nodes.size());
        const Node& node = _nodes[nodeIdx];
        return std::span<Edge>(_edges.data() + node.edgeBegin, node.edgeEnd - node.edgeBegin);
    }

    /**
     * \brief Returns the number of edges originating from the given node.
     * \param nodeIdx Index of the node
     * \return The number of edges
     *
     * This method can only be called after finalize().
     * Time complexity: O(1)
     */
    [[nodiscard]] size_t getNodeDegree(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(_finalized);
        OVITO_ASSERT(nodeIdx < _nodes.size());
        const Node& node = _nodes[nodeIdx];
        return node.edgeEnd - node.edgeBegin;
    }

    /**
     * \brief Returns a reference to the node at the given index.
     * \param nodeIdx Index of the node
     * \return Reference to the node
     */
    [[nodiscard]] const Node& getNode(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(nodeIdx < _nodes.size());
        return _nodes[nodeIdx];
    }

    /**
     * \brief Returns a mutable reference to the node at the given index.
     * \param nodeIdx Index of the node
     * \return Reference to the node
     */
    [[nodiscard]] Node& getNode(NodeIndex nodeIdx)
    {
        OVITO_ASSERT(nodeIdx < _nodes.size());
        return _nodes[nodeIdx];
    }

    /**
     * \brief Returns all nodes in the graph.
     * \return A span of all nodes
     */
    [[nodiscard]] std::span<const Node> nodes() const { return _nodes; }

    /**
     * \brief Returns all edges in the graph.
     * \return A span of all edges
     *
     * Note: After finalization, edges are sorted by source node.
     */
    [[nodiscard]] std::span<const Edge> edges() const { return _edges; }

    /**
     * \brief Returns the number of nodes in the graph.
     */
    [[nodiscard]] size_t nodeCount() const { return _nodes.size(); }

    /**
     * \brief Returns the number of edges in the graph.
     *
     * This includes both directions after finalization.
     */
    [[nodiscard]] size_t edgeCount() const { return _edges.size(); }

    /**
     * \brief Returns true if the graph has been finalized.
     */
    [[nodiscard]] bool isFinalized() const { return _finalized; }

    /**
     * \brief Clears the graph and resets it to the unfinalized state.
     */
    void clear()
    {
        _nodes.clear();
        _edges.clear();
        _finalized = false;
    }

    /**
     * \brief Creates subgraphs for each connected component in the graph.
     * \return A vector of SubGraph objects, one per connected component
     *
     * This method identifies all connected components using a depth-first search
     * and creates a SubGraph for each component containing all nodes and edges
     * within that component.
     *
     * IMPORTANT: The returned SubGraph objects hold non-owning pointers to this Graph.
     * The caller must ensure this Graph outlives the returned SubGraphs.
     *
     * This method can only be called after finalize().
     * Time complexity: O(V + E) where V is the number of nodes and E is the number of edges.
     */
    [[nodiscard]] std::vector<SubGraph<NodeData, EdgeData>> createConnectedComponentSubGraphs() const
    {
        OVITO_ASSERT(_finalized);

        std::vector<SubGraph<NodeData, EdgeData>> result;
        std::vector<bool> visited(_nodes.size(), false);
        std::vector<size_t> componentMap(_nodes.size(), SIZE_MAX);  // Maps node index to component index

        // For each unvisited node, perform DFS to find its connected component
        for(const Node& node : _nodes) {
            if(visited[node.index]) continue;

            // Create a new subgraph for this component (with non-owning pointer to this graph)
            result.emplace_back(this);
            SubGraph<NodeData, EdgeData>& subgraph = result.back();

            size_t componentIdx = result.size() - 1;

            // DFS to find all nodes in this component
            std::vector<NodeIndex> stack;
            stack.push_back(node.index);
            visited[node.index] = true;
            componentMap[node.index] = componentIdx;

            while(!stack.empty()) {
                NodeIndex currentIdx = stack.back();
                stack.pop_back();

                // Add this node to the subgraph
                subgraph.addNode(currentIdx);

                // Visit all neighbors
                for(const Edge& edge : getNodeEdges(currentIdx)) {
                    if(!visited[edge.target]) {
                        visited[edge.target] = true;
                        componentMap[edge.target] = componentIdx;
                        stack.push_back(edge.target);
                    }
                }
            }
        }

        // Now add edges to each subgraph
        for(const Edge& edge : _edges) {
            // Only add each edge once (use the original edges before they were doubled for undirected graph)
            // Since edges are doubled during finalize(), we can identify original edges by checking source < target
            if(edge.source < edge.target) {
                size_t componentIdx = componentMap[edge.source];
                OVITO_ASSERT(componentIdx == componentMap[edge.target]);  // Both nodes should be in same component
                result[componentIdx].addEdge(edge.source, edge.target);
            }
        }

        // Finalize all subgraphs
        for(auto& subgraph : result) {
            subgraph.finalize();
        }

        return result;
    }

private:
    /// Whether the graph has been finalized
    bool _finalized = false;

    /// List of nodes
    std::vector<Node> _nodes;

    /// List of edges (sorted by source node after finalization)
    std::vector<Edge> _edges;
};

/**
 * \brief A lightweight subgraph view that references a parent graph or subgraph.
 *
 * This class provides a hierarchical subgraph structure where:
 * - Only a subset of nodes and edges from the parent are included
 * - Edge properties can be overridden in the subgraph
 * - If an edge property is not overridden, it is inherited from the parent (or parent's parent, etc.)
 * - Node indices remain the same as in the parent graph
 *
 * The subgraph is lightweight and does not copy node/edge data from the parent.
 * It only stores:
 * - A vector of node indices that are part of this subgraph
 * - A vector of edges (source, target pairs) that are part of this subgraph
 * - A vector of overridden edge properties
 *
 * The subgraph uses an offline construction model similar to Graph:
 * 1. Add nodes using addNode()
 * 2. Add edges using addEdge()
 * 3. Optionally override edge data using overrideEdgeData()
 * 4. Call finalize() to build lookup structures for fast queries
 *
 * IMPORTANT: The caller must ensure that the parent Graph or SubGraph outlives
 * this SubGraph instance, as it holds a non-owning pointer to the parent.
 *
 * \tparam NodeData Custom data type to store with each node (must match parent)
 * \tparam EdgeData Custom data type to store with each edge (must match parent)
 */
template<typename NodeData = void, typename EdgeData = void>
class SubGraph
{
public:
    using NodeIndex = typename Graph<NodeData, EdgeData>::NodeIndex;
    using EdgeIndex = typename Graph<NodeData, EdgeData>::EdgeIndex;
    using ParentGraph = Graph<NodeData, EdgeData>;
    using ParentSubGraph = SubGraph<NodeData, EdgeData>;

    /// Type for edge key (ordered pair of nodes)
    struct EdgeKey {
        NodeIndex node1;
        NodeIndex node2;

        EdgeKey(NodeIndex n1, NodeIndex n2) : node1(std::min(n1, n2)), node2(std::max(n1, n2)) {}

        bool operator==(const EdgeKey& other) const { return node1 == other.node1 && node2 == other.node2; }

        bool operator<(const EdgeKey& other) const
        {
            if(node1 != other.node1) return node1 < other.node1;
            return node2 < other.node2;
        }
    };

    /// Stores an edge data override
    struct EdgeDataOverride {
        EdgeKey edge;
        EdgeData data;

        EdgeDataOverride(const EdgeKey& e, EdgeData&& d)
            requires(!std::is_void_v<EdgeData>)
            : edge(e), data(std::move(d))
        {
        }
    };

    /**
     * \brief Constructor for subgraph from a parent Graph.
     * \param parent Non-owning pointer to the parent graph (must outlive this SubGraph)
     */
    explicit SubGraph(const ParentGraph* parent) : _parent(parent), _parentSubGraph(nullptr)
    {
        OVITO_ASSERT(_parent);
        OVITO_ASSERT(_parent->isFinalized());
    }

    /**
     * \brief Constructor for subgraph from a parent SubGraph.
     * \param parent Non-owning pointer to the parent subgraph (must outlive this SubGraph)
     */
    explicit SubGraph(const ParentSubGraph* parent) : _parent(nullptr), _parentSubGraph(parent)
    {
        OVITO_ASSERT(_parentSubGraph);
        OVITO_ASSERT(_parentSubGraph->isFinalized());
    }

    /**
     * \brief Adds a node to the subgraph.
     * \param nodeIdx Index of the node in the parent graph
     *
     * The node must exist in the parent graph.
     * This method can only be called before finalize().
     */
    void addNode(NodeIndex nodeIdx)
    {
        OVITO_ASSERT(!_finalized);
        if(_parent) {
            OVITO_ASSERT(nodeIdx < _parent->nodeCount());
        }
        else {
            OVITO_ASSERT(_parentSubGraph->containsNode(nodeIdx));
        }
        _nodes.push_back(nodeIdx);
    }

    /**
     * \brief Adds an edge to the subgraph.
     * \param source Source node index
     * \param target Target node index
     *
     * Both nodes must be part of this subgraph.
     * This method can only be called before finalize().
     */
    void addEdge(NodeIndex source, NodeIndex target)
    {
        OVITO_ASSERT(!_finalized);
        _edges.push_back(EdgeKey(source, target));
    }

    /**
     * \brief Overrides the edge data for a specific edge in this subgraph.
     * \param source Source node index
     * \param target Target node index
     * \param data The new edge data
     *
     * This method can only be called before finalize().
     */
    void overrideEdgeData(NodeIndex source, NodeIndex target, EdgeData&& data)
        requires(!std::is_void_v<EdgeData>)
    {
        OVITO_ASSERT(!_finalized);
        _edgeDataOverrides.emplace_back(EdgeKey(source, target), std::move(data));
    }

    /**
     * \brief Finalizes the subgraph and builds lookup structures for fast queries.
     *
     * After calling this method:
     * - No more nodes, edges, or overrides can be added
     * - containsNode() and containsEdge() can be called efficiently
     * - getEdgeData() can be called efficiently
     *
     * Time complexity: O(N log N + E log E + D log D) where N is the number of nodes,
     * E is the number of edges, and D is the number of data overrides.
     */
    void finalize()
    {
        OVITO_ASSERT(!_finalized);

        // Sort nodes for binary search
        std::sort(_nodes.begin(), _nodes.end());

        // Remove duplicates from nodes
        _nodes.erase(std::unique(_nodes.begin(), _nodes.end()), _nodes.end());

        // Sort edges for binary search
        std::sort(_edges.begin(), _edges.end());

        // Remove duplicates from edges
        _edges.erase(std::unique(_edges.begin(), _edges.end()), _edges.end());

        // Sort edge data overrides for binary search
        if constexpr(!std::is_void_v<EdgeData>) {
            std::sort(_edgeDataOverrides.begin(), _edgeDataOverrides.end(), [](const EdgeDataOverride& a, const EdgeDataOverride& b) {
                return a.edge < b.edge;
            });

            // Remove duplicates (keeping last override for each edge)
            auto it = std::unique(_edgeDataOverrides.rbegin(),
                                  _edgeDataOverrides.rend(),
                                  [](const EdgeDataOverride& a, const EdgeDataOverride& b) { return a.edge == b.edge; });
            _edgeDataOverrides.erase(_edgeDataOverrides.begin(), it.base());
        }

        _finalized = true;
    }

    /**
     * \brief Gets the edge data for an edge, with hierarchical fallback.
     * \param source Source node index
     * \param target Target node index
     * \return Reference to the edge data (from this subgraph or inherited from parent)
     *
     * This method searches for edge data in the following order:
     * 1. This subgraph's overrides
     * 2. Parent subgraph's overrides (recursively)
     * 3. Original parent graph's data
     *
     * This method can only be called after finalize().
     */
    [[nodiscard]] const EdgeData& getEdgeData(NodeIndex source, NodeIndex target) const
        requires(!std::is_void_v<EdgeData>)
    {
        OVITO_ASSERT(_finalized);
        EdgeKey key(source, target);

        // Binary search in our overrides
        auto it = std::lower_bound(
            _edgeDataOverrides.begin(), _edgeDataOverrides.end(), key, [](const EdgeDataOverride& override, const EdgeKey& k) {
                return override.edge < k;
            });

        if(it != _edgeDataOverrides.end() && it->edge == key) {
            return it->data;
        }

        // Check parent subgraph recursively
        if(_parentSubGraph) {
            return _parentSubGraph->getEdgeData(source, target);
        }

        // Get from parent graph
        OVITO_ASSERT(_parent);
        for(const auto& edge : _parent->getNodeEdges(source)) {
            if(edge.target == target) {
                return edge.data;
            }
        }

        // Should never reach here if edge exists
        OVITO_ASSERT(false);
        static EdgeData dummy{};
        return dummy;
    }

    /**
     * \brief Checks if a node is part of this subgraph.
     * \param nodeIdx Node index
     * \return True if the node is in this subgraph
     *
     * This method can only be called after finalize().
     * Time complexity: O(log N)
     */
    [[nodiscard]] bool containsNode(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(_finalized);
        return std::binary_search(_nodes.begin(), _nodes.end(), nodeIdx);
    }

    /**
     * \brief Checks if an edge is part of this subgraph.
     * \param source Source node index
     * \param target Target node index
     * \return True if the edge is in this subgraph
     *
     * This method can only be called after finalize().
     * Time complexity: O(log E)
     */
    [[nodiscard]] bool containsEdge(NodeIndex source, NodeIndex target) const
    {
        OVITO_ASSERT(_finalized);
        EdgeKey key(source, target);
        return std::ranges::binary_search(_edges, key);
    }

    /**
     * \brief Returns all nodes in the subgraph.
     * \return Span of node indices
     */
    [[nodiscard]] std::span<const NodeIndex> nodes() const { return _nodes; }

    /**
     * \brief Returns all edges in the subgraph.
     * \return Span of edge keys
     */
    [[nodiscard]] std::span<const EdgeKey> edges() const { return _edges; }

    /**
     * \brief Returns the number of nodes in the subgraph.
     */
    [[nodiscard]] size_t nodeCount() const { return _nodes.size(); }

    /**
     * \brief Returns the number of edges in the subgraph.
     */
    [[nodiscard]] size_t edgeCount() const { return _edges.size(); }

    /**
     * \brief Returns true if the subgraph has been finalized.
     */
    [[nodiscard]] bool isFinalized() const { return _finalized; }

    /**
     * \brief Returns a reference to the node data from the parent graph.
     * \param nodeIdx Node index
     * \return Reference to the node data
     *
     * The node must be part of this subgraph.
     */
    [[nodiscard]] const typename ParentGraph::Node& getNode(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(containsNode(nodeIdx));
        if(_parent) {
            return _parent->getNode(nodeIdx);
        }
        else {
            return _parentSubGraph->getNode(nodeIdx);
        }
    }

    /**
     * \brief Returns all edges of a node in this subgraph.
     * \param nodeIdx Node index
     * \return Vector of target node indices for edges originating from this node
     *
     * The node must be part of this subgraph.
     * Time complexity: O(E) where E is the number of edges in the subgraph.
     */
    [[nodiscard]] std::vector<NodeIndex> getNodeEdges(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(containsNode(nodeIdx));

        std::vector<NodeIndex> result;
        for(const EdgeKey& edgeKey : _edges) {
            if(edgeKey.node1 == nodeIdx) {
                result.push_back(edgeKey.node2);
            }
            else if(edgeKey.node2 == nodeIdx) {
                result.push_back(edgeKey.node1);
            }
        }
        return result;
    }

    /**
     * \brief Returns the degree of a node in this subgraph.
     * \param nodeIdx Node index
     * \return Number of edges connected to this node in the subgraph
     *
     * Time complexity: O(E) where E is the number of edges in the subgraph.
     */
    [[nodiscard]] size_t getNodeDegree(NodeIndex nodeIdx) const
    {
        OVITO_ASSERT(containsNode(nodeIdx));

        size_t degree = 0;
        for(const EdgeKey& edgeKey : _edges) {
            if(edgeKey.node1 == nodeIdx || edgeKey.node2 == nodeIdx) {
                degree++;
            }
        }
        return degree;
    }

    /**
     * \brief Clears the subgraph (removes all nodes and edges).
     */
    void clear()
    {
        _nodes.clear();
        _edges.clear();
        if constexpr(!std::is_void_v<EdgeData>) {
            _edgeDataOverrides.clear();
        }
        _finalized = false;
    }

private:
    /// Non-owning pointer to parent graph (if this is a direct child of a Graph)
    const ParentGraph* _parent;

    /// Non-owning pointer to parent subgraph (if this is a child of another SubGraph)
    const ParentSubGraph* _parentSubGraph;

    /// Whether the subgraph has been finalized
    bool _finalized = false;

    /// Vector of nodes in this subgraph (sorted after finalization)
    std::vector<NodeIndex> _nodes;

    /// Vector of edges in this subgraph (sorted after finalization)
    std::vector<EdgeKey> _edges;

    /// Vector of overridden edge data (sorted after finalization)
    [[no_unique_address]] std::vector<EdgeDataOverride> _edgeDataOverrides;
};

}  // namespace Ovito
