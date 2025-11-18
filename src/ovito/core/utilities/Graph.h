///////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either
//  under the terms of the GNU General Public License version 3 as published
//  by the Free Software Foundation, or (at your option) any later version.
//
//  OVITO is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
//  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
//  details.
//
//  You should have received a copy of the GNU General Public License along
//  with OVITO; if not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <utility>
#include <memory>

namespace Ovito {

// Forward declaration
template<typename NodeId, typename NodeProperty, typename EdgeProperty>
class UndirectedSubgraph;

/******************************************************************************
 * An undirected graph class that supports arbitrary node identifiers and
 * stores custom properties for both nodes and edges.
 *
 * Template parameters:
 *   NodeId: Type used for node identifiers (e.g., int, size_t, custom type)
 *   NodeProperty: Type of properties stored with each node
 *   EdgeProperty: Type of properties stored with each edge
 ******************************************************************************/
template<typename NodeIdType, typename NodeProperty = std::monostate, typename EdgeProperty = std::monostate>
class UndirectedGraph : public std::enable_shared_from_this<UndirectedGraph<NodeIdType, NodeProperty, EdgeProperty>>
{
public:
    using NodeId = NodeIdType;

    /// Represents an edge between two nodes
    struct Edge {
        NodeId node1;
        NodeId node2;

        /// Creates an edge with normalized node ordering (smaller ID first)
        Edge(NodeId n1, NodeId n2)
        {
            if(n1 < n2) {
                node1 = n1;
                node2 = n2;
            }
            else {
                node1 = n2;
                node2 = n1;
            }
        }

        bool operator==(const Edge& other) const { return node1 == other.node1 && node2 == other.node2; }
    };

    /// Hash function for edges
    struct EdgeHash {
        std::size_t operator()(const Edge& edge) const
        {
            std::size_t h1 = std::hash<NodeId>{}(edge.node1);
            std::size_t h2 = std::hash<NodeId>{}(edge.node2);

            // 64-bit-friendly mixing (boost::hash_combine pattern)
            h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2);
            return h1;
        }
    };

public:
    /// Default constructor
    UndirectedGraph() = default;

    /******************************************************************************
     * Adds a node to the graph with optional properties.
     ******************************************************************************/
    void addNode(NodeId id, NodeProperty property = NodeProperty{})
    {
        _nodeProperties[id] = std::move(property);
        // Ensure the node exists in the adjacency list even if it has no edges
        if(_adjacency.find(id) == _adjacency.end()) {
            _adjacency[id] = std::unordered_set<NodeId>();
        }
    }

    /******************************************************************************
     * Adds an edge between two nodes with optional properties.
     * The nodes must already exist in the graph.
     ******************************************************************************/
    void addEdge(NodeId node1, NodeId node2, EdgeProperty property = EdgeProperty{})
    {
        OVITO_ASSERT(contains(node1));
        OVITO_ASSERT(contains(node2));
        OVITO_ASSERT(node1 != node2);  // No self-loops

        Edge edge(node1, node2);
        _edgeProperties[edge] = std::move(property);
        _adjacency[node1].insert(node2);
        _adjacency[node2].insert(node1);
    }

    /******************************************************************************
     * Removes a node and all its incident edges from the graph.
     ******************************************************************************/
    void removeNode(NodeId id)
    {
        if(!contains(id)) {
            return;
        }

        // Remove all edges incident to this node
        for(NodeId neighbor : _adjacency[id]) {
            _adjacency[neighbor].erase(id);
            Edge edge(id, neighbor);
            _edgeProperties.erase(edge);
        }

        _adjacency.erase(id);
        _nodeProperties.erase(id);
    }

    /******************************************************************************
     * Removes an edge from the graph.
     ******************************************************************************/
    void removeEdge(NodeId node1, NodeId node2)
    {
        Edge edge(node1, node2);
        _edgeProperties.erase(edge);

        auto it1 = _adjacency.find(node1);
        if(it1 != _adjacency.end()) {
            it1->second.erase(node2);
        }

        auto it2 = _adjacency.find(node2);
        if(it2 != _adjacency.end()) {
            it2->second.erase(node1);
        }
    }

    /******************************************************************************
     * Checks if a node exists in the graph.
     ******************************************************************************/
    [[nodiscard]] bool contains(NodeId id) const { return _nodeProperties.contains(id); }

    /******************************************************************************
     * Checks if an edge exists between two nodes.
     ******************************************************************************/
    [[nodiscard]] bool contains(NodeId node1, NodeId node2) const { return _edgeProperties.contains({node1, node2}); }

    /******************************************************************************
     * Returns the degree (number of neighbors) of a node.
     ******************************************************************************/
    [[nodiscard]] size_t degree(NodeId id) const
    {
        auto it = _adjacency.find(id);
        if(it == _adjacency.end()) return 0;
        return it->second.size();
    }

    /******************************************************************************
     * Returns the set of neighbors of a node.
     ******************************************************************************/
    [[nodiscard]] const std::unordered_set<NodeId>& neighbors(NodeId id) const
    {
        auto it = _adjacency.find(id);
        OVITO_ASSERT(it != _adjacency.end());
        return it->second;
    }

    /******************************************************************************
     * Returns a mutable reference to the property of a node.
     ******************************************************************************/
    NodeProperty& nodeProperty(NodeId id)
    {
        auto it = _nodeProperties.find(id);
        OVITO_ASSERT(it != _nodeProperties.end());
        return it->second;
    }

    /******************************************************************************
     * Returns a const reference to the property of a node.
     ******************************************************************************/
    [[nodiscard]] const NodeProperty& nodeProperty(NodeId id) const
    {
        auto it = _nodeProperties.find(id);
        OVITO_ASSERT(it != _nodeProperties.end());
        return it->second;
    }

    /******************************************************************************
     * Returns a mutable reference to the property of an edge.
     ******************************************************************************/
    EdgeProperty& edgeProperty(NodeId node1, NodeId node2)
    {
        Edge edge(node1, node2);
        auto it = _edgeProperties.find(edge);
        OVITO_ASSERT(it != _edgeProperties.end());
        return it->second;
    }

    /******************************************************************************
     * Returns a const reference to the property of an edge.
     ******************************************************************************/
    [[nodiscard]] const EdgeProperty& edgeProperty(NodeId node1, NodeId node2) const
    {
        Edge edge(node1, node2);
        auto it = _edgeProperties.find(edge);
        OVITO_ASSERT(it != _edgeProperties.end());
        return it->second;
    }

    /******************************************************************************
     * Returns the total number of nodes in the graph.
     ******************************************************************************/
    [[nodiscard]] size_t nodeCount() const { return _nodeProperties.size(); }

    /******************************************************************************
     * Returns the total number of edges in the graph.
     ******************************************************************************/
    [[nodiscard]] size_t edgeCount() const { return _edgeProperties.size(); }

    /******************************************************************************
     * Returns a vector of all node IDs in the graph.
     ******************************************************************************/
    [[nodiscard]] std::vector<NodeId> nodes() const
    {
        std::vector<NodeId> result;
        result.reserve(_nodeProperties.size());
        for(const auto& [id, _] : _nodeProperties) {
            result.push_back(id);
        }
        return result;
    }

    /******************************************************************************
     * Returns a vector of all edges in the graph.
     ******************************************************************************/
    [[nodiscard]] std::vector<Edge> edges() const
    {
        std::vector<Edge> result;
        result.reserve(_edgeProperties.size());
        for(const auto& [edge, _] : _edgeProperties) {
            result.push_back(edge);
        }
        return result;
    }

    /******************************************************************************
     * Clears all nodes and edges from the graph.
     ******************************************************************************/
    void clear()
    {
        _nodeProperties.clear();
        _edgeProperties.clear();
        _adjacency.clear();
    }

    /******************************************************************************
     * Creates a subgraph from this graph containing only the specified nodes.
     * All edges between the specified nodes are included in the subgraph.
     * Returns a shared pointer to ensure proper lifetime management.
     ******************************************************************************/
    template<typename Container>
    [[nodiscard]] std::shared_ptr<UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>> createSubgraph(const Container& nodeIds) const;

    /******************************************************************************
     * Finds all connected components in the graph and creates a subgraph for each.
     * Returns a vector of subgraphs, one per connected component.
     * For undirected graphs, connected components are equivalent to strongly
     * connected components.
     ******************************************************************************/
    [[nodiscard]] std::vector<std::shared_ptr<UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>>> createConnectedComponentSubgraphs()
        const;

private:
    /// Storage for node properties
    std::unordered_map<NodeId, NodeProperty> _nodeProperties;

    /// Storage for edge properties
    std::unordered_map<Edge, EdgeProperty, EdgeHash> _edgeProperties;

    /// Adjacency list: maps each node to its neighbors
    std::unordered_map<NodeId, std::unordered_set<NodeId>> _adjacency;
};

/******************************************************************************
 * A subgraph that references a parent graph and allows overriding node and edge properties.
 *
 * Subgraphs can be created from:
 * 1. A base graph - includes all edges between specified nodes
 * 2. Another subgraph - maintains topology, allows property overrides
 *
 * Both node and edge properties use copy-on-write: modifications create local copies
 * while unchanged properties are read from the parent chain. This allows efficient
 * property modifications in chained subgraphs without affecting parent graphs.
 *
 * Lifetime management: Uses shared_ptr to keep parent graphs alive.
 ******************************************************************************/
template<typename NodeIdType, typename NodeProperty = std::monostate, typename EdgePropertyType = std::monostate>
class UndirectedSubgraph : public std::enable_shared_from_this<UndirectedSubgraph<NodeIdType, NodeProperty, EdgePropertyType>>
{
public:
    using NodeId = NodeIdType;
    using EdgeProperty = EdgePropertyType;
    using Edge = typename UndirectedGraph<NodeId, NodeProperty, EdgeProperty>::Edge;
    using EdgeHash = typename UndirectedGraph<NodeId, NodeProperty, EdgeProperty>::EdgeHash;
    using GraphType = UndirectedGraph<NodeId, NodeProperty, EdgeProperty>;
    using SubgraphType = UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>;

    /******************************************************************************
     * Private constructor for creating a subgraph from a graph.
     ******************************************************************************/
    UndirectedSubgraph(std::shared_ptr<const GraphType> parent, const std::unordered_set<NodeId>& nodes)
        : _parentGraph(std::move(parent)), _nodes(nodes)
    {
        OVITO_ASSERT(_parentGraph);

        // Build adjacency list and edge set from parent graph
        for(NodeId node : _nodes) {
            OVITO_ASSERT(_parentGraph->contains(node));
            _adjacency[node] = std::unordered_set<NodeId>();
        }

        // Add all edges between nodes in the subgraph
        for(NodeId node1 : _nodes) {
            for(NodeId node2 : _parentGraph->neighbors(node1)) {
                if(_nodes.count(node2) > 0 && node1 < node2) {  // Add each edge only once
                    Edge edge(node1, node2);
                    _edges.insert(edge);
                    _adjacency[node1].insert(node2);
                    _adjacency[node2].insert(node1);
                }
            }
        }
    }

    /******************************************************************************
     * Private constructor for creating a subgraph from another subgraph.
     * Does not copy topology - reads directly from parent chain.
     ******************************************************************************/
    explicit UndirectedSubgraph(std::shared_ptr<const SubgraphType> parent) : _parentSubgraph(std::move(parent))
    {
        OVITO_ASSERT(_parentSubgraph);
        // Topology (nodes, edges, adjacency) is read from parent - no copying
    }

    /******************************************************************************
     * Private constructor for creating a filtered subgraph from another subgraph.
     * Creates a new topology with only the specified nodes and their edges.
     ******************************************************************************/
    UndirectedSubgraph(std::shared_ptr<const SubgraphType> parent, const std::unordered_set<NodeId>& nodes)
        : _parentSubgraph(std::move(parent)), _nodes(nodes)
    {
        OVITO_ASSERT(_parentSubgraph);

        // Build adjacency list and edge set from parent subgraph
        for(NodeId node : _nodes) {
            OVITO_ASSERT(_parentSubgraph->contains(node));
            _adjacency[node] = std::unordered_set<NodeId>();
        }

        // Add all edges between nodes in this subgraph that exist in parent
        for(NodeId node1 : _nodes) {
            for(NodeId node2 : _parentSubgraph->neighbors(node1)) {
                if(_nodes.count(node2) > 0 && node1 < node2) {  // Add each edge only once
                    Edge edge(node1, node2);
                    _edges.insert(edge);
                    _adjacency[node1].insert(node2);
                    _adjacency[node2].insert(node1);
                }
            }
        }
    }

public:
    /******************************************************************************
     * Creates a subgraph from a base graph with specified nodes.
     * All edges between the specified nodes are included.
     ******************************************************************************/
    template<typename Container>
    [[nodiscard]] static std::shared_ptr<SubgraphType> fromGraph(std::shared_ptr<const GraphType> parent, const Container& nodeIds)
    {
        std::unordered_set<NodeId> nodeSet(nodeIds.begin(), nodeIds.end());
        return std::shared_ptr<SubgraphType>(new SubgraphType(std::move(parent), nodeSet));
    }

    [[nodiscard]] static std::shared_ptr<SubgraphType> fromGraph(std::shared_ptr<const GraphType> parent,
                                                                 const std::unordered_set<NodeId>& nodeIds)
    {
        return std::shared_ptr<SubgraphType>(new SubgraphType(std::move(parent), nodeIds));
    }

    /******************************************************************************
     * Creates a subgraph from another subgraph, maintaining the same topology.
     * Allows overriding node and edge properties without modifying the parent.
     ******************************************************************************/
    [[nodiscard]] static std::shared_ptr<SubgraphType> fromSubgraph(std::shared_ptr<const SubgraphType> parent)
    {
        return std::shared_ptr<SubgraphType>(new SubgraphType(std::move(parent)));
    }

    /******************************************************************************
     * Returns the root graph (walks up the parent chain).
     ******************************************************************************/
    [[nodiscard]] const GraphType* rootGraph() const
    {
        if(_parentGraph) {
            return _parentGraph.get();
        }
        else {
            OVITO_ASSERT(_parentSubgraph);
            return _parentSubgraph->rootGraph();
        }
    }

    /******************************************************************************
     * Checks if a node exists in this subgraph.
     ******************************************************************************/
    [[nodiscard]] bool contains(NodeId id) const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->contains(id);
        }
        else {
            return _nodes.contains(id);
        }
    }

    /******************************************************************************
     * Checks if an edge exists in this subgraph.
     ******************************************************************************/
    [[nodiscard]] bool contains(NodeId node1, NodeId node2) const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->contains(node1, node2);
        }
        else {
            Edge edge(node1, node2);
            return _edges.contains(edge);
        }
    }

    /******************************************************************************
     * Returns the degree (number of neighbors) of a node in this subgraph.
     ******************************************************************************/
    [[nodiscard]] size_t degree(NodeId id) const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->degree(id);
        }
        else if(!_adjacency.empty()) {
            auto it = _adjacency.find(id);
            if(it == _adjacency.end()) return 0;
            return it->second.size();
        }
        else if(_parentGraph) {
            // Fallback to parent graph when local adjacency is not populated
            return _parentGraph->degree(id);
        }
        else {
            return 0;
        }
    }

    /******************************************************************************
     * Returns the set of neighbors of a node in this subgraph.
     ******************************************************************************/
    [[nodiscard]] const std::unordered_set<NodeId>& neighbors(NodeId id) const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->neighbors(id);
        }
        else if(!_adjacency.empty()) {
            auto it = _adjacency.find(id);
            OVITO_ASSERT(it != _adjacency.end());
            return it->second;
        }
        else {
            OVITO_ASSERT(_parentGraph);
            // Fallback to parent graph when local adjacency is not populated
            return _parentGraph->neighbors(id);
        }
    }

    /******************************************************************************
     * Returns a const reference to the property of a node.
     * Checks local overrides first, then walks up the parent chain.
     ******************************************************************************/
    [[nodiscard]] const NodeProperty& nodeProperty(NodeId id) const
    {
        OVITO_ASSERT(contains(id));

        // Check local overrides first
        auto it = _nodePropertyOverrides.find(id);
        if(it != _nodePropertyOverrides.end()) {
            return it->second;
        }

        // Walk up parent chain
        if(_parentSubgraph) {
            return _parentSubgraph->nodeProperty(id);
        }
        else {
            OVITO_ASSERT(_parentGraph);
            return _parentGraph->nodeProperty(id);
        }
    }

    /******************************************************************************
     * Returns a mutable reference to the property of a node.
     * Performs copy-on-write: if not locally overridden, copies from parent first.
     ******************************************************************************/
    NodeProperty& nodeProperty(NodeId id)
    {
        OVITO_ASSERT(contains(id));

        // Check if already overridden locally
        auto it = _nodePropertyOverrides.find(id);
        if(it != _nodePropertyOverrides.end()) {
            return it->second;
        }

        // Copy from parent (copy-on-write)
        NodeProperty parentProperty;
        if(_parentSubgraph) {
            parentProperty = _parentSubgraph->nodeProperty(id);
        }
        else {
            OVITO_ASSERT(_parentGraph);
            parentProperty = _parentGraph->nodeProperty(id);
        }

        // Store local copy and return reference
        auto [insertIt, _] = _nodePropertyOverrides.emplace(id, std::move(parentProperty));
        return insertIt->second;
    }

    /******************************************************************************
     * Sets the property of a node using copy-on-write semantics.
     * Only stores locally modified properties.
     ******************************************************************************/
    void setNodeProperty(NodeId id, NodeProperty property)
    {
        OVITO_ASSERT(contains(id));
        _nodePropertyOverrides[id] = std::move(property);
    }

    /******************************************************************************
     * Returns a const reference to the property of an edge.
     * Checks local overrides first, then walks up the parent chain.
     ******************************************************************************/
    [[nodiscard]] const EdgeProperty& edgeProperty(NodeId node1, NodeId node2) const
    {
        Edge edge(node1, node2);
        OVITO_ASSERT(contains(node1, node2));

        // Check local overrides first
        auto it = _edgePropertyOverrides.find(edge);
        if(it != _edgePropertyOverrides.end()) {
            return it->second;
        }

        // Walk up parent chain
        if(_parentSubgraph) {
            return _parentSubgraph->edgeProperty(node1, node2);
        }
        else {
            OVITO_ASSERT(_parentGraph);
            return _parentGraph->edgeProperty(node1, node2);
        }
    }

    /******************************************************************************
     * Uses copy-on-write semantics - only stores locally modified properties.
     ******************************************************************************/
    void setEdgeProperty(NodeId node1, NodeId node2, EdgeProperty property)
    {
        Edge edge(node1, node2);
        OVITO_ASSERT(contains(node1, node2));
        _edgePropertyOverrides[edge] = std::move(property);
    }

    /******************************************************************************
     * Returns a mutable reference to the property of an edge.
     * Performs copy-on-write: if not locally overridden, copies from parent first.
     ******************************************************************************/
    EdgeProperty& edgeProperty(NodeId node1, NodeId node2)
    {
        Edge edge(node1, node2);
        OVITO_ASSERT(contains(node1, node2));

        // Check if already overridden locally
        auto it = _edgePropertyOverrides.find(edge);
        if(it != _edgePropertyOverrides.end()) {
            return it->second;
        }

        // Copy from parent (copy-on-write)
        EdgeProperty parentProperty;
        if(_parentSubgraph) {
            parentProperty = _parentSubgraph->edgeProperty(node1, node2);
        }
        else {
            OVITO_ASSERT(_parentGraph);
            parentProperty = _parentGraph->edgeProperty(node1, node2);
        }

        // Store local copy and return reference
        auto [insertIt, _] = _edgePropertyOverrides.emplace(edge, std::move(parentProperty));
        return insertIt->second;
    }

    /******************************************************************************
     * Checks if an edge property has been overridden in this subgraph.
     ******************************************************************************/
    [[nodiscard]] bool hasEdgePropertyOverride(NodeId node1, NodeId node2) const
    {
        Edge edge(node1, node2);
        return _edgePropertyOverrides.contains(edge);
    }

    /******************************************************************************
     * Removes a local edge property override, reverting to parent's property.
     ******************************************************************************/
    void clearEdgePropertyOverride(NodeId node1, NodeId node2)
    {
        Edge edge(node1, node2);
        _edgePropertyOverrides.erase(edge);
    }

    /******************************************************************************
     * Clears all edge property overrides in this subgraph.
     ******************************************************************************/
    void clearAllEdgePropertyOverrides() { _edgePropertyOverrides.clear(); }

    /******************************************************************************
     * Checks if a node property has been overridden in this subgraph.
     ******************************************************************************/
    [[nodiscard]] bool hasNodePropertyOverride(NodeId id) const { return _nodePropertyOverrides.contains(id); }

    /******************************************************************************
     * Removes a local node property override, reverting to parent's property.
     ******************************************************************************/
    void clearNodePropertyOverride(NodeId id) { _nodePropertyOverrides.erase(id); }

    /******************************************************************************
     * Clears all node property overrides in this subgraph.
     ******************************************************************************/
    void clearAllNodePropertyOverrides() { _nodePropertyOverrides.clear(); }

    /******************************************************************************
     * Returns the total number of nodes in the subgraph.
     ******************************************************************************/
    [[nodiscard]] size_t nodeCount() const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->nodeCount();
        }
        else {
            OVITO_ASSERT(_parentGraph || !_nodes.empty());
            return _nodes.size();
        }
    }

    /******************************************************************************
     * Returns the total number of edges in the subgraph.
     ******************************************************************************/
    [[nodiscard]] size_t edgeCount() const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->edgeCount();
        }
        else {
            OVITO_ASSERT(_parentGraph || !_edges.empty());
            return _edges.size();
        }
    }

    /******************************************************************************
     * Returns a vector of all node IDs in the subgraph.
     ******************************************************************************/
    [[nodiscard]] const std::unordered_set<NodeId>& nodes() const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->nodes();
        }
        else {
            OVITO_ASSERT(_parentGraph || !_nodes.empty());
            return _nodes;
        }
    }

    /******************************************************************************
     * Returns a vector of all edges in the subgraph.
     ******************************************************************************/
    [[nodiscard]] const std::unordered_set<Edge, EdgeHash>& edges() const
    {
        if(_parentSubgraph) {
            return _parentSubgraph->edges();
        }
        else {
            OVITO_ASSERT(_parentGraph || !_edges.empty());
            return _edges;
        }
    }

    /******************************************************************************
     * Creates a child subgraph from this subgraph.
     * Maintains the same topology, allows further node and edge property overrides.
     ******************************************************************************/
    [[nodiscard]] std::shared_ptr<SubgraphType> createSubgraph() const
    {
        // Graph should only be used as a shared_ptr to ensure proper lifetime management
        OVITO_ASSERT(!this->weak_from_this().expired());

        // Try to use shared_from_this() if this object is owned by a shared_ptr
        std::shared_ptr<const SubgraphType> parentPtr;
        try {
            parentPtr = this->shared_from_this();
        }
        catch(const std::bad_weak_ptr&) {
            // Object is not owned by shared_ptr, create a non-owning shared_ptr
            // WARNING: Caller must ensure the subgraph outlives all child subgraphs!
            parentPtr = std::shared_ptr<const SubgraphType>(this, [](const SubgraphType*) {});
        }
        return fromSubgraph(parentPtr);
    }

    [[nodiscard]] std::shared_ptr<SubgraphType> copy() const { return createSubgraph(); }

    /******************************************************************************
     * Creates a child subgraph containing only a subset of nodes from this subgraph.
     * All edges between the specified nodes are included.
     * The new subgraph is a child of this subgraph, maintaining the parent-child relationship.
     ******************************************************************************/
    template<typename Container>
    [[nodiscard]] std::shared_ptr<SubgraphType> createSubgraph(const Container& nodeIds) const;

    friend class UndirectedGraph<NodeId, NodeProperty, EdgeProperty>;

private:
    /// Parent graph or subgraph (keeps it alive via shared ownership)
    std::shared_ptr<const GraphType> _parentGraph;
    std::shared_ptr<const SubgraphType> _parentSubgraph;

    /// Set of nodes in this subgraph (only set when created from graph)
    std::unordered_set<NodeId> _nodes;

    /// Set of edges in this subgraph (only set when created from graph)
    std::unordered_set<Edge, EdgeHash> _edges;

    /// Adjacency list for this subgraph (only set when created from graph)
    std::unordered_map<NodeId, std::unordered_set<NodeId>> _adjacency;

    /// Local overrides for edge properties (copy-on-write)
    std::unordered_map<Edge, EdgeProperty, EdgeHash> _edgePropertyOverrides;

    /// Local overrides for node properties (copy-on-write)
    std::unordered_map<NodeId, NodeProperty> _nodePropertyOverrides;
};

/******************************************************************************
 * Implementation of UndirectedGraph::createSubgraph
 ******************************************************************************/
template<typename NodeId, typename NodeProperty, typename EdgeProperty>
template<typename Container>
std::shared_ptr<UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>> UndirectedGraph<NodeId, NodeProperty, EdgeProperty>::createSubgraph(
    const Container& nodeIds) const
{
    using GraphType = UndirectedGraph<NodeId, NodeProperty, EdgeProperty>;
    // Graph should only be used as a shared_ptr to ensure proper lifetime management
    OVITO_ASSERT(!this->weak_from_this().expired());

#if 0
    // Try to use shared_from_this() if this object is owned by a shared_ptr
    // Otherwise, the caller must ensure the graph outlives the subgraph
    std::shared_ptr<const GraphType> parentPtr;
    try {
        parentPtr = this->shared_from_this();
    } catch(const std::bad_weak_ptr&) {
        // Object is not owned by shared_ptr, create a non-owning shared_ptr
        // WARNING: Caller must ensure the graph outlives all subgraphs!
        parentPtr = std::shared_ptr<const GraphType>(this, [](const GraphType*) {});
    }
    return UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>::fromGraph(parentPtr, nodeIds);
#else
    return fromSubgraph(this->shared_from_this());
#endif
}

/******************************************************************************
 * Implementation of UndirectedGraph::createConnectedComponentSubgraphs
 ******************************************************************************/
template<typename NodeId, typename NodeProperty, typename EdgeProperty>
std::vector<std::shared_ptr<UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>>>
UndirectedGraph<NodeId, NodeProperty, EdgeProperty>::createConnectedComponentSubgraphs() const
{
    using GraphType = UndirectedGraph<NodeId, NodeProperty, EdgeProperty>;
    using SubgraphType = UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>;

    // Graph should only be used as a shared_ptr to ensure proper lifetime management
    OVITO_ASSERT(!this->weak_from_this().expired());

    std::vector<std::shared_ptr<SubgraphType>> components;
    std::unordered_set<NodeId> visited;

    // Iterate through all nodes to find connected components
    for(const auto& [nodeId, _] : _nodeProperties) {
        if(visited.count(nodeId) > 0) {
            continue;  // Already part of a component
        }

        // BFS to find all nodes in this connected component
        std::vector<NodeId> componentNodes;
        std::vector<NodeId> queue;
        queue.push_back(nodeId);
        visited.insert(nodeId);

        while(!queue.empty()) {
            NodeId current = queue.back();
            queue.pop_back();
            componentNodes.push_back(current);

            // Add all unvisited neighbors to the queue
            auto adjIt = _adjacency.find(current);
            if(adjIt != _adjacency.end()) {
                for(NodeId neighbor : adjIt->second) {
                    if(visited.count(neighbor) == 0) {
                        visited.insert(neighbor);
                        queue.push_back(neighbor);
                    }
                }
            }
        }

#if 0
        // Create a subgraph for this connected component
        // Try to use shared_from_this() if this object is owned by a shared_ptr
        std::shared_ptr<const GraphType> parentPtr;
        try {
            parentPtr = this->shared_from_this();
        } catch(const std::bad_weak_ptr&) {
            // Object is not owned by shared_ptr, create a non-owning shared_ptr
            // WARNING: Caller must ensure the graph outlives all subgraphs!
            parentPtr = std::shared_ptr<const GraphType>(this, [](const GraphType*) {});
        }
        auto subgraph = UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>::fromGraph(
            parentPtr, componentNodes);
#else
        auto subgraph = UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>::fromGraph(this->shared_from_this(), componentNodes);
#endif

        components.push_back(std::move(subgraph));
    }

    return components;
}

/******************************************************************************
 * Implementation of UndirectedSubgraph::createSubgraph with filtered nodes
 ******************************************************************************/
template<typename NodeId, typename NodeProperty, typename EdgeProperty>
template<typename Container>
std::shared_ptr<UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>>
UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>::createSubgraph(const Container& nodeIds) const
{
    using SubgraphType = UndirectedSubgraph<NodeId, NodeProperty, EdgeProperty>;

    // Graph should only be used as a shared_ptr to ensure proper lifetime management
    OVITO_ASSERT(!this->weak_from_this().expired());

    // Convert container to unordered_set and verify all nodes exist
    std::unordered_set<NodeId> filteredNodes;
    for(const NodeId& id : nodeIds) {
        OVITO_ASSERT(contains(id));
        filteredNodes.insert(id);
    }

    // Create a new subgraph with filtered nodes as a child of this subgraph
    // Try to use shared_from_this() if this object is owned by a shared_ptr
    std::shared_ptr<const SubgraphType> parentPtr;
    try {
        parentPtr = this->shared_from_this();
    }
    catch(const std::bad_weak_ptr&) {
        // Object is not owned by shared_ptr, create a non-owning shared_ptr
        // WARNING: Caller must ensure the subgraph outlives all child subgraphs!
        parentPtr = std::shared_ptr<const SubgraphType>(this, [](const SubgraphType*) {});
    }
    return std::shared_ptr<SubgraphType>(new SubgraphType(parentPtr, filteredNodes));
}

}  // namespace Ovito
