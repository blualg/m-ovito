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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "ClusterVector.h"

namespace Ovito {

struct BurgersCircuit;          // defined in BurgersCircuit.h

/**
 * Every dislocation line is delimited by two dislocation nodes.
 */
struct OVITO_CRYSTALANALYSIS_EXPORT DislocationNode
{
    /// The dislocation line delimited by this node.
    DislocationLine* line;

    /// The opposite node of this node's dislocation line.
    DislocationNode* oppositeNode;

    /// Pointer to the next node in the circular linked list of nodes that form a dislocation network junction.
    /// If this node is not part of a junction, i.e., if it is a dangling node, then this points to the node itself.
    DislocationNode* junctionRing = this;

    /// The Burgers circuit associated with this node.
    /// This field is only valid during the line tracing process.
    BurgersCircuit* circuit = nullptr;

    /// Returns the (signed) Burgers vector of the node.
    /// This is the Burgers vector of the segment if this node is a forward node,
    /// or the negative Burgers vector if this node is a backward node.
    inline ClusterVector burgersVector() const;

    /// Returns the position of the node by looking up the coordinates of the
    /// start or end point of the dislocation segment to which the node belongs.
    inline const Point3& position() const;

    /// Returns true if this node is the forward node of its line, that is,
    /// when it is at the end of the associated dislocation line.
    inline bool isForwardNode() const;

    /// Returns true if this node is the backward node of its line, that is,
    /// when it is at the beginning of the associated dislocation line.
    inline bool isBackwardNode() const;

    /// Determines whether the given node forms a junction with the given node.
    bool formsJunctionWith(DislocationNode* other) const {
        DislocationNode* n = this->junctionRing;
        do {
            if(other == n) return true;
            n = n->junctionRing;
        }
        while(n != this->junctionRing);
        return false;
    }

    /// Makes two nodes part of a junction.
    /// If any of the two nodes were already part of a junction, then
    /// a single junction is created that encompasses all nodes.
    void connectNodes(DislocationNode* other) {
        OVITO_ASSERT(!other->formsJunctionWith(this));
        OVITO_ASSERT(!this->formsJunctionWith(other));

        DislocationNode* tempStorage = junctionRing;
        junctionRing = other->junctionRing;
        other->junctionRing = tempStorage;

        OVITO_ASSERT(other->formsJunctionWith(this));
        OVITO_ASSERT(this->formsJunctionWith(other));
    }

    /// If this node is part of a junction, dissolves the junction.
    /// The nodes of all junction arms will become dangling nodes.
    void dissolveJunction() {
        DislocationNode* n = this->junctionRing;
        while(n != this) {
            DislocationNode* next = n->junctionRing;
            n->junctionRing = n;
            n = next;
        }
        this->junctionRing = this;
    }

    /// Counts the number of arms belonging to the junction.
    int countJunctionArms() const {
        int armCount = 1;
        for(DislocationNode* armNode = this->junctionRing; armNode != this; armNode = armNode->junctionRing)
            armCount++;
        return armCount;
    }

    /// Return whether the end of a dislocation line, represented by this node, does not merge into a junction.
    bool isDangling() const {
        return (junctionRing == this);
    }
};

/**
 * A single dislocation line.
 *
 * Each dislocation has a Burgers vector and consists of a piecewise-linear curve in space.
 * The line is delimited by a dislocation node at each end, which serve as connection points
 * to form dislocation networks.
 */
struct DislocationLine
{
    /// The unique identifier of the dislocation line.
    int id;

    /// The points forming the piecewise linear curve.
    std::deque<Point3> vertices;

    /// Stores the circumference of the dislocation core at every sampling point along the line.
    /// This information is used to coarsen the sampling point array adaptively since a large
    /// core size leads to a high sampling rate.
    std::deque<int> coreSize;

    /// The Burgers vector of the dislocation line. It is expressed in the coordinate system of
    /// the crystal cluster which the line is embedded in.
    ClusterVector burgersVector;

    /// The two nodes that delimit the line.
    std::array<DislocationNode*, 2> nodes;

    /// The parent line if this line has been joined to another.
    DislocationLine* replacedWith = nullptr;

    /// A user-defined color assigned to the dislocation line.
    Color customColor = Color(-1, -1, -1);

    /// Constructs a new dislocation line with the given Burgers vector, connecting two dislocation nodes.
    DislocationLine(const ClusterVector& b, DislocationNode* forwardNode, DislocationNode* backwardNode) : burgersVector(b) {
        OVITO_ASSERT(!b.localVec().isZero());
        nodes[0] = forwardNode;
        nodes[1] = backwardNode;
        forwardNode->line = this;
        backwardNode->line = this;
        forwardNode->oppositeNode = backwardNode;
        backwardNode->oppositeNode = forwardNode;
    }

    /// Returns the forward-pointing node at the end of the dislocation line.
    DislocationNode& forwardNode() const { return *nodes[0]; }

    /// Returns the backward-pointing node at the start of the dislocation line.
    DislocationNode& backwardNode() const { return *nodes[1]; }

    /// Returns true if this line forms a closed loop, that is, when its two nodes form a single 2-junction.
    /// Note that an infinite dislocation line, passing through a periodic boundary, is also considered a loop.
    bool isClosedLoop() const {
        OVITO_ASSERT(nodes[0] && nodes[1]);
        return (nodes[0]->junctionRing == nodes[1]) && (nodes[1]->junctionRing == nodes[0]);
    }

    /// Returns true if this dislocation is an infinite dislocation line passing through a periodic boundary.
    /// A dislocation is considered infinite if it is a closed loop but its start and end points do not coincide.
    bool isInfiniteLine() const {
        return isClosedLoop() && vertices.back().equals(vertices.front(), CA_ATOM_VECTOR_EPSILON) == false;
    }

    /// Calculates the line length of the dislocation line.
    FloatType calculateLength() const {
        OVITO_ASSERT(!isDegenerate());

        FloatType length = 0;
        auto i1 = vertices.begin();
        for(;;) {
            auto i2 = i1 + 1;
            if(i2 == vertices.end()) break;
            length += (*i1 - *i2).length();
            i1 = i2;
        }
        return length;
    }

    /// Returns true if this line consists of less than two points.
    bool isDegenerate() const { return vertices.size() <= 1; }

    /// Reverses the direction of the line.
    /// This flips both the line sense and the dislocation's Burgers vector.
    void flipOrientation() {
        burgersVector = -burgersVector;
        std::swap(nodes[0], nodes[1]);
        std::reverse(vertices.begin(), vertices.end());
        std::reverse(coreSize.begin(), coreSize.end());
    }

    /// Computes a location along the dislocation line.
    Point3 getPointOnLine(FloatType t) const
    {
        if(vertices.empty())
            return Point3::Origin();

        t *= calculateLength();

        FloatType sum = 0;
        auto i1 = vertices.begin();
        for(;;) {
            auto i2 = i1 + 1;
            if(i2 == vertices.end()) break;
            Vector3 delta = *i2 - *i1;
            FloatType len = delta.length();
            if(sum + len >= t && len != 0) {
                return *i1 + (((t - sum) / len) * delta);
            }
            sum += len;
            i1 = i2;
        }

        return vertices.back();
    };

    /// Get the fully replaced dislocation ID (following successive replacements)
    int replacedId() const {
        DislocationLine* currentLine = replacedWith;
        int currentId = id;
        while(currentLine) {
            currentId = currentLine->id;
            currentLine = currentLine->replacedWith;
        };
        return currentId;
    }
};

/// Returns true if this node is the forward node of its dislocation, that is,
/// when it is at the end of the associated dislocation line.
inline bool DislocationNode::isForwardNode() const
{
    return &line->forwardNode() == this;
}

/// Returns true if this node is the backward node of its dislocation, that is,
/// when it is at the beginning of the associated dislocation line.
inline bool DislocationNode::isBackwardNode() const
{
    return &line->backwardNode() == this;
}

/// Returns the (signed) Burgers vector of the node.
/// This is the Burgers vector of the dislocation if this node is a forward node,
/// or the negative Burgers vector if this node is a backward node.
inline ClusterVector DislocationNode::burgersVector() const
{
    if(isForwardNode())
        return line->burgersVector;
    else
        return -line->burgersVector;
}

/// Returns the position of the node by looking up the coordinates of the
/// start or end point of the dislocation line to which the node belongs.
inline const Point3& DislocationNode::position() const
{
    if(isForwardNode())
        return line->vertices.back();
    else
        return line->vertices.front();
}

}   // End of namespace
