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

    /// Guess ideal vectors for those edges of the Delaunay tessellation,
    /// which are not yet assigned a vector.
    _elasticMapping->complementUnassignedEdges(progress);

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

    PropertyFactory<Point3> linePosition1Access(Lines::OOClass(), 0, Lines::Position1Property);
    PropertyFactory<Point3> linePosition2Access(Lines::OOClass(), 0, Lines::Position2Property);

    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        if(tessellation().isGhostCell(cell))
            continue;

        // Get the four Delaunay vertices of the current tetrahedral cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);

        // Retrieve the lattice vectors assigned to the six edges of the tetrahedron.
        EdgeVector edgeVectors[6];
        for(int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
            edgeVectors[edgeIndex] = elasticMapping().getEdgeClusterVector(cellVertices, edgeIndex);

        // Perform Burgers circuit test on each of the four faces of the tetrahedron.
        for(int face = 0; face < 4; face++) {
            const EdgeVector& e1 = edgeVectors[ElasticMapping::CellEdgeCircuits[face][0]];
            const EdgeVector& e2 = edgeVectors[ElasticMapping::CellEdgeCircuits[face][1]];
            const EdgeVector& e3 = edgeVectors[ElasticMapping::CellEdgeCircuits[face][2]];
            if(!e1.isValid() || !e2.isValid() || !e3.isValid())
                continue; // One of the edges has no valid cluster vector.

            // Perform Burgers circuit test on the face.
            // Calculate b = e1 + e2 - e3.
            // Third edge must be flipped, because it's oriented in the opposite direction.
            Vector3 burgersVector = e1.vec() + e1.transition()->reverseTransform(e2.vec()) - e3.vec();
            if(burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON))
                continue; // Burgers circuit test passed.

            // Perform disclination test on the face.
            ClusterTransition* t1 = e1.transition();
            ClusterTransition* t2 = e2.transition();
            ClusterTransition* t3 = e3.transition();
            if(!t1->isSelfTransition() || !t2->isSelfTransition() || !t3->isSelfTransition()) {
                Matrix3 frankRotation = t3->reverse->tm * t2->tm * t1->tm;
                if(!frankRotation.equals(Matrix3::Identity(), CA_TRANSITION_MATRIX_EPSILON))
                    continue; // Disclination test failed.
            }

            // Create a new dislocation segment.
            Point3 p0 = tessellation().vertexPosition(cellVertices[face]);
            Vector3 p1 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(face, 0)]) - Point3::Origin();
            Vector3 p2 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(face, 1)]) - Point3::Origin();
            Vector3 p3 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(face, 2)]) - Point3::Origin();
            Vector3 sum = p1 + p2 + p3;
            Point3 faceCenter = Point3::Origin() + sum / FloatType(3);
            Point3 cellCenter = (p0 + sum) / FloatType(4);
            linePosition1Access.push_back(faceCenter);
            linePosition2Access.push_back(cellCenter);
        }
    }

    _dislocationSegments->addProperty(linePosition1Access.take());
    _dislocationSegments->addProperty(linePosition2Access.take());
}

}  // namespace Ovito
