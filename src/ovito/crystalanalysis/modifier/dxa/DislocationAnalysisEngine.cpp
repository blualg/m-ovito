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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetwork.h>
#include <ovito/crystalanalysis/objects/ClusterGraph.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>

#include <utility>
#include "DislocationAnalysisEngine.h"
#include "DislocationAnalysisModifier.h"

#if 0
#include <fstream>
#endif

namespace Ovito {

/******************************************************************************
 * Constructor.
 ******************************************************************************/
DislocationAnalysisEngine::DislocationAnalysisEngine(PropertyPtr structures, size_t particleCount, int inputCrystalStructure,
                                                     int maxTrialCircuitSize, int maxCircuitElongation, ConstPropertyPtr particleSelection,
                                                     ConstPropertyPtr crystalClusters, std::vector<Matrix3> preferredCrystalOrientations,
                                                     bool onlyPerfectDislocations, bool markCoreAtoms, int defectMeshSmoothingLevel,
                                                     DataOORef<DislocationNetwork> dislocationNetwork, DataOORef<SurfaceMesh> defectMesh,
                                                     DataOORef<SurfaceMesh> outputInterfaceMesh, int lineSmoothingLevel,
                                                     FloatType linePointInterval)
    : StructureIdentificationModifier::Algorithm(std::move(structures)),
      _inputCrystalStructure(inputCrystalStructure),
      _onlyPerfectDislocations(onlyPerfectDislocations),
      _markCoreAtoms(markCoreAtoms),
      _defectMeshSmoothingLevel(defectMeshSmoothingLevel),
      _lineSmoothingLevel(lineSmoothingLevel),
      _maxTrialCircuitSize(maxTrialCircuitSize),
      _maxCircuitElongation(maxCircuitElongation),
      _linePointInterval(linePointInterval),
      _preferredCrystalOrientations(std::move(preferredCrystalOrientations)),
      _crystalClusters(std::move(crystalClusters)),
      _defectMesh(std::move(defectMesh)),
      _outputInterfaceMesh(std::move(outputInterfaceMesh)),
      _dislocationNetwork(std::move(dislocationNetwork))
{
}

/******************************************************************************
 * Performs the actual analysis.
 ******************************************************************************/
void DislocationAnalysisEngine::identifyStructures(const Particles* particles, const SimulationCell* simulationCell,
                                                   const Property* selection)
{
    if(!simulationCell) throw Exception(DislocationAnalysisModifier::tr("DXA requires a simulation cell to be defined."));
    if(simulationCell->is2D()) throw Exception(DislocationAnalysisModifier::tr("DXA does not support 2d simulations."));

    this_task::setProgressText(DislocationAnalysisModifier::tr("Dislocation analysis (DXA)"));

    const Property* positions = particles->expectProperty(Particles::PositionProperty);
    _simCellVolume = simulationCell->volume3D();
    _structureAnalysis.emplace(positions, simulationCell, (StructureAnalysis::LatticeStructureType)_inputCrystalStructure, selection,
                               const_cast<ClusterGraph*>(_dislocationNetwork->clusterGraph()), structures(),
                               std::move(_preferredCrystalOrientations), !_onlyPerfectDislocations);
    _tessellation.emplace();
    _elasticMapping.emplace(*_structureAnalysis, *_tessellation);
    _interfaceMesh.emplace(*_elasticMapping);
    _dislocationTracer.emplace(*_interfaceMesh, _maxTrialCircuitSize, _maxCircuitElongation, dislocationNetwork(), _markCoreAtoms);
    setAtomClusters(_structureAnalysis->atomClusters());
    if(_markCoreAtoms) {
        this_task::beginProgressSubStepsWithWeights({35, 6, 1, 220, 60, 1, 53, 190, 146 * 5, 20, 4, 4});
    }
    else {
        this_task::beginProgressSubStepsWithWeights({35, 6, 1, 220, 60, 1, 53, 190, 146, 20, 4, 4});
    }
    _structureAnalysis->identifyStructures();
    this_task::throwIfCanceled();

    this_task::nextProgressSubStep();
    _structureAnalysis->buildClusters();
    this_task::throwIfCanceled();

    this_task::nextProgressSubStep();
    _structureAnalysis->connectClusters();
    this_task::throwIfCanceled();

#if 0
    Point3 corners[8];
    corners[0] = _structureAnalysis.cell().reducedToAbsolute(Point3(0,0,0));
    corners[1] = _structureAnalysis.cell().reducedToAbsolute(Point3(1,0,0));
    corners[2] = _structureAnalysis.cell().reducedToAbsolute(Point3(1,1,0));
    corners[3] = _structureAnalysis.cell().reducedToAbsolute(Point3(0,1,0));
    corners[4] = _structureAnalysis.cell().reducedToAbsolute(Point3(0,0,1));
    corners[5] = _structureAnalysis.cell().reducedToAbsolute(Point3(1,0,1));
    corners[6] = _structureAnalysis.cell().reducedToAbsolute(Point3(1,1,1));
    corners[7] = _structureAnalysis.cell().reducedToAbsolute(Point3(0,1,1));

    std::ofstream stream("cell.vtk");
    stream << "# vtk DataFile Version 3.0" << std::endl;
    stream << "# Simulation cell" << std::endl;
    stream << "ASCII" << std::endl;
    stream << "DATASET UNSTRUCTURED_GRID" << std::endl;
    stream << "POINTS 8 double" << std::endl;
    for(int i = 0; i < 8; i++)
        stream << corners[i].x() << " " << corners[i].y() << " " << corners[i].z() << std::endl;

    stream << std::endl << "CELLS 1 9" << std::endl;
    stream << "8 0 1 2 3 4 5 6 7" << std::endl;

    stream << std::endl << "CELL_TYPES 1" << std::endl;
    stream << "12" << std::endl;  // Hexahedron
#endif

    this_task::nextProgressSubStep();
    FloatType ghostLayerSize = FloatType(3.5) * _structureAnalysis->maximumNeighborDistance();
    _tessellation->generateTessellation(_structureAnalysis->cell(), BufferReadAccess<Point3>(positions).cbegin(),
                                        _structureAnalysis->atomCount(), ghostLayerSize,
                                        false,  // flag coverDomainWithFiniteTets
                                        selection ? BufferReadAccess<SelectionIntType>(selection).cbegin() : nullptr);
    this_task::throwIfCanceled();

    // Build list of edges in the tessellation.
    this_task::nextProgressSubStep();
    _elasticMapping->generateTessellationEdges();
    this_task::throwIfCanceled();

    // Assign each vertex to a cluster.
    this_task::nextProgressSubStep();
    _elasticMapping->assignVerticesToClusters();
    this_task::throwIfCanceled();

    // Determine the ideal vector corresponding to each edge of the tessellation.
    this_task::nextProgressSubStep();
    _elasticMapping->assignIdealVectorsToEdges(4);
    this_task::throwIfCanceled();

    // Free some memory that is no longer needed.
    _structureAnalysis->freeNeighborLists();
    this_task::throwIfCanceled();

    // Create the mesh facets.
    this_task::nextProgressSubStep();
    _interfaceMesh->createMesh(_structureAnalysis->maximumNeighborDistance(), crystalClusters());
    this_task::throwIfCanceled();

    // Trace dislocation lines.
    this_task::nextProgressSubStep();
    _dislocationTracer->traceDislocationSegments();
    _dislocationTracer->finishDislocationSegments(_inputCrystalStructure);
    this_task::throwIfCanceled();

    if(_markCoreAtoms) {
        assignDislocationIDs(particles->elementCount());
    }
#if 0

    auto isWrappedFacet = [this](const InterfaceMesh::Face* f) -> bool {
        InterfaceMesh::edge_index e = f->edges();
        do {
            Vector3 v = e->vertex1()->pos() - e->vertex2()->pos();
            if(_structureAnalysis.cell().isWrappedVector(v))
                return true;
            e = e->nextFaceEdge();
        }
        while(e != f->edges());
        return false;
    };

    // Count facets which are not crossing the periodic boundaries.
    size_t numFacets = 0;
    for(const InterfaceMesh::Face* f : _interfaceMesh.faces()) {
        if(isWrappedFacet(f) == false)
            numFacets++;
    }

    std::ofstream stream("mesh.vtk");
    stream << "# vtk DataFile Version 3.0\n";
    stream << "# Interface mesh\n";
    stream << "ASCII\n";
    stream << "DATASET UNSTRUCTURED_GRID\n";
    stream << "POINTS " << _interfaceMesh.vertices().size() << " float\n";
    for(const InterfaceMesh::Vertex* n : _interfaceMesh.vertices()) {
        const Point3& pos = n->pos();
        stream << pos.x() << " " << pos.y() << " " << pos.z() << "\n";
    }
    stream << "\nCELLS " << numFacets << " " << (numFacets*4) << "\n";
    for(const InterfaceMesh::Face* f : _interfaceMesh.faces()) {
        if(isWrappedFacet(f) == false) {
            stream << f->edgeCount();
            InterfaceMesh::edge_index e = f->edges();
            do {
                stream << " " << e->vertex1()->index();
                e = e->nextFaceEdge();
            }
            while(e != f->edges());
            stream << "\n";
        }
    }

    stream << "\nCELL_TYPES " << numFacets << "\n";
    for(size_t i = 0; i < numFacets; i++)
        stream << "5\n";    // Triangle

    stream << "\nCELL_DATA " << numFacets << "\n";

    stream << "\nSCALARS dislocation_segment int 1\n";
    stream << "\nLOOKUP_TABLE default\n";
    for(const InterfaceMesh::Face* f : _interfaceMesh.faces()) {
        if(isWrappedFacet(f) == false) {
            if(f->circuit != NULL && (f->circuit->isDangling == false || f->testFlag(1))) {
                DislocationSegment* segment = f->circuit->dislocationNode->segment;
                while(segment->replacedWith != NULL) segment = segment->replacedWith;
                stream << segment->id << "\n";
            }
            else
                stream << "-1\n";
        }
    }

    stream << "\nSCALARS is_primary_segment int 1\n";
    stream << "\nLOOKUP_TABLE default\n";
    for(const InterfaceMesh::Face* f : _interfaceMesh.faces()) {
        if(isWrappedFacet(f) == false)
            stream << f->testFlag(1) << "\n";
    }

    stream.close();
#endif

    // Generate the defect mesh.
    this_task::nextProgressSubStep();
    SurfaceMeshBuilder defectMeshBuilder(_defectMesh);
    _interfaceMesh->generateDefectMesh(*_dislocationTracer, defectMeshBuilder);
#ifdef OVITO_DEBUG
    _defectMesh->verifyMeshIntegrity();
#endif
    this_task::throwIfCanceled();

#if 0
    _tessellation.dumpToVTKFile("tessellation.vtk");
#endif

    this_task::nextProgressSubStep();

    // Post-process surface mesh.
    if(_defectMeshSmoothingLevel > 0) defectMeshBuilder.smoothMesh(_defectMeshSmoothingLevel);

    this_task::nextProgressSubStep();

    // Post-process dislocation lines.
    if(_lineSmoothingLevel > 0 || _linePointInterval > 0) {
        dislocationNetwork()->smoothDislocationLines(_lineSmoothingLevel, _linePointInterval);
        this_task::throwIfCanceled();
    }

    this_task::endProgressSubSteps();

    // Return the results of the compute engine.
    if(_outputInterfaceMesh) {
        _outputInterfaceMesh->setTopology(interfaceMesh().topology());
        _outputInterfaceMesh->setSpaceFillingRegion(_defectMesh->spaceFillingRegion());
        _outputInterfaceMesh->makeVerticesMutable()->setElementCount(interfaceMesh().vertexCount());
        _outputInterfaceMesh->makeVerticesMutable()->createProperty(interfaceMesh().vertexProperty(SurfaceMeshVertices::PositionProperty));
        _outputInterfaceMesh->makeFacesMutable()->setElementCount(interfaceMesh().faceCount());
        _outputInterfaceMesh->makeRegionsMutable()->setElementCount(interfaceMesh().regionCount());
    }

    // Release data that is no longer needed.
    _structureAnalysis.reset();
    _tessellation.reset();
    _elasticMapping.reset();
    _interfaceMesh.reset();
    _dislocationTracer.reset();
    _crystalClusters.reset();
}

/******************************************************************************
 * Create the output dislocation ID atom property and assign determined values.
 ******************************************************************************/
void DislocationAnalysisEngine::assignDislocationIDs(size_t numParticles)
{
    // Create the output dislocation ID atom property and assign determined values
    _atomDislocations =
        Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, numParticles, Particles::DislocationProperty);
    _atomDislocations->fill(-1);

    const BufferWriteAccess<int32_t, access_mode::write> dislocationsBuffer(_atomDislocations);
    for(DelaunayTessellation::CellHandle cell = 0; cell < _tessellation->numberOfTetrahedra(); ++cell) {
        // Check if tet has dislocation core info
        if(const auto& dislocInfo = _dislocationTracer->dislocationCoreInfo(cell)) {
            OVITO_ASSERT(dislocInfo->first);
            // Remove any tetrahedra originally marked, where the line has been clipped
            if(dislocInfo->first->isDangling() && dislocInfo->second) {
                continue;
            }
            for(size_t lv = 0; lv < 4; ++lv) {
                size_t index = _tessellation->vertexIndex(_tessellation->cellVertex(cell, lv));
                dislocationsBuffer[index] = dislocInfo->first->segment->replacedId();
            }
        }
    }
}

/******************************************************************************
 * Computes the structure identification statistics.
 ******************************************************************************/
std::vector<int64_t> DislocationAnalysisEngine::computeStructureStatistics(const Property* structures, PipelineFlowState& state,
                                                                           const OOWeakRef<const PipelineNode>& createdByNode,
                                                                           const std::any& modifierParameters) const
{
    std::vector<int64_t> typeCounts =
        StructureIdentificationModifier::Algorithm::computeStructureStatistics(structures, state, createdByNode, modifierParameters);

    // Output defect mesh.
    state.addObjectWithUniqueId<SurfaceMesh>(_defectMesh);

    // Output interface mesh.
    if(_outputInterfaceMesh) state.addObjectWithUniqueId<SurfaceMesh>(_outputInterfaceMesh);

#if 0
    // Output cluster graph to data collection.
    if(const ClusterGraphObject* oldClusterGraph = state.getObject<ClusterGraphObject>())
        state.removeObject(oldClusterGraph);
    ClusterGraphObject* clusterGraphObj = state.createObject<ClusterGraphObject>(createdByNode);
    clusterGraphObj->setStorage(clusterGraph());
#endif

    // Output dislocations.
    while(!dislocationNetwork()->crystalStructures().empty())
        dislocationNetwork()->removeCrystalStructure(dislocationNetwork()->crystalStructures().size() - 1);
    for(const ElementType* stype : structures->elementTypes())
        dislocationNetwork()->addCrystalStructure(static_object_cast<MicrostructurePhase>(stype));
    state.addObject(dislocationNetwork());

    // Output particle properties.
    if(atomClusters()) {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(atomClusters());
    }

    if(_atomDislocations) {
        Particles* particles = state.expectMutableObject<Particles>();
        particles->createProperty(_atomDislocations);
    }

    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.OTHER"),
                       QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_OTHER)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.FCC"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_FCC)),
                       createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HCP"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_HCP)),
                       createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.BCC"), QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_BCC)),
                       createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.CubicDiamond"),
                       QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_CUBIC_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HexagonalDiamond"),
                       QVariant::fromValue(typeCounts.at(StructureAnalysis::LATTICE_HEX_DIAMOND)), createdByNode);
    state.addAttribute(QStringLiteral("DislocationAnalysis.cell_volume"), QVariant::fromValue(simCellVolume()), createdByNode);

    // Compute dislocation line statistics.
    FloatType totalLineLength = generateDislocationStatistics(createdByNode, state, dislocationNetwork(), false,
                                                              dislocationNetwork()->structureById(_inputCrystalStructure));
    size_t totalSegmentCount = dislocationNetwork()->segments().size();

    if(totalSegmentCount == 0)
        state.setStatus(PipelineStatus(PipelineStatus::Success, DislocationAnalysisModifier::tr("No dislocations found")));
    else
        state.setStatus(
            PipelineStatus(PipelineStatus::Success, DislocationAnalysisModifier::tr("Found %1 dislocation segments\nTotal line length: %2")
                                                        .arg(totalSegmentCount)
                                                        .arg(totalLineLength)));

    return typeCounts;
}

/******************************************************************************
 * Computes statistical information on the identified dislocation lines and
 * outputs it to the pipeline as data tables and global attributes.
 ******************************************************************************/
FloatType DislocationAnalysisEngine::generateDislocationStatistics(const OOWeakRef<const PipelineNode>& pipelineNode,
                                                                   PipelineFlowState& state, const DislocationNetwork* dislocations,
                                                                   bool replaceDataObjects, const MicrostructurePhase* defaultStructure)
{
    std::map<const BurgersVectorFamily*, FloatType> dislocationLengths;
    std::map<const BurgersVectorFamily*, int> segmentCounts;
    std::map<const BurgersVectorFamily*, const MicrostructurePhase*> dislocationCrystalStructures;

    const BurgersVectorFamily* defaultFamily = nullptr;
    if(defaultStructure) {
        defaultFamily = defaultStructure->defaultBurgersVectorFamily();
        for(const BurgersVectorFamily* family : defaultStructure->burgersVectorFamilies()) {
            dislocationLengths[family] = 0;
            segmentCounts[family] = 0;
            dislocationCrystalStructures[family] = defaultStructure;
        }
    }

    // Classify, count and measure length of dislocation segments.
    FloatType totalLineLength = 0;
    for(const DislocationSegment* segment : dislocations->segments()) {
        FloatType len = segment->calculateLength();
        totalLineLength += len;

        Cluster* cluster = segment->burgersVector.cluster();
        OVITO_ASSERT(cluster != nullptr);
        const MicrostructurePhase* structure = dislocations->structureById(cluster->structure);
        if(structure == nullptr) continue;
        const BurgersVectorFamily* family = defaultFamily;
        if(structure == defaultStructure) {
            family = structure->defaultBurgersVectorFamily();
            for(const BurgersVectorFamily* f : structure->burgersVectorFamilies()) {
                if(f->isMember(segment->burgersVector.localVec(), structure)) {
                    family = f;
                    break;
                }
            }
        }
        if(family) {
            segmentCounts[family]++;
            dislocationLengths[family] += len;
            dislocationCrystalStructures[family] = structure;
        }
    }

    // Output a data table with the dislocation line lengths.
    int maxId = 0;
    for(const auto& entry : dislocationLengths) maxId = std::max(maxId, entry.first->numericId());
    PropertyPtr dislocationLengthsProperty = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized, maxId + 1, DataBuffer::FloatDefault, 1, DislocationAnalysisModifier::tr("Total line length"));
    BufferWriteAccess<FloatType, access_mode::write> dislocationLengthsAccess(dislocationLengthsProperty);
    for(const auto& entry : dislocationLengths) dislocationLengthsAccess[entry.first->numericId()] = entry.second;
    dislocationLengthsAccess.reset();
    PropertyPtr dislocationTypeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxId + 1, DataBuffer::Int32, 1,
                                                                             DislocationAnalysisModifier::tr("Dislocation type"));
    boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(dislocationTypeIds).begin(), 0,
                             dislocationTypeIds->size());

    for(const auto& entry : dislocationLengths) dislocationTypeIds->addElementType(entry.first);

    DataTable* lengthTableObj =
        replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-lengths")) : nullptr;
    if(!lengthTableObj) {
        lengthTableObj = state.createObject<DataTable>(QStringLiteral("disloc-lengths"), pipelineNode, DataTable::BarChart,
                                                       DislocationAnalysisModifier::tr("Dislocation lengths"),
                                                       std::move(dislocationLengthsProperty), std::move(dislocationTypeIds));
        lengthTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
    }
    else {
        ConstPropertyPtr x = std::move(dislocationTypeIds);
        ConstPropertyPtr y = std::move(dislocationLengthsProperty);
        lengthTableObj->setContent(maxId + 1, DataRefVector<Property>{{y, x}});
        lengthTableObj->setX(std::move(x));
        lengthTableObj->setY(std::move(y));
    }

    // Output a data table with the dislocation segment counts.
    PropertyPtr dislocationCountsProperty = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized, maxId + 1, DataBuffer::Int32, 1, DislocationAnalysisModifier::tr("Dislocation count"));
    BufferWriteAccessAndRef<int32_t, access_mode::write> dislocationCountsAccess(dislocationCountsProperty);
    for(const auto& entry : segmentCounts) dislocationCountsAccess[entry.first->numericId()] = entry.second;
    dislocationCountsAccess.reset();

    DataTable* countTableObj =
        replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-counts")) : nullptr;
    if(!countTableObj) {
        countTableObj =
            state.createObject<DataTable>(QStringLiteral("disloc-counts"), pipelineNode, DataTable::BarChart,
                                          DislocationAnalysisModifier::tr("Dislocation counts"), std::move(dislocationCountsProperty));
        countTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
    }
    else
        countTableObj->setContent(maxId + 1, DataRefVector<Property>{{std::move(dislocationCountsProperty)}});
    countTableObj->insertProperty(0, lengthTableObj->x());
    countTableObj->setX(lengthTableObj->x());

    if(replaceDataObjects)
        state.setAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), pipelineNode);
    else
        state.addAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), pipelineNode);

    for(const auto& dlen : dislocationLengths) {
        const MicrostructurePhase* structure = dislocationCrystalStructures[dlen.first];
        QString bstr;
        if(dlen.first->burgersVector() != Vector3::Zero()) {
            bstr = DislocationVis::formatBurgersVector(dlen.first->burgersVector(), structure);
            bstr.remove(QChar(' '));
            bstr.replace(QChar('['), QChar('<'));
            bstr.replace(QChar(']'), QChar('>'));
        }
        else
            bstr = "other";
        if(replaceDataObjects)
            state.setAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), pipelineNode);
        else
            state.addAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), pipelineNode);
    }

    return totalLineLength;
}

}  // namespace Ovito
