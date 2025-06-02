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
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/lines/Lines.h>
#include "StructureAnalysis.h"
#include "ElasticMapping2.h"
#include "InterfaceMesh.h"
#include "DislocationTracer.h"

namespace Ovito {

/*
 * Computation engine of the DislocationAnalysisModifier, which performs the actual dislocation analysis.
 */
class DislocationAnalysisEngine2 : public StructureIdentificationModifier::Algorithm
{
public:
    /// Constructor.
    DislocationAnalysisEngine2(PropertyPtr structures, size_t particleCount, int inputCrystalStructure,
                              ConstPropertyPtr particleSelection, ConstPropertyPtr crystalClusters,
                              std::vector<Matrix3> preferredCrystalOrientations,
                              DataOORef<Lines> dislocationSegments, DataOORef<Lines> unassignedEdges, DataOORef<SurfaceMesh> interfaceMesh);

    /// Performs the atomic structure classification.
    virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) override;

    /// Computes the structure identification statistics.
    virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state,
                                                            const OOWeakRef<const PipelineNode>& createdByNode,
                                                            const std::any& modifierParameters) const override;

    /// Returns the array of atom cluster IDs.
    const PropertyPtr& atomClusters() const { return _atomClusters; }

    /// Assigns the array of atom cluster IDs.
    void setAtomClusters(PropertyPtr prop) { _atomClusters = std::move(prop); }

    /// Returns the cluster graph.
    ClusterGraph* clusterGraph() const { return _clusterGraph; }

    /// Returns the total volume of the input simulation cell.
    FloatType simulationCellVolume() const { return _simulationCellVolume; }

    /// Returns the Delaunay tessellation.
    const DelaunayTessellation& tessellation() const { OVITO_ASSERT(_tessellation.has_value()); return *_tessellation; }

    /// Gives access to the elastic mapping computation engine.
    ElasticMapping2& elasticMapping() { OVITO_ASSERT(_elasticMapping.has_value()); return *_elasticMapping; }

    /// Returns the input particle property that stores the cluster assignment of atoms.
    const ConstPropertyPtr& crystalClusters() const { return _crystalClusters; }

private:

    /// Marks atoms that are part of perfect crystalline regions and can be omitted from the Delaunay construction.
    PropertyPtr markPerfectCrystallineRegions(TaskProgress& progress, const Property* selection);

    /// Extracts the dislocation lines segments from the elastic mapping.
    void extractDislocationSegments(TaskProgress& progress, PropertyFactory<Point3>& linePosition1Access,
                                    PropertyFactory<Point3>& linePosition2Access, PropertyFactory<Cluster::VecType>& burgersVectorAccess, PropertyFactory<ColorG>& lineColorAccess,
                                    PropertyFactory<int>& stageAccess, int stage);

    void extractInterfaceMesh(FloatType alpha, int stage);

private:
    int _inputCrystalStructure;
    int _minClusterSize = 5; // Minimum number of atoms in crystal clusters to be considered for dislocation analysis.
    std::vector<Matrix3> _preferredCrystalOrientations;
    std::optional<StructureAnalysis> _structureAnalysis;
    std::optional<DelaunayTessellation> _tessellation;
    std::optional<ElasticMapping2> _elasticMapping;
    ConstPropertyPtr _crystalClusters;

    DataOORef<ClusterGraph> _clusterGraph;

    /// This stores the atom-to-cluster assignments computed by the modifier.
    PropertyPtr _atomClusters;

    PropertyPtr _delaunayAtomBuffer;

    /// The dislocation line segments computed by the modifier.
    DataOORef<Lines> _dislocationSegments;

    /// The unassigned Delaunay edges.
    DataOORef<Lines> _unassignedEdges;

    /// The interface mesh computed by the algorithm.
    DataOORef<SurfaceMesh> _interfaceMesh;

    /// The total volume of the input simulation cell.
    /// This is used to compute the dislocation density.
    FloatType _simulationCellVolume;
};

}  // namespace Ovito
