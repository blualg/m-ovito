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
    progress.setText(DislocationAnalysisModifier::tr("DXA: Identifying structures"));
    _structureAnalysis->identifyStructures(progress, simulationCell);
    qInfo() << "Structure identification took" << timer.restart() << "ms";
    progress.setText(DislocationAnalysisModifier::tr("DXA: Building clusters"));
    _structureAnalysis->buildClusters(progress);
    qInfo() << "Building clusters took" << timer.restart() << "ms";
    progress.setText(DislocationAnalysisModifier::tr("DXA: Connecting clusters"));
    _structureAnalysis->connectClusters(progress);
    qInfo() << "Connecting clusters took" << timer.restart() << "ms";

    QElapsedTimer totalTimer;
    totalTimer.start();

    progress.setText(DislocationAnalysisModifier::tr("DXA: Forming super clusters"));
    _structureAnalysis->formSuperClusters(progress);
    qInfo() << "Forming super clusters took" << timer.restart() << "ms";

    progress.setText(DislocationAnalysisModifier::tr("DXA: Dissolving small clusters"));
    _structureAnalysis->dissolveSmallClusters(progress, _minClusterSize);
    qInfo() << "Dissolving small clusters took" << timer.restart() << "ms";

    progress.setText(DislocationAnalysisModifier::tr("DXA: Marking perfect crystalline regions"));
    _delaunayAtomBuffer = markPerfectCrystallineRegions(progress, selection);
    qInfo() << "Marking perfect crystalline regions took" << timer.restart() << "ms";

    progress.setText(DislocationAnalysisModifier::tr("DXA: Generating Delaunay tessellation"));
    FloatType ghostLayerSize = FloatType(3.5) * _structureAnalysis->maximumNeighborDistance();
    _tessellation->generateTessellation(simulationCell, BufferReadAccess<Point3>(positions).cbegin(),
                                        _structureAnalysis->atomCount(), ghostLayerSize,
                                        false,  // flag coverDomainWithFiniteTets
                                        BufferReadAccess<SelectionIntType>(_delaunayAtomBuffer).cbegin(),
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
//    _structureAnalysis->freeNeighborLists();

    OVITO_ASSERT(_dislocationSegments->elementCount() == 0);
    OVITO_ASSERT(_dislocationSegments->properties().empty());

    PropertyFactory<Point3> linePosition1Access(Lines::OOClass(), 0, Lines::Position1Property);
    PropertyFactory<Point3> linePosition2Access(Lines::OOClass(), 0, Lines::Position2Property);
    PropertyFactory<Cluster::VecType> burgersVectorAccess(Lines::OOClass(), 0, QStringLiteral("Burgers Vector"), QStringList() << "X" << "Y" << "Z");
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
        if(_unassignedEdges) {
            _elasticMapping->extractUnassignedEdges(progress, edgePosition1Access, edgePosition2Access, edgeAtomAccess, edgeStageAccess, stage);
            qInfo() << "Extracting unassigned edges took" << timer.restart() << "ms";
        }
        if(_interfaceMesh) {
            extractInterfaceMesh(alpha, stage);
            qInfo() << "Extracting interface mesh took" << timer.restart() << "ms";
        }
        stage++;
    };

    progress.setText(DislocationAnalysisModifier::tr("DXA: Build Delaunay facet lookup map"));
    _elasticMapping->buildFacetLookupMap(progress);
    qInfo() << "Building Delaunay facet lookup map took" << timer.restart() << "ms";

    for(;;) {
        this_task::throwIfCanceled();
//        dumpSnapshot();
        progress.setText(DislocationAnalysisModifier::tr("DXA: Complementing elastic mapping"));
        timer.restart();
        if(!_elasticMapping->complementEdgeVectors(progress))
            break;
        qInfo() << "Complementing edge vectors took" << timer.restart() << "ms";
    }
    dumpSnapshot();
    qInfo() << "DXA took" << totalTimer.restart() << "ms";

    _dislocationSegments->addProperty(linePosition1Access.take());
    _dislocationSegments->addProperty(linePosition2Access.take());
    _dislocationSegments->addProperty(burgersVectorAccess.take());
    _dislocationSegments->addProperty(segmentColorAccess.take());
    _dislocationSegments->addProperty(segmentStageAccess.take());

    if(_unassignedEdges) {
        _unassignedEdges->addProperty(edgePosition1Access.take());
        _unassignedEdges->addProperty(edgePosition2Access.take());
        _unassignedEdges->addProperty(edgeStageAccess.take());
        _unassignedEdges->addProperty(edgeAtomAccess.take());
    }

    // Release data that is no longer needed.
    _structureAnalysis.reset();
    _tessellation.reset();
    _elasticMapping.reset();
    _crystalClusters.reset();
}

/******************************************************************************
 * Marks atoms that are part of perfect crystalline regions and can be omitted from the Delaunay construction.
 ******************************************************************************/
PropertyPtr DislocationAnalysisEngine2::markPerfectCrystallineRegions(TaskProgress& progress, const Property* selection)
{
    size_t atomCount = _structureAnalysis->atomCount();
    progress.setMaximum(atomCount);

    BufferReadAccess<Point3> positionAccess(_structureAnalysis->positions());
    BufferReadAccess<SelectionIntType> selectionAccess(selection);
    PropertyFactory<SelectionIntType> imperfectAtoms(Particles::OOClass(), atomCount, QStringLiteral("Delaunay Vertices"));

    for(size_t atomIndex = 0; atomIndex < atomCount; atomIndex++) {
        progress.setValueIntermittent(atomIndex);

        // Skip unselected atoms.
        if(selectionAccess && selectionAccess[atomIndex] == 0) {
            imperfectAtoms[atomIndex] = 0; // Exclude this atom from the Delaunay construction.
            continue;
        }

        Cluster* cluster = _structureAnalysis->atomCluster(atomIndex);
        if(cluster->id == 0) {
            // The atom is not part of any crystal cluster, so it is a defect atom.
            imperfectAtoms[atomIndex] = 1; // Include this atom in the Delaunay construction as a defect atom.
            continue;
        }

        const Point3& p1 = positionAccess[atomIndex];

        imperfectAtoms[atomIndex] = 0;
        int numNeighbors = _structureAnalysis->numberOfNeighbors(atomIndex);
        for(int neighborIndex = 0; neighborIndex < numNeighbors; neighborIndex++) {
            const auto neighborAtomIndex = _structureAnalysis->getNeighbor(atomIndex, neighborIndex);
            Cluster* neighborCluster = _structureAnalysis->atomCluster(neighborAtomIndex);
            if(neighborCluster->id == 0) {
                imperfectAtoms[atomIndex] = 1;
                break;
            }
#if 0
            const Point3& p2 = positionAccess[neighborAtomIndex];
            if(_structureAnalysis->cell().isWrappedVector(p2 - p1)) {
                imperfectAtoms[atomIndex] = 1;
                break;
            }
#endif
        }
    }
    return static_object_cast<Property>(imperfectAtoms.take());
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
    if(_unassignedEdges) {
        // Output unassigned edges.
        state.addObject(_unassignedEdges);
    }
    if(_interfaceMesh) {
        // Output the interface mesh.
        state.addObjectWithUniqueId<SurfaceMesh>(_interfaceMesh);
    }

    // Output particle properties.
    if(atomClusters()) {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(atomClusters());
    }
    if(_delaunayAtomBuffer) {
        // Create a property that marks atoms that are part of the Delaunay tessellation.
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(_delaunayAtomBuffer);
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
                                                            PropertyFactory<Point3>& linePosition2Access, PropertyFactory<Cluster::VecType>& burgersVectorAccess, PropertyFactory<ColorG>& lineColorAccess,
                                                            PropertyFactory<int>& stageAccess, int stage)
{
    elasticMapping().extractDislocationSegments(progress, [&](DelaunayTessellation::CellHandle cell, int facet, Cluster* cluster, const Cluster::VecType& burgersVector) {

        // Express the Burgers vector in a cluster having the primary lattice structure.
        Cluster::VecType b = burgersVector;
        if(cluster->structure != _inputCrystalStructure) {
            for(ClusterTransition* t = cluster->transitions; t != nullptr && t->distance <= 1; t = t->next) {
                if(t->cluster2->structure == _inputCrystalStructure) {
                    cluster = t->cluster2;
                    b = t->transform(burgersVector);
                    break;
                }
            }
        }

        const std::array<DelaunayTessellation::VertexHandle, 4> cellVertices = tessellation().cellVertices(cell);
        Point3 p0 = tessellation().vertexPosition(cellVertices[facet]);
        Vector3 p1 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 0)]) - Point3::Origin();
        Vector3 p2 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 1)]) - Point3::Origin();
        Vector3 p3 = tessellation().vertexPosition(cellVertices[DelaunayTessellation::cellFacetVertexIndex(facet, 2)]) - Point3::Origin();
        Vector3 sum = p1 + p2 + p3;
        Point3 faceCenter = Point3::Origin() + sum / FloatType(3);
        Point3 cellCenter = (p0 + sum) / FloatType(4);
        linePosition1Access.push_back(faceCenter);
        linePosition2Access.push_back(cellCenter);
        burgersVectorAccess.push_back(b);
        ParticleType::PredefinedStructureType structureType = ParticleType::PredefinedStructureType::OTHER;
        if(cluster->structure == StructureAnalysis::LATTICE_FCC)
            structureType = ParticleType::PredefinedStructureType::FCC;
        else if(cluster->structure == StructureAnalysis::LATTICE_BCC)
            structureType = ParticleType::PredefinedStructureType::BCC;
        lineColorAccess.push_back(MicrostructurePhase::getBurgersVectorColor(structureType, b).toDataType<GraphicsFloatType>());
        stageAccess.push_back(stage);
    });
}

void DislocationAnalysisEngine2::extractInterfaceMesh(FloatType alpha, int stage)
{
    // A helper method that decides using the alpha-shape criterion which tetrahedra are filled and which are not.
    auto isInteriorTetrahedron = [&](DelaunayTessellation::CellHandle cell) -> bool {
        if(!tessellation().isFiniteCell(cell))
            return false; // Skip ghost cells.

        // A tetrahedron with four vertex atoms that are all perfectly crystalline is always considered filled.
        bool allCrystalline = true;
        for(int v = 0; v < 4; v++) {
            ElasticMapping2::AtomIndex atomIndex = tessellation().inputPointIndex(tessellation().cellVertex(cell, v));
            if(_structureAnalysis->atomCluster(atomIndex)->id == 0)
                allCrystalline = false;
        }
        if(allCrystalline)
            return true;

        // Check if the tetrahedron is filled using the alpha-shape criterion.
        if(auto alphaTestResult = tessellation().alphaTest(cell, alpha)) {
            return *alphaTestResult;
        }
        else {
            // If the alpha test is inconclusive (which may happen if the element is a sliver tetrahedron),
            // then we check the surrounding tetrahedra. Only if all four neighbors are classified as filled or inconclusive,
            // then we accept the sliver tetrahedron as filled too.
            for(int f = 0; f < 4; f++) {
                DelaunayTessellation::CellHandle adjacentCell = tessellation().mirrorFacet(cell, f).first;
                if(!tessellation().isFiniteCell(adjacentCell))
                    return false;
                auto adjacentAlphaTestResult = tessellation().alphaTest(adjacentCell, alpha);
                if(adjacentAlphaTestResult.has_value() && !adjacentAlphaTestResult.value())
                    return false;
            }
            return true;
        }
    };

    SurfaceMeshBuilder meshBuilder(_interfaceMesh);
    auto oldFaceCount = meshBuilder.faceCount();

    // Create the 'good' region (region 0).
    meshBuilder.mutableRegions()->setElementCount(1);
    OVITO_ASSERT(meshBuilder.regionCount() == 1);

    // Stores the triangle mesh vertices created for the vertices of the tetrahedral mesh.
    BufferReadAccess<Point3> positions(_structureAnalysis->positions());
    std::vector<SurfaceMesh::vertex_index> vertexMap(positions.size(), SurfaceMesh::InvalidIndex);

    // Create the vertex coordinates array, which will dynamically grow.
    BufferWriteAccessAndRef<Point3, access_mode::write> vertexPositions = DataOORef<DataBuffer>(meshBuilder.mutableVertexProperty(SurfaceMeshVertices::PositionProperty));
    if(!vertexPositions)
        vertexPositions = SurfaceMeshVertices::OOClass().createStandardProperty(DataBuffer::Uninitialized, 0, SurfaceMeshVertices::PositionProperty);

    // Create the per-face region array, which will dynamically grow.
    BufferWriteAccessAndRef<SurfaceMesh::region_index, access_mode::write> faceRegions = DataOORef<DataBuffer>(meshBuilder.mutableFaceProperty(SurfaceMeshFaces::RegionProperty));
    if(!faceRegions)
        faceRegions = SurfaceMeshFaces::OOClass().createStandardProperty(DataBuffer::Uninitialized, 0, SurfaceMeshFaces::RegionProperty);

    SurfaceMeshTopology* topo = meshBuilder.mutableTopology();
    for(DelaunayTessellation::CellHandle cell : tessellation().cells()) {
        // Consider only filled local tetrahedra.
        if(!isInteriorTetrahedron(cell))
            continue;
        // Iterate over the four facets of the tetrahedron cell.
        for(int f = 0; f < 4; f++) {
            // Check if the adjacent tetrahedron is empty.
            DelaunayTessellation::Facet mirrorFacet = tessellation().mirrorFacet(cell, f);
            DelaunayTessellation::CellHandle adjacentCell = mirrorFacet.first;
            if(isInteriorTetrahedron(adjacentCell))
                continue;

            // Create the three vertices of the face or use existing output vertices.
            std::array<SurfaceMesh::vertex_index,3> facetVertices;
            std::array<DelaunayTessellation::VertexHandle,3> vertexHandles;
            std::array<size_t,3> vertexIndices;
            for(int v = 0; v < 3; v++) {
                vertexHandles[v] = tessellation().cellVertex(cell, DelaunayTessellation::cellFacetVertexIndex(f, 2 - v));
                size_t vertexIndex = vertexIndices[v] = tessellation().inputPointIndex(vertexHandles[v]);
                OVITO_ASSERT(vertexIndex < vertexMap.size());
                if(vertexMap[vertexIndex] == SurfaceMesh::InvalidIndex) {
                    vertexMap[vertexIndex] = topo->createVertex();
                    vertexPositions.push_back(positions[vertexIndex]);
                }
                facetVertices[v] = vertexMap[vertexIndex];
            }
            //OVITO_ASSERT(facetVertices[0] != facetVertices[1] && facetVertices[1] != facetVertices[2] && facetVertices[2] != facetVertices[0]);

            // Create a new triangle facet.
            SurfaceMesh::face_index face = topo->createFaceAndEdges(facetVertices.begin(), facetVertices.end());
            faceRegions.push_back(stage);
#if 0
            // Insert new facet into lookup map.
            reorderFaceVertices(vertexIndices);
            _faceLookupMap.emplace(vertexIndices, face);

            // Insert into contiguous list of tetrahedron faces.
            if(tessellation().getCellIndex(cell) == -1) {
                tessellation().setCellIndex(cell, _tetrahedraFaceList.size());
                _tetrahedraFaceList.push_back(std::array<SurfaceMesh::face_index, 4>{{ SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex, SurfaceMesh::InvalidIndex }});
            }
            _tetrahedraFaceList[tessellation().getCellIndex(cell)][f] = face;
#endif
        }
    }

    // Store the vertex coordinates in the mesh.
    meshBuilder.mutableVertices()->setContent(topo->vertexCount(), { static_object_cast<Property>(vertexPositions.take()) });

    // Store the per-face region information in the mesh.
    meshBuilder.mutableFaces()->setContent(topo->faceCount(), { static_object_cast<Property>(faceRegions.take()) });
}

}  // namespace Ovito
