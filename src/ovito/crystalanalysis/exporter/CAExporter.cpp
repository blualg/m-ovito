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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include "CAExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CAExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> CAExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(any_moveonly&& frameData, int frameNumber, const QString& filePath) override {
            // The exportable frame data.
            const PipelineFlowState state = any_cast<PipelineFlowState>(std::move(frameData));

            // Perform the following in a worker thread.
            co_await ExecutorAwaiter(ThreadPoolExecutor());

            // The CA file exporter, which manages the export settings.
            const CAExporter* exporter = static_cast<const CAExporter*>(this->exporter());

            // Get simulation cell info.
            const SimulationCell* simulationCell = state.expectObject<SimulationCell>();

            // Get dislocation lines.
            const DislocationNetwork* dislocations = state.getObject<DislocationNetwork>();

            // Get defect surface mesh.
            const SurfaceMesh* defectMesh = exporter->meshExportEnabled() ? state.getObject<SurfaceMesh>() : nullptr;

            if(!dislocations && !defectMesh)
                throw Exception(tr("Dataset to be exported contains no dislocation lines nor a surface mesh. Cannot write CA file."));

            // Write file header.
            textStream() << "CA_FILE_VERSION 6\n";
            textStream() << "CA_LIB_VERSION 0.0.0\n";

            std::vector<const MicrostructurePhase*> crystalStructures;
            if(dislocations) {
                for(const MicrostructurePhase* phase : dislocations->crystalStructures()) {
                    if(phase->numericId() != 0)
                        crystalStructures.push_back(phase);
                }
            }

            // Write list of structure types.
            textStream() << "STRUCTURE_TYPES " << crystalStructures.size() << "\n";
            for(const MicrostructurePhase* s : crystalStructures) {
                textStream() << "STRUCTURE_TYPE " << s->numericId() << "\n";
                textStream() << "NAME " << (s->shortName().isEmpty() ? s->name() : s->shortName()) << "\n";
                textStream() << "FULL_NAME " << s->longName() << "\n";
                textStream() << "COLOR " << s->color().r() << " " << s->color().g() << " " << s->color().b() << "\n";
                if(s->dimensionality() == MicrostructurePhase::Dimensionality::Volumetric) textStream() << "TYPE LATTICE\n";
                else if(s->dimensionality() == MicrostructurePhase::Dimensionality::Planar) textStream() << "TYPE INTERFACE\n";
                else if(s->dimensionality() == MicrostructurePhase::Dimensionality::Pointlike) textStream() << "TYPE POINTDEFECT\n";
                textStream() << "BURGERS_VECTOR_FAMILIES " << s->burgersVectorFamilies().size() << "\n";
                int bvfId = 0;
                for(const BurgersVectorFamily* bvf : s->burgersVectorFamilies()) {
                    textStream() << "BURGERS_VECTOR_FAMILY ID " << bvfId << "\n" << bvf->name() << "\n";
                    textStream() << bvf->burgersVector().x() << " " << bvf->burgersVector().y() << " " << bvf->burgersVector().z() << "\n";
                    textStream() << bvf->color().r() << " " << bvf->color().g() << " " << bvf->color().b() << "\n";
                    bvfId++;
                }
                textStream() << "END_STRUCTURE_TYPE\n";
            }

            // Write simulation cell geometry.
            const AffineTransformation& cell = simulationCell->cellMatrix();
            textStream() << "SIMULATION_CELL_ORIGIN " << cell.column(3).x() << " " << cell.column(3).y() << " " << cell.column(3).z() << "\n";
            textStream() << "SIMULATION_CELL_MATRIX" << "\n"
                    << cell.column(0).x() << " " << cell.column(1).x() << " " << cell.column(2).x() << "\n"
                    << cell.column(0).y() << " " << cell.column(1).y() << " " << cell.column(2).y() << "\n"
                    << cell.column(0).z() << " " << cell.column(1).z() << " " << cell.column(2).z() << "\n";
            textStream() << "PBC_FLAGS "
                    << (int)simulationCell->pbcFlags()[0] << " "
                    << (int)simulationCell->pbcFlags()[1] << " "
                    << (int)simulationCell->pbcFlags()[2] << "\n";

            // Select the dislocation network to be exported.
            // Optionally, convert the selected Microstructure object to a DislocationNetwork object for export.
            const auto clusterGraph = dislocations->clusterGraph();

            // Write list of clusters.
            if(clusterGraph) {
                textStream() << "CLUSTERS " << (clusterGraph->clusters().size() - 1) << "\n";
                for(const Cluster* cluster : clusterGraph->clusters()) {
                    if(cluster->id == 0) continue;
                    OVITO_ASSERT(clusterGraph->clusters()[cluster->id] == cluster);
                    textStream() << "CLUSTER " << cluster->id << "\n";
                    textStream() << "CLUSTER_STRUCTURE " << cluster->structure << "\n";
                    textStream() << "CLUSTER_ORIENTATION\n";
                    for(size_t row = 0; row < 3; row++)
                        textStream() << cluster->orientation(row,0) << " " << cluster->orientation(row,1) << " " << cluster->orientation(row,2) << "\n";
                    textStream() << "CLUSTER_COLOR " << cluster->color.r() << " " << cluster->color.g() << " " << cluster->color.b() << "\n";
                    textStream() << "CLUSTER_SIZE " << cluster->atomCount << "\n";
                    textStream() << "END_CLUSTER\n";
                }

                // Count cluster transitions.
                size_t numClusterTransitions = 0;
                for(const ClusterTransition* t : clusterGraph->clusterTransitions()) {
                    if(!t->isSelfTransition())
                        numClusterTransitions++;
                }

                // Serialize cluster transitions.
                textStream() << "CLUSTER_TRANSITIONS " << numClusterTransitions << "\n";
                for(const ClusterTransition* t : clusterGraph->clusterTransitions()) {
                    if(t->isSelfTransition()) continue;
                    textStream() << "TRANSITION " << (t->cluster1->id - 1) << " " << (t->cluster2->id - 1) << "\n";
                    textStream() << t->tm.column(0).x() << " " << t->tm.column(1).x() << " " << t->tm.column(2).x() << " "
                            << t->tm.column(0).y() << " " << t->tm.column(1).y() << " " << t->tm.column(2).y() << " "
                            << t->tm.column(0).z() << " " << t->tm.column(1).z() << " " << t->tm.column(2).z() << "\n";
                }
            }

            if(dislocations) {
                // Write list of dislocation segments.
                textStream() << "DISLOCATIONS " << dislocations->segments().size() << "\n";
                for(const DislocationSegment* segment : dislocations->segments()) {

                    // Make sure consecutive identifiers have been assigned to segments.
                    OVITO_ASSERT(segment->id >= 0 && segment->id < dislocations->segments().size());
                    OVITO_ASSERT(dislocations->segments()[segment->id] == segment);

                    textStream() << segment->id << "\n";
                    textStream() << segment->burgersVector.localVec().x() << " " << segment->burgersVector.localVec().y() << " " << segment->burgersVector.localVec().z() << "\n";
                    textStream() << segment->burgersVector.cluster()->id << "\n";

                    // Write polyline.
                    textStream() << segment->line.size() << "\n";
                    if(segment->coreSize.empty()) {
                        for(const Point3& p : segment->line) {
                            textStream() << p.x() << " " << p.y() << " " << p.z() << "\n";
                        }
                    }
                    else {
                        OVITO_ASSERT(segment->coreSize.size() == segment->line.size());
                        auto cs = segment->coreSize.cbegin();
                        for(const Point3& p : segment->line) {
                            textStream() << p.x() << " " << p.y() << " " << p.z() << " " << (*cs++) << "\n";
                        }
                    }
                }

                // Write dislocation connectivity information.
                textStream() << "DISLOCATION_JUNCTIONS\n";
                for(const DislocationSegment* segment : dislocations->segments()) {
                    OVITO_ASSERT(segment->forwardNode().junctionRing->segment->id < dislocations->segments().size());
                    OVITO_ASSERT(segment->backwardNode().junctionRing->segment->id < dislocations->segments().size());

                    for(int nodeIndex = 0; nodeIndex < 2; nodeIndex++) {
                        const DislocationNode* otherNode = segment->nodes[nodeIndex]->junctionRing;
                        textStream() << (int)otherNode->isForwardNode() << " " << otherNode->segment->id << "\n";
                    }
                }
            }

            if(defectMesh && defectMesh->topology()->isClosed()) {
                defectMesh->verifyMeshIntegrity();
                const Property* vertexCoords = defectMesh->vertices()->getProperty(SurfaceMeshVertices::PositionProperty);
                const SurfaceMeshTopology* topology = defectMesh->topology();

                // Serialize list of vertices.
                textStream() << "DEFECT_MESH_VERTICES " << vertexCoords->size() << "\n";
                for(const Point3& vertex : BufferReadAccess<Point3>(vertexCoords)) {
                    textStream() << vertex.x() << " " << vertex.y() << " " << vertex.z() << "\n";
                }

                // Serialize list of facets.
                textStream() << "DEFECT_MESH_FACETS " << topology->faceCount() << "\n";
                for(SurfaceMesh::face_index face = 0; face < topology->faceCount(); face++) {
                    SurfaceMesh::edge_index e = topology->firstFaceEdge(face);
                    do {
                        textStream() << topology->vertex1(e) << " ";
                        e = topology->nextFaceEdge(e);
                    }
                    while(e != topology->firstFaceEdge(face));
                    textStream() << "\n";
                }

                // Serialize face adjacency information.
                for(SurfaceMesh::face_index face = 0; face < topology->faceCount(); face++) {
                    SurfaceMesh::edge_index e = topology->firstFaceEdge(face);
                    do {
                        textStream() << topology->adjacentFace(topology->oppositeEdge(e)) << " ";
                        e = topology->nextFaceEdge(e);
                    }
                    while(e != topology->firstFaceEdge(face));
                    textStream() << "\n";
                }
            }
        }
    };

    return OORef<Job>::create(this, filePath, true);}

}   // End of namespace
