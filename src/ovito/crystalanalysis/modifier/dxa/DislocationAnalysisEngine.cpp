////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/crystalanalysis/objects/DislocationNetworkObject.h>
#include <ovito/crystalanalysis/objects/ClusterGraphObject.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
#include "DislocationAnalysisEngine.h"
#include "DislocationAnalysisModifier.h"

#if 0
#include <fstream>
#endif

namespace Ovito::CrystalAnalysis {

/******************************************************************************
* Constructor.
******************************************************************************/
DislocationAnalysisEngine::DislocationAnalysisEngine(
		const ModifierEvaluationRequest& request,
		ParticleOrderingFingerprint fingerprint,
		ConstPropertyPtr positions, const SimulationCellObject* simCell, const OORefVector<ElementType>& structureTypes,
		int inputCrystalStructure, int maxTrialCircuitSize, int maxCircuitElongation,
		ConstPropertyPtr particleSelection,
		ConstPropertyPtr crystalClusters,
		std::vector<Matrix3> preferredCrystalOrientations,
		bool onlyPerfectDislocations, int defectMeshSmoothingLevel, DataOORef<SurfaceMesh> defectMesh, DataOORef<SurfaceMesh> outputInterfaceMesh,
		int lineSmoothingLevel, FloatType linePointInterval) :
	StructureIdentificationModifier::StructureIdentificationEngine(request, std::move(fingerprint), positions, simCell, structureTypes, std::move(particleSelection)),
	_simCellVolume(simCell->volume3D()),
	_structureAnalysis(std::make_unique<StructureAnalysis>(positions, simCell, (StructureAnalysis::LatticeStructureType)inputCrystalStructure, selection(), structures(), std::move(preferredCrystalOrientations), !onlyPerfectDislocations)),
	_tessellation(std::make_unique<DelaunayTessellation>()),
	_elasticMapping(std::make_unique<ElasticMapping>(*_structureAnalysis, *_tessellation)),
	_interfaceMesh(std::make_unique<InterfaceMesh>(*_elasticMapping)),
	_dislocationTracer(std::make_unique<DislocationTracer>(*_interfaceMesh, _structureAnalysis->clusterGraph(), maxTrialCircuitSize, maxCircuitElongation)),
	_inputCrystalStructure(inputCrystalStructure),
	_crystalClusters(crystalClusters),
	_onlyPerfectDislocations(onlyPerfectDislocations),
	_defectMeshSmoothingLevel(defectMeshSmoothingLevel),
	_lineSmoothingLevel(lineSmoothingLevel),
	_linePointInterval(linePointInterval),
	_defectMesh(std::move(defectMesh)),
	_outputInterfaceMesh(std::move(outputInterfaceMesh))
{
	setAtomClusters(_structureAnalysis->atomClusters());
	setDislocationNetwork(_dislocationTracer->network());
	setClusterGraph(_dislocationTracer->clusterGraph());
}

/******************************************************************************
* Performs the actual analysis. This method is executed in a worker thread.
******************************************************************************/
void DislocationAnalysisEngine::perform()
{
	setProgressText(DislocationAnalysisModifier::tr("Dislocation analysis (DXA)"));

	beginProgressSubStepsWithWeights({ 35, 6, 1, 220, 60, 1, 53, 190, 146, 20, 4, 4 });
	if(!_structureAnalysis->identifyStructures())
		return;

	nextProgressSubStep();
	if(!_structureAnalysis->buildClusters())
		return;

	nextProgressSubStep();
	if(!_structureAnalysis->connectClusters())
		return;

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

	nextProgressSubStep();
	FloatType ghostLayerSize = FloatType(3.5) * _structureAnalysis->maximumNeighborDistance();
	if(!_tessellation->generateTessellation(_structureAnalysis->cell(), 
			ConstPropertyAccess<Point3>(positions()).cbegin(),
			_structureAnalysis->atomCount(), 
			ghostLayerSize, 
			false, // flag coverDomainWithFiniteTets
			selection() ? ConstPropertyAccess<int>(selection()).cbegin() : nullptr, 
			*this))
		return;

	// Build list of edges in the tessellation.
	nextProgressSubStep();
	if(!_elasticMapping->generateTessellationEdges(*this))
		return;

	// Assign each vertex to a cluster.
	nextProgressSubStep();
	if(!_elasticMapping->assignVerticesToClusters(*this))
		return;

	// Determine the ideal vector corresponding to each edge of the tessellation.
	nextProgressSubStep();
	if(!_elasticMapping->assignIdealVectorsToEdges(4, *this))
		return;

	// Free some memory that is no longer needed.
	_structureAnalysis->freeNeighborLists();

	// Create the mesh facets.
	nextProgressSubStep();
	if(!_interfaceMesh->createMesh(_structureAnalysis->maximumNeighborDistance(), crystalClusters(), *this))
		return;

	// Trace dislocation lines.
	nextProgressSubStep();
	if(!_dislocationTracer->traceDislocationSegments(*this))
		return;
	_dislocationTracer->finishDislocationSegments(_inputCrystalStructure);

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
		stream << "5\n";	// Triangle

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
	nextProgressSubStep();
	SurfaceMeshAccess defectMeshAccess(_defectMesh);
	if(!_interfaceMesh->generateDefectMesh(*_dislocationTracer, defectMeshAccess, *this))
		return;

#if 0
	_tessellation.dumpToVTKFile("tessellation.vtk");
#endif

	nextProgressSubStep();

	// Post-process surface mesh.
	if(_defectMeshSmoothingLevel > 0 && !defectMeshAccess.smoothMesh(_defectMeshSmoothingLevel, *this))
		return;

	nextProgressSubStep();

	// Post-process dislocation lines.
	if(_lineSmoothingLevel > 0 || _linePointInterval > 0) {
		if(!dislocationNetwork()->smoothDislocationLines(_lineSmoothingLevel, _linePointInterval, *this))
			return;
	}

	endProgressSubSteps();

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
	releaseWorkingData();
	_structureAnalysis.reset();
	_tessellation.reset();
	_elasticMapping.reset();
	_interfaceMesh.reset();
	_dislocationTracer.reset();
	_crystalClusters.reset();
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void DislocationAnalysisEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	DislocationAnalysisModifier* modifier = static_object_cast<DislocationAnalysisModifier>(request.modifier());
	StructureIdentificationEngine::applyResults(request, state);

	// Output defect mesh.
	state.addObjectWithUniqueId<SurfaceMesh>(_defectMesh);

	// Output interface mesh.
	if(_outputInterfaceMesh)
		state.addObjectWithUniqueId<SurfaceMesh>(_outputInterfaceMesh);

	// Output cluster graph.
	if(const ClusterGraphObject* oldClusterGraph = state.getObject<ClusterGraphObject>())
		state.removeObject(oldClusterGraph);
	ClusterGraphObject* clusterGraphObj = state.createObject<ClusterGraphObject>(request.modApp());
	clusterGraphObj->setStorage(clusterGraph());

	// Output dislocations.
	DislocationNetworkObject* dislocationsObj = state.createObject<DislocationNetworkObject>(request.modApp());
	dislocationsObj->setStorage(dislocationNetwork());
	while(!dislocationsObj->crystalStructures().empty())
		dislocationsObj->removeCrystalStructure(dislocationsObj->crystalStructures().size()-1);
	for(ElementType* stype : modifier->structureTypes())
		dislocationsObj->addCrystalStructure(static_object_cast<MicrostructurePhase>(stype));
	dislocationsObj->setDomain(state.getObject<SimulationCellObject>());
	dislocationsObj->setVisElement(modifier->dislocationVis());

	// Output particle properties.
	if(atomClusters()) {
		ParticlesObject* particles = state.expectMutableObject<ParticlesObject>();
		particles->createProperty(atomClusters());
	}

	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.OTHER"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_OTHER)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.FCC"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_FCC)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HCP"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_HCP)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.BCC"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_BCC)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.CubicDiamond"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_CUBIC_DIAMOND)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.counts.HexagonalDiamond"), QVariant::fromValue(getTypeCount(StructureAnalysis::LATTICE_HEX_DIAMOND)), request.modApp());
	state.addAttribute(QStringLiteral("DislocationAnalysis.cell_volume"), QVariant::fromValue(simCellVolume()), request.modApp());

	// Compute dislocation line statistics.
	FloatType totalLineLength = generateDislocationStatistics(request.modApp(), state, dislocationsObj, false, dislocationsObj->structureById(modifier->inputCrystalStructure()));
	size_t totalSegmentCount = dislocationsObj->storage()->segments().size();	

	if(totalSegmentCount == 0)
		state.setStatus(PipelineStatus(PipelineStatus::Success, DislocationAnalysisModifier::tr("No dislocations found")));
	else
		state.setStatus(PipelineStatus(PipelineStatus::Success, DislocationAnalysisModifier::tr("Found %1 dislocation segments\nTotal line length: %2").arg(totalSegmentCount).arg(totalLineLength)));
}

/******************************************************************************
* Computes statistical information on the identified dislocation lines and 
* outputs it to the pipeline as data tables and global attributes.
******************************************************************************/
FloatType  DislocationAnalysisEngine::generateDislocationStatistics(const PipelineObject* dataSource, PipelineFlowState& state, DislocationNetworkObject* dislocationsObj, bool replaceDataObjects, const MicrostructurePhase* defaultStructure)
{
	std::map<const BurgersVectorFamily*,FloatType> dislocationLengths;
	std::map<const BurgersVectorFamily*,int> segmentCounts;
	std::map<const BurgersVectorFamily*,const MicrostructurePhase*> dislocationCrystalStructures;

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
	for(const DislocationSegment* segment : dislocationsObj->storage()->segments()) {
		FloatType len = segment->calculateLength();
		totalLineLength += len;

		Cluster* cluster = segment->burgersVector.cluster();
		OVITO_ASSERT(cluster != nullptr);
		const MicrostructurePhase* structure = dislocationsObj->structureById(cluster->structure);
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
	for(const auto& entry : dislocationLengths)
		maxId = std::max(maxId, entry.first->numericId());
	PropertyAccessAndRef<FloatType> dislocationLengthsProperty = DataTable::OOClass().createUserProperty(maxId+1, PropertyObject::Float, 1, DislocationAnalysisModifier::tr("Total line length"), DataBuffer::InitializeMemory);
	for(const auto& entry : dislocationLengths)
		dislocationLengthsProperty[entry.first->numericId()] = entry.second;
	PropertyAccessAndRef<int> dislocationTypeIds = DataTable::OOClass().createUserProperty(maxId+1, PropertyObject::Int, 1, DislocationAnalysisModifier::tr("Dislocation type"));
	boost::algorithm::iota_n(dislocationTypeIds.begin(), 0, dislocationTypeIds.size());

	for(const auto& entry : dislocationLengths)
		dislocationTypeIds.buffer()->addElementType(entry.first);

	DataTable* lengthTableObj = replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-lengths")) : nullptr;
	if(!lengthTableObj) {
		lengthTableObj = state.createObject<DataTable>(QStringLiteral("disloc-lengths"), dataSource, DataTable::BarChart, DislocationAnalysisModifier::tr("Dislocation lengths"), dislocationLengthsProperty.take(), dislocationTypeIds.take());
		lengthTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
	}
	else {
		DataOORef<const PropertyObject> x = dislocationTypeIds.take();
		DataOORef<const PropertyObject> y = dislocationLengthsProperty.take();
		lengthTableObj->setContent(maxId + 1, DataRefVector<PropertyObject>{{ y, x }});
		lengthTableObj->setX(std::move(x));
		lengthTableObj->setY(std::move(y));
	}
	
	// Output a data table with the dislocation segment counts.
	PropertyAccessAndRef<int> dislocationCountsProperty = DataTable::OOClass().createUserProperty(maxId+1, PropertyObject::Int, 1, DislocationAnalysisModifier::tr("Dislocation count"), DataBuffer::InitializeMemory);
	for(const auto& entry : segmentCounts)
		dislocationCountsProperty[entry.first->numericId()] = entry.second;

	DataTable* countTableObj = replaceDataObjects ? state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("disloc-counts")) : nullptr;
	if(!countTableObj) {
		countTableObj = state.createObject<DataTable>(QStringLiteral("disloc-counts"), dataSource, DataTable::BarChart, DislocationAnalysisModifier::tr("Dislocation counts"), dislocationCountsProperty.take());
		countTableObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(DataTable::plotMode)});
	}
	else
		countTableObj->setContent(maxId+1, DataRefVector<PropertyObject>{{ dislocationCountsProperty.take() }});
	countTableObj->insertProperty(0, lengthTableObj->x());
	countTableObj->setX(lengthTableObj->x());

	if(replaceDataObjects)
		state.setAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), dataSource);
	else
		state.addAttribute(QStringLiteral("DislocationAnalysis.total_line_length"), QVariant::fromValue(totalLineLength), dataSource);

	for(const auto& dlen : dislocationLengths) {
		const MicrostructurePhase* structure = dislocationCrystalStructures[dlen.first];
		QString bstr;
		if(dlen.first->burgersVector() != Vector3::Zero()) {
			bstr = DislocationVis::formatBurgersVector(dlen.first->burgersVector(), structure);
			bstr.remove(QChar(' '));
			bstr.replace(QChar('['), QChar('<'));
			bstr.replace(QChar(']'), QChar('>'));
		}
		else bstr = "other";
		if(replaceDataObjects)
			state.setAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), dataSource);
		else
			state.addAttribute(QStringLiteral("DislocationAnalysis.length.%1").arg(bstr), QVariant::fromValue(dlen.second), dataSource);
	}

	return totalLineLength;
}

}	// End of namespace
