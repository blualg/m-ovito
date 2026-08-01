////////////////////////////////////////////////////////////////////////////////////////
//
//  Ring Finder modifier for m-ovito.
//
//  This native implementation is based on the ovito-org/RingFinder extension
//  (GPL-3.0-only). This file is distributed under the GNU General Public
//  License version 3 only.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/mesh/surface/SurfaceMeshFaces.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include "RingFinderModifier.h"

#include <algorithm>
#include <deque>
#include <map>
#include <numeric>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(RingFinderModifier);
OVITO_CLASSINFO(RingFinderModifier, "Description", "Find shortest rings formed by bonds.");
OVITO_CLASSINFO(RingFinderModifier, "DisplayName", "Ring Finder");
OVITO_CLASSINFO(RingFinderModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(RingFinderModifier, minimumRingSize);
DEFINE_PROPERTY_FIELD(RingFinderModifier, maximumRingSize);
DEFINE_PROPERTY_FIELD(RingFinderModifier, createPolygons);
SET_PROPERTY_FIELD_LABEL(RingFinderModifier, minimumRingSize, "Minimum ring size");
SET_PROPERTY_FIELD_LABEL(RingFinderModifier, maximumRingSize, "Maximum ring size");
SET_PROPERTY_FIELD_LABEL(RingFinderModifier, createPolygons, "Create polygons");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RingFinderModifier, minimumRingSize, IntegerParameterUnit, 3);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(RingFinderModifier, maximumRingSize, IntegerParameterUnit, 3);

namespace {

struct SearchLabel
{
    int parent = -1;
    int distance = 0;
    bool active = false;
};

using LabelMap = std::unordered_map<int, SearchLabel>;

struct RingRecord
{
    int size = 0;
    std::vector<int> particles;
};

std::vector<std::vector<int>> buildAdjacency(const Bonds& bonds, size_t particleCount)
{
    BufferReadAccess<ParticleIndexPair> topology(bonds.expectProperty(Bonds::TopologyProperty));
    std::vector<std::vector<int>> adjacency(particleCount);

    for(const ParticleIndexPair& pair : topology) {
        const int64_t a64 = pair[0];
        const int64_t b64 = pair[1];
        if(a64 < 0 || b64 < 0)
            continue;
        if(static_cast<size_t>(a64) >= particleCount || static_cast<size_t>(b64) >= particleCount)
            continue;
        const int a = static_cast<int>(a64);
        const int b = static_cast<int>(b64);
        if(a == b)
            continue;
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
    }

    for(std::vector<int>& neighbors : adjacency) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
    return adjacency;
}

std::vector<LabelMap> buildSearchLabels(const std::vector<std::vector<int>>& adjacency,
                                        int maximumRingSize,
                                        TaskProgress& progress)
{
    const int maximumDepth = (maximumRingSize + 1) / 2;
    std::vector<LabelMap> labels(adjacency.size());

    progress.setText(RingFinderModifier::tr("Building ring-search graph"));
    progress.setMaximum(static_cast<qlonglong>(std::max<size_t>(adjacency.size(), 1)));

    for(size_t start = 0; start < adjacency.size(); ++start) {
        this_task::throwIfCanceled();
        progress.setValueIntermittent(static_cast<qlonglong>(start));

        LabelMap& startLabels = labels[start];
        startLabels.reserve(64);
        startLabels.emplace(static_cast<int>(start), SearchLabel{static_cast<int>(start), 0, false});

        std::deque<int> queue;
        queue.push_back(static_cast<int>(start));
        while(!queue.empty()) {
            const int vertex = queue.front();
            queue.pop_front();

            const SearchLabel vertexLabel = startLabels.at(vertex);
            if(vertexLabel.distance >= maximumDepth)
                continue;

            for(int neighbor : adjacency[vertex]) {
                if(neighbor == vertexLabel.parent)
                    continue;
                if(startLabels.find(neighbor) != startLabels.end())
                    continue;

                startLabels.emplace(neighbor, SearchLabel{vertex, vertexLabel.distance + 1, false});
                queue.push_back(neighbor);
            }
        }
    }

    progress.setValue(static_cast<qlonglong>(adjacency.size()));
    return labels;
}

bool hasLowerStartAtomOnParentPath(const LabelMap& labels, int start, int atom)
{
    int current = atom;
    while(true) {
        const auto iter = labels.find(current);
        if(iter == labels.end())
            return false;

        const int parent = iter->second.parent;
        if(parent == current)
            return false;
        if(parent < start)
            return true;
        current = parent;
    }
}

int labelDistance(const std::vector<LabelMap>& labels, int start, int target)
{
    if(start < 0 || target < 0 || static_cast<size_t>(start) >= labels.size())
        return -1;
    const auto iter = labels[start].find(target);
    return iter == labels[start].end() ? -1 : iter->second.distance;
}

bool validateAntipodes(const std::vector<LabelMap>& labels, const std::vector<int>& ring)
{
    const int size = static_cast<int>(ring.size());
    if(size < 3)
        return false;

    if(size % 2 == 0) {
        const int half = size / 2;
        for(int i = 0; i < half; ++i) {
            if(labelDistance(labels, ring[i], ring[i + half]) != half)
                return false;
        }
    }
    else {
        const int lowerHalf = size / 2;
        const int upperHalf = lowerHalf + 1;
        for(int i = 0; i < lowerHalf; ++i) {
            if(labelDistance(labels, ring[i], ring[i + lowerHalf]) != lowerHalf)
                return false;
            if(labelDistance(labels, ring[i], ring[i + upperHalf]) != lowerHalf)
                return false;
        }
        if(labelDistance(labels, ring[lowerHalf], ring[lowerHalf * 2]) != lowerHalf)
            return false;
    }
    return true;
}

std::vector<int> reversedRingWithSameStart(const std::vector<int>& ring)
{
    std::vector<int> reversed;
    reversed.reserve(ring.size());
    reversed.push_back(ring.front());
    for(auto iter = ring.rbegin(); iter != ring.rend() - 1; ++iter)
        reversed.push_back(*iter);
    return reversed;
}

std::vector<int> buildRingFromCollision(const LabelMap& labels, int start, int vertex, int neighbor)
{
    std::vector<int> ring;
    int inner = neighbor;
    const int neighborDistance = labels.at(neighbor).distance;
    for(int i = 0; i <= neighborDistance; ++i) {
        ring.push_back(inner);
        inner = labels.at(inner).parent;
    }
    std::reverse(ring.begin(), ring.end());

    inner = vertex;
    const int vertexDistance = labels.at(vertex).distance;
    for(int i = 0; i <= vertexDistance; ++i) {
        if(std::find(ring.begin(), ring.end(), inner) != ring.end())
            break;
        ring.push_back(inner);
        inner = labels.at(inner).parent;
    }
    return ring;
}

std::vector<RingRecord> findRings(const std::vector<std::vector<int>>& adjacency,
                                  std::vector<LabelMap>& labels,
                                  int minimumRingSize,
                                  int maximumRingSize,
                                  TaskProgress& progress)
{
    const int maximumDepth = (maximumRingSize + 1) / 2;
    std::set<std::vector<int>> seenRings;
    std::vector<RingRecord> rings;

    progress.setText(RingFinderModifier::tr("Finding shortest bond rings"));
    progress.setMaximum(static_cast<qlonglong>(std::max<size_t>(adjacency.size(), 1)));

    for(size_t startIndex = 0; startIndex < adjacency.size(); ++startIndex) {
        this_task::throwIfCanceled();
        progress.setValueIntermittent(static_cast<qlonglong>(startIndex));

        const int start = static_cast<int>(startIndex);
        LabelMap& startLabels = labels[startIndex];
        for(auto& [_, label] : startLabels)
            label.active = false;

        auto startIter = startLabels.find(start);
        if(startIter == startLabels.end())
            continue;
        startIter->second.active = true;

        std::deque<int> queue;
        queue.push_back(start);

        while(!queue.empty()) {
            const int vertex = queue.front();
            queue.pop_front();
            const SearchLabel vertexLabel = startLabels.at(vertex);
            if(vertexLabel.active && vertexLabel.distance == maximumDepth)
                continue;

            for(int neighbor : adjacency[vertex]) {
                if(neighbor == vertexLabel.parent || neighbor < start)
                    continue;
                if(hasLowerStartAtomOnParentPath(startLabels, start, vertexLabel.parent))
                    continue;

                const auto neighborIter = startLabels.find(neighbor);
                if(neighborIter == startLabels.end())
                    continue;

                if(neighborIter->second.active) {
                    std::vector<int> ring = buildRingFromCollision(startLabels, start, vertex, neighbor);
                    const int ringSize = static_cast<int>(ring.size());
                    if(ringSize != vertexLabel.distance + neighborIter->second.distance + 1)
                        continue;
                    if(ringSize < minimumRingSize || ringSize > maximumRingSize)
                        continue;
                    if(std::find(ring.begin(), ring.end(), start) == ring.end())
                        continue;
                    if(std::any_of(ring.begin(), ring.end(), [start](int atom) { return atom < start; }))
                        continue;

                    const std::vector<int> reversed = reversedRingWithSameStart(ring);
                    if(seenRings.find(reversed) != seenRings.end())
                        continue;
                    if(!validateAntipodes(labels, ring))
                        continue;

                    if(seenRings.insert(ring).second)
                        rings.push_back(RingRecord{ringSize, std::move(ring)});
                }
                else {
                    neighborIter->second.active = true;
                    queue.push_back(neighbor);
                }
            }
        }
    }

    progress.setValue(static_cast<qlonglong>(adjacency.size()));
    std::sort(rings.begin(), rings.end(), [](const RingRecord& left, const RingRecord& right) {
        if(left.size != right.size)
            return left.size < right.size;
        return left.particles < right.particles;
    });
    return rings;
}

void createHistogramTable(PipelineFlowState& state,
                          const OOWeakRef<const PipelineNode>& createdByNode,
                          int maximumRingSize,
                          const std::map<int, int>& ringCounts)
{
    const size_t rowCount = static_cast<size_t>(maximumRingSize + 1);
    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Ring Size"));
    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Counts"));
    {
        BufferWriteAccess<FloatType, access_mode::discard_write> xData(x);
        BufferWriteAccess<FloatType, access_mode::discard_write> yData(y);
        for(int size = 0; size <= maximumRingSize; ++size) {
            const size_t row = static_cast<size_t>(size);
            xData[row] = static_cast<FloatType>(size);
            const auto iter = ringCounts.find(size);
            yData[row] = static_cast<FloatType>(iter == ringCounts.end() ? 0 : iter->second);
        }
    }

    DataTable* table = state.createObject<DataTable>(RingFinderModifier::HistogramTableIdentifier.toString(),
                                                     createdByNode,
                                                     DataTable::BarChart,
                                                     RingFinderModifier::tr("Ring size histogram"),
                                                     std::move(y),
                                                     std::move(x));
    table->setAxisLabelX(RingFinderModifier::tr("Ring Size"));
    table->setAxisLabelY(RingFinderModifier::tr("Counts"));
}

void createRingListTables(PipelineFlowState& state,
                          const OOWeakRef<const PipelineNode>& createdByNode,
                          int minimumRingSize,
                          int maximumRingSize,
                          const std::vector<RingRecord>& rings)
{
    for(int ringSize = minimumRingSize; ringSize <= maximumRingSize; ++ringSize) {
        const auto first = std::lower_bound(rings.begin(), rings.end(), ringSize,
                                            [](const RingRecord& record, int size) { return record.size < size; });
        const auto last = std::upper_bound(rings.begin(), rings.end(), ringSize,
                                           [](int size, const RingRecord& record) { return size < record.size; });
        const size_t rowCount = static_cast<size_t>(std::distance(first, last));

        DataTable* table = state.createObject<DataTable>(QStringLiteral("%1-rings").arg(ringSize),
                                                         createdByNode,
                                                         DataTable::None,
                                                         RingFinderModifier::tr("%1-rings").arg(ringSize));
        table->setElementCount(rowCount);
        Property* indices = table->createProperty(DataBuffer::Initialized,
                                                  QStringLiteral("Particle Indices"),
                                                  DataBuffer::Int64,
                                                  static_cast<size_t>(ringSize));
        BufferWriteAccess<int64_t*, access_mode::discard_write> indexData(indices);
        size_t row = 0;
        for(auto iter = first; iter != last; ++iter, ++row) {
            for(int component = 0; component < ringSize; ++component)
                indexData.set(row, component, static_cast<int64_t>(iter->particles[component]));
        }
        table->setX(indices);
    }
}

void createRingMesh(PipelineFlowState& state,
                    const OOWeakRef<const PipelineNode>& createdByNode,
                    const Particles& particles,
                    const std::vector<RingRecord>& rings)
{
    if(rings.empty())
        return;

    BufferReadAccess<Point3> positions(particles.expectProperty(Particles::PositionProperty));
    std::set<int> uniqueParticleIndices;
    for(const RingRecord& ring : rings) {
        for(int particleIndex : ring.particles) {
            if(particleIndex >= 0 && static_cast<size_t>(particleIndex) < positions.size())
                uniqueParticleIndices.insert(particleIndex);
        }
    }
    if(uniqueParticleIndices.empty())
        return;

    SurfaceMesh* mesh = state.createObject<SurfaceMesh>(createdByNode, RingFinderModifier::tr("Rings"));
    mesh->setIdentifier(RingFinderModifier::MeshIdentifier.toString());
    if(const SimulationCell* cell = state.getObject<SimulationCell>())
        mesh->setDomain(cell);
    if(SurfaceMeshVis* vis = mesh->visElement<SurfaceMeshVis>()) {
        vis->setObjectTitle(RingFinderModifier::tr("Rings"));
        vis->setSmoothShading(false);
        vis->setShowCap(false);
        vis->setSurfaceTransparency(0.45);
        vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(SurfaceMeshVis::smoothShading),
                                           SHADOW_PROPERTY_FIELD(SurfaceMeshVis::showCap)});
    }

    SurfaceMeshBuilder builder(mesh);
    SurfaceMeshBuilder::VertexGrower vertexGrower(builder);
    std::map<int, SurfaceMesh::vertex_index> meshVertexForParticle;
    for(int particleIndex : uniqueParticleIndices)
        meshVertexForParticle[particleIndex] = vertexGrower.createVertex(positions[particleIndex]);
    vertexGrower.reset();

    SurfaceMeshBuilder::FaceGrower faceGrower(builder);
    std::vector<int> faceSizes;
    faceSizes.reserve(rings.size());

    for(const RingRecord& ring : rings) {
        std::vector<SurfaceMesh::vertex_index> vertices;
        vertices.reserve(ring.particles.size());
        for(int particleIndex : ring.particles) {
            const auto iter = meshVertexForParticle.find(particleIndex);
            if(iter == meshVertexForParticle.end())
                continue;
            vertices.push_back(iter->second);
        }
        if(vertices.size() < 3)
            continue;
        faceGrower.createFace(vertices);
        faceSizes.push_back(ring.size);
    }
    faceGrower.reset();

    Property* ringSizeProperty = builder.createFaceProperty(DataBuffer::Initialized,
                                                            QStringLiteral("Ring Size"),
                                                            DataBuffer::Int32);
    BufferWriteAccess<int32_t, access_mode::discard_write> ringSizeData(ringSizeProperty);
    for(size_t faceIndex = 0; faceIndex < faceSizes.size(); ++faceIndex)
        ringSizeData[faceIndex] = faceSizes[faceIndex];
}

}  // namespace

bool RingFinderModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

QVariant RingFinderModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    return tr("%1-%2").arg(minimumRingSize()).arg(maximumRingSize());
}

Future<PipelineFlowState> RingFinderModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                               PipelineFlowState&& state)
{
    const int minimumSize = minimumRingSize();
    const int maximumSize = maximumRingSize();
    const bool outputPolygons = createPolygons();
    const OOWeakRef<const PipelineNode> createdByNode = request.modificationNode();

    if(minimumSize > maximumSize)
        throw Exception(tr("Minimum ring size must not be larger than maximum ring size."));

    state.expectObject<Particles>()->verifyIntegrity();

    return asyncLaunch([state = std::move(state), createdByNode, minimumSize, maximumSize, outputPolygons]() mutable {
        TaskProgress progress(this_task::ui());
        progress.setText(RingFinderModifier::tr("Finding bond rings"));

        const Particles* particles = state.expectObject<Particles>();
        const Bonds* bonds = particles->bonds();
        if(!bonds || !bonds->getProperty(Bonds::TopologyProperty))
            throw Exception(RingFinderModifier::tr("Ring Finder requires a bond topology. Add Create bonds or Load topology upstream first."));

        const size_t particleCount = particles->elementCount();
        std::vector<std::vector<int>> adjacency = buildAdjacency(*bonds, particleCount);
        std::vector<LabelMap> labels = buildSearchLabels(adjacency, maximumSize, progress);
        std::vector<RingRecord> rings = findRings(adjacency, labels, minimumSize, maximumSize, progress);

        std::map<int, int> ringCounts;
        for(int size = minimumSize; size <= maximumSize; ++size)
            ringCounts[size] = 0;
        for(const RingRecord& ring : rings)
            ringCounts[ring.size]++;

        createHistogramTable(state, createdByNode, maximumSize, ringCounts);
        createRingListTables(state, createdByNode, minimumSize, maximumSize, rings);
        if(outputPolygons)
            createRingMesh(state, createdByNode, *particles, rings);

        state.setAttribute(QStringLiteral("RingCount"),
                           QVariant::fromValue(static_cast<double>(rings.size())),
                           createdByNode);
        for(int size = minimumSize; size <= maximumSize; ++size) {
            state.setAttribute(QStringLiteral("%1-RingCount").arg(size),
                               QVariant::fromValue(static_cast<double>(ringCounts[size])),
                               createdByNode);
        }
        state.setAttribute(QStringLiteral("RingFinder.minimum_ring_size"),
                           QVariant::fromValue(minimumSize),
                           createdByNode);
        state.setAttribute(QStringLiteral("RingFinder.maximum_ring_size"),
                           QVariant::fromValue(maximumSize),
                           createdByNode);
        state.setAttribute(QStringLiteral("RingFinder.polygon_count"),
                           QVariant::fromValue(outputPolygons ? static_cast<double>(rings.size()) : 0.0),
                           createdByNode);

        QString statusText = RingFinderModifier::tr("Ring Finder found %1 ring(s) from %2 particles and %3 bonds.")
                                 .arg(rings.size())
                                 .arg(particleCount)
                                 .arg(bonds->elementCount());
        for(int size = minimumSize; size <= maximumSize; ++size)
            statusText += RingFinderModifier::tr(" %1-rings: %2.").arg(size).arg(ringCounts[size]);
        if(outputPolygons)
            statusText += RingFinderModifier::tr(" Polygon facets were created for visualization.");
        state.setStatus(PipelineStatus(PipelineStatus::Success, statusText));

        return std::move(state);
    });
}

}  // namespace Ovito
