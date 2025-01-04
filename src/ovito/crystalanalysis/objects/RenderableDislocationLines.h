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
#include <ovito/crystalanalysis/objects/ClusterVector.h>

namespace Ovito {

/**
 * \brief A non-periodic version of the dislocation lines that is generated from a periodic DislocationNetworkObject.
 */
class OVITO_CRYSTALANALYSIS_EXPORT RenderableDislocationLines
{
public:

    /// A linear segment of a dislocation line.
    struct Segment
    {
        /// The two vertices of the segment.
        std::array<Point3, 2> verts;

        /// The Burgers vector of the segment.
        Vector3 burgersVector;

        /// The crystallite the dislocation segment is embedded in.
        int region;

        /// Identifies the original dislocation line this segment is part of.
        int dislocationIndex;

        /// Equal comparison operator.
        bool operator==(const Segment& other) const { return verts == other.verts && dislocationIndex == other.dislocationIndex && burgersVector == other.burgersVector && region == other.region; }
    };

    /// Constructor.
    RenderableDislocationLines(std::vector<Segment> lineSegments, DataOORef<const ClusterGraph> clusterGraph) :
        _lineSegments(std::move(lineSegments)), _clusterGraph(std::move(clusterGraph)) {}

    /// Returns the list of clipped and wrapped line segments.
    const std::vector<Segment>& lineSegments() const { return _lineSegments; }

    /// Returns the cluster graph.
    const DataOORef<const ClusterGraph>& clusterGraph() const { return _clusterGraph; }

private:

    /// The list of clipped and wrapped line segments.
    std::vector<Segment> _lineSegments;

    /// The associated cluster graph.
    DataOORef<const ClusterGraph> _clusterGraph;
};

}   // End of namespace
