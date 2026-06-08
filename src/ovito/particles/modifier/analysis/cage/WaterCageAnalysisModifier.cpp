////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include "WaterCageAnalysisModifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ovito {

namespace {

using EdgeKey = uint64_t;

struct RingFace
{
    int size = 0;
    std::vector<int> vertices;
    std::vector<EdgeKey> edges;
};

struct CageRecord
{
    std::map<int, int> faceCounts;
    QString signature;
    int typeId = 0;
    bool isCandidate = false;
    int candidateKind = 0;  // 0 = complete, 1 = open/partial, 2 = distorted complete.
    int boundaryEdgeCount = 0;
    int missingFaceCount = 0;
    FloatType distortionScore = 0;
    std::vector<int> ringIds;
    std::vector<int> vertices;
    std::vector<EdgeKey> edges;
    Point3 center = Point3::Origin();
    FloatType radius = 0;
};

struct SearchStats
{
    size_t exploredStates = 0;
    bool truncated = false;
};

void publishSearchProgress(TaskProgress& progress,
                           const QString& label,
                           const SearchStats& stats,
                           int maximumSearchStates,
                           size_t& nextProgressUpdate)
{
    if(!stats.truncated && stats.exploredStates < nextProgressUpdate)
        return;

    const size_t limit = static_cast<size_t>(std::max(maximumSearchStates, 1));
    progress.setText(WaterCageAnalysisModifier::tr("%1 (%2/%3 search states)")
                         .arg(label)
                         .arg(static_cast<qulonglong>(stats.exploredStates))
                         .arg(static_cast<qulonglong>(limit)));
    progress.setValue(static_cast<qlonglong>(std::min(stats.exploredStates, limit)));
    nextProgressUpdate = stats.exploredStates + 4096;
}

struct CageTypeSummary
{
    int typeId = 0;
    QString signature;
    std::map<int, int> faceCounts;
    int count = 0;
    int waterCount = 0;
};

inline EdgeKey makeEdgeKey(int a, int b)
{
    if(a > b)
        std::swap(a, b);
    return (static_cast<EdgeKey>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
}

inline std::pair<int, int> decodeEdgeKey(EdgeKey edge)
{
    return {
        static_cast<int>(static_cast<uint32_t>(edge >> 32)),
        static_cast<int>(static_cast<uint32_t>(edge & 0xffffffffu))
    };
}

bool graphHasEdge(const std::vector<std::vector<int>>& graph, int a, int b)
{
    const std::vector<int>& neighbors = graph[a];
    return std::binary_search(neighbors.begin(), neighbors.end(), b);
}

QString ringKeyFromVertices(const std::vector<int>& vertices)
{
    std::vector<int> sortedVertices = vertices;
    std::sort(sortedVertices.begin(), sortedVertices.end());
    QString key;
    key.reserve(sortedVertices.size() * 8);
    for(int vertex : sortedVertices) {
        key += QString::number(vertex);
        key += QLatin1Char(',');
    }
    return key;
}

QString cageSignatureFromCounts(const std::map<int, int>& faceCounts)
{
    QStringList terms;
    for(const auto& [ringSize, count] : faceCounts) {
        if(count > 0)
            terms << QStringLiteral("%1^%2").arg(ringSize).arg(count);
    }
    return terms.isEmpty() ? WaterCageAnalysisModifier::tr("unknown") : terms.join(QLatin1Char(' '));
}

int standardCageTypeId(const std::map<int, int>& faceCounts)
{
    const int ring4 = faceCounts.contains(4) ? faceCounts.at(4) : 0;
    const int ring5 = faceCounts.contains(5) ? faceCounts.at(5) : 0;
    const int ring6 = faceCounts.contains(6) ? faceCounts.at(6) : 0;
    const int ring7 = faceCounts.contains(7) ? faceCounts.at(7) : 0;
    if(ring4 != 0 || ring5 != 12 || ring7 != 0)
        return 0;
    if(ring6 == 0)
        return 1;
    if(ring6 == 2)
        return 2;
    if(ring6 == 4)
        return 3;
    return 0;
}

int cageWaterCountFromCounts(const std::map<int, int>& faceCounts)
{
    int edgeIncidenceCount = 0;
    int faceCount = 0;
    for(const auto& [ringSize, count] : faceCounts) {
        edgeIncidenceCount += ringSize * count;
        faceCount += count;
    }

    if(edgeIncidenceCount % 2 != 0)
        return 0;
    const int edgeCount = edgeIncidenceCount / 2;
    return edgeCount - faceCount + 2;
}

int faceCountValue(const std::map<int, int>& faceCounts, int ringSize)
{
    const auto iter = faceCounts.find(ringSize);
    return iter == faceCounts.end() ? 0 : iter->second;
}

Color cageColor(size_t cageIndex)
{
    const double hue = std::fmod(0.618033988749895 * static_cast<double>(cageIndex), 1.0);
    const double saturation = 0.72;
    const double value = 0.95;
    const double sector = hue * 6.0;
    const int i = static_cast<int>(std::floor(sector));
    const double f = sector - i;
    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * f);
    const double t = value * (1.0 - saturation * (1.0 - f));

    switch(i % 6) {
    case 0: return Color(value, t, p);
    case 1: return Color(q, value, p);
    case 2: return Color(p, value, t);
    case 3: return Color(p, q, value);
    case 4: return Color(t, p, value);
    default: return Color(value, p, q);
    }
}

ColorG toGraphicsColor(const Color& color)
{
    return ColorG(static_cast<GraphicsFloatType>(color.r()),
                  static_cast<GraphicsFloatType>(color.g()),
                  static_cast<GraphicsFloatType>(color.b()));
}

bool isChordlessRing(const std::vector<std::vector<int>>& graph, const std::vector<int>& path)
{
    const int ringSize = static_cast<int>(path.size());
    for(int i = 0; i < ringSize; ++i) {
        for(int j = i + 1; j < ringSize; ++j) {
            const bool consecutive = (j == i + 1) || (i == 0 && j == ringSize - 1);
            if(consecutive)
                continue;
            if(graphHasEdge(graph, path[i], path[j]))
                return false;
        }
    }
    return true;
}

std::vector<RingFace> enumerateRings(const std::vector<std::vector<int>>& graph,
                                     int minimumRingSize,
                                     int maximumRingSize,
                                     int maximumRingRecords,
                                     TaskProgress& progress)
{
    std::vector<RingFace> rings;
    std::unordered_set<QString> seenRings;
    std::vector<int> path;
    std::vector<uint8_t> inPath(graph.size(), 0);

    auto recordRing = [&](const std::vector<int>& ringPath) {
        if(!isChordlessRing(graph, ringPath))
            return;

        const QString key = ringKeyFromVertices(ringPath);
        if(!seenRings.insert(key).second)
            return;
        if(rings.size() >= static_cast<size_t>(std::max(maximumRingRecords, 1))) {
            throw Exception(WaterCageAnalysisModifier::tr(
                "Water ring enumeration exceeded the maximum ring/search state limit (%1). "
                "Use a smaller O-O cutoff, restrict the oxygen selection, or increase the limit.")
                .arg(maximumRingRecords));
        }

        RingFace ring;
        ring.size = static_cast<int>(ringPath.size());
        ring.vertices = ringPath;
        ring.edges.reserve(ring.vertices.size());
        for(size_t i = 0; i < ring.vertices.size(); ++i) {
            const int a = ring.vertices[i];
            const int b = ring.vertices[(i + 1) % ring.vertices.size()];
            ring.edges.push_back(makeEdgeKey(a, b));
        }
        std::sort(ring.edges.begin(), ring.edges.end());
        rings.push_back(std::move(ring));
    };

    std::function<void(int, int)> dfs = [&](int startVertex, int currentVertex) {
        this_task::throwIfCanceled();
        const int depth = static_cast<int>(path.size());
        for(int neighbor : graph[currentVertex]) {
            if(neighbor == startVertex) {
                if(depth >= minimumRingSize && depth <= maximumRingSize)
                    recordRing(path);
                continue;
            }
            if(neighbor <= startVertex || inPath[neighbor] || depth >= maximumRingSize)
                continue;

            inPath[neighbor] = 1;
            path.push_back(neighbor);
            dfs(startVertex, neighbor);
            path.pop_back();
            inPath[neighbor] = 0;
        }
    };

    progress.setText(WaterCageAnalysisModifier::tr("Finding water rings"));
    progress.setMaximum(static_cast<qlonglong>(graph.size()));
    for(size_t start = 0; start < graph.size(); ++start) {
        this_task::throwIfCanceled();
        path.clear();
        path.push_back(static_cast<int>(start));
        inPath[start] = 1;
        dfs(static_cast<int>(start), static_cast<int>(start));
        inPath[start] = 0;
        if(start % 128 == 0)
            progress.setValue(static_cast<qlonglong>(start));
    }
    progress.setValue(static_cast<qlonglong>(graph.size()));
    return rings;
}

bool canAddRingToState(const RingFace& ring,
                       int targetHexagons,
                       int pentagonCount,
                       int hexagonCount,
                       const std::unordered_map<EdgeKey, uint8_t>& edgeUse)
{
    if(ring.size == 5) {
        if(pentagonCount >= 12)
            return false;
    }
    else if(ring.size == 6) {
        if(hexagonCount >= targetHexagons)
            return false;
    }
    else {
        return false;
    }

    for(EdgeKey edge : ring.edges) {
        const auto iter = edgeUse.find(edge);
        if(iter != edgeUse.end() && iter->second >= 2)
            return false;
    }
    return true;
}

bool validateClosedCage(const std::vector<int>& selectedRings,
                        int targetHexagons,
                        const std::vector<RingFace>& rings,
                        const std::unordered_map<EdgeKey, uint8_t>& edgeUse,
                        std::vector<int>& cageVertices,
                        std::vector<EdgeKey>& cageEdges)
{
    const int targetFaces = 12 + targetHexagons;
    const int targetEdges = 30 + 3 * targetHexagons;
    const int targetVertices = 20 + 2 * targetHexagons;
    if(static_cast<int>(selectedRings.size()) != targetFaces)
        return false;

    int pentagons = 0;
    int hexagons = 0;
    std::unordered_set<int> vertexSet;
    for(int ringId : selectedRings) {
        const RingFace& ring = rings[ringId];
        if(ring.size == 5)
            pentagons++;
        else if(ring.size == 6)
            hexagons++;
        else
            return false;
        for(int vertex : ring.vertices)
            vertexSet.insert(vertex);
    }
    if(pentagons != 12 || hexagons != targetHexagons)
        return false;
    if(static_cast<int>(edgeUse.size()) != targetEdges || static_cast<int>(vertexSet.size()) != targetVertices)
        return false;
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(edge);
        if(count != 2)
            return false;
    }
    if(static_cast<int>(vertexSet.size()) - static_cast<int>(edgeUse.size()) + static_cast<int>(selectedRings.size()) != 2)
        return false;

    std::unordered_map<int, std::vector<int>> vertexGraph;
    vertexGraph.reserve(vertexSet.size());
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(count);
        const auto [a, b] = decodeEdgeKey(edge);
        if(!vertexSet.contains(a) || !vertexSet.contains(b) || a == b)
            return false;
        vertexGraph[a].push_back(b);
        vertexGraph[b].push_back(a);
    }
    for(const auto& [vertex, neighbors] : vertexGraph) {
        Q_UNUSED(vertex);
        if(neighbors.size() < 3)
            return false;
    }
    std::vector<int> stack;
    std::unordered_set<int> visitedVertices;
    stack.push_back(*vertexSet.begin());
    visitedVertices.insert(stack.back());
    while(!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        const auto iter = vertexGraph.find(vertex);
        if(iter == vertexGraph.end())
            return false;
        for(int neighbor : iter->second) {
            if(visitedVertices.insert(neighbor).second)
                stack.push_back(neighbor);
        }
    }
    if(visitedVertices.size() != vertexSet.size())
        return false;

    cageVertices.assign(vertexSet.begin(), vertexSet.end());
    std::sort(cageVertices.begin(), cageVertices.end());
    cageEdges.clear();
    cageEdges.reserve(edgeUse.size());
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(count);
        cageEdges.push_back(edge);
    }
    std::sort(cageEdges.begin(), cageEdges.end());
    return true;
}

QString faceSetKey(std::vector<int> faceIds)
{
    std::sort(faceIds.begin(), faceIds.end());
    QString key;
    key.reserve(faceIds.size() * 8);
    for(int faceId : faceIds) {
        key += QString::number(faceId);
        key += QLatin1Char(',');
    }
    return key;
}

bool collectPatchGeometry(const std::vector<int>& selectedRings,
                          const std::vector<RingFace>& rings,
                          const std::unordered_map<EdgeKey, uint8_t>& edgeUse,
                          std::map<int, int>& faceCounts,
                          std::vector<int>& patchVertices,
                          std::vector<EdgeKey>& patchEdges)
{
    if(selectedRings.empty() || edgeUse.empty())
        return false;

    std::unordered_set<int> vertexSet;
    faceCounts.clear();
    for(int ringId : selectedRings) {
        if(ringId < 0 || ringId >= static_cast<int>(rings.size()))
            return false;
        const RingFace& ring = rings[ringId];
        faceCounts[ring.size]++;
        for(int vertex : ring.vertices)
            vertexSet.insert(vertex);
    }
    if(vertexSet.empty())
        return false;

    std::unordered_map<int, std::vector<int>> vertexGraph;
    vertexGraph.reserve(vertexSet.size());
    patchEdges.clear();
    patchEdges.reserve(edgeUse.size());
    for(const auto& [edge, count] : edgeUse) {
        if(count == 0)
            continue;
        const auto [a, b] = decodeEdgeKey(edge);
        if(!vertexSet.contains(a) || !vertexSet.contains(b) || a == b)
            return false;
        patchEdges.push_back(edge);
        vertexGraph[a].push_back(b);
        vertexGraph[b].push_back(a);
    }
    if(patchEdges.empty())
        return false;

    std::vector<int> stack;
    std::unordered_set<int> visitedVertices;
    stack.push_back(*vertexSet.begin());
    visitedVertices.insert(stack.back());
    while(!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        const auto iter = vertexGraph.find(vertex);
        if(iter == vertexGraph.end())
            return false;
        for(int neighbor : iter->second) {
            if(visitedVertices.insert(neighbor).second)
                stack.push_back(neighbor);
        }
    }
    if(visitedVertices.size() != vertexSet.size())
        return false;

    patchVertices.assign(vertexSet.begin(), vertexSet.end());
    std::sort(patchVertices.begin(), patchVertices.end());
    std::sort(patchEdges.begin(), patchEdges.end());
    return true;
}

bool validatePartialStandardCage(const std::vector<int>& selectedRings,
                                 int targetHexagons,
                                 int maximumMissingFaces,
                                 const std::vector<RingFace>& rings,
                                 const std::unordered_map<EdgeKey, uint8_t>& edgeUse,
                                 std::map<int, int>& faceCounts,
                                 std::vector<int>& candidateVertices,
                                 std::vector<EdgeKey>& candidateEdges,
                                 int& boundaryEdgeCount,
                                 int& missingFaceCount)
{
    const int targetFaces = 12 + targetHexagons;
    const int observedFaces = static_cast<int>(selectedRings.size());
    missingFaceCount = targetFaces - observedFaces;
    if(missingFaceCount <= 0 || missingFaceCount > std::max(maximumMissingFaces, 1))
        return false;

    if(!collectPatchGeometry(selectedRings, rings, edgeUse, faceCounts, candidateVertices, candidateEdges))
        return false;

    const int observedPentagons = faceCountValue(faceCounts, 5);
    const int observedHexagons = faceCountValue(faceCounts, 6);
    if(observedPentagons > 12 || observedHexagons > targetHexagons)
        return false;
    if(12 - observedPentagons + targetHexagons - observedHexagons != missingFaceCount)
        return false;

    boundaryEdgeCount = 0;
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(edge);
        if(count == 1)
            boundaryEdgeCount++;
        else if(count != 2)
            return false;
    }
    if(boundaryEdgeCount == 0)
        return false;

    const int eulerCharacteristic = static_cast<int>(candidateVertices.size()) - static_cast<int>(candidateEdges.size()) + observedFaces;
    if(eulerCharacteristic < 0 || eulerCharacteristic > 1)
        return false;

    std::unordered_map<int, int> vertexDegree;
    vertexDegree.reserve(candidateVertices.size());
    for(EdgeKey edge : candidateEdges) {
        const auto [a, b] = decodeEdgeKey(edge);
        vertexDegree[a]++;
        vertexDegree[b]++;
    }
    for(int vertex : candidateVertices) {
        const int degree = vertexDegree[vertex];
        if(degree < 2 || degree > 3)
            return false;
    }

    return true;
}

bool recordPartialStandardCandidate(const std::vector<int>& selectedRings,
                                    int targetHexagons,
                                    const std::vector<RingFace>& rings,
                                    const std::unordered_map<EdgeKey, uint8_t>& edgeUse,
                                    std::unordered_set<QString>& seenCandidates,
                                    std::vector<CageRecord>& candidates,
                                    int maximumMissingFaces,
                                    int maximumCandidates)
{
    if(static_cast<int>(candidates.size()) >= std::max(maximumCandidates, 1))
        return false;

    std::map<int, int> faceCounts;
    std::vector<int> candidateVertices;
    std::vector<EdgeKey> candidateEdges;
    int boundaryEdgeCount = 0;
    int missingFaceCount = 0;
    if(!validatePartialStandardCage(selectedRings, targetHexagons, maximumMissingFaces, rings, edgeUse,
                                    faceCounts, candidateVertices, candidateEdges, boundaryEdgeCount, missingFaceCount))
        return false;

    const QString key = QStringLiteral("partial:%1:").arg(targetHexagons) + faceSetKey(selectedRings);
    if(!seenCandidates.insert(key).second)
        return false;

    CageRecord candidate;
    candidate.faceCounts = std::move(faceCounts);
    candidate.signature = WaterCageAnalysisModifier::tr("partial %1 (missing %2 face%3)")
                              .arg(cageSignatureFromCounts(targetHexagons == 0
                                  ? std::map<int, int>{{5, 12}}
                                  : std::map<int, int>{{5, 12}, {6, targetHexagons}}))
                              .arg(missingFaceCount)
                              .arg(missingFaceCount == 1 ? QString() : QStringLiteral("s"));
    candidate.isCandidate = true;
    candidate.candidateKind = 1;
    candidate.boundaryEdgeCount = boundaryEdgeCount;
    candidate.missingFaceCount = missingFaceCount;
    candidate.ringIds = selectedRings;
    candidate.vertices = std::move(candidateVertices);
    candidate.edges = std::move(candidateEdges);
    candidates.push_back(std::move(candidate));
    return true;
}

void searchCagesFromSeed(int seedRingId,
                         int targetHexagons,
                         const std::vector<RingFace>& rings,
                         const std::unordered_map<EdgeKey, std::vector<int>>& edgeToRings,
                         int maximumSearchStates,
                         std::unordered_set<QString>& seenCages,
                         std::unordered_set<QString>& seenCandidates,
                         std::vector<CageRecord>& cages,
                         std::vector<CageRecord>& completeCandidateSourceCages,
                         std::vector<CageRecord>& candidates,
                         bool recordCompleteCages,
                         bool recordCandidates,
                         int maximumCandidateMissingFaces,
                         int maximumCandidateFragments,
                         SearchStats& stats,
                         TaskProgress& progress,
                         const QString& progressLabel,
                         size_t& nextProgressUpdate)
{
    const int targetFaces = 12 + targetHexagons;
    std::vector<int> selectedRings;
    std::unordered_map<EdgeKey, uint8_t> edgeUse;
    selectedRings.reserve(static_cast<size_t>(targetFaces));
    edgeUse.reserve(static_cast<size_t>(targetFaces * 6));

    auto addRingEdges = [&](const RingFace& ring) {
        for(EdgeKey edge : ring.edges)
            edgeUse[edge]++;
    };

    auto removeRingEdges = [&](const RingFace& ring) {
        for(EdgeKey edge : ring.edges) {
            auto iter = edgeUse.find(edge);
            OVITO_ASSERT(iter != edgeUse.end() && iter->second > 0);
            iter->second--;
            if(iter->second == 0)
                edgeUse.erase(iter);
        }
    };

    std::function<void(int, int)> recurse =
        [&](int pentagonCount, int hexagonCount) {
            this_task::throwIfCanceled();
            if(stats.truncated)
                return;
            if(++stats.exploredStates > static_cast<size_t>(std::max(maximumSearchStates, 1))) {
                stats.truncated = true;
                publishSearchProgress(progress, progressLabel, stats, maximumSearchStates, nextProgressUpdate);
                return;
            }
            publishSearchProgress(progress, progressLabel, stats, maximumSearchStates, nextProgressUpdate);

            if(static_cast<int>(selectedRings.size()) > targetFaces || pentagonCount > 12 || hexagonCount > targetHexagons)
                return;

            std::vector<EdgeKey> boundaryEdges;
            boundaryEdges.reserve(edgeUse.size());
            for(const auto& [edge, count] : edgeUse) {
                if(count == 1)
                    boundaryEdges.push_back(edge);
                else if(count > 2)
                    return;
            }

            if(boundaryEdges.empty()) {
                std::vector<int> cageVertices;
                std::vector<EdgeKey> cageEdges;
                if(validateClosedCage(selectedRings, targetHexagons, rings, edgeUse, cageVertices, cageEdges)) {
                    const QString key = faceSetKey(selectedRings);
                    if(seenCages.insert(key).second) {
                        CageRecord cage;
                        cage.faceCounts[5] = 12;
                        if(targetHexagons > 0)
                            cage.faceCounts[6] = targetHexagons;
                        cage.signature = cageSignatureFromCounts(cage.faceCounts);
                        cage.ringIds = selectedRings;
                        cage.vertices = std::move(cageVertices);
                        cage.edges = std::move(cageEdges);
                        if(recordCompleteCages)
                            cages.push_back(std::move(cage));
                        else if(recordCandidates)
                            completeCandidateSourceCages.push_back(std::move(cage));
                    }
                }
                return;
            }

            if(static_cast<int>(selectedRings.size()) == targetFaces) {
                if(recordCandidates)
                    recordPartialStandardCandidate(selectedRings, targetHexagons, rings, edgeUse, seenCandidates, candidates,
                                                   maximumCandidateMissingFaces, maximumCandidateFragments);
                return;
            }

            EdgeKey selectedBoundary = boundaryEdges.front();
            std::vector<int> bestCandidates;
            bool foundCandidateList = false;
            for(EdgeKey boundaryEdge : boundaryEdges) {
                std::vector<int> candidateRingIds;
                const auto edgeRingsIter = edgeToRings.find(boundaryEdge);
                if(edgeRingsIter != edgeToRings.end()) {
                    for(int candidateRingId : edgeRingsIter->second) {
                        if(candidateRingId <= seedRingId)
                            continue;
                        if(std::find(selectedRings.begin(), selectedRings.end(), candidateRingId) != selectedRings.end())
                            continue;
                        const RingFace& candidateRing = rings[candidateRingId];
                        if(!canAddRingToState(candidateRing, targetHexagons, pentagonCount, hexagonCount, edgeUse))
                            continue;
                        candidateRingIds.push_back(candidateRingId);
                    }
                }

                if(candidateRingIds.empty()) {
                    if(recordCandidates)
                        recordPartialStandardCandidate(selectedRings, targetHexagons, rings, edgeUse, seenCandidates, candidates,
                                                       maximumCandidateMissingFaces, maximumCandidateFragments);
                    continue;
                }
                if(!foundCandidateList || candidateRingIds.size() < bestCandidates.size()) {
                    selectedBoundary = boundaryEdge;
                    bestCandidates = std::move(candidateRingIds);
                    foundCandidateList = true;
                    if(bestCandidates.size() == 1)
                        break;
                }
            }

            Q_UNUSED(selectedBoundary);
            if(!foundCandidateList) {
                if(recordCandidates)
                    recordPartialStandardCandidate(selectedRings, targetHexagons, rings, edgeUse, seenCandidates, candidates,
                                                   maximumCandidateMissingFaces, maximumCandidateFragments);
                return;
            }
            std::sort(bestCandidates.begin(), bestCandidates.end());
            for(int candidateRingId : bestCandidates) {
                const RingFace& candidateRing = rings[candidateRingId];
                bool valid = true;
                for(EdgeKey edge : candidateRing.edges) {
                    const auto iter = edgeUse.find(edge);
                    const uint8_t count = iter == edgeUse.end() ? 0 : iter->second;
                    if(count >= 2) {
                        valid = false;
                        break;
                    }
                }
                if(!valid)
                    continue;

                selectedRings.push_back(candidateRingId);
                addRingEdges(candidateRing);
                recurse(pentagonCount + (candidateRing.size == 5 ? 1 : 0),
                        hexagonCount + (candidateRing.size == 6 ? 1 : 0));
                removeRingEdges(candidateRing);
                selectedRings.pop_back();
                if(stats.truncated)
                    return;
            }
        };

    const RingFace& seedRing = rings[seedRingId];
    if(seedRing.size == 6 && targetHexagons == 0)
        return;
    if(seedRing.size != 5 && seedRing.size != 6)
        return;

    selectedRings.push_back(seedRingId);
    addRingEdges(seedRing);
    recurse(seedRing.size == 5 ? 1 : 0, seedRing.size == 6 ? 1 : 0);
}

bool canAddRingToGeneralState(const RingFace& ring,
                              int maximumCageFaces,
                              int currentFaceCount,
                              const std::unordered_map<EdgeKey, uint8_t>& edgeUse)
{
    if(currentFaceCount >= maximumCageFaces)
        return false;
    for(EdgeKey edge : ring.edges) {
        const auto iter = edgeUse.find(edge);
        if(iter != edgeUse.end() && iter->second >= 2)
            return false;
    }
    return true;
}

bool validateGeneralClosedCage(const std::vector<int>& selectedRings,
                               const std::vector<RingFace>& rings,
                               const std::unordered_map<EdgeKey, uint8_t>& edgeUse,
                               std::map<int, int>& faceCounts,
                               std::vector<int>& cageVertices,
                               std::vector<EdgeKey>& cageEdges)
{
    if(selectedRings.size() < 4)
        return false;

    std::unordered_set<int> vertexSet;
    faceCounts.clear();
    for(int ringId : selectedRings) {
        if(ringId < 0 || ringId >= static_cast<int>(rings.size()))
            return false;
        const RingFace& ring = rings[ringId];
        if(ring.size < 3)
            return false;
        faceCounts[ring.size]++;
        for(int vertex : ring.vertices)
            vertexSet.insert(vertex);
    }

    if(edgeUse.empty() || vertexSet.empty())
        return false;
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(edge);
        if(count != 2)
            return false;
    }
    if(static_cast<int>(vertexSet.size()) - static_cast<int>(edgeUse.size()) + static_cast<int>(selectedRings.size()) != 2)
        return false;

    std::unordered_map<int, std::vector<int>> vertexGraph;
    vertexGraph.reserve(vertexSet.size());
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(count);
        const auto [a, b] = decodeEdgeKey(edge);
        if(!vertexSet.contains(a) || !vertexSet.contains(b) || a == b)
            return false;
        vertexGraph[a].push_back(b);
        vertexGraph[b].push_back(a);
    }
    for(const auto& [vertex, neighbors] : vertexGraph) {
        Q_UNUSED(vertex);
        if(neighbors.size() < 3)
            return false;
    }
    std::vector<int> stack;
    std::unordered_set<int> visitedVertices;
    stack.push_back(*vertexSet.begin());
    visitedVertices.insert(stack.back());
    while(!stack.empty()) {
        const int vertex = stack.back();
        stack.pop_back();
        const auto iter = vertexGraph.find(vertex);
        if(iter == vertexGraph.end())
            return false;
        for(int neighbor : iter->second) {
            if(visitedVertices.insert(neighbor).second)
                stack.push_back(neighbor);
        }
    }
    if(visitedVertices.size() != vertexSet.size())
        return false;

    cageVertices.assign(vertexSet.begin(), vertexSet.end());
    std::sort(cageVertices.begin(), cageVertices.end());
    cageEdges.clear();
    cageEdges.reserve(edgeUse.size());
    for(const auto& [edge, count] : edgeUse) {
        Q_UNUSED(count);
        cageEdges.push_back(edge);
    }
    std::sort(cageEdges.begin(), cageEdges.end());
    return true;
}

void searchGeneralCagesFromSeed(int seedRingId,
                                const std::vector<RingFace>& rings,
                                const std::unordered_map<EdgeKey, std::vector<int>>& edgeToRings,
                                const std::unordered_set<int>& excludedRingIds,
                                int maximumCageFaces,
                                int maximumSearchStates,
                                std::unordered_set<QString>& seenCages,
                                std::unordered_set<QString>& seenCandidates,
                                std::vector<CageRecord>& cages,
                                std::vector<CageRecord>& candidates,
                                bool recordCompleteCages,
                                bool recordCandidates,
                                int minimumCandidateFaces,
                                int maximumCandidateFragments,
                                SearchStats& stats,
                                TaskProgress& progress,
                                const QString& progressLabel,
                                size_t& nextProgressUpdate)
{
    if(excludedRingIds.contains(seedRingId))
        return;
    Q_UNUSED(seenCandidates);
    Q_UNUSED(candidates);
    Q_UNUSED(recordCandidates);
    Q_UNUSED(minimumCandidateFaces);
    Q_UNUSED(maximumCandidateFragments);

    std::vector<int> selectedRings;
    std::unordered_map<EdgeKey, uint8_t> edgeUse;
    selectedRings.reserve(static_cast<size_t>(std::max(maximumCageFaces, 1)));
    edgeUse.reserve(static_cast<size_t>(std::max(maximumCageFaces, 1) * 6));

    auto addRingEdges = [&](const RingFace& ring) {
        for(EdgeKey edge : ring.edges)
            edgeUse[edge]++;
    };

    auto removeRingEdges = [&](const RingFace& ring) {
        for(EdgeKey edge : ring.edges) {
            auto iter = edgeUse.find(edge);
            OVITO_ASSERT(iter != edgeUse.end() && iter->second > 0);
            iter->second--;
            if(iter->second == 0)
                edgeUse.erase(iter);
        }
    };

    std::function<void()> recurse =
        [&]() {
            this_task::throwIfCanceled();
            if(stats.truncated)
                return;
            if(++stats.exploredStates > static_cast<size_t>(std::max(maximumSearchStates, 1))) {
                stats.truncated = true;
                publishSearchProgress(progress, progressLabel, stats, maximumSearchStates, nextProgressUpdate);
                return;
            }
            publishSearchProgress(progress, progressLabel, stats, maximumSearchStates, nextProgressUpdate);
            if(static_cast<int>(selectedRings.size()) > maximumCageFaces)
                return;

            std::vector<EdgeKey> boundaryEdges;
            boundaryEdges.reserve(edgeUse.size());
            for(const auto& [edge, count] : edgeUse) {
                if(count == 1)
                    boundaryEdges.push_back(edge);
                else if(count > 2)
                    return;
            }

            if(boundaryEdges.empty()) {
                std::map<int, int> faceCounts;
                std::vector<int> cageVertices;
                std::vector<EdgeKey> cageEdges;
                if(recordCompleteCages && validateGeneralClosedCage(selectedRings, rings, edgeUse, faceCounts, cageVertices, cageEdges)) {
                    const QString key = faceSetKey(selectedRings);
                    if(seenCages.insert(key).second) {
                        CageRecord cage;
                        cage.faceCounts = std::move(faceCounts);
                        cage.signature = cageSignatureFromCounts(cage.faceCounts);
                        cage.ringIds = selectedRings;
                        cage.vertices = std::move(cageVertices);
                        cage.edges = std::move(cageEdges);
                        cages.push_back(std::move(cage));
                    }
                }
                return;
            }

            if(static_cast<int>(selectedRings.size()) == maximumCageFaces) {
                return;
            }

            std::vector<int> bestCandidates;
            bool foundCandidateList = false;
            for(EdgeKey boundaryEdge : boundaryEdges) {
                std::vector<int> candidateRingIds;
                const auto edgeRingsIter = edgeToRings.find(boundaryEdge);
                if(edgeRingsIter != edgeToRings.end()) {
                    for(int candidateRingId : edgeRingsIter->second) {
                        if(candidateRingId <= seedRingId)
                            continue;
                        if(excludedRingIds.contains(candidateRingId))
                            continue;
                        if(std::find(selectedRings.begin(), selectedRings.end(), candidateRingId) != selectedRings.end())
                            continue;
                        const RingFace& candidateRing = rings[candidateRingId];
                        if(!canAddRingToGeneralState(candidateRing, maximumCageFaces, static_cast<int>(selectedRings.size()), edgeUse))
                            continue;
                        candidateRingIds.push_back(candidateRingId);
                    }
                }

                if(candidateRingIds.empty()) {
                    return;
                }
                if(!foundCandidateList || candidateRingIds.size() < bestCandidates.size()) {
                    bestCandidates = std::move(candidateRingIds);
                    foundCandidateList = true;
                    if(bestCandidates.size() == 1)
                        break;
                }
            }

            std::sort(bestCandidates.begin(), bestCandidates.end());
            for(int candidateRingId : bestCandidates) {
                const RingFace& candidateRing = rings[candidateRingId];
                selectedRings.push_back(candidateRingId);
                addRingEdges(candidateRing);
                recurse();
                removeRingEdges(candidateRing);
                selectedRings.pop_back();
                if(stats.truncated)
                    return;
            }
        };

    const RingFace& seedRing = rings[seedRingId];
    selectedRings.push_back(seedRingId);
    addRingEdges(seedRing);
    recurse();
}

std::vector<Point3> unwrappedCageVertexPositions(const CageRecord& cage,
                                                 const std::vector<size_t>& oxygenParticleIndices,
                                                 const BufferReadAccess<Point3>& positions,
                                                 const SimulationCell* cell);

bool candidateHasThreeDimensionalFaceSpread(const CageRecord& candidate,
                                            const std::vector<RingFace>& rings,
                                            const std::vector<size_t>& oxygenParticleIndices,
                                            const BufferReadAccess<Point3>& positions,
                                            const SimulationCell* cell)
{
    if(!candidate.isCandidate || candidate.ringIds.size() < 3 || candidate.vertices.size() < 4)
        return false;

    const std::vector<Point3> vertexPositions = unwrappedCageVertexPositions(candidate, oxygenParticleIndices, positions, cell);
    if(vertexPositions.size() != candidate.vertices.size())
        return false;

    std::unordered_map<int, size_t> indexForVertex;
    indexForVertex.reserve(candidate.vertices.size());
    for(size_t vertexIndex = 0; vertexIndex < candidate.vertices.size(); ++vertexIndex)
        indexForVertex[candidate.vertices[vertexIndex]] = vertexIndex;

    std::vector<std::array<FloatType, 3>> faceNormals;
    faceNormals.reserve(candidate.ringIds.size());
    for(int ringId : candidate.ringIds) {
        if(ringId < 0 || ringId >= static_cast<int>(rings.size()))
            continue;

        const RingFace& ring = rings[ringId];
        if(ring.vertices.size() < 3)
            continue;

        const auto firstIter = indexForVertex.find(ring.vertices.front());
        if(firstIter == indexForVertex.end())
            continue;

        const Point3 origin = vertexPositions[firstIter->second];
        Vector3 normal = Vector3::Zero();
        for(size_t i = 1; i + 1 < ring.vertices.size(); ++i) {
            const auto iterA = indexForVertex.find(ring.vertices[i]);
            const auto iterB = indexForVertex.find(ring.vertices[i + 1]);
            if(iterA == indexForVertex.end() || iterB == indexForVertex.end())
                continue;
            normal += (vertexPositions[iterA->second] - origin).cross(vertexPositions[iterB->second] - origin);
        }

        const FloatType normalLength = normal.length();
        if(normalLength > FloatType(1e-8)) {
            const Vector3 unitNormal = normal / normalLength;
            faceNormals.push_back({unitNormal.x(), unitNormal.y(), unitNormal.z()});
        }
    }

    if(faceNormals.size() < 3)
        return false;

    FloatType maxRadius = FloatType(0);
    const Point3 referencePosition = vertexPositions.front();
    for(const Point3& point : vertexPositions)
        maxRadius = std::max(maxRadius, (point - referencePosition).length());
    if(maxRadius <= FloatType(1e-8))
        return false;

    FloatType minimumAbsDot = FloatType(1);
    FloatType minimumNormalSpan = std::numeric_limits<FloatType>::max();
    for(size_t i = 0; i < faceNormals.size(); ++i) {
        FloatType minProjection = std::numeric_limits<FloatType>::max();
        FloatType maxProjection = -std::numeric_limits<FloatType>::max();
        for(const Point3& point : vertexPositions) {
            const Vector3 delta = point - referencePosition;
            const FloatType projection = faceNormals[i][0] * delta.x()
                + faceNormals[i][1] * delta.y()
                + faceNormals[i][2] * delta.z();
            minProjection = std::min(minProjection, projection);
            maxProjection = std::max(maxProjection, projection);
        }
        minimumNormalSpan = std::min(minimumNormalSpan, maxProjection - minProjection);

        for(size_t j = i + 1; j < faceNormals.size(); ++j) {
            const FloatType dot = faceNormals[i][0] * faceNormals[j][0]
                + faceNormals[i][1] * faceNormals[j][1]
                + faceNormals[i][2] * faceNormals[j][2];
            minimumAbsDot = std::min(minimumAbsDot, std::abs(dot));
        }
    }

    // Reject flat sheets: their ring normals remain nearly parallel and at least one
    // face-normal direction has negligible thickness through the full patch.
    return minimumAbsDot < FloatType(0.90) && minimumNormalSpan >= FloatType(0.25) * maxRadius;
}

Point3 cageCenter(const CageRecord& cage,
                  const std::vector<size_t>& oxygenParticleIndices,
                  const BufferReadAccess<Point3>& positions,
                  const SimulationCell* cell)
{
    std::vector<Point3> cagePositions;
    cagePositions.reserve(cage.vertices.size());
    if(cage.vertices.empty())
        return Point3::Origin();

    std::unordered_map<int, size_t> indexForVertex;
    indexForVertex.reserve(cage.vertices.size());
    for(size_t vertexIndex = 0; vertexIndex < cage.vertices.size(); ++vertexIndex)
        indexForVertex[cage.vertices[vertexIndex]] = vertexIndex;

    std::vector<std::vector<size_t>> adjacency(cage.vertices.size());
    for(EdgeKey edge : cage.edges) {
        const auto [localA, localB] = decodeEdgeKey(edge);
        const auto iterA = indexForVertex.find(localA);
        const auto iterB = indexForVertex.find(localB);
        if(iterA == indexForVertex.end() || iterB == indexForVertex.end())
            continue;
        adjacency[iterA->second].push_back(iterB->second);
        adjacency[iterB->second].push_back(iterA->second);
    }

    cagePositions.assign(cage.vertices.size(), Point3::Origin());
    std::vector<uint8_t> visited(cage.vertices.size(), 0);
    std::vector<size_t> stack;
    stack.push_back(0);
    visited[0] = 1;
    cagePositions[0] = positions[oxygenParticleIndices[cage.vertices[0]]];
    while(!stack.empty()) {
        const size_t current = stack.back();
        stack.pop_back();
        const int currentLocal = cage.vertices[current];
        const Point3 currentNativePosition = positions[oxygenParticleIndices[currentLocal]];
        for(size_t neighbor : adjacency[current]) {
            if(visited[neighbor])
                continue;
            const int neighborLocal = cage.vertices[neighbor];
            Vector3 offset = positions[oxygenParticleIndices[neighborLocal]] - currentNativePosition;
            if(cell)
                offset = cell->wrapVector(offset);
            cagePositions[neighbor] = cagePositions[current] + offset;
            visited[neighbor] = 1;
            stack.push_back(neighbor);
        }
    }

    const Point3 reference = cagePositions[0];
    Vector3 offsetSum = Vector3::Zero();
    for(size_t vertexIndex = 0; vertexIndex < cagePositions.size(); ++vertexIndex) {
        if(!visited[vertexIndex]) {
            Vector3 offset = positions[oxygenParticleIndices[cage.vertices[vertexIndex]]] - reference;
            if(cell)
                offset = cell->wrapVector(offset);
            cagePositions[vertexIndex] = reference + offset;
        }
        offsetSum += cagePositions[vertexIndex] - reference;
    }
    return reference + offsetSum / static_cast<FloatType>(cagePositions.size());
}

FloatType cageMeanRadius(const CageRecord& cage,
                         const std::vector<size_t>& oxygenParticleIndices,
                         const BufferReadAccess<Point3>& positions,
                         const SimulationCell* cell)
{
    if(cage.vertices.empty())
        return 0;

    const std::vector<Point3> cagePositions = unwrappedCageVertexPositions(cage, oxygenParticleIndices, positions, cell);
    FloatType radiusSum = 0;
    for(const Point3& vertexPosition : cagePositions)
        radiusSum += (vertexPosition - cage.center).length();
    return radiusSum / static_cast<FloatType>(cagePositions.size());
}

FloatType cageEdgeLengthCoefficientOfVariation(const CageRecord& cage,
                                               const std::vector<size_t>& oxygenParticleIndices,
                                               const BufferReadAccess<Point3>& positions,
                                               const SimulationCell* cell)
{
    if(cage.edges.size() < 2 || cage.vertices.empty())
        return 0;

    std::vector<FloatType> edgeLengths;
    edgeLengths.reserve(cage.edges.size());
    for(EdgeKey edge : cage.edges) {
        const auto [localA, localB] = decodeEdgeKey(edge);
        if(localA < 0 || localB < 0
                || localA >= static_cast<int>(oxygenParticleIndices.size())
                || localB >= static_cast<int>(oxygenParticleIndices.size()))
            continue;
        Vector3 edgeVector = positions[oxygenParticleIndices[localA]] - positions[oxygenParticleIndices[localB]];
        if(cell)
            edgeVector = cell->wrapVector(edgeVector);
        edgeLengths.push_back(edgeVector.length());
    }
    if(edgeLengths.size() < 2)
        return 0;

    const FloatType mean = std::accumulate(edgeLengths.begin(), edgeLengths.end(), FloatType(0)) / static_cast<FloatType>(edgeLengths.size());
    if(mean <= FloatType(1e-12))
        return 0;

    FloatType variance = 0;
    for(FloatType length : edgeLengths) {
        const FloatType delta = length - mean;
        variance += delta * delta;
    }
    variance /= static_cast<FloatType>(edgeLengths.size());
    return std::sqrt(variance) / mean;
}

std::vector<Point3> unwrappedCageVertexPositions(const CageRecord& cage,
                                                 const std::vector<size_t>& oxygenParticleIndices,
                                                 const BufferReadAccess<Point3>& positions,
                                                 const SimulationCell* cell)
{
    std::vector<Point3> cagePositions;
    cagePositions.reserve(cage.vertices.size());
    if(cage.vertices.empty())
        return cagePositions;

    std::unordered_map<int, size_t> indexForVertex;
    indexForVertex.reserve(cage.vertices.size());
    for(size_t vertexIndex = 0; vertexIndex < cage.vertices.size(); ++vertexIndex)
        indexForVertex[cage.vertices[vertexIndex]] = vertexIndex;

    std::vector<std::vector<size_t>> adjacency(cage.vertices.size());
    for(EdgeKey edge : cage.edges) {
        const auto [localA, localB] = decodeEdgeKey(edge);
        const auto iterA = indexForVertex.find(localA);
        const auto iterB = indexForVertex.find(localB);
        if(iterA == indexForVertex.end() || iterB == indexForVertex.end())
            continue;
        adjacency[iterA->second].push_back(iterB->second);
        adjacency[iterB->second].push_back(iterA->second);
    }

    cagePositions.assign(cage.vertices.size(), Point3::Origin());
    std::vector<uint8_t> visited(cage.vertices.size(), 0);
    std::vector<size_t> stack;
    stack.push_back(0);
    visited[0] = 1;
    cagePositions[0] = positions[oxygenParticleIndices[cage.vertices[0]]];
    while(!stack.empty()) {
        const size_t current = stack.back();
        stack.pop_back();
        const int currentLocal = cage.vertices[current];
        const Point3 currentNativePosition = positions[oxygenParticleIndices[currentLocal]];
        for(size_t neighbor : adjacency[current]) {
            if(visited[neighbor])
                continue;
            const int neighborLocal = cage.vertices[neighbor];
            Vector3 offset = positions[oxygenParticleIndices[neighborLocal]] - currentNativePosition;
            if(cell)
                offset = cell->wrapVector(offset);
            cagePositions[neighbor] = cagePositions[current] + offset;
            visited[neighbor] = 1;
            stack.push_back(neighbor);
        }
    }

    const Point3 reference = cagePositions[0];
    for(size_t vertexIndex = 0; vertexIndex < cagePositions.size(); ++vertexIndex) {
        if(visited[vertexIndex])
            continue;
        Vector3 offset = positions[oxygenParticleIndices[cage.vertices[vertexIndex]]] - reference;
        if(cell)
            offset = cell->wrapVector(offset);
        cagePositions[vertexIndex] = reference + offset;
    }
    return cagePositions;
}

void addCageType(PropertyPtr& cageTypeProperty, int typeId, const QString& name, const Color& color)
{
    DataOORef<ParticleType> type = DataOORef<ParticleType>::create();
    type->initializeType([&]() {
        type->setNumericId(typeId);
    }, OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty));
    type->setName(name);
    type->setColor(color);
    cageTypeProperty->addElementType(std::move(type));
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(WaterCageAnalysisModifier);
OVITO_CLASSINFO(WaterCageAnalysisModifier, "DisplayName", "Water cage analysis");
OVITO_CLASSINFO(WaterCageAnalysisModifier, "Description",
                "Identify clathrate-style water cages from closed 5- and 6-membered oxygen ring networks.");
OVITO_CLASSINFO(WaterCageAnalysisModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, oxygenTypes);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, oxygenExpression);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, oxygenNeighborCutoff);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, find512Cages);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, find51262Cages);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, find51264Cages);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, findGeneralCompleteCages);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, findOpenPartialCageCandidates);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, minimumGeneralRingSize);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, maximumGeneralRingSize);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, maximumGeneralCageFaces);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, minimumCandidateFaces);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, maximumCandidateMissingFaces);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, distortedCageThreshold);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, maximumCandidateFragments);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, createCageVisualization);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(WaterCageAnalysisModifier, maximumSearchStates);
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, oxygenTypes, "Water oxygen atom type(s)");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, oxygenExpression, "Water oxygen expression");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, oxygenNeighborCutoff, "O-O neighbor cutoff");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, find512Cages, "Find 5^12 cages");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, find51262Cages, "Find 5^12 6^2 cages");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, find51264Cages, "Find 5^12 6^4 cages");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, findGeneralCompleteCages, "Find general complete cages");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, findOpenPartialCageCandidates, "Find open/partial 3D cage candidates");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, minimumGeneralRingSize, "Minimum general ring size");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, maximumGeneralRingSize, "Maximum general ring size");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, maximumGeneralCageFaces, "Maximum general cage faces");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, minimumCandidateFaces, "Minimum candidate faces");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, maximumCandidateMissingFaces, "Maximum missing candidate faces");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, distortedCageThreshold, "Distorted cage threshold");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, maximumCandidateFragments, "Maximum candidate fragments");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, createCageVisualization, "Create cage visualization bonds");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, onlySelectedParticles, "Use only selected oxygen particles");
SET_PROPERTY_FIELD_LABEL(WaterCageAnalysisModifier, maximumSearchStates, "Maximum ring/search states");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, oxygenNeighborCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(WaterCageAnalysisModifier, minimumGeneralRingSize, IntegerParameterUnit, 3, 12);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(WaterCageAnalysisModifier, maximumGeneralRingSize, IntegerParameterUnit, 3, 12);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, maximumGeneralCageFaces, IntegerParameterUnit, 4);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, minimumCandidateFaces, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, maximumCandidateMissingFaces, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, distortedCageThreshold, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, maximumCandidateFragments, IntegerParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(WaterCageAnalysisModifier, maximumSearchStates, IntegerParameterUnit, 1);

bool WaterCageAnalysisModifier::WaterCageAnalysisModifierClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

Future<PipelineFlowState> WaterCageAnalysisModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> particleIdentifiers = particles->getProperty(Particles::IdentifierProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    if(!particleTypes)
        throw Exception(tr("Water cage analysis requires the particle property 'Particle Type'."));
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
        throw Exception(tr("Water cage analysis requires a typed 'Particle Type' property with defined element types."));

    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection)
        throw Exception(tr("The option 'Use only selected oxygen particles' requires a particle selection."));

    if(oxygenNeighborCutoff() <= 0)
        throw Exception(tr("The O-O neighbor cutoff must be positive."));
    if(!find512Cages() && !find51262Cages() && !find51264Cages() && !findGeneralCompleteCages() && !findOpenPartialCageCandidates())
        throw Exception(tr("Please enable at least one cage type or candidate search mode."));
    if(findGeneralCompleteCages() || findOpenPartialCageCandidates()) {
        if(minimumGeneralRingSize() > maximumGeneralRingSize())
            throw Exception(tr("The minimum general ring size cannot be larger than the maximum general ring size."));
        if(maximumGeneralCageFaces() < 4)
            throw Exception(tr("The maximum general cage faces value must be at least 4."));
    }
    if(findOpenPartialCageCandidates()) {
        if(maximumCandidateMissingFaces() < 1)
            throw Exception(tr("The maximum missing candidate faces value must be positive."));
        if(distortedCageThreshold() < 0)
            throw Exception(tr("The distorted cage threshold cannot be negative."));
        if(maximumCandidateFragments() < 1)
            throw Exception(tr("The maximum candidate fragments value must be positive."));
    }

    const SimulationCell* cell = state.getObject<SimulationCell>();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            particleIdentifiers = std::move(particleIdentifiers),
            particleTypes = std::move(particleTypes),
            selection = std::move(selection),
            oxygenTypes = oxygenTypes(),
            oxygenExpression = oxygenExpression(),
            cutoffRadius = oxygenNeighborCutoff(),
            selectedOnly = onlySelectedParticles(),
            find512 = find512Cages(),
            find51262 = find51262Cages(),
            find51264 = find51264Cages(),
            findGeneral = findGeneralCompleteCages(),
            findCandidates = findOpenPartialCageCandidates(),
            minGeneralRingSize = minimumGeneralRingSize(),
            maxGeneralRingSize = maximumGeneralRingSize(),
            maxGeneralCageFaces = maximumGeneralCageFaces(),
            maxCandidateMissingFaces = maximumCandidateMissingFaces(),
            distortedCageThreshold = distortedCageThreshold(),
            maxCandidateFragments = maximumCandidateFragments(),
            createVisualization = createCageVisualization(),
            maxSearchStates = maximumSearchStates(),
            cell,
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Preparing water cage analysis"));

        const Particles* inputParticles = state.expectObject<Particles>();
        const Property* typeProperty = inputParticles->getProperty(Particles::TypeProperty);

        size_t oxygenMatchCount = 0;
        std::vector<uint8_t> oxygenMask = evaluateParticleSelector(
            state, inputParticles, typeProperty, particleTypes,
            oxygenTypes, oxygenExpression,
            tr("water oxygen selector"),
            tr("Water cage analysis"),
            &oxygenMatchCount);
        if(selectedOnly) {
            oxygenMatchCount = 0;
            for(size_t particleIndex = 0; particleIndex < oxygenMask.size(); ++particleIndex) {
                oxygenMask[particleIndex] = (oxygenMask[particleIndex] && selection[particleIndex]) ? 1 : 0;
                if(oxygenMask[particleIndex])
                    oxygenMatchCount++;
            }
        }

        const size_t minimumRequiredOxygens = findCandidates
            ? static_cast<size_t>(std::max(4, 20 - std::max(maxCandidateMissingFaces, 1)))
            : size_t(20);
        if(oxygenMatchCount < minimumRequiredOxygens)
            throw Exception(tr("Water cage analysis needs at least %1 selected water oxygen particles for the enabled cage modes.")
                                .arg(minimumRequiredOxygens));

        std::vector<int> localOxygenIndex(inputParticles->elementCount(), -1);
        std::vector<size_t> oxygenParticleIndices;
        oxygenParticleIndices.reserve(oxygenMatchCount);
        for(size_t particleIndex = 0; particleIndex < oxygenMask.size(); ++particleIndex) {
            if(!oxygenMask[particleIndex])
                continue;
            localOxygenIndex[particleIndex] = static_cast<int>(oxygenParticleIndices.size());
            oxygenParticleIndices.push_back(particleIndex);
        }

        progress.setText(tr("Building O-O neighbor network"));
        PropertyPtr oxygenSelectionProperty = createSelectionPropertyFromMask(oxygenMask);
        BufferReadAccess<SelectionIntType> oxygenSelection(oxygenSelectionProperty);
        const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(positions, false, cutoffRadius / 2);
        CutoffNeighborFinder neighborFinder(cutoffRadius, positions, cellData, oxygenSelection);

        std::vector<std::vector<int>> oxygenGraph(oxygenParticleIndices.size());
        std::unordered_set<EdgeKey> networkEdges;
        for(size_t localIndex = 0; localIndex < oxygenParticleIndices.size(); ++localIndex) {
            this_task::throwIfCanceled();
            const size_t particleIndex = oxygenParticleIndices[localIndex];
            for(CutoffNeighborFinder::Query query(neighborFinder, particleIndex); !query.atEnd(); query.next()) {
                const size_t neighborParticleIndex = query.current();
                if(neighborParticleIndex >= localOxygenIndex.size())
                    continue;
                const int neighborLocalIndex = localOxygenIndex[neighborParticleIndex];
                if(neighborLocalIndex < 0 || neighborLocalIndex == static_cast<int>(localIndex))
                    continue;
                const int a = static_cast<int>(localIndex);
                const int b = neighborLocalIndex;
                const EdgeKey edge = makeEdgeKey(a, b);
                if(!networkEdges.insert(edge).second)
                    continue;
                oxygenGraph[a].push_back(b);
                oxygenGraph[b].push_back(a);
            }
        }

        for(std::vector<int>& neighbors : oxygenGraph) {
            std::sort(neighbors.begin(), neighbors.end());
            neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
        }
        const double averageOxygenDegree = oxygenParticleIndices.empty()
            ? 0.0
            : (2.0 * static_cast<double>(networkEdges.size())) / static_cast<double>(oxygenParticleIndices.size());
        const bool oxygenNetworkOverconnected = averageOxygenDegree > 6.0;

        const int ringSearchMinimum = (findGeneral || findCandidates) ? std::min(minGeneralRingSize, 5) : 5;
        const int ringSearchMaximum = (findGeneral || findCandidates) ? std::max(maxGeneralRingSize, 6) : 6;
        const std::vector<RingFace> rings = enumerateRings(oxygenGraph, ringSearchMinimum, ringSearchMaximum, maxSearchStates, progress);
        std::map<int, size_t> ringCounts;
        for(const RingFace& ring : rings) {
            ringCounts[ring.size]++;
        }
        const size_t quadrilateralRingCount = ringCounts[4];
        const size_t pentagonalRingCount = ringCounts[5];
        const size_t hexagonalRingCount = ringCounts[6];
        const size_t heptagonalRingCount = ringCounts[7];

        std::unordered_map<EdgeKey, std::vector<int>> edgeToRings;
        edgeToRings.reserve(rings.size() * 6);
        for(size_t ringIndex = 0; ringIndex < rings.size(); ++ringIndex) {
            for(EdgeKey edge : rings[ringIndex].edges)
                edgeToRings[edge].push_back(static_cast<int>(ringIndex));
        }
        for(auto& [edge, ringIds] : edgeToRings) {
            Q_UNUSED(edge);
            std::sort(ringIds.begin(), ringIds.end());
        }

        struct StandardCageSearchTarget {
            int hexagonCount = 0;
            bool recordComplete = false;
        };
        std::vector<StandardCageSearchTarget> standardTargets;
        if(find512 || findCandidates)
            standardTargets.push_back({0, find512});
        if(find51262 || findCandidates)
            standardTargets.push_back({2, find51262});
        if(find51264 || findCandidates)
            standardTargets.push_back({4, find51264});

        std::vector<CageRecord> cages;
        std::vector<CageRecord> completeCandidateSourceCages;
        std::vector<CageRecord> candidateCages;
        std::unordered_set<QString> seenCages;
        std::unordered_set<QString> seenCandidates;
        SearchStats searchStats;

        progress.setText(tr("Searching standard water cages"));
        progress.setMaximum(static_cast<qlonglong>(std::max(maxSearchStates, 1)));
        progress.setValue(0);
        size_t nextProgressUpdate = 1;
        for(const StandardCageSearchTarget& target : standardTargets) {
            const int targetHexagons = target.hexagonCount;
            const QString progressLabel = tr("Searching %1 water cages").arg(cageSignatureFromCounts(
                targetHexagons == 0 ? std::map<int, int>{{5, 12}} : std::map<int, int>{{5, 12}, {6, targetHexagons}}));
            for(size_t seedRingId = 0; seedRingId < rings.size(); ++seedRingId) {
                this_task::throwIfCanceled();
                searchCagesFromSeed(static_cast<int>(seedRingId),
                                    targetHexagons,
                                    rings,
                                    edgeToRings,
                                    maxSearchStates,
                                    seenCages,
                                    seenCandidates,
                                    cages,
                                    completeCandidateSourceCages,
                                    candidateCages,
                                    target.recordComplete,
                                    findCandidates,
                                    maxCandidateMissingFaces,
                                    maxCandidateFragments,
                                    searchStats,
                                    progress,
                                    progressLabel,
                                    nextProgressUpdate);
                if(searchStats.truncated)
                    break;
            }
            if(searchStats.truncated)
                break;
        }

        std::unordered_set<int> strictCageRingIds;
        for(const CageRecord& cage : cages)
            strictCageRingIds.insert(cage.ringIds.begin(), cage.ringIds.end());
        for(const CageRecord& cage : completeCandidateSourceCages)
            strictCageRingIds.insert(cage.ringIds.begin(), cage.ringIds.end());

        if(findGeneral && !searchStats.truncated) {
            progress.setText(tr("Searching general complete water cages"));
            const QString progressLabel = tr("Searching general complete water cages");
            for(size_t seedRingId = 0; seedRingId < rings.size(); ++seedRingId) {
                this_task::throwIfCanceled();
                searchGeneralCagesFromSeed(static_cast<int>(seedRingId),
                                           rings,
                                           edgeToRings,
                                           strictCageRingIds,
                                           maxGeneralCageFaces,
                                           maxSearchStates,
                                           seenCages,
                                           seenCandidates,
                                           cages,
                                            candidateCages,
                                            findGeneral,
                                            false,
                                            maxCandidateMissingFaces,
                                            maxCandidateFragments,
                                           searchStats,
                                           progress,
                                           progressLabel,
                                           nextProgressUpdate);
                if(searchStats.truncated)
                    break;
            }
        }
        progress.setValue(static_cast<qlonglong>(std::max(maxSearchStates, 1)));

        std::sort(cages.begin(), cages.end(), [](const CageRecord& left, const CageRecord& right) {
            if(left.signature != right.signature)
                return left.signature < right.signature;
            return left.vertices < right.vertices;
        });
        std::sort(completeCandidateSourceCages.begin(), completeCandidateSourceCages.end(), [](const CageRecord& left, const CageRecord& right) {
            if(left.signature != right.signature)
                return left.signature < right.signature;
            return left.vertices < right.vertices;
        });
        std::sort(candidateCages.begin(), candidateCages.end(), [](const CageRecord& left, const CageRecord& right) {
            if(left.signature != right.signature)
                return left.signature < right.signature;
            return left.vertices < right.vertices;
        });

        int overlapCandidateRejectCount = 0;
        int planarCandidateRejectCount = 0;
        if((!cages.empty() || !completeCandidateSourceCages.empty()) && !candidateCages.empty()) {
            std::unordered_set<int> completeCageRingIds;
            for(const CageRecord& cage : cages)
                completeCageRingIds.insert(cage.ringIds.begin(), cage.ringIds.end());
            for(const CageRecord& cage : completeCandidateSourceCages)
                completeCageRingIds.insert(cage.ringIds.begin(), cage.ringIds.end());

            std::vector<CageRecord> filteredCandidates;
            filteredCandidates.reserve(candidateCages.size());
            for(CageRecord& candidate : candidateCages) {
                const bool overlapsCompleteCage = std::any_of(candidate.ringIds.begin(), candidate.ringIds.end(), [&](int ringId) {
                    return completeCageRingIds.contains(ringId);
                });
                if(overlapsCompleteCage) {
                    overlapCandidateRejectCount++;
                    continue;
                }
                filteredCandidates.push_back(std::move(candidate));
            }
            candidateCages = std::move(filteredCandidates);
        }

        if(!candidateCages.empty()) {
            std::vector<CageRecord> filteredCandidates;
            filteredCandidates.reserve(candidateCages.size());
            for(CageRecord& candidate : candidateCages) {
                if(!candidateHasThreeDimensionalFaceSpread(candidate, rings, oxygenParticleIndices, positions, cell)) {
                    planarCandidateRejectCount++;
                    continue;
                }
                filteredCandidates.push_back(std::move(candidate));
            }
            candidateCages = std::move(filteredCandidates);
        }

        auto recordDistortedCandidate = [&](const CageRecord& cage) {
            if(findCandidates
                    && standardCageTypeId(cage.faceCounts) != 0
                    && cage.distortionScore >= distortedCageThreshold
                    && static_cast<int>(candidateCages.size()) < std::max(maxCandidateFragments, 1)) {
                CageRecord distortedCandidate = cage;
                distortedCandidate.isCandidate = true;
                distortedCandidate.candidateKind = 2;
                distortedCandidate.missingFaceCount = 0;
                distortedCandidate.boundaryEdgeCount = 0;
                distortedCandidate.signature = tr("distorted %1").arg(cageSignatureFromCounts(cage.faceCounts));
                candidateCages.push_back(std::move(distortedCandidate));
            }
        };

        for(CageRecord& cage : cages) {
            cage.center = cageCenter(cage, oxygenParticleIndices, positions, cell);
            cage.radius = cageMeanRadius(cage, oxygenParticleIndices, positions, cell);
            cage.distortionScore = cageEdgeLengthCoefficientOfVariation(cage, oxygenParticleIndices, positions, cell);
            recordDistortedCandidate(cage);
        }
        for(CageRecord& cage : completeCandidateSourceCages) {
            cage.center = cageCenter(cage, oxygenParticleIndices, positions, cell);
            cage.radius = cageMeanRadius(cage, oxygenParticleIndices, positions, cell);
            cage.distortionScore = cageEdgeLengthCoefficientOfVariation(cage, oxygenParticleIndices, positions, cell);
            recordDistortedCandidate(cage);
        }
        for(CageRecord& candidate : candidateCages) {
            candidate.center = cageCenter(candidate, oxygenParticleIndices, positions, cell);
            candidate.radius = cageMeanRadius(candidate, oxygenParticleIndices, positions, cell);
            candidate.distortionScore = cageEdgeLengthCoefficientOfVariation(candidate, oxygenParticleIndices, positions, cell);
        }

        std::vector<int32_t> cageMembership(inputParticles->elementCount(), 0);
        for(const CageRecord& cage : cages) {
            for(int vertex : cage.vertices)
                cageMembership[oxygenParticleIndices[vertex]]++;
        }
        std::vector<int32_t> candidateMembership(inputParticles->elementCount(), 0);
        for(const CageRecord& candidate : candidateCages) {
            for(int vertex : candidate.vertices)
                candidateMembership[oxygenParticleIndices[vertex]]++;
        }

        Particles* mutableParticles = state.makeMutable(inputParticles);
        PropertyPtr membershipProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                 inputParticles->elementCount(),
                                                                                 Property::Int32,
                                                                                 1,
                                                                                 QStringLiteral("Water Cage Membership"));
        {
            BufferWriteAccess<int32_t, access_mode::discard_write> membership(membershipProperty);
            for(size_t particleIndex = 0; particleIndex < cageMembership.size(); ++particleIndex)
                membership[particleIndex] = cageMembership[particleIndex];
        }
        mutableParticles->createProperty(std::move(membershipProperty));
        PropertyPtr candidateMembershipProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                          inputParticles->elementCount(),
                                                                                          Property::Int32,
                                                                                          1,
                                                                                          QStringLiteral("Water Cage Candidate Membership"));
        {
            BufferWriteAccess<int32_t, access_mode::discard_write> membership(candidateMembershipProperty);
            for(size_t particleIndex = 0; particleIndex < candidateMembership.size(); ++particleIndex)
                membership[particleIndex] = candidateMembership[particleIndex];
        }
        mutableParticles->createProperty(std::move(candidateMembershipProperty));

        std::map<QString, int> typeIdForSignature;
        std::map<QString, int> candidateTypeIdForSignature;
        std::map<int, CageTypeSummary> typeSummaries;
        int nextGeneralTypeId = 100;
        int nextCandidateTypeId = 1000;
        int count512 = 0;
        int count51262 = 0;
        int count51264 = 0;
        int countGeneral = 0;
        int countCandidates = 0;
        int countPartialCandidates = 0;
        int countDistortedCandidates = 0;

        for(CageRecord& cage : cages) {
            if(cage.signature.isEmpty())
                cage.signature = cageSignatureFromCounts(cage.faceCounts);
            int typeId = standardCageTypeId(cage.faceCounts);
            if(typeId == 0) {
                auto [iter, inserted] = typeIdForSignature.try_emplace(cage.signature, nextGeneralTypeId);
                if(inserted)
                    nextGeneralTypeId++;
                typeId = iter->second;
            }
            cage.typeId = typeId;

            CageTypeSummary& summary = typeSummaries[typeId];
            summary.typeId = typeId;
            summary.signature = cage.signature;
            summary.faceCounts = cage.faceCounts;
            summary.waterCount = cageWaterCountFromCounts(cage.faceCounts);
            summary.count++;

            if(typeId == 1)
                count512++;
            else if(typeId == 2)
                count51262++;
            else if(typeId == 3)
                count51264++;
            else
                countGeneral++;
        }
        for(CageRecord& candidate : candidateCages) {
            if(candidate.signature.isEmpty())
                candidate.signature = tr("candidate %1").arg(cageSignatureFromCounts(candidate.faceCounts));
            auto [iter, inserted] = candidateTypeIdForSignature.try_emplace(candidate.signature, nextCandidateTypeId);
            if(inserted)
                nextCandidateTypeId++;
            candidate.typeId = iter->second;

            CageTypeSummary& summary = typeSummaries[candidate.typeId];
            summary.typeId = candidate.typeId;
            summary.signature = candidate.signature;
            summary.faceCounts = candidate.faceCounts;
            summary.waterCount = static_cast<int>(candidate.vertices.size());
            summary.count++;
            countCandidates++;
            if(candidate.candidateKind == 2)
                countDistortedCandidates++;
            else
                countPartialCandidates++;
        }

        std::vector<const CageRecord*> outputRecords;
        outputRecords.reserve(cages.size() + candidateCages.size());
        for(const CageRecord& cage : cages)
            outputRecords.push_back(&cage);
        for(const CageRecord& candidate : candidateCages)
            outputRecords.push_back(&candidate);

        size_t cageParticleCount = 0;
        for(const CageRecord* cage : outputRecords)
            cageParticleCount += 1 + cage->vertices.size();

        std::vector<Bond> cageBonds;
        std::vector<int32_t> cageBondIds;
        cageBonds.reserve(outputRecords.size() * 36);
        cageBondIds.reserve(outputRecords.size() * 36);

        if(cageParticleCount != 0) {
        Particles* cageParticles = state.createObject<Particles>(CageContainerIdentifier.toString(), createdByNode);
        cageParticles->setElementCount(cageParticleCount);

        PropertyPtr cageTypeProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized,
                                                                                   cageParticleCount,
                                                                                   Particles::TypeProperty);
        for(const auto& [typeId, summary] : typeSummaries) {
            const Color typeColor = typeId == 1 ? Color(0.95f, 0.35f, 0.15f)
                : typeId == 2 ? Color(0.20f, 0.55f, 0.95f)
                : typeId == 3 ? Color(0.30f, 0.75f, 0.30f)
                : cageColor(static_cast<size_t>(typeId));
            addCageType(cageTypeProperty, typeId, summary.signature, typeColor);
        }

        {
            BufferWriteAccess<Point3, access_mode::discard_write> cagePositions(
                cageParticles->createProperty(DataBuffer::Initialized, Particles::PositionProperty));
            BufferWriteAccess<IdentifierIntType, access_mode::discard_write> cageIdentifiers(
                cageParticles->createProperty(DataBuffer::Initialized, Particles::IdentifierProperty));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageIdProperty(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Cage ID"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageCenterFlags(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Is Cage Center"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageCandidateFlags(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Is Cage Candidate"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageBoundaryEdgeCounts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Cage Boundary Edge Count"), Property::Int32));
            BufferWriteAccess<int64_t, access_mode::discard_write> sourceParticleIndices(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Source Particle Index"), Property::Int64));
            BufferWriteAccess<IdentifierIntType, access_mode::discard_write> sourceParticleIds(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Source Particle Identifier"), Property::Int64));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageWaterCounts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Cage Water Count"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageRing4Counts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("4-ring Faces"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageRing5Counts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("5-ring Faces"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageRing6Counts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("6-ring Faces"), Property::Int32));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageRing7Counts(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("7-ring Faces"), Property::Int32));
            BufferWriteAccess<FloatType, access_mode::discard_write> cageRadii(
                cageParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Cage Mean Radius"), Property::FloatDefault));
            BufferWriteAccess<ColorG, access_mode::discard_write> cageColors(
                cageParticles->createProperty(DataBuffer::Initialized, Particles::ColorProperty));
            BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> cageParticleRadii(
                cageParticles->createProperty(DataBuffer::Initialized, Particles::RadiusProperty));
            BufferWriteAccess<int32_t, access_mode::discard_write> cageTypes(cageTypeProperty);

            size_t outputParticleIndex = 0;
            for(size_t cageIndex = 0; cageIndex < outputRecords.size(); ++cageIndex) {
                const CageRecord& cage = *outputRecords[cageIndex];
                const int32_t cageId = static_cast<int32_t>(cageIndex + 1);
                const Color color = cageColor(cageIndex);
                const int waterCount = cage.isCandidate ? static_cast<int>(cage.vertices.size()) : cageWaterCountFromCounts(cage.faceCounts);
                const std::vector<Point3> vertexPositions = unwrappedCageVertexPositions(cage, oxygenParticleIndices, positions, cell);

                const size_t centerParticleIndex = outputParticleIndex++;
                cagePositions[centerParticleIndex] = cage.center;
                cageIdentifiers[centerParticleIndex] = static_cast<IdentifierIntType>(cageId);
                cageTypes[centerParticleIndex] = cage.typeId;
                cageIdProperty[centerParticleIndex] = cageId;
                cageCenterFlags[centerParticleIndex] = 1;
                cageCandidateFlags[centerParticleIndex] = cage.isCandidate ? 1 : 0;
                cageBoundaryEdgeCounts[centerParticleIndex] = cage.boundaryEdgeCount;
                sourceParticleIndices[centerParticleIndex] = -1;
                sourceParticleIds[centerParticleIndex] = 0;
                cageWaterCounts[centerParticleIndex] = waterCount;
                cageRing4Counts[centerParticleIndex] = faceCountValue(cage.faceCounts, 4);
                cageRing5Counts[centerParticleIndex] = faceCountValue(cage.faceCounts, 5);
                cageRing6Counts[centerParticleIndex] = faceCountValue(cage.faceCounts, 6);
                cageRing7Counts[centerParticleIndex] = faceCountValue(cage.faceCounts, 7);
                cageRadii[centerParticleIndex] = cage.radius;
                cageColors[centerParticleIndex] = toGraphicsColor(color);
                cageParticleRadii[centerParticleIndex] = GraphicsFloatType(0.35);

                std::unordered_map<int, size_t> visualIndexForLocalOxygen;
                visualIndexForLocalOxygen.reserve(cage.vertices.size());
                for(size_t vertexOrdinal = 0; vertexOrdinal < cage.vertices.size(); ++vertexOrdinal) {
                    const int localOxygen = cage.vertices[vertexOrdinal];
                    const size_t sourceParticleIndex = oxygenParticleIndices[localOxygen];
                    const size_t visualParticleIndex = outputParticleIndex++;
                    visualIndexForLocalOxygen[localOxygen] = visualParticleIndex;

                    cagePositions[visualParticleIndex] = vertexPositions[vertexOrdinal];
                    cageIdentifiers[visualParticleIndex] =
                        static_cast<IdentifierIntType>(static_cast<int64_t>(cageId) * 1000000 + static_cast<int64_t>(vertexOrdinal + 1));
                    cageTypes[visualParticleIndex] = cage.typeId;
                    cageIdProperty[visualParticleIndex] = cageId;
                    cageCenterFlags[visualParticleIndex] = 0;
                    cageCandidateFlags[visualParticleIndex] = cage.isCandidate ? 1 : 0;
                    cageBoundaryEdgeCounts[visualParticleIndex] = cage.boundaryEdgeCount;
                    sourceParticleIndices[visualParticleIndex] = static_cast<int64_t>(sourceParticleIndex);
                    sourceParticleIds[visualParticleIndex] =
                        particleIdentifiers ? particleIdentifiers[sourceParticleIndex] : static_cast<IdentifierIntType>(sourceParticleIndex + 1);
                    cageWaterCounts[visualParticleIndex] = waterCount;
                    cageRing4Counts[visualParticleIndex] = faceCountValue(cage.faceCounts, 4);
                    cageRing5Counts[visualParticleIndex] = faceCountValue(cage.faceCounts, 5);
                    cageRing6Counts[visualParticleIndex] = faceCountValue(cage.faceCounts, 6);
                    cageRing7Counts[visualParticleIndex] = faceCountValue(cage.faceCounts, 7);
                    cageRadii[visualParticleIndex] = cage.radius;
                    cageColors[visualParticleIndex] = toGraphicsColor(color);
                    cageParticleRadii[visualParticleIndex] = GraphicsFloatType(0.12);
                }

                if(createVisualization) {
                    std::unordered_set<EdgeKey> visualEdges;
                    visualEdges.reserve(cage.edges.size());
                    for(EdgeKey edge : cage.edges) {
                        const auto [localA, localB] = decodeEdgeKey(edge);
                        const auto iterA = visualIndexForLocalOxygen.find(localA);
                        const auto iterB = visualIndexForLocalOxygen.find(localB);
                        if(iterA == visualIndexForLocalOxygen.end() || iterB == visualIndexForLocalOxygen.end())
                            continue;
                        if(iterA->second == iterB->second || iterA->second >= cageParticleCount || iterB->second >= cageParticleCount)
                            continue;
                        const EdgeKey visualEdge = makeEdgeKey(static_cast<int>(iterA->second), static_cast<int>(iterB->second));
                        if(!visualEdges.insert(visualEdge).second)
                            continue;
                        cageBonds.push_back(Bond{iterA->second, iterB->second, Vector3I::Zero()});
                        cageBondIds.push_back(cageId);
                    }
                }
            }
        }

        cageParticles->createProperty(std::move(cageTypeProperty));

        if(createVisualization && !cageBonds.empty()) {
            PropertyPtr bondCageIdProperty = Bonds::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                                 cageBonds.size(),
                                                                                 Property::Int32,
                                                                                 1,
                                                                                 QStringLiteral("Cage ID"));
            {
                BufferWriteAccess<int32_t, access_mode::discard_write> bondCageIds(bondCageIdProperty);
                for(size_t bondIndex = 0; bondIndex < cageBondIds.size(); ++bondIndex)
                    bondCageIds[bondIndex] = cageBondIds[bondIndex];
            }
            std::vector<PropertyPtr> bondProperties;
            bondProperties.push_back(std::move(bondCageIdProperty));
            cageParticles->addBonds(cageBonds, nullptr, bondProperties);
        }
        }

        const size_t typeCount = typeSummaries.size();
        PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                typeCount,
                                                                Property::FloatDefault,
                                                                1,
                                                                QStringLiteral("Cage Type ID"));
        PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                typeCount,
                                                                Property::FloatDefault,
                                                                1,
                                                                QStringLiteral("Count"));
        PropertyPtr tableWaterCounts = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                               typeCount,
                                                                               Property::Int32,
                                                                               1,
                                                                               QStringLiteral("Water Count"));
        PropertyPtr tableFaceCounts = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                                              typeCount,
                                                                              Property::Int32,
                                                                              4,
                                                                              QStringLiteral("Ring Face Counts"),
                                                                              0,
                                                                              QStringList{QStringLiteral("4-ring"),
                                                                                          QStringLiteral("5-ring"),
                                                                                          QStringLiteral("6-ring"),
                                                                                          QStringLiteral("7-ring")});
        {
            BufferWriteAccess<FloatType, access_mode::discard_write> xData(x);
            BufferWriteAccess<FloatType, access_mode::discard_write> yData(y);
            BufferWriteAccess<int32_t, access_mode::discard_write> tableWaterCountData(tableWaterCounts);
            BufferWriteAccess<int32_t*, access_mode::discard_write> tableFaceCountData(tableFaceCounts);
            size_t rowIndex = 0;
            for(const auto& [typeId, summary] : typeSummaries) {
                xData[rowIndex] = static_cast<FloatType>(typeId);
                yData[rowIndex] = static_cast<FloatType>(summary.count);
                tableWaterCountData[rowIndex] = summary.waterCount;
                tableFaceCountData.set(rowIndex, 0, faceCountValue(summary.faceCounts, 4));
                tableFaceCountData.set(rowIndex, 1, faceCountValue(summary.faceCounts, 5));
                tableFaceCountData.set(rowIndex, 2, faceCountValue(summary.faceCounts, 6));
                tableFaceCountData.set(rowIndex, 3, faceCountValue(summary.faceCounts, 7));
                rowIndex++;
            }
        }
        DataTable* countsTable = state.createObject<DataTable>(CageCountTableIdentifier.toString(),
                                                               createdByNode,
                                                               DataTable::BarChart,
                                                               tr("Water cage counts"),
                                                               std::move(y),
                                                               std::move(x));
        countsTable->setAxisLabelX(tr("Cage type ID (1=5^12, 2=5^12 6^2, 3=5^12 6^4, 100+=general, 1000+=partial/distorted candidates)"));
        countsTable->setAxisLabelY(tr("Count"));
        countsTable->createProperty(std::move(tableWaterCounts));
        countsTable->createProperty(std::move(tableFaceCounts));

        state.setAttribute(QStringLiteral("WaterCage.oxygen_count"), QVariant::fromValue(static_cast<double>(oxygenParticleIndices.size())), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.oo_edges"), QVariant::fromValue(static_cast<double>(networkEdges.size())), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.average_oxygen_degree"), QVariant::fromValue(averageOxygenDegree), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.ring_4_count"), QVariant::fromValue(static_cast<double>(quadrilateralRingCount)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.ring_5_count"), QVariant::fromValue(static_cast<double>(pentagonalRingCount)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.ring_6_count"), QVariant::fromValue(static_cast<double>(hexagonalRingCount)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.ring_7_count"), QVariant::fromValue(static_cast<double>(heptagonalRingCount)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.total_cages"), QVariant::fromValue(static_cast<double>(cages.size())), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_5_12"), QVariant::fromValue(count512), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_5_12_6_2"), QVariant::fromValue(count51262), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_5_12_6_4"), QVariant::fromValue(count51264), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_general"), QVariant::fromValue(countGeneral), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_candidates"), QVariant::fromValue(countCandidates), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_partial_candidates"), QVariant::fromValue(countPartialCandidates), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.count_distorted_candidates"), QVariant::fromValue(countDistortedCandidates), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.rejected_overlap_candidates"), QVariant::fromValue(overlapCandidateRejectCount), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.rejected_planar_candidates"), QVariant::fromValue(planarCandidateRejectCount), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.visualization_particles"), QVariant::fromValue(static_cast<double>(cageParticleCount)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.visualization_bonds"), QVariant::fromValue(static_cast<double>(cageBonds.size())), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.search_states"), QVariant::fromValue(static_cast<double>(searchStats.exploredStates)), createdByNode);
        state.setAttribute(QStringLiteral("WaterCage.search_truncated"), QVariant::fromValue(searchStats.truncated ? 1 : 0), createdByNode);

        QString statusText = tr("Water cage analysis found %1 strict cage(s): %2 5^12, %3 5^12 6^2, %4 5^12 6^4, %5 general/other; %6 candidate cage(s): %7 open/partial, %8 distorted complete.")
                                 .arg(cages.size())
                                 .arg(count512)
                                 .arg(count51262)
                                 .arg(count51264)
                                 .arg(countGeneral)
                                 .arg(countCandidates)
                                 .arg(countPartialCandidates)
                                 .arg(countDistortedCandidates);
        statusText += tr(" Network: %1 oxygen atoms, %2 O-O edges, %3 four-rings, %4 five-rings, %5 six-rings, %6 seven-rings.")
                          .arg(oxygenParticleIndices.size())
                          .arg(networkEdges.size())
                          .arg(quadrilateralRingCount)
                          .arg(pentagonalRingCount)
                          .arg(hexagonalRingCount)
                          .arg(heptagonalRingCount);
        if(overlapCandidateRejectCount > 0 || planarCandidateRejectCount > 0)
            statusText += tr(" Candidate filtering skipped %1 complete-cage-overlap fragment(s) and %2 near-planar sheet fragment(s).")
                              .arg(overlapCandidateRejectCount)
                              .arg(planarCandidateRejectCount);
        if(searchStats.truncated)
            statusText += tr(" Cage search stopped at the maximum state limit; increase the limit for a more exhaustive search.");
        if(oxygenNetworkOverconnected)
            statusText += tr(" The O-O network is overconnected (average degree %1); the cutoff is likely too large and extra O-O edges can hide the true cage face rings. Try a smaller first-shell O-O cutoff.")
                              .arg(averageOxygenDegree, 0, 'f', 2);
        if(cageParticleCount != 0)
            statusText += tr(" A 'Water Cage Membership' particle property, water-cages visualization container, and cage-count table were created.");
        else
            statusText += tr(" A 'Water Cage Membership' particle property and cage-count table were created.");

        state.setStatus(PipelineStatus((searchStats.truncated || oxygenNetworkOverconnected) ? PipelineStatus::Warning : PipelineStatus::Success,
                                       statusText));
        return std::move(state);
    });
}

}  // namespace Ovito
