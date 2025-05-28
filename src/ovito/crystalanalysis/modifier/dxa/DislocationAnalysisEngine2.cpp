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
#include <ovito/delaunay/ManifoldConstructionHelper.h>
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
                                                     DataOORef<Lines> dislocationSegments, DataOORef<Lines> unassignedEdges, DataOORef<SurfaceMesh> interfaceMesh)
    : StructureIdentificationModifier::Algorithm(std::move(structures)),
      _inputCrystalStructure(inputCrystalStructure),
      _preferredCrystalOrientations(std::move(preferredCrystalOrientations)),
      _crystalClusters(std::move(crystalClusters)),
      _dislocationSegments(std::move(dislocationSegments)),
      _unassignedEdges(std::move(unassignedEdges)),
      _interfaceMesh(std::move(interfaceMesh)),
      _clusterGraph(DataOORef<ClusterGraph>::create())
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
                               _clusterGraph, structures(),
                               std::move(_preferredCrystalOrientations));
    QElapsedTimer timer;
    _tessellation.emplace();
    _elasticMapping.emplace(*_structureAnalysis, *_tessellation);
    setAtomClusters(_structureAnalysis->atomClusters());

    timer.start();
    _structureAnalysis->identifyStructures(progress, simulationCell);
    qInfo() << "Structure identification took" << timer.restart() << "ms";
    _structureAnalysis->buildClusters(progress);
    qInfo() << "Building clusters took" << timer.restart() << "ms";
    _structureAnalysis->connectClusters(progress);
    qInfo() << "Connecting clusters took" << timer.restart() << "ms";

    progress.setText(DislocationAnalysisModifier::tr("DXA: Generating Delaunay tessellation"));
    FloatType ghostLayerSize = FloatType(3.5) * _structureAnalysis->maximumNeighborDistance();
    _tessellation->generateTessellation(simulationCell, BufferReadAccess<Point3>(positions).cbegin(),
                                        _structureAnalysis->atomCount(), ghostLayerSize,
                                        false,  // flag coverDomainWithFiniteTets
                                        selection ? BufferReadAccess<SelectionIntType>(selection).cbegin() : nullptr,
                                        progress);
    qInfo() << "Generating Delaunay tessellation took" << timer.restart() << "ms";

    /// Create a lookup map that allows to retrieve the primary Delaunay cell image that belongs to a
    /// triangular face formed by three particles. In addition, use the alpha criterion to identify
    /// tetrahedra that are outside the crystalline region and should be ignored in the analysis.
    constexpr FloatType alphaFactor = 3.5; // This factor is used to choose the alpha value proportional to the maximum interatomic distance in the crystal lattice.
    const FloatType alpha = alphaFactor * _structureAnalysis->maximumNeighborDistance();
    progress.setText(DislocationAnalysisModifier::tr("DXA: Classifying tetrahedra"));
    _elasticMapping->classifyTetrahedra(alpha, progress);
    qInfo() << "Classifying tetrahedra took" << timer.restart() << "ms";

    // Build a list of Delaunay edges.
    progress.setText(DislocationAnalysisModifier::tr("DXA: Generating list of Delaunay edges"));
    _elasticMapping->generateTessellationEdges(progress);
    qInfo() << "Generating list of Delaunay edges took" << timer.restart() << "ms";

    // Assign each atom to a crystal cluster.
    progress.setText(DislocationAnalysisModifier::tr("DXA: Assigning atoms to clusters"));
    _elasticMapping->assignAtomsToClusters(progress);
    qInfo() << "Assigning atoms to clusters took" << timer.restart() << "ms";

    // Assign ideal lattice vectors to the edges of the Delaunay tessellation.
    progress.setText(DislocationAnalysisModifier::tr("DXA: Assigning ideal lattice vectors to edges"));
    _elasticMapping->assignIdealVectorsToEdges(4, progress);
    qInfo() << "Assigning ideal lattice vectors to edges took" << timer.restart() << "ms";

    // Free some memory that is no longer needed.
    _structureAnalysis->freeNeighborLists();

    OVITO_ASSERT(_dislocationSegments->elementCount() == 0);
    OVITO_ASSERT(_dislocationSegments->properties().empty());

    PropertyFactory<Point3> linePosition1Access(Lines::OOClass(), 0, Lines::Position1Property);
    PropertyFactory<Point3> linePosition2Access(Lines::OOClass(), 0, Lines::Position2Property);
    PropertyFactory<Vector3> burgersVectorAccess(Lines::OOClass(), 0, QStringLiteral("Burgers Vector"), QStringList() << "X" << "Y" << "Z");
    PropertyFactory<ColorG> segmentColorAccess(Lines::OOClass(), 0, Lines::ColorProperty);
    PropertyFactory<int> segmentStageAccess(Lines::OOClass(), 0, QStringLiteral("Stage"));

    PropertyFactory<Point3> edgePosition1Access(Lines::OOClass(), 0, Lines::Position1Property);
    PropertyFactory<Point3> edgePosition2Access(Lines::OOClass(), 0, Lines::Position2Property);
    PropertyFactory<int> edgeStageAccess(Lines::OOClass(), 0, QStringLiteral("Stage"));
    PropertyFactory<int64_t*> edgeAtomAccess(Lines::OOClass(), 0, QStringLiteral("Atoms"), 2, QStringList() << "Atom 1" << "Atom 2");

    int stage = 0;
    auto dumpSnapshot = [&]() {
        progress.setText(DislocationAnalysisModifier::tr("DXA: Dumping snapshot %1").arg(stage + 1));
        timer.restart();
        extractDislocationSegments(progress, linePosition1Access, linePosition2Access, burgersVectorAccess, segmentColorAccess, segmentStageAccess, stage);
        qInfo() << "Extracting dislocation segments took" << timer.restart() << "ms";
        _elasticMapping->extractUnassignedEdges(progress, edgePosition1Access, edgePosition2Access, edgeAtomAccess, edgeStageAccess, stage);
        qInfo() << "Extracting unassigned edges took" << timer.restart() << "ms";
        extractInterfaceMesh(alpha, stage);
        qInfo() << "Extracting interface mesh took" << timer.restart() << "ms";
        stage++;
    };

#if 1
    dumpSnapshot();
    progress.setText(DislocationAnalysisModifier::tr("DXA: Complementing elastic mapping"));
    _elasticMapping->complementEdgeVectors();
    qInfo() << "Complementing edge vectors took" << timer.restart() << "ms";
    dumpSnapshot();
#else
    for(;;) {
        this_task::throwIfCanceled();

        progress.setText(DislocationAnalysisModifier::tr("DXA: Complementing elastic mapping"));
        timer.restart();
        if(!_elasticMapping->complementEdgeVectors())
            break;
        qInfo() << "Complementing edge vectors took" << timer.restart() << "ms";

        if(stage >= 10) {
            qWarning() << "WARNING: Number of required DXA iterations exceeds limit, stopping.";
            break;
        }
    }
    dumpSnapshot();
#endif

    _dislocationSegments->addProperty(linePosition1Access.take());
    _dislocationSegments->addProperty(linePosition2Access.take());
    _dislocationSegments->addProperty(burgersVectorAccess.take());
    _dislocationSegments->addProperty(segmentColorAccess.take());
    _dislocationSegments->addProperty(segmentStageAccess.take());

    _unassignedEdges->addProperty(edgePosition1Access.take());
    _unassignedEdges->addProperty(edgePosition2Access.take());
    _unassignedEdges->addProperty(edgeStageAccess.take());
    _unassignedEdges->addProperty(edgeAtomAccess.take());

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

    // Output dislocation segments.
    state.addObject(_dislocationSegments);
    // Output unassigned edges.
    state.addObject(_unassignedEdges);

    // Output the interface mesh.
    state.addObjectWithUniqueId<SurfaceMesh>(_interfaceMesh);

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
void DislocationAnalysisEngine2::extractDislocationSegments(TaskProgress& progress, PropertyFactory<Point3>& linePosition1Access,
                                                            PropertyFactory<Point3>& linePosition2Access, PropertyFactory<Vector3>& burgersVectorAccess, PropertyFactory<ColorG>& lineColorAccess,
                                                            PropertyFactory<int>& stageAccess, int stage)
{
    size_t numberOfSegments = 0;
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        if(!elasticMapping().isFilledCell(cell))
            continue;

        this_task::throwIfCanceled();

        // Get the four Delaunay vertices of the current tetrahedral cell.
        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);

        // Get the six oriented edges of the Delaunay cell.
        const std::array<ElasticMapping2::OrientedEdge, 6> cellEdges = elasticMapping().getOrientedEdges(cell);

        // Perform Burgers circuit test on each of the four facets of the tetrahedron.
        for(int facet = 0; facet < 4; facet++) {
            const std::array<ElasticMapping2::OrientedEdge, 3> facetEdges = ElasticMapping2::getFacetCircuitEdges(cellEdges, facet);
            if(!facetEdges[0] || !facetEdges[1] || !facetEdges[2])
                continue; // Skip if one of the edges is missing.
            if(facetEdges[0].isBlocked() || facetEdges[1].isBlocked() || facetEdges[2].isBlocked())
                continue; // Skip if one of the edges is blocked.
            if(!facetEdges[0].hasEdgeVector() && !facetEdges[1].hasEdgeVector() && !facetEdges[2].hasEdgeVector())
                continue; // Skip facet if none of the edges are valid.

            // Perform Burgers circuit test on the face.
            Vector3 burgersVector = Vector3::Zero();
            Cluster* cluster = elasticMapping().clusterOfAtom(facetEdges[0].atom1());
            if(facetEdges[0].hasEdgeVector()) {
                burgersVector += facetEdges[0].vector();
            }
            if(facetEdges[1].hasEdgeVector()) {
                if(facetEdges[0].hasEdgeVector())
                    burgersVector += facetEdges[0].transition()->reverseTransform(facetEdges[1].vector());
                else if(facetEdges[2].hasEdgeVector())
                    burgersVector += facetEdges[2].transition()->transform(facetEdges[1].transition()->transform(facetEdges[1].vector()));
                else
                    burgersVector = facetEdges[1].vector();
            }
            if(facetEdges[2].hasEdgeVector())
                burgersVector += facetEdges[2].transition()->transform(facetEdges[2].vector());
            if(burgersVector.isZero(CA_LATTICE_VECTOR_EPSILON))
                continue; // Burgers circuit test passed.

            // Perform disclination test on the face.
            if(facetEdges[0].hasEdgeVector() && facetEdges[1].hasEdgeVector() && facetEdges[2].hasEdgeVector()) {
                ClusterTransition* t1 = facetEdges[0].transition();
                ClusterTransition* t2 = facetEdges[1].transition();
                ClusterTransition* t3 = facetEdges[2].transition();
                if(!t1->isSelfTransition() || !t2->isSelfTransition() || !t3->isSelfTransition()) {
                    Matrix3 frankRotation = t3->tm * t2->tm * t1->tm;
                    if(!frankRotation.equals(Matrix3::Identity(), CA_TRANSITION_MATRIX_EPSILON))
                        continue; // Disclination test failed.
                }
            }

            // Create a new dislocation segment.
            Point3 p0 = tessellation().vertexPosition(cellVertices[facet]);
            Vector3 p1 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 0)]) - Point3::Origin();
            Vector3 p2 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 1)]) - Point3::Origin();
            Vector3 p3 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 2)]) - Point3::Origin();
            Vector3 sum = p1 + p2 + p3;
            Point3 faceCenter = Point3::Origin() + sum / FloatType(3);
            Point3 cellCenter = (p0 + sum) / FloatType(4);
            linePosition1Access.push_back(faceCenter);
            linePosition2Access.push_back(cellCenter);
            burgersVectorAccess.push_back(burgersVector);
            ParticleType::PredefinedStructureType structureType = ParticleType::PredefinedStructureType::OTHER;
            if(cluster->structure == StructureAnalysis::LATTICE_FCC)
                structureType = ParticleType::PredefinedStructureType::FCC;
            else if(cluster->structure == StructureAnalysis::LATTICE_BCC)
                structureType = ParticleType::PredefinedStructureType::BCC;
            lineColorAccess.push_back(MicrostructurePhase::getBurgersVectorColor(structureType, burgersVector).toDataType<GraphicsFloatType>());
            stageAccess.push_back(stage);
            numberOfSegments++;
        }
    }
}

void DislocationAnalysisEngine2::extractInterfaceMesh(FloatType alpha, int stage)
{
    SurfaceMeshBuilder meshBuilder(_interfaceMesh);
    auto oldFaceCount = meshBuilder.faceCount();

    // Create the 'good' region (region 0).
    meshBuilder.mutableRegions()->setElementCount(1);
    OVITO_ASSERT(meshBuilder.regionCount() == 1);

    // Determines if a tetrahedron belongs to the good or bad crystal region.
    auto tetrahedronRegion = [this](DelaunayTessellation::CellHandle cell) {
        return elasticMapping().isElasticMappingCompatible(cell) ? 0 : SurfaceMesh::InvalidIndex;
    };

    // Construct a one-sided surface mesh.
    ManifoldConstructionHelper manifoldConstructor(const_cast<DelaunayTessellation&>(elasticMapping().tessellation()), meshBuilder, alpha, false, _structureAnalysis->positions(), TaskProgress::Ignore);
    manifoldConstructor.construct(tetrahedronRegion);

    // Assign faces to regions, one per stage.
    BufferWriteAccess<SurfaceMesh::region_index, access_mode::read_write> faceRegions;
    if(!meshBuilder.faceProperty(SurfaceMeshFaces::RegionProperty))
        faceRegions = meshBuilder.createFaceProperty(DataBuffer::Initialized, SurfaceMeshFaces::RegionProperty);
    else
        faceRegions = meshBuilder.mutableFaceProperty(SurfaceMeshFaces::RegionProperty);

    std::fill(faceRegions.begin() + oldFaceCount, faceRegions.end(), stage);
}

}  // namespace Ovito
