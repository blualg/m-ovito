////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//  Copyright 2020 Peter Mahler Larsen
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
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/particles/util/PTMNeighborFinder.h>
#include "GrainSegmentationEngine.h"
#include "GrainSegmentationModifier.h"
#include <ovito/core/utilities/DisjointSet.h>
#include "ThresholdSelection.h"

#include <boost/heap/priority_queue.hpp>
#include <ptm/ptm_functions.h>

#define DEBUG_OUTPUT 0
#if DEBUG_OUTPUT
#include <sys/time.h>
#endif

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
GrainSegmentationEngine1::GrainSegmentationEngine1(
            ConstPropertyPtr positions,
            ConstPropertyPtr structureProperty,
            ConstPropertyPtr orientationProperty,
            ConstPropertyPtr correspondenceProperty,
            const SimulationCell* simCell,
            GrainSegmentationModifier::MergeAlgorithm algorithmType,
            bool handleCoherentInterfaces,
            bool outputBonds) :
    _positions(std::move(positions)),
    _simCell(simCell),
    _algorithmType(algorithmType),
    _handleBoundaries(handleCoherentInterfaces),
    _structureTypes(structureProperty),
    _orientations(orientationProperty),
    _correspondences(correspondenceProperty),
    _outputBondsToPipeline(outputBonds)
{
    _numParticles = _positions->size();
}

/******************************************************************************
* The grain segmentation algorithm.
******************************************************************************/
void GrainSegmentationEngine1::perform()
{
    TaskProgress progress(this_task::ui());

    // First phase of grain segmentation algorithm:
    createNeighborBonds(progress);
    this_task::throwIfCanceled();
    rotateInterfaceAtoms(progress);
    this_task::throwIfCanceled();
    computeDisorientationAngles(progress);
    this_task::throwIfCanceled();
    determineMergeSequence(progress);
    this_task::throwIfCanceled();

    // Release data that is no longer needed.
    _positions.reset();

    //if(!_outputBondsToPipeline)
    //  decltype(_neighborBonds){}.swap(_neighborBonds);
}

/******************************************************************************
* Creates neighbor bonds from stored PTM data.
******************************************************************************/
void GrainSegmentationEngine1::createNeighborBonds(TaskProgress& progress)
{
    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - building neighbor lists"));

    PTMNeighborFinder neighFinder(false, positions(), cell(), nullptr, structureTypes(), orientations(), correspondences());

    // Perform analysis on each particle.
    EnumerableThreadSpecific<PTMNeighborFinder::Query> neighQueries;
    EnumerableThreadSpecific<std::vector<NeighborBond>> neighborBonds;
    parallelForInnerOuter(_numParticles, 1024, progress, [&](auto&& iterate) {

        // Construct thread-local neighbor finder.
        PTMNeighborFinder::Query& neighQuery = neighQueries.create(neighFinder);

        // Thread-local list of generated bonds connecting neighboring lattice atoms.
        std::vector<NeighborBond>& threadlocalNeighborBonds = neighborBonds.create();

        iterate([&](size_t index) {
            // Get PTM information.
            neighQuery.findNeighbors(index);
            auto structureType = neighQuery.structureType();
            int numNeighbors = neighQuery.neighborCount();
            if(structureType == PTMAlgorithm::OTHER)
                numNeighbors = std::min(numNeighbors, (int)MAX_DISORDERED_NEIGHBORS);

            for(int j = 0; j < numNeighbors; j++) {
                size_t neighborIndex = neighQuery.neighbors()[j].index;
                FloatType length = sqrt(neighQuery.neighbors()[j].distanceSq);

// TODO: apply canonical selection here rather than just using particle indices
                // Create a bond to the neighbor, but skip every other bond to create just one bond per particle pair.
                if(index < neighborIndex)
                    threadlocalNeighborBonds.push_back({index,
                                                        neighborIndex,
                                                        std::numeric_limits<FloatType>::infinity(),
                                                        length});

                // Check if neighbor vector spans more than half of a periodic simulation cell.
                Vector3 neighborVector = neighQuery.neighbors()[j].delta;
                for(size_t dim = 0; dim < 3; dim++) {
                    if(cell().hasPbc(dim)) {
                        if(std::abs(cell().reciprocalCellMatrix().prodrow(neighborVector, dim)) >= FloatType(0.5)+FLOATTYPE_EPSILON) {
                            static const QString axes[3] = { QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") };
                            throw Exception(GrainSegmentationModifier::tr("Simulation box is too short along cell vector %1 (%2) to perform analysis. "
                                    "Please extend it first using the 'Replicate' modifier.").arg(dim+1).arg(axes[dim]));
                        }
                    }
                }
            }
        });
    });

    // Concatenate thread-local bonds to global bonds list.
    neighborBonds.visitEach([&](const std::vector<NeighborBond>& r) {
        _neighborBonds.insert(_neighborBonds.end(), r.cbegin(), r.cend());
    });
}

bool GrainSegmentationEngine1::interface_cubic_hex(NeighborBond& bond, InterfaceHandler& interfaceHandler,
                                                   Quaternion& output)
{
    bond.disorientation = std::numeric_limits<FloatType>::infinity();
    if (!interfaceHandler.reorder_bond(bond, _adjustedStructureTypes)) {
        return false;
    }

    auto a = bond.a;
    auto b = bond.b;
    bond.disorientation = PTMAlgorithm::calculate_interfacial_disorientation(_adjustedStructureTypes[a],
                                                                             _adjustedStructureTypes[b],
                                                                             _adjustedOrientations[a],
                                                                             _adjustedOrientations[b],
                                                                             output);
    return bond.disorientation < _misorientationThreshold;
}

/******************************************************************************
* Rotates defect phase atoms to an equivalent parent-phase orientation.
******************************************************************************/
void GrainSegmentationEngine1::rotateInterfaceAtoms(TaskProgress& progress)
{
    // Make a copy of structure types and orientations.
    BufferReadAccess<PTMAlgorithm::StructureType> structuresArray(structureTypes());
    _adjustedStructureTypes = std::vector<PTMAlgorithm::StructureType>(structuresArray.cbegin(), structuresArray.cend());
    _adjustedOrientations = std::vector<Quaternion>(orientations()->size());
    std::ranges::transform(BufferReadAccess<QuaternionG>(orientations()), _adjustedOrientations.begin(), &QuaternionG::toDataType<FloatType>);

    // Only rotate hexagonal atoms if handling of coherent interfaces is enabled
    if(!_handleBoundaries)
        return;

    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - rotating minority atoms"));

    // Construct local neighbor list builder.
    PTMNeighborFinder neighFinder(false, positions(), cell(), nullptr, structureTypes(), orientations(), correspondences());
    PTMNeighborFinder::Query neighQuery(neighFinder);

    // TODO: replace comparator with a lambda function
    boost::heap::priority_queue<NeighborBond, boost::heap::compare<PriorityQueueCompare>> pq;

    Quaternion rotated;
    auto interfaceHandler = InterfaceHandler(structuresArray);

    // Populate priority queue with bonds at an cubic-hexagonal interface
    for(auto bond : _neighborBonds) {
        if(interface_cubic_hex(bond, interfaceHandler, rotated)) {
            pq.push({bond.a, bond.b, bond.disorientation});
        }
    }
    this_task::throwIfCanceled();

    while(pq.size()) {
        auto bond = *pq.begin();
        pq.pop();

        if(!interface_cubic_hex(bond, interfaceHandler, rotated)) {
            continue;
        }

        // flip structure from 'defect' phase to parent phase and adjust orientation
        size_t index = bond.b;
        auto defectStructureType = _adjustedStructureTypes[index];
        _adjustedStructureTypes[index] = interfaceHandler.parent_phase(defectStructureType);
        _adjustedOrientations[index] = rotated;

        // find neighbors to add to the queue
        neighQuery.findNeighbors(index);
        int numNeighbors = neighQuery.neighborCount();
        for(int j = 0; j < numNeighbors; j++) {
            size_t neighborIndex = neighQuery.neighbors()[j].index;
            bond.a = index;
            bond.b = neighborIndex;
            if(interface_cubic_hex(bond, interfaceHandler, rotated)) {
                pq.push({bond.a, bond.b, bond.disorientation});
            }
        }
    }
}

/******************************************************************************
* Calculates the disorientation angle for each graph edge (i.e. bond).
******************************************************************************/
void GrainSegmentationEngine1::computeDisorientationAngles(TaskProgress& progress)
{
    // Compute disorientation angles associated with the neighbor graph edges.
    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - misorientation calculation"));

    parallelFor(_neighborBonds.size(), 1024, progress, [&](size_t bondIndex) {
        NeighborBond& bond = _neighborBonds[bondIndex];
        bond.disorientation = PTMAlgorithm::calculate_disorientation(_adjustedStructureTypes[bond.a],
                                                                     _adjustedStructureTypes[bond.b],
                                                                     _adjustedOrientations[bond.a],
                                                                     _adjustedOrientations[bond.b]);
    });

    // Sort graph edges by disorientation.
    std::ranges::sort(_neighborBonds, [](const NeighborBond& a, const NeighborBond& b) {
        return a.disorientation < b.disorientation;
    });
}

/******************************************************************************
* Computes the disorientation angle between two crystal clusters of the
* given lattice type. Furthermore, the function computes the weighted average
* of the two cluster orientations. The norm of the two input quaternions
* and the output quaternion represents the size of the clusters.
******************************************************************************/
FloatType GrainSegmentationEngine1::calculate_disorientation(int structureType, Quaternion& qa, const Quaternion& qb)
{
    FloatType qa_norm = qa.norm();
    FloatType qb_norm = qb.norm();
    double qtarget[4] = { qa.w()/qa_norm, qa.x()/qa_norm, qa.y()/qa_norm, qa.z()/qa_norm };
    double q[4]    = { qb.w()/qb_norm, qb.x()/qb_norm, qb.y()/qb_norm, qb.z()/qb_norm };

    // Convert structure type back to PTM representation
    int type = 0;
    if(structureType == PTMAlgorithm::OTHER) {
        qWarning() << "Grain segmentation: remap failure - disordered structure input";
        return std::numeric_limits<FloatType>::max();
    }
    else if(structureType == PTMAlgorithm::FCC) type = PTM_MATCH_FCC;
    else if(structureType == PTMAlgorithm::HCP) type = PTM_MATCH_HCP;
    else if(structureType == PTMAlgorithm::BCC) type = PTM_MATCH_BCC;
    else if(structureType == PTMAlgorithm::SC) type = PTM_MATCH_SC;
    else if(structureType == PTMAlgorithm::CUBIC_DIAMOND) type = PTM_MATCH_DCUB;
    else if(structureType == PTMAlgorithm::HEX_DIAMOND) type = PTM_MATCH_DHEX;
    else if(structureType == PTMAlgorithm::GRAPHENE) type = PTM_MATCH_GRAPHENE;

    FloatType disorientation = (FloatType)ptm_map_and_calculate_disorientation(type, qtarget, q);
    if (disorientation == std::numeric_limits<FloatType>::infinity()) {
        qWarning() << "Grain segmentation: disorientation calculation failure";
        OVITO_ASSERT(false);
    }

    qa.w() += q[0] * qb_norm;
    qa.x() += q[1] * qb_norm;
    qa.y() += q[2] * qb_norm;
    qa.z() += q[3] * qb_norm;
    return disorientation;
}

/******************************************************************************
* Clustering using minimum spanning tree algorithm.
******************************************************************************/
void GrainSegmentationEngine1::minimum_spanning_tree_clustering(std::vector<Quaternion>& qsum, DisjointSet& uf, TaskProgress& progress)
{
    size_t progressVal = 0;
    for(const NeighborBond& edge : _neighborBonds) {

        if (edge.disorientation < _misorientationThreshold) {
            size_t pa = uf.find(edge.a);
            size_t pb = uf.find(edge.b);
            if(pa != pb && isCrystallineBond(edge)) {
                size_t parent = uf.merge(pa, pb);
                size_t child = (parent == pa) ? pb : pa;
                FloatType disorientation = calculate_disorientation(_adjustedStructureTypes[parent], qsum[parent], qsum[child]);
                OVITO_ASSERT(edge.a < edge.b);
                _dendrogram.emplace_back(parent, child, edge.disorientation, disorientation, 1, qsum[parent]);
            }
        }

        // Update progress indicator.
        if((progressVal++ % 1024) == 0) {
            progress.incrementValue(1024);
        }
    }
}

/******************************************************************************
* Builds grains by iterative region merging
******************************************************************************/
void GrainSegmentationEngine1::determineMergeSequence(TaskProgress& progress)
{
    // The graph used for the Node Pair Sampling methods
    Graph graph(_numParticles, neighborBonds().size());

    // Build graph.
    if(_algorithmType == GrainSegmentationModifier::GraphClusteringAutomatic || _algorithmType == GrainSegmentationModifier::GraphClusteringManual) {

        progress.setText(GrainSegmentationModifier::tr("Grain segmentation - building graph"));
        progress.setMaximum(neighborBonds().size());

        size_t counter = 0;
        for(auto edge: neighborBonds()) {
            if(isCrystallineBond(edge) && edge.disorientation < _misorientationThreshold) {
                FloatType weight = calculateGraphWeight(edge.disorientation);
                graph.add_edge(edge.a, edge.b, weight);
            }

            if((counter++ % 1024) == 0) {
                progress.incrementValue(1024);
            }
        }
    }

    // Build dendrogram.
    std::vector<Quaternion> qsum(_adjustedOrientations.cbegin(), _adjustedOrientations.cend());
    DisjointSet uf(_numParticles);
    _dendrogram.resize(0);

    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - region merging"));
    progress.setMaximum(_numParticles);  //TODO: make this num. crystalline particles

    if(_algorithmType == GrainSegmentationModifier::GraphClusteringAutomatic || _algorithmType == GrainSegmentationModifier::GraphClusteringManual) {
        node_pair_sampling_clustering(graph, qsum, progress);
    }
    else {
        minimum_spanning_tree_clustering(qsum, uf, progress);
    }
    this_task::throwIfCanceled();

    // Sort dendrogram entries by distance.
    std::ranges::sort(_dendrogram, [](const DendrogramNode& a, const DendrogramNode& b) { return a.distance < b.distance; });

    this_task::throwIfCanceled();

#if DEBUG_OUTPUT
char filename[128];
struct timeval tp;
gettimeofday(&tp, NULL);
long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
sprintf(filename, "dump_%lu.txt", ms);
FILE* fout = fopen(filename, "w");
#endif

    // Scan through the entire merge list to determine merge sizes.
    size_t numPlot = 0;
    uf.clear();
    for(DendrogramNode& node : _dendrogram) {
        size_t sa = uf.nodesize(uf.find(node.a));
        size_t sb = uf.nodesize(uf.find(node.b));
        size_t dsize = std::min(sa, sb);
        node.merge_size = 2. / (1. / sa + 1. / sb);    //harmonic mean
        uf.merge(node.a, node.b);

#if DEBUG_OUTPUT
if (fout)
    fprintf(fout, "%lu %lu %lu %lu %lu %e\n", node.a, node.b, sa, sb, dsize, node.distance);
#endif

        // We don't want to plot very small merges - they extend the x-axis by a lot and don't provide much useful information
        node.size = dsize;
        if(dsize >= _minPlotSize) {
            numPlot++;
        }
    }

#if DEBUG_OUTPUT
fclose(fout);
#endif

    if(_algorithmType == GrainSegmentationModifier::GraphClusteringAutomatic || _algorithmType == GrainSegmentationModifier::GraphClusteringManual) {

        // Create PropertyStorage objects for the output plot.
        BufferWriteAccess<FloatType, access_mode::discard_read_write> mergeDistanceArray = _mergeDistance = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Log merge distance"));
        BufferWriteAccess<FloatType, access_mode::discard_read_write> mergeSizeArray = _mergeSize = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Delta merge size"));

        // Generate output data plot points from dendrogram data.
        FloatType* mergeDistanceIter = mergeDistanceArray.begin();
        FloatType* mergeSizeIter = mergeSizeArray.begin();
        for(const DendrogramNode& node : _dendrogram) {
            if(node.size >= _minPlotSize) {
                *mergeDistanceIter++ = std::log(node.distance);
                *mergeSizeIter++ = node.size;
            }
        }

        auto regressor = ThresholdSelection::Regressor(_dendrogram);
        _suggestedMergingThreshold = regressor.calculate_threshold(_dendrogram, 1.5);

        // Create PropertyStorage objects for the output plot.
        numPlot = 0;
        for(auto y : regressor.ys) {
            numPlot += (y > 0) ? 1 : 0; // plot positive distances only, for clarity
        }

        BufferWriteAccess<FloatType, access_mode::discard_write> logMergeSizeArray = _logMergeSize = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Log geometric merge size"));
        BufferWriteAccess<FloatType, access_mode::discard_write> logMergeDistanceArray = _logMergeDistance = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Log merge distance"));

        // Generate output data plot points from dendrogram data.
        FloatType* logMergeDistanceIter = logMergeDistanceArray.begin();
        FloatType* logMergeSizeIter = logMergeSizeArray.begin();
        for(size_t i=0;i<regressor.residuals.size();i++) {
            if(regressor.ys[i] > 0) {
                *logMergeSizeIter++ = regressor.xs[i];
                *logMergeDistanceIter++ = regressor.ys[i];
            }
        }
    }
    else {
        // Create PropertyStorage objects for the output plot.
        BufferWriteAccess<FloatType, access_mode::discard_write> mergeDistanceArray = _mergeDistance = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Misorientation (degrees)"));
        BufferWriteAccess<FloatType, access_mode::discard_write> mergeSizeArray = _mergeSize = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, numPlot, DataBuffer::FloatDefault, 1, GrainSegmentationModifier::tr("Merge size"));

        // Generate output data plot points from dendrogram data.
        FloatType* mergeDistanceIter = mergeDistanceArray.begin();
        FloatType* mergeSizeIter = mergeSizeArray.begin();
        for(const DendrogramNode& node : _dendrogram) {
            if(node.size >= _minPlotSize) {
                *mergeDistanceIter++ = node.distance;
                *mergeSizeIter++ = node.size;
            }
        }
    }
}

/******************************************************************************
* The grain segmentation algorithm.
******************************************************************************/
void GrainSegmentationEngine2::perform()
{
    // Second phase: Execute merge steps up to the threshold set by the user or the adaptively determined threshold.
    TaskProgress progress(this_task::ui());
    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - merging clusters"));

    // Either use user-defined merge threshold or automatically computed threshold.
    FloatType mergingThreshold = _mergingThreshold;
    if(_engine1->_algorithmType == GrainSegmentationModifier::GraphClusteringAutomatic) {
        mergingThreshold = _engine1->suggestedMergingThreshold();
    }

    if(_engine1->_algorithmType == GrainSegmentationModifier::MinimumSpanningTree) {
        mergingThreshold = log(mergingThreshold);
    }

    const std::vector<GrainSegmentationEngine1::DendrogramNode>& dendrogram = _engine1->_dendrogram;

    std::vector<Quaternion> meanOrientation(_engine1->orientations()->size());
    std::ranges::transform(BufferReadAccess<QuaternionG>(_engine1->orientations()), meanOrientation.begin(), [](const QuaternionG& q) { return q.toDataType<FloatType>(); });

    // Iterate through merge list until distance cutoff is met.
    DisjointSet uf(_numParticles);
    for(auto node = dendrogram.cbegin(); node != dendrogram.cend(); ++node) {
        this_task::throwIfCanceled();

        if(std::log(node->distance) > mergingThreshold)
            break;

        uf.merge(node->a, node->b);
        size_t parent = uf.find(node->a);
        OVITO_ASSERT(node->orientation.norm() > FLOATTYPE_EPSILON);
        meanOrientation[parent] = node->orientation;
    }

    // Relabels the clusters to obtain a contiguous sequence of cluster IDs.
    std::vector<size_t> clusterRemapping(_numParticles);

    // Assign new consecutive IDs to root clusters.
    _numClusters = 1;
    BufferReadAccess<int32_t> structuresArray(_engine1->structureTypes());
    std::vector<int> clusterStructureTypes;
    std::vector<Quaternion> clusterOrientations;
    for(size_t i = 0; i < _numParticles; i++) {
        if(uf.find(i) == i) {
            // If the cluster's size is below the threshold, dissolve the cluster.
            if(uf.nodesize(i) < _minGrainAtomCount || structuresArray[i] == PTMAlgorithm::OTHER) {
                clusterRemapping[i] = 0;
            }
            else {
                clusterRemapping[i] = _numClusters;
                _numClusters++;
                clusterStructureTypes.push_back(structuresArray[i]);
                clusterOrientations.push_back(meanOrientation[i].normalized());
            }
        }
    }
    this_task::throwIfCanceled();

    // Allocate and fill output array storing the grain IDs (1-based identifiers).
    _grainIds =  DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, _numClusters - 1, Property::IntIdentifier, 1, QStringLiteral("Grain Identifier"));
    boost::algorithm::iota_n(BufferWriteAccess<IdentifierIntType, access_mode::discard_write>(_grainIds).begin(), IdentifierIntType{1}, _grainIds->size());
    this_task::throwIfCanceled();

    // Allocate output array storing the grain sizes.
    _grainSizes = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, _numClusters - 1, DataBuffer::Int64, 1, QStringLiteral("Grain Size"));

    // Allocate output array storing the structure type of grains.
    _grainStructureTypes = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, _numClusters - 1, Property::Int32, 1, QStringLiteral("Structure Type"));
    std::ranges::copy(clusterStructureTypes, BufferWriteAccess<int32_t, access_mode::discard_write>(_grainStructureTypes).begin());
    // Transfer the set of PTM crystal structure types to the structure column of the grain table.
    for(const ElementType* type : _engine1->structureTypes()->elementTypes()) {
        if(type->enabled())
            _grainStructureTypes->addElementType(type);
    }
    this_task::throwIfCanceled();

    // Allocate output array with each grain's unique color.
    // Fill it with random color values (using constant random seed to keep it reproducible).
    _grainColors = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, _numClusters - 1, DataBuffer::FloatGraphics, 3, QStringLiteral("Color"), 0, QStringList() << QStringLiteral("R") << QStringLiteral("G") << QStringLiteral("B"));
    std::default_random_engine rng(1);
    boost::random::uniform_real_distribution<FloatType> uniform_dist(0, 1);
    std::ranges::generate(BufferWriteAccess<ColorG, access_mode::discard_write>(_grainColors), [&]() { return ColorG::fromHSV(static_cast<GraphicsFloatType>(uniform_dist(rng)), 1.0f - static_cast<GraphicsFloatType>(uniform_dist(rng)) * 0.8f, 1.0f - static_cast<GraphicsFloatType>(uniform_dist(rng)) * 0.5f); });
    this_task::throwIfCanceled();

    // Allocate output array storing the mean lattice orientation of grains (represented by a quaternion).
    _grainOrientations = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, _numClusters - 1, DataBuffer::FloatDefault, 4, QStringLiteral("Orientation"), 0, QStringList() << QStringLiteral("X") << QStringLiteral("Y") << QStringLiteral("Z") << QStringLiteral("W"));
    OVITO_ASSERT(clusterOrientations.size() == _grainOrientations->size());
    std::ranges::copy(clusterOrientations, BufferWriteAccess<Quaternion, access_mode::discard_write>(_grainOrientations).begin());

    // Determine new IDs for non-root clusters.
    for(size_t particleIndex = 0; particleIndex < _numParticles; particleIndex++)
        clusterRemapping[particleIndex] = clusterRemapping[uf.find(particleIndex)];
    this_task::throwIfCanceled();

    // Relabel atoms after cluster IDs have changed.
    // Also count the number of atoms in each cluster.
    {
        BufferWriteAccess<int64_t, access_mode::read_write> atomClustersArray(atomClusters());
        BufferWriteAccess<int64_t, access_mode::read_write> grainSizeArray(_grainSizes);
        for(size_t particleIndex = 0; particleIndex < _numParticles; particleIndex++) {
            size_t gid = clusterRemapping[particleIndex];
            atomClustersArray[particleIndex] = gid;
            if(gid != 0)
                grainSizeArray[gid - 1]++;
        }
    }
    this_task::throwIfCanceled();

    // Reorder grains by size (large to small).
    if(_numClusters > 1) {

        // Determine the index remapping for reordering the grain list by size.
        std::vector<size_t> mapping(_numClusters - 1);
        boost::algorithm::iota(mapping, size_t(0));
        std::ranges::sort(mapping, [grainSizeArray = BufferReadAccess<int64_t>(_grainSizes)](size_t a, size_t b) {
            return grainSizeArray[a] > grainSizeArray[b];
        });
        this_task::throwIfCanceled();

        // Use index map to reorder grain data arrays.
        _grainSizes->reorderElements(mapping);
        _grainStructureTypes->reorderElements(mapping);
        _grainOrientations->reorderElements(mapping);
        this_task::throwIfCanceled();

        // Invert the grain index map.

        std::vector<size_t> inverseMapping(_numClusters);
        inverseMapping[0] = 0; // Keep cluster ID 0 in place.
        for(size_t i = 1; i < _numClusters; i++)
            inverseMapping[mapping[i-1]+1] = i;

        // Remap per-particle grain IDs.

        for(auto& id : BufferWriteAccess<int64_t, access_mode::read_write>(atomClusters()))
            id = inverseMapping[id];
        this_task::throwIfCanceled();

        // Adopt orphan atoms.
        if(_adoptOrphanAtoms)
            mergeOrphanAtoms(progress);
    }
}

/******************************************************************************
* Merges any orphan atoms into the closest cluster.
******************************************************************************/
void GrainSegmentationEngine2::mergeOrphanAtoms(TaskProgress& progress)
{
    progress.setText(GrainSegmentationModifier::tr("Grain segmentation - merging orphan atoms"));
    progress.setValue(0);

    BufferWriteAccess<int64_t, access_mode::read_write> atomClustersArray(atomClusters());
    BufferWriteAccess<int64_t, access_mode::read_write> grainSizeArray(_grainSizes);

    /// The bonds connecting neighboring non-crystalline atoms.
    std::vector<GrainSegmentationEngine1::NeighborBond> noncrystallineBonds;
    for(auto nb : _engine1->neighborBonds()) {
        if (atomClustersArray[nb.a] == 0 || atomClustersArray[nb.b] == 0) {
            // Add bonds for both atoms
            noncrystallineBonds.push_back(nb);

            std::swap(nb.a, nb.b);
            noncrystallineBonds.push_back(nb);
        }
    }
    this_task::throwIfCanceled();

    std::ranges::sort(noncrystallineBonds,
                 [](const GrainSegmentationEngine1::NeighborBond& a, const GrainSegmentationEngine1::NeighborBond& b)
                 {return a.a < b.a;});

    boost::heap::priority_queue<PQNode, boost::heap::compare<PQCompareLength>> pq;

    // Populate priority queue with bonds at a crystalline-noncrystalline interface
    for(auto bond : _engine1->neighborBonds()) {
        auto clusterA = atomClustersArray[bond.a];
        auto clusterB = atomClustersArray[bond.b];

        if(clusterA != 0 && clusterB == 0) {
            pq.push({clusterA, bond.b, bond.length});
        }
        else if(clusterA == 0 && clusterB != 0) {
            pq.push({clusterB, bond.a, bond.length});
        }
    }

    while(pq.size()) {
        auto node = *pq.begin();
        pq.pop();

        if(atomClustersArray[node.particleIndex] != 0)
            continue;

        atomClustersArray[node.particleIndex] = node.cluster;
        grainSizeArray[node.cluster - 1]++;

        // Get the range of bonds adjacent to the current atom.
        auto bondsRange = std::ranges::equal_range(noncrystallineBonds, GrainSegmentationEngine1::NeighborBond{node.particleIndex, 0, 0, 0},
            [](const GrainSegmentationEngine1::NeighborBond& a, const GrainSegmentationEngine1::NeighborBond& b)
            { return a.a < b.a; });

        // Find the closest cluster atom in the neighborhood (using PTM ordering).
        for(const GrainSegmentationEngine1::NeighborBond& bond : bondsRange) {
            OVITO_ASSERT(bond.a == node.particleIndex);

            auto neighborIndex = bond.b;
            if(neighborIndex == std::numeric_limits<size_t>::max()) break;
            if(atomClustersArray[neighborIndex] != 0) continue;

            pq.push({node.cluster, neighborIndex, node.length + bond.length});
        }
    }
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void GrainSegmentationEngine1::applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, BondsVis* bondsVis) const
{
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    // Output the edges of the neighbor graph.
    if(_outputBondsToPipeline) {

        std::vector<Bond> bonds;
        std::vector<FloatType> disorientations;
        BufferReadAccess<Point3> positionsArray(particles->expectProperty(Particles::PositionProperty));

        for(auto edge : neighborBonds()) {
            if(isCrystallineBond(edge)) {
                Bond bond;
                bond.index1 = edge.a;
                bond.index2 = edge.b;
                disorientations.push_back(edge.disorientation);

                // Determine PBC bond shift using minimum image convention.
                Vector3 delta = positionsArray[bond.index1] - positionsArray[bond.index2];
                for(size_t dim = 0; dim < 3; dim++) {
                    if(cell().pbcFlags()[dim])
                        bond.pbcShift[dim] = (int)std::floor(cell().reciprocalCellMatrix().prodrow(delta, dim) + FloatType(0.5));
                    else
                        bond.pbcShift[dim] = 0;
                }

                bonds.push_back(bond);
            }
        }

        // Output disorientation angles as a bond property.
        PropertyPtr neighborDisorientationAngles = Bonds::OOClass().createUserProperty(DataBuffer::Uninitialized, bonds.size(), DataBuffer::FloatDefault, 1, QStringLiteral("Disorientation"));
        BufferWriteAccess<FloatType, access_mode::discard_write> disorientationAnglesAccess(neighborDisorientationAngles);
        for(size_t i = 0; i < disorientations.size(); i++) {
            disorientationAnglesAccess[i] = disorientations[i];
        }
        disorientationAnglesAccess.reset();

        particles->addBonds(bonds, bondsVis, { std::move(neighborDisorientationAngles) });
    }

    // Output a data plot with the dendrogram points.
    if(mergeSize() && mergeDistance())
        state.createObject<DataTable>(QStringLiteral("grains-merge"), createdByNode, DataTable::Scatter, GrainSegmentationModifier::tr("Merge size vs. distance"), mergeSize(), mergeDistance());

    // Output a data plot with the log-log dendrogram points.
    if(logMergeSize() && logMergeDistance())
        state.createObject<DataTable>(QStringLiteral("grains-log"), createdByNode, DataTable::Scatter, GrainSegmentationModifier::tr("Log distance vs. log merge size"), logMergeDistance(), logMergeSize());

    if(_algorithmType == GrainSegmentationModifier::GraphClusteringAutomatic)
        state.addAttribute(QStringLiteral("GrainSegmentation.auto_merge_threshold"), QVariant::fromValue(suggestedMergingThreshold()), createdByNode);
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void GrainSegmentationEngine2::applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, BondsVis* bondsVis) const
{
    // Output the results from the 1st algorithm stage.
    _engine1->applyResults(state, createdByNode, bondsVis);

    Particles* particles = state.expectMutableObject<Particles>();

    // Output per-particle properties.
    if(atomClusters()) {
        particles->createProperty(atomClusters());

        if(_colorParticlesByGrain) {

            // Assign colors to particles according to the grains they belong to.
            BufferReadAccess<ColorG> grainColorsArray(_grainColors);
            BufferWriteAccess<ColorG, access_mode::discard_write> particleColorsArray = particles->createProperty(Particles::ColorProperty);
            std::ranges::transform(BufferReadAccess<int64_t>(atomClusters()), particleColorsArray.begin(), [&](int64_t cluster) {
                if(cluster != 0)
                    return grainColorsArray[cluster - 1];
                else
                    return ColorG(0.8f, 0.8f, 0.8f); // Special color for non-crystalline particles not part of any grain.
            });
        }
    }

    // Output a data table with the list of grains.
    // The X-column consists of the grain IDs, the Y-column contains the grain sizes.
    DataTable* grainTable = state.createObject<DataTable>(QStringLiteral("grains"), createdByNode, DataTable::Scatter, GrainSegmentationModifier::tr("Grain list"), _grainSizes, _grainIds);
    // Add extra columns to the table containing other per-grain data.
    grainTable->createProperty(_grainColors);
    grainTable->createProperty(_grainStructureTypes);
    grainTable->createProperty(_grainOrientations);

    size_t numGrains = 0;
    if(atomClusters()->size() != 0) {
        BufferReadAccess<int64_t> atomClustersData(atomClusters());
        numGrains = *std::ranges::max_element(atomClustersData);
    }

    state.addAttribute(QStringLiteral("GrainSegmentation.grain_count"), QVariant::fromValue(numGrains), createdByNode);

    state.setStatus(PipelineStatus(
        GrainSegmentationModifier::tr("Found %1 grains").arg(numGrains),
        GrainSegmentationModifier::tr("%1 grains").arg(numGrains)));
}

}   // End of namespace
