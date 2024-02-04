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
#include <ovito/delaunay/DelaunayTessellation.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include "StructureAnalysis.h"
#include "ElasticMapping.h"
#include "InterfaceMesh.h"
#include "DislocationTracer.h"

namespace Ovito {

/*
 * Computation engine of the DislocationAnalysisModifier, which performs the actual dislocation analysis.
 */
class DislocationAnalysisEngine : public StructureIdentificationModifier::StructureIdentificationEngine
{
public:

    /// Constructor.
    DislocationAnalysisEngine(const ModifierEvaluationRequest& request, ElementOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell,
            const OORefVector<ElementType>& structureTypes, int inputCrystalStructure, int maxTrialCircuitSize, int maxCircuitElongation,
            ConstPropertyPtr particleSelection,
            ConstPropertyPtr crystalClusters,
            std::vector<Matrix3> preferredCrystalOrientations,
            bool onlyPerfectDislocations, int defectMeshSmoothingLevel, DataOORef<SurfaceMesh> defectMesh, DataOORef<SurfaceMesh> outputInterfaceMesh,
            int lineSmoothingLevel, FloatType linePointInterval);

    /// Computes the modifier's results and stores them in this object for later retrieval.
    virtual void perform() override;

    /// Injects the computed results into the data pipeline.
    virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Returns the array of atom cluster IDs.
    const PropertyPtr& atomClusters() const { return _atomClusters; }

    /// Assigns the array of atom cluster IDs.
    void setAtomClusters(PropertyPtr prop) { _atomClusters = std::move(prop); }

    /// Returns the created cluster graph.
    const std::shared_ptr<ClusterGraph>& clusterGraph() { return _clusterGraph; }

    /// Sets the created cluster graph.
    void setClusterGraph(std::shared_ptr<ClusterGraph> graph) { _clusterGraph = std::move(graph); }

    /// Returns the extracted dislocations.
    const std::shared_ptr<DislocationNetwork>& dislocationNetwork() const { return _dislocationNetwork; }

    /// Sets the extracted dislocations.
    void setDislocationNetwork(std::shared_ptr<DislocationNetwork> network) { _dislocationNetwork = std::move(network); }

    /// Returns the total volume of the input simulation cell.
    FloatType simCellVolume() const { return _simCellVolume; }

    /// Returns the computed interface mesh.
    const InterfaceMesh& interfaceMesh() const { return *_interfaceMesh; }

    /// Gives access to the elastic mapping computation engine.
    ElasticMapping& elasticMapping() { return *_elasticMapping; }

    /// Returns the input particle property that stores the cluster assignment of atoms.
    const ConstPropertyPtr& crystalClusters() const { return _crystalClusters; }

    /// Computes statistical information on the identified dislocation lines and outputs it to the pipeline as data tables and global attributes.
    static FloatType generateDislocationStatistics(const OOWeakRef<const PipelineNode>& pipelineNode, PipelineFlowState& state, DislocationNetworkObject* dislocationsObj, bool replaceDataObjects, const MicrostructurePhase* defaultStructure);

private:

    int _inputCrystalStructure;
    bool _onlyPerfectDislocations;
    int _defectMeshSmoothingLevel;
    int _lineSmoothingLevel;
    FloatType _linePointInterval;
    std::unique_ptr<StructureAnalysis> _structureAnalysis;
    std::unique_ptr<DelaunayTessellation> _tessellation;
    std::unique_ptr<ElasticMapping> _elasticMapping;
    std::unique_ptr<InterfaceMesh> _interfaceMesh;
    std::unique_ptr<DislocationTracer> _dislocationTracer;
    ConstPropertyPtr _crystalClusters;

    /// The defect mesh produced by the modifier.
    DataOORef<SurfaceMesh> _defectMesh;

    /// This stores the interface mesh produced by the modifier for visualization purposes.
    DataOORef<SurfaceMesh> _outputInterfaceMesh;

    /// This stores the cached atom-to-cluster assignments computed by the modifier.
    PropertyPtr _atomClusters;

    /// This stores the cached cluster graph computed by the modifier.
    std::shared_ptr<ClusterGraph> _clusterGraph;

    /// This stores the cached dislocations computed by the modifier.
    std::shared_ptr<DislocationNetwork> _dislocationNetwork;

    /// The total volume of the input simulation cell.
    FloatType _simCellVolume;
};

}   // End of namespace
