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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/crystalanalysis/objects/ClusterGraph.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
#include "DislocationAnalysisEngine2.h"
#include "DislocationAnalysisModifier.h"

namespace Ovito {

/******************************************************************************
 * Constructor.
 ******************************************************************************/
DislocationAnalysisEngine2::DislocationAnalysisEngine2(PropertyPtr structures, size_t particleCount, int inputCrystalStructure,
                                                     ConstPropertyPtr particleSelection,
                                                     ConstPropertyPtr crystalClusters, std::vector<Matrix3> preferredCrystalOrientations,
                                                     DataOORef<DislocationNetwork> dislocationNetwork,
                                                     DataOORef<Lines> dislocationSegments,
                                                     int lineSmoothingLevel, FloatType linePointInterval)
    : StructureIdentificationModifier::Algorithm(std::move(structures)),
      _inputCrystalStructure(inputCrystalStructure),
      _lineSmoothingLevel(lineSmoothingLevel),
      _linePointInterval(linePointInterval),
      _preferredCrystalOrientations(std::move(preferredCrystalOrientations)),
      _crystalClusters(std::move(crystalClusters)),
      _dislocationNetwork(std::move(dislocationNetwork)),
      _dislocationSegments(std::move(dislocationSegments))
{
}

/******************************************************************************
 * Performs the structural analysis of the input particle positions.
 ******************************************************************************/
void DislocationAnalysisEngine2::identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection)
{
    if(!simulationCell)
        throw Exception(DislocationAnalysisModifier::tr("DXA requires a simulation cell to be defined."));
    if(simulationCell->is2D())
        throw Exception(DislocationAnalysisModifier::tr("DXA does not support 2d simulations."));

    // Set up progress reporting.
    TaskProgress progress(this_task::ui());
    progress.setText(DislocationAnalysisModifier::tr("Dislocation analysis (DXA)"));

    // Store the simulation cell volume, which will be used later to compute the dislocation density.
    _simulationCellVolume = simulationCell->volume3D();

    const Property* positions = particles->expectProperty(Particles::PositionProperty);
    _structureAnalysis.emplace(positions, simulationCell, (StructureAnalysis::LatticeStructureType)_inputCrystalStructure, selection,
                               const_cast<ClusterGraph*>(_dislocationNetwork->clusterGraph()), structures(),
                               std::move(_preferredCrystalOrientations));
    _tessellation.emplace();
    _elasticMapping.emplace(*_structureAnalysis, *_tessellation);
    setAtomClusters(_structureAnalysis->atomClusters());
    _structureAnalysis->identifyStructures(progress, simulationCell);
    _structureAnalysis->buildClusters(progress);
    _structureAnalysis->connectClusters(progress);

    FloatType ghostLayerSize = FloatType(3.5) * _structureAnalysis->maximumNeighborDistance();
    _tessellation->generateTessellation(simulationCell, BufferReadAccess<Point3>(positions).cbegin(),
                                        _structureAnalysis->atomCount(), ghostLayerSize,
                                        false,  // flag coverDomainWithFiniteTets
                                        selection ? BufferReadAccess<SelectionIntType>(selection).cbegin() : nullptr,
                                        progress);

    // Build a list of Delaunay edges.
    _elasticMapping->generateTessellationEdges(progress);

    // Assign each atom to a crystal cluster.
    _elasticMapping->assignAtomsToClusters(progress);

    // Assign ideal lattice vectors to the edges of the Delaunay tessellation.
    _elasticMapping->assignIdealVectorsToEdges(4, progress);

    // Free some memory that is no longer needed.
    _structureAnalysis->freeNeighborLists();

    // Extract dislocation line segments.
    extractDislocationSegments(progress);

    // Post-process dislocation lines.
    if(_lineSmoothingLevel > 0 || _linePointInterval > 0)
        dislocationNetwork()->smoothDislocationLines(_lineSmoothingLevel, _linePointInterval, progress);

    // Release data that is no longer needed.
    _structureAnalysis.reset();
    _tessellation.reset();
    _elasticMapping.reset();
    _crystalClusters.reset();
}

/******************************************************************************
 * Computes the structure identification statistics.
 ******************************************************************************/
std::vector<int64_t> DislocationAnalysisEngine2::computeStructureStatistics(const Property* structures, PipelineFlowState& state,
                                                                           const OOWeakRef<const PipelineNode>& createdByNode,
                                                                           const std::any& modifierParameters) const
{
    std::vector<int64_t> typeCounts = StructureIdentificationModifier::Algorithm::computeStructureStatistics(structures, state, createdByNode, modifierParameters);

    // Output dislocations.
    while(!dislocationNetwork()->crystalStructures().empty())
        dislocationNetwork()->removeCrystalStructure(dislocationNetwork()->crystalStructures().size() - 1);
    for(const ElementType* stype : structures->elementTypes())
        dislocationNetwork()->addCrystalStructure(static_object_cast<MicrostructurePhase>(stype));
#if 0
    state.addObject(dislocationNetwork());
#endif

    // Output dislocation segments.
    state.addObject(_dislocationSegments);

    // Output particle properties.
    if(atomClusters()) {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(atomClusters());
    }

    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.OTHER"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_OTHER)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.FCC"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_FCC)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HCP"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_HCP)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.BCC"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_BCC)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.CubicDiamond"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_CUBIC_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HexagonalDiamond"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_HEX_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.cell_volume"), QVariant::fromValue(simulationCellVolume()), createdByNode);

    return typeCounts;
}

/******************************************************************************
 * Extracts the dislocation lines segments from the elastic mapping.
 ******************************************************************************/
void DislocationAnalysisEngine2::extractDislocationSegments(TaskProgress& progress)
{
    OVITO_ASSERT(_dislocationSegments->elementCount() == 0);
    OVITO_ASSERT(_dislocationSegments->properties().empty());

    for(DelaunayTessellation::CellHandle cell : _tessellation->cells()) {
        if(!_tessellation->isGhostCell(cell))
            continue;
        for(int face = 0; face < 4; face++) {

        }
    }
}

}  // namespace Ovito
