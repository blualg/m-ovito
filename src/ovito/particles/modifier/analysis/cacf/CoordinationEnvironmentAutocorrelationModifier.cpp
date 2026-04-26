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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "CoordinationEnvironmentAutocorrelationModifier.h"

#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace Ovito {

namespace {

struct CoordinationSnapshot {
    std::vector<IdentifierIntType> centralKeys;
    std::vector<std::vector<IdentifierIntType>> shellMembers;
    std::unordered_map<IdentifierIntType, size_t> indexByCentral;
    size_t totalShellMembers = 0;
};

struct CoordinationAccumulator {
    std::vector<CoordinationSnapshot> snapshots;
    size_t totalCentralAtoms = 0;
    size_t totalNonEmptyShells = 0;
    size_t totalShellMembers = 0;
    bool usedParticleIndices = false;
};

struct IndicatorContext {
    CoordinationEnvironmentAutocorrelationModifier::IndicatorMode mode =
        CoordinationEnvironmentAutocorrelationModifier::Overall;
    int sameChainBondPathDistance = 3;
    std::unordered_map<IdentifierIntType, IdentifierIntType> moleculeByAtom;
    std::unordered_map<IdentifierIntType, std::vector<IdentifierIntType>> bondNeighborsByAtom;
};

struct CacfCurves {
    std::vector<double> lagFrames;
    std::vector<double> values;
};

struct CacfComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

std::vector<std::vector<int>> buildFrameBatches(const std::vector<int>& frames, size_t batchSize)
{
    OVITO_ASSERT(batchSize > 0);

    std::vector<std::vector<int>> batches;
    batches.reserve((frames.size() + batchSize - 1) / batchSize);
    for(size_t begin = 0; begin < frames.size(); begin += batchSize) {
        const size_t end = std::min(begin + batchSize, frames.size());
        batches.emplace_back(frames.begin() + static_cast<ptrdiff_t>(begin), frames.begin() + static_cast<ptrdiff_t>(end));
    }
    return batches;
}

std::vector<int> parseTypeIds(const QString& typeListText, const Property* typeProperty, const QString& roleDescription)
{
    if(!typeProperty || !typeProperty->isTypedProperty())
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
            "CACF requires a typed 'Particle Type' property with defined element types."));

    const QString trimmedText = typeListText.trimmed();
    if(trimmedText.isEmpty())
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr("Please enter at least one %1.").arg(roleDescription));

    QHash<QString, int> nameToId;
    for(const ElementType* type : typeProperty->elementTypes()) {
        if(!type->name().isEmpty())
            nameToId.insert(type->name(), type->numericId());
        nameToId.insert(type->nameOrNumericId(), type->numericId());
    }

    std::vector<int> typeIds;
    const QStringList tokens = trimmedText.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for(QString token : tokens) {
        token = token.trimmed();
        if(token.isEmpty())
            continue;

        int typeId = 0;
        if(nameToId.contains(token)) {
            typeId = nameToId.value(token);
        }
        else {
            bool ok = false;
            typeId = token.toInt(&ok);
            if(!ok || !typeProperty->elementType(typeId)) {
                throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
                    "Unknown %1 '%2'. Use particle type names or numeric IDs separated by commas.")
                    .arg(roleDescription, token));
            }
        }

        if(std::find(typeIds.begin(), typeIds.end(), typeId) == typeIds.end())
            typeIds.push_back(typeId);
    }

    if(typeIds.empty())
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr("Please enter at least one valid %1.").arg(roleDescription));

    return typeIds;
}

size_t setIntersectionCount(const std::vector<IdentifierIntType>& a, const std::vector<IdentifierIntType>& b)
{
    size_t count = 0;
    auto ita = a.begin();
    auto itb = b.begin();
    while(ita != a.end() && itb != b.end()) {
        if(*ita < *itb)
            ++ita;
        else if(*itb < *ita)
            ++itb;
        else {
            ++count;
            ++ita;
            ++itb;
        }
    }
    return count;
}

DataTable* createLineTable(DataCollection* collection,
                           const QStringView identifier,
                           const QString& title,
                           const std::vector<double>& xValues,
                           const std::vector<double>& yValues,
                           const QString& axisLabelX,
                           const QString& axisLabelY,
                           const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(xValues.empty() || yValues.empty())
        return nullptr;

    OVITO_ASSERT(xValues.size() == yValues.size());
    const size_t rowCount = xValues.size();

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            axisLabelY);
    BufferWriteAccess<FloatType, access_mode::discard_write> yAcc(y);
    for(size_t i = 0; i < rowCount; ++i)
        yAcc[i] = static_cast<FloatType>(yValues[i]);

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("Lag"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t i = 0; i < rowCount; ++i)
        xAcc[i] = static_cast<FloatType>(xValues[i]);

    DataTable* table = collection->createObject<DataTable>(identifier.toString(),
                                                           createdByNode,
                                                           DataTable::Line,
                                                           title,
                                                           std::move(y),
                                                           std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

QString indicatorModeLabel(CoordinationEnvironmentAutocorrelationModifier::IndicatorMode mode)
{
    switch(mode) {
    case CoordinationEnvironmentAutocorrelationModifier::Overall:
        return CoordinationEnvironmentAutocorrelationModifier::tr("Overall");
    case CoordinationEnvironmentAutocorrelationModifier::InterchainDifferentChain:
        return CoordinationEnvironmentAutocorrelationModifier::tr("Interchain hopping (different chain)");
    case CoordinationEnvironmentAutocorrelationModifier::InterchainDifferentChainOrSameChainBondPath:
        return CoordinationEnvironmentAutocorrelationModifier::tr("Interchain hopping (different chain+same chain bond path)");
    }
    OVITO_ASSERT(false);
    return {};
}

std::unordered_map<IdentifierIntType, IdentifierIntType> buildMoleculeMap(const Particles* particles,
                                                                          const BufferReadAccess<IdentifierIntType>& identifiers)
{
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
            "This indicator mode requires the particle property 'Molecule Identifier'. Load molecular topology first."));

    std::unordered_map<IdentifierIntType, IdentifierIntType> result;
    result.reserve(moleculeIds.size());
    for(size_t particleIndex = 0; particleIndex < moleculeIds.size(); ++particleIndex) {
        const IdentifierIntType atomKey = identifiers ? identifiers[particleIndex]
                                                      : static_cast<IdentifierIntType>(particleIndex);
        result.insert({atomKey, moleculeIds[particleIndex]});
    }
    return result;
}

std::unordered_map<IdentifierIntType, std::vector<IdentifierIntType>> buildBondGraph(const Particles* particles,
                                                                                      const BufferReadAccess<IdentifierIntType>& identifiers)
{
    const Bonds* bonds = particles->bonds();
    if(!bonds)
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
            "The same-chain bond-path indicator requires bond topology. Load a topology file or create bonds upstream first."));

    BufferReadAccess<ParticleIndexPair> topology = bonds->getProperty(Bonds::TopologyProperty);
    if(!topology)
        throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
            "The same-chain bond-path indicator requires the bond topology property."));

    std::unordered_map<IdentifierIntType, std::vector<IdentifierIntType>> graph;
    graph.reserve(topology.size() * 2);
    for(const ParticleIndexPair& bond : topology) {
        const size_t firstIndex = static_cast<size_t>(bond[0]);
        const size_t secondIndex = static_cast<size_t>(bond[1]);
        const IdentifierIntType firstKey = identifiers ? identifiers[firstIndex]
                                                       : static_cast<IdentifierIntType>(firstIndex);
        const IdentifierIntType secondKey = identifiers ? identifiers[secondIndex]
                                                        : static_cast<IdentifierIntType>(secondIndex);
        graph[firstKey].push_back(secondKey);
        graph[secondKey].push_back(firstKey);
    }
    return graph;
}

void computeShellDifferences(const std::vector<IdentifierIntType>& initialShell,
                             const std::vector<IdentifierIntType>& laggedShell,
                             std::vector<IdentifierIntType>& leavingMembers,
                             std::vector<IdentifierIntType>& enteringMembers)
{
    leavingMembers.clear();
    enteringMembers.clear();

    auto initialIter = initialShell.begin();
    auto laggedIter = laggedShell.begin();
    while(initialIter != initialShell.end() && laggedIter != laggedShell.end()) {
        if(*initialIter < *laggedIter) {
            leavingMembers.push_back(*initialIter);
            ++initialIter;
        }
        else if(*laggedIter < *initialIter) {
            enteringMembers.push_back(*laggedIter);
            ++laggedIter;
        }
        else {
            ++initialIter;
            ++laggedIter;
        }
    }
    leavingMembers.insert(leavingMembers.end(), initialIter, initialShell.end());
    enteringMembers.insert(enteringMembers.end(), laggedIter, laggedShell.end());
}

bool differentChainIndicatorMatches(const std::vector<IdentifierIntType>& changedMembers,
                                    const std::vector<IdentifierIntType>& shellMembers,
                                    const std::unordered_map<IdentifierIntType, IdentifierIntType>& moleculeByAtom)
{
    const std::unordered_set<IdentifierIntType> changedSet(changedMembers.begin(), changedMembers.end());
    for(const IdentifierIntType changedAtom : changedMembers) {
        const auto changedIter = moleculeByAtom.find(changedAtom);
        if(changedIter == moleculeByAtom.end())
            continue;

        for(const IdentifierIntType shellAtom : shellMembers) {
            if(changedSet.find(shellAtom) != changedSet.end())
                continue;
            const auto shellIter = moleculeByAtom.find(shellAtom);
            if(shellIter == moleculeByAtom.end())
                continue;
            if(shellIter->second != changedIter->second)
                return true;
        }
    }
    return false;
}

bool hasSameChainBondPathAtLeastN(IdentifierIntType sourceAtom,
                                  const std::vector<IdentifierIntType>& shellMembers,
                                  const std::unordered_set<IdentifierIntType>& changedSet,
                                  const std::unordered_map<IdentifierIntType, IdentifierIntType>& moleculeByAtom,
                                  const std::unordered_map<IdentifierIntType, std::vector<IdentifierIntType>>& bondNeighborsByAtom,
                                  int minimumDistance)
{
    const auto sourceMolIter = moleculeByAtom.find(sourceAtom);
    if(sourceMolIter == moleculeByAtom.end())
        return false;

    std::unordered_set<IdentifierIntType> targetAtoms;
    for(const IdentifierIntType shellAtom : shellMembers) {
        if(changedSet.find(shellAtom) != changedSet.end())
            continue;
        const auto shellMolIter = moleculeByAtom.find(shellAtom);
        if(shellMolIter == moleculeByAtom.end() || shellMolIter->second != sourceMolIter->second)
            continue;
        targetAtoms.insert(shellAtom);
    }
    if(targetAtoms.empty())
        return false;

    std::queue<std::pair<IdentifierIntType, int>> frontier;
    std::unordered_set<IdentifierIntType> visited;
    frontier.push({sourceAtom, 0});
    visited.insert(sourceAtom);

    while(!frontier.empty()) {
        const auto [currentAtom, currentDistance] = frontier.front();
        frontier.pop();

        if(currentAtom != sourceAtom && targetAtoms.find(currentAtom) != targetAtoms.end() && currentDistance >= minimumDistance)
            return true;

        const auto neighborIter = bondNeighborsByAtom.find(currentAtom);
        if(neighborIter == bondNeighborsByAtom.end())
            continue;

        for(const IdentifierIntType bondedNeighbor : neighborIter->second) {
            if(!visited.insert(bondedNeighbor).second)
                continue;
            frontier.push({bondedNeighbor, currentDistance + 1});
        }
    }

    return false;
}

bool sameChainBondPathIndicatorMatches(const std::vector<IdentifierIntType>& changedMembers,
                                       const std::vector<IdentifierIntType>& shellMembers,
                                       const IndicatorContext& indicatorContext)
{
    const std::unordered_set<IdentifierIntType> changedSet(changedMembers.begin(), changedMembers.end());
    for(const IdentifierIntType changedAtom : changedMembers) {
        if(hasSameChainBondPathAtLeastN(changedAtom,
                                        shellMembers,
                                        changedSet,
                                        indicatorContext.moleculeByAtom,
                                        indicatorContext.bondNeighborsByAtom,
                                        std::max(1, indicatorContext.sameChainBondPathDistance))) {
            return true;
        }
    }
    return false;
}

bool indicatorMatches(const std::vector<IdentifierIntType>& initialShell,
                      const std::vector<IdentifierIntType>& laggedShell,
                      const IndicatorContext& indicatorContext)
{
    if(indicatorContext.mode == CoordinationEnvironmentAutocorrelationModifier::Overall)
        return true;

    // For the paper-style interchain CACF, the indicator acts as a persistence gate:
    // it stays 1 while the shell is unchanged or has only undergone local/nonqualifying
    // changes, and drops to 0 once a qualifying interchain-type decorrelation event occurs.
    if(initialShell.size() == laggedShell.size() && std::equal(initialShell.begin(), initialShell.end(), laggedShell.begin()))
        return true;

    std::vector<IdentifierIntType> leavingMembers;
    std::vector<IdentifierIntType> enteringMembers;
    computeShellDifferences(initialShell, laggedShell, leavingMembers, enteringMembers);

    switch(indicatorContext.mode) {
    case CoordinationEnvironmentAutocorrelationModifier::InterchainDifferentChain:
        return !(differentChainIndicatorMatches(leavingMembers, initialShell, indicatorContext.moleculeByAtom)
                 || differentChainIndicatorMatches(enteringMembers, laggedShell, indicatorContext.moleculeByAtom));
    case CoordinationEnvironmentAutocorrelationModifier::InterchainDifferentChainOrSameChainBondPath:
        return !(differentChainIndicatorMatches(leavingMembers, initialShell, indicatorContext.moleculeByAtom)
                 || differentChainIndicatorMatches(enteringMembers, laggedShell, indicatorContext.moleculeByAtom)
                 || sameChainBondPathIndicatorMatches(leavingMembers, initialShell, indicatorContext)
                 || sameChainBondPathIndicatorMatches(enteringMembers, laggedShell, indicatorContext));
    case CoordinationEnvironmentAutocorrelationModifier::Overall:
        break;
    }
    return false;
}

void appendSnapshot(CoordinationAccumulator& accumulator,
                    const PipelineFlowState& sampleState,
                    const std::unordered_set<int>& centralTypeIds,
                    const std::unordered_set<int>& shellTypeIds,
                    FloatType cutoffRadius)
{
    const Particles* particles = sampleState.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<int32_t> particleTypes = particles->expectProperty(Particles::TypeProperty);
    BufferReadAccess<IdentifierIntType> identifiers = particles->getProperty(Particles::IdentifierProperty);
    accumulator.usedParticleIndices = accumulator.usedParticleIndices || !identifiers;

    const SimulationCell* cell = sampleState.getObject<SimulationCell>();
    const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(positions, false, cutoffRadius / 2);
    CutoffNeighborFinder neighborFinder(cutoffRadius, positions, cellData, {});

    CoordinationSnapshot snapshot;
    snapshot.centralKeys.reserve(positions.size());
    snapshot.shellMembers.reserve(positions.size());

    for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
        if(centralTypeIds.find(particleTypes[particleIndex]) == centralTypeIds.end())
            continue;

        const IdentifierIntType centralKey = identifiers ? identifiers[particleIndex]
                                                          : static_cast<IdentifierIntType>(particleIndex);
        snapshot.indexByCentral.insert({centralKey, snapshot.centralKeys.size()});
        snapshot.centralKeys.push_back(centralKey);

        std::vector<IdentifierIntType> shellMembers;
        for(CutoffNeighborFinder::Query neighQuery(neighborFinder, particleIndex); !neighQuery.atEnd(); neighQuery.next()) {
            const size_t neighborIndex = neighQuery.current();
            if(neighborIndex == particleIndex)
                continue;
            if(shellTypeIds.find(particleTypes[neighborIndex]) == shellTypeIds.end())
                continue;
            shellMembers.push_back(identifiers ? identifiers[neighborIndex]
                                               : static_cast<IdentifierIntType>(neighborIndex));
        }

        std::sort(shellMembers.begin(), shellMembers.end());
        shellMembers.erase(std::unique(shellMembers.begin(), shellMembers.end()), shellMembers.end());

        snapshot.totalShellMembers += shellMembers.size();
        accumulator.totalCentralAtoms++;
        if(!shellMembers.empty())
            accumulator.totalNonEmptyShells++;
        snapshot.shellMembers.push_back(std::move(shellMembers));
    }

    accumulator.totalShellMembers += snapshot.totalShellMembers;
    accumulator.snapshots.push_back(std::move(snapshot));
}

CacfCurves computeCacfCurves(const CoordinationAccumulator& accumulator,
                             const std::vector<int>& sampledFrames,
                             int requestedMaxLag,
                             const IndicatorContext& indicatorContext)
{
    OVITO_ASSERT(accumulator.snapshots.size() == sampledFrames.size());

    const size_t frameCount = accumulator.snapshots.size();
    const size_t maxLagEffective = std::min<size_t>((requestedMaxLag > 0 ? static_cast<size_t>(requestedMaxLag) : frameCount - 1), frameCount - 1);

    CacfCurves curves;
    curves.lagFrames.assign(maxLagEffective + 1, 0.0);
    curves.values.assign(maxLagEffective + 1, 0.0);

    parallelForChunks(maxLagEffective + 1, 8, [&](size_t, size_t fromLag, size_t toLag) {
        for(size_t lag = fromLag; lag < toLag; ++lag) {
            this_task::throwIfCanceled();

            const double lagFrames = static_cast<double>(sampledFrames[lag] - sampledFrames[0]);
            double correlationSum = 0.0;
            size_t contributionCount = 0;

            for(size_t origin = 0; origin + lag < frameCount; ++origin) {
                const CoordinationSnapshot& originSnapshot = accumulator.snapshots[origin];
                const CoordinationSnapshot& targetSnapshot = accumulator.snapshots[origin + lag];

                for(size_t centralIndex = 0; centralIndex < originSnapshot.centralKeys.size(); ++centralIndex) {
                    const std::vector<IdentifierIntType>& initialShell = originSnapshot.shellMembers[centralIndex];
                    if(initialShell.empty())
                        continue;

                    contributionCount++;
                    const auto targetIter = targetSnapshot.indexByCentral.find(originSnapshot.centralKeys[centralIndex]);
                    if(targetIter == targetSnapshot.indexByCentral.end())
                        continue;

                    const std::vector<IdentifierIntType>& laggedShell = targetSnapshot.shellMembers[targetIter->second];
                    if(indicatorMatches(initialShell, laggedShell, indicatorContext)) {
                        correlationSum += static_cast<double>(setIntersectionCount(initialShell, laggedShell))
                                          / static_cast<double>(initialShell.size());
                    }
                }
            }

            curves.lagFrames[lag] = lagFrames;
            curves.values[lag] = contributionCount > 0
                ? (correlationSum / static_cast<double>(contributionCount))
                : std::numeric_limits<double>::quiet_NaN();
        }
    });

    return curves;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(CoordinationEnvironmentAutocorrelationModifier);
OVITO_CLASSINFO(CoordinationEnvironmentAutocorrelationModifier, "DisplayName",
                "Coordination environment autocorrelation function");
OVITO_CLASSINFO(CoordinationEnvironmentAutocorrelationModifier, "Description",
                "Compute the coordination environment autocorrelation function (CACF) for a chosen central atom shell.");
OVITO_CLASSINFO(CoordinationEnvironmentAutocorrelationModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, centralTypes);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, shellTypes);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, cutoff);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, indicatorMode);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, sameChainBondPathDistance);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, intervalStart);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, maxLag);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, centralTypes, "Central atom type(s)");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, shellTypes, "Shell atom type(s)");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, cutoff, "Distance cutoff");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, indicatorMode, "Indicator function");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, sameChainBondPathDistance, "Minimum bond-path distance N");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_LABEL(CoordinationEnvironmentAutocorrelationModifier, maxLag, "Maximum lag (sampled-frame steps)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(CoordinationEnvironmentAutocorrelationModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CoordinationEnvironmentAutocorrelationModifier, sameChainBondPathDistance, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CoordinationEnvironmentAutocorrelationModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CoordinationEnvironmentAutocorrelationModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CoordinationEnvironmentAutocorrelationModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(CoordinationEnvironmentAutocorrelationModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(CoordinationEnvironmentAutocorrelationModificationNode);
DEFINE_REFERENCE_FIELD(CoordinationEnvironmentAutocorrelationModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(CoordinationEnvironmentAutocorrelationModifier, CoordinationEnvironmentAutocorrelationModificationNode);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool CoordinationEnvironmentAutocorrelationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

/******************************************************************************
 * Returns a concise description for the pipeline editor.
 ******************************************************************************/
QVariant CoordinationEnvironmentAutocorrelationModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    const QString central = centralTypes().trimmed();
    const QString shell = shellTypes().trimmed();
    if(central.isEmpty() || shell.isEmpty())
        return {};
    return tr("%1 around %2").arg(shell, central);
}

/******************************************************************************
 * Builds the sampled frame list.
 ******************************************************************************/
std::vector<int> CoordinationEnvironmentAutocorrelationModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);
    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("CACF analysis requires an upstream data source with multiple trajectory frames."));

    const int stride = std::max(1, samplingFrequency());
    int firstFrame = 0;
    int lastFrame = numFrames - 1;

    if(useCustomFrameInterval()) {
        firstFrame = std::clamp(intervalStart(), 0, numFrames - 1);
        lastFrame = std::clamp(intervalEnd(), 0, numFrames - 1);
        if(lastFrame < firstFrame)
            std::swap(firstFrame, lastFrame);
    }

    std::vector<int> result;
    result.reserve(((lastFrame - firstFrame) / stride) + 1);
    for(int frame = firstFrame; frame <= lastFrame; frame += stride)
        result.push_back(frame);

    if(result.size() < 2)
        throw Exception(tr("CACF analysis requires at least two sampled trajectory frames."));

    return result;
}

/******************************************************************************
 * Asks the modifier for the set of animation time intervals that should be cached.
 ******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    if(request.modificationNode()->numberOfSourceFrames() > 0) {
        const std::vector<int> frames = sampledFrames(request.modificationNode());
        if(!frames.empty()) {
            request.mutableCachingIntervals().add(TimeInterval(
                request.modificationNode()->sourceFrameToAnimationTime(frames.front()),
                request.modificationNode()->sourceFrameToAnimationTime(frames.back())));
        }
    }

    Modifier::inputCachingHints(request);
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                                         PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                                         TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
 * Is called by the ModificationNode to let the modifier adjust the validity interval.
 ******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> CoordinationEnvironmentAutocorrelationModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                                           PipelineFlowState&& state)
{
    if(auto* modNode = dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "CACF analysis is idle. Open the Run section and click 'Run CACF analysis' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "CACF analysis is queued. Click 'Run CACF analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeCoordinationData(request, std::move(state));
}

/******************************************************************************
 * Computes the cached CACF table by traversing the sampled trajectory.
 ******************************************************************************/
Future<PipelineFlowState> CoordinationEnvironmentAutocorrelationModifier::computeCoordinationData(const ModifierEvaluationRequest& request,
                                                                                                  PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    const std::vector<int> centralTypeIds = parseTypeIds(centralTypes(), particleTypeProperty, tr("central atom type"));
    const std::vector<int> shellTypeIds = parseTypeIds(shellTypes(), particleTypeProperty, tr("shell atom type"));
    if(cutoff() <= 0)
        throw Exception(tr("The distance cutoff must be positive."));

    const std::unordered_set<int> centralTypeSet(centralTypeIds.begin(), centralTypeIds.end());
    const std::unordered_set<int> shellTypeSet(shellTypeIds.begin(), shellTypeIds.end());
    const IndicatorMode selectedIndicatorMode = indicatorMode();

    IndicatorContext indicatorContext;
    indicatorContext.mode = selectedIndicatorMode;
    indicatorContext.sameChainBondPathDistance = sameChainBondPathDistance();
    if(selectedIndicatorMode == InterchainDifferentChain || selectedIndicatorMode == InterchainDifferentChainOrSameChainBondPath) {
        BufferReadAccess<IdentifierIntType> identifiers = particles->getProperty(Particles::IdentifierProperty);
        indicatorContext.moleculeByAtom = buildMoleculeMap(particles, identifiers);
        if(selectedIndicatorMode == InterchainDifferentChainOrSameChainBondPath)
            indicatorContext.bondNeighborsByAtom = buildBondGraph(particles, identifiers);
    }

    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode())
        ? dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    CoordinationAccumulator accumulator;
    accumulator.snapshots.reserve(frames.size());

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, CoordinationAccumulator&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [centralTypeSet, shellTypeSet, cutoffRadius = cutoff()](const std::vector<int>&,
                                                                    std::vector<SharedFuture<PipelineFlowState>> batchFutures,
                                                                    CoordinationAccumulator& accumulator) {
                for(SharedFuture<PipelineFlowState>& future : batchFutures) {
                    this_task::throwIfCanceled();
                    appendSnapshot(accumulator, future.result(), centralTypeSet, shellTypeSet, cutoffRadius);
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this),
              [this, request, state = std::move(state), frames, cacheGenerationId, indicatorContext = std::move(indicatorContext)](CoordinationAccumulator accumulator) mutable -> Future<PipelineFlowState> {
        OORef<CoordinationEnvironmentAutocorrelationModifier> self(this);
        const int completedRunRequestId = runRequestId();

        return asyncLaunch([self = std::move(self),
                            request = ModifierEvaluationRequest(request),
                            state = std::move(state),
                            frames,
                            accumulator = std::move(accumulator),
                            indicatorContext = std::move(indicatorContext),
                            completedRunRequestId,
                            cacheGenerationId]() mutable {
            CacfComputationResult computationResult{std::move(state)};

            if(!dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode()))
                return computationResult;

            this_task::throwIfCanceled();

            if(accumulator.snapshots.empty())
                throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr("CACF analysis did not collect any trajectory snapshots."));
            if(accumulator.totalCentralAtoms == 0)
                throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
                    "No particles matched the selected central atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalNonEmptyShells == 0)
                throw Exception(CoordinationEnvironmentAutocorrelationModifier::tr(
                    "No central atoms had any shell atoms of the selected type(s) within the cutoff. Increase the cutoff or adjust the atom types."));

            const CacfCurves curves = computeCacfCurves(accumulator, frames, self->maxLag(), indicatorContext);

            computationResult.results = DataOORef<DataCollection>::create();
            const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
            createLineTable(computationResult.results,
                            CoordinationEnvironmentAutocorrelationModifier::correlationTableId(),
                            CoordinationEnvironmentAutocorrelationModifier::tr("Coordination environment autocorrelation function"),
                            curves.lagFrames,
                            curves.values,
                            CoordinationEnvironmentAutocorrelationModifier::tr("Lag (source frames)"),
                            QStringLiteral("CACF"),
                            createdByNode);

            const double averageCentralAtoms = static_cast<double>(accumulator.totalCentralAtoms) / static_cast<double>(accumulator.snapshots.size());
            const double averageShellSize = accumulator.totalCentralAtoms > 0
                ? static_cast<double>(accumulator.totalShellMembers) / static_cast<double>(accumulator.totalCentralAtoms)
                : 0.0;

            computationResult.results->setAttribute(QStringLiteral("CACF.central_types"), self->centralTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.shell_types"), self->shellTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.cutoff"), static_cast<double>(self->cutoff()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.indicator_mode"), indicatorModeLabel(self->indicatorMode()), createdByNode);
            if(self->indicatorMode() == InterchainDifferentChainOrSameChainBondPath)
                computationResult.results->setAttribute(QStringLiteral("CACF.same_chain_bond_path_distance"), static_cast<double>(self->sameChainBondPathDistance()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.central_atom_count"), averageCentralAtoms, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.average_shell_size"), averageShellSize, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("CACF.maximum_lag"), static_cast<double>(curves.lagFrames.empty() ? 0.0 : curves.lagFrames.back()), createdByNode);
            if(!curves.values.empty()) {
                computationResult.results->setAttribute(QStringLiteral("CACF.zero_lag"), curves.values.front(), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("CACF.final_value"), curves.values.back(), createdByNode);
            }

            if(accumulator.usedParticleIndices) {
                computationResult.warningText = CoordinationEnvironmentAutocorrelationModifier::tr(
                    "The input does not provide 'Particle Identifier', so CACF matching falls back to particle indices. "
                    "This assumes the particle order stays stable across trajectory frames.");
            }

            computationResult.completedRunRequestId = completedRunRequestId;
            computationResult.cacheGenerationId = cacheGenerationId;
            return computationResult;
        }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](CacfComputationResult computationResult) mutable {
            auto* modNode = dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode());
            if(!modNode || !computationResult.results)
                return std::move(computationResult.state);

            if(modNode->cacheGenerationId() != computationResult.cacheGenerationId
               || runRequestId() != computationResult.completedRunRequestId) {
                return std::move(computationResult.state);
            }

            modNode->setCachedResults(computationResult.results);
            modNode->setCachedWarningText(computationResult.warningText);
            modNode->setCompletedRunRequestId(computationResult.completedRunRequestId);
            return applyCachedResults(request, std::move(computationResult.state));
        });
    });
}

/******************************************************************************
 * Applies the cached CACF data to the current pipeline state.
 ******************************************************************************/
PipelineFlowState CoordinationEnvironmentAutocorrelationModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                                     PipelineFlowState state) const
{
    auto* modNode = dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

/******************************************************************************
 * Clears all cached CACF data.
 ******************************************************************************/
void CoordinationEnvironmentAutocorrelationModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

/******************************************************************************
 * Is called when a referenced target generated an event.
 ******************************************************************************/
bool CoordinationEnvironmentAutocorrelationModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
