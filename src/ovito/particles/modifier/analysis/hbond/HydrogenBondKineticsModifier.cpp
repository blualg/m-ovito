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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "HydrogenBondAnalysisModifier.h"
#include "HydrogenBondKineticsModifier.h"

#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace Ovito {

namespace {

struct PairKey {
    IdentifierIntType donorId = 0;
    IdentifierIntType acceptorId = 0;

    auto operator<=>(const PairKey&) const = default;
};

struct TripletKey {
    IdentifierIntType donorId = 0;
    IdentifierIntType hydrogenId = 0;
    IdentifierIntType acceptorId = 0;

    auto operator<=>(const TripletKey&) const = default;
};

struct PairKeyHash {
    size_t operator()(const PairKey& key) const noexcept
    {
        size_t seed = static_cast<size_t>(key.donorId);
        seed ^= static_cast<size_t>(key.acceptorId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct TripletKeyHash {
    size_t operator()(const TripletKey& key) const noexcept
    {
        size_t seed = static_cast<size_t>(key.donorId);
        seed ^= static_cast<size_t>(key.hydrogenId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= static_cast<size_t>(key.acceptorId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct DonorHydrogenPair {
    size_t donorIndex = 0;
    size_t hydrogenIndex = 0;
    Vector3 donorToHydrogenDelta = Vector3::Zero();
};

struct CandidateTripletSample {
    TripletKey triplet;
    PairKey pair;
    double distance = 0.0;
    double theta = 0.0;
};

struct FrameHydrogenBondSnapshot {
    int frame = 0;
    std::vector<CandidateTripletSample> candidates;
    size_t donorCount = 0;
    size_t hydrogenCount = 0;
    size_t acceptorCount = 0;
    size_t donorHydrogenPairCount = 0;
    bool usedParticleIndices = false;
};

struct HydrogenBondAccumulator {
    std::vector<FrameHydrogenBondSnapshot> snapshots;
    size_t totalDonorAtoms = 0;
    size_t totalHydrogenAtoms = 0;
    size_t totalAcceptorAtoms = 0;
    size_t totalDonorHydrogenPairs = 0;
    size_t totalCandidateTriplets = 0;
    bool usedParticleIndices = false;
};

struct FrameState {
    std::unordered_set<TripletKey, TripletKeyHash> activeTriplets;
    std::unordered_set<PairKey, PairKeyHash> vicinityPairs;
};

struct PmfDefinition {
    double distanceMaximum = 0.0;
    int distanceBins = 0;
    int angleBins = 0;
    double boundaryFreeEnergy = 0.0;
    double vicinityCutoff = 0.0;
    size_t basinBinCount = 0;
    size_t populatedBinCount = 0;
    std::vector<int64_t> counts;
    std::vector<double> freeEnergy;
    std::vector<char> inBasin;
};

struct HydrogenBondKineticsCurves {
    std::vector<double> lagFrames;
    std::vector<double> c;
    std::vector<double> n;
    std::vector<double> cPlusN;
    std::vector<double> sampleCounts;
};

struct HydrogenBondKineticsComputationResult {
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
        throw Exception(HydrogenBondKineticsModifier::tr(
            "Hydrogen-bond kinetics requires a typed 'Particle Type' property with defined element types."));

    const QString trimmedText = typeListText.trimmed();
    if(trimmedText.isEmpty())
        throw Exception(HydrogenBondKineticsModifier::tr("Please enter at least one %1.").arg(roleDescription));

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
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "Unknown %1 '%2'. Use particle type names or numeric IDs separated by commas.")
                    .arg(roleDescription, token));
            }
        }

        if(std::find(typeIds.begin(), typeIds.end(), typeId) == typeIds.end())
            typeIds.push_back(typeId);
    }

    if(typeIds.empty())
        throw Exception(HydrogenBondKineticsModifier::tr("Please enter at least one valid %1.").arg(roleDescription));

    return typeIds;
}

QString canonicalizeTypeList(QString typeListText)
{
    QStringList tokens = typeListText.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for(QString& token : tokens)
        token = token.trimmed();
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const QString& token) { return token.isEmpty(); }), tokens.end());
    std::sort(tokens.begin(), tokens.end());
    return tokens.join(QStringLiteral(","));
}

PropertyPtr createTypeSelectionProperty(const BufferReadAccess<int>& particleTypes, const std::unordered_set<int>& allowedTypes)
{
    PropertyPtr selectionProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particleTypes.size(), Particles::SelectionProperty);
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> selection(selectionProperty);
    for(size_t particleIndex = 0; particleIndex < particleTypes.size(); ++particleIndex)
        selection[particleIndex] = allowedTypes.find(particleTypes[particleIndex]) != allowedTypes.end() ? 1 : 0;
    return selectionProperty;
}

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

Vector3 donorHydrogenVector(const Point3& donorPosition,
                            const Point3& hydrogenPosition,
                            const Vector3I* bondShift,
                            const SimulationCellData* cell)
{
    Vector3 delta = hydrogenPosition - donorPosition;
    if(bondShift) {
        if(!cell && *bondShift != Vector3I::Zero()) {
            throw Exception(HydrogenBondKineticsModifier::tr(
                "Cannot evaluate periodic donor-hydrogen bonds without a simulation cell."));
        }
        if(cell)
            delta += cell->cellMatrix() * bondShift->toDataType<FloatType>();
    }
    else if(cell) {
        delta = cell->wrapVector(delta);
    }
    return delta;
}

QString definitionModeLabel(HydrogenBondKineticsModifier::DefinitionMode mode)
{
    switch(mode) {
    case HydrogenBondKineticsModifier::FixedGeometry:
        return HydrogenBondKineticsModifier::tr("Fixed geometry");
    case HydrogenBondKineticsModifier::PMFDerived:
        return HydrogenBondKineticsModifier::tr("PMF-derived");
    }
    OVITO_ASSERT(false);
    return {};
}

PmfDefinition loadUpstreamPmfDefinition(const PipelineFlowState& state,
                                        const QString& donorTypes,
                                        const QString& hydrogenTypes,
                                        const QString& acceptorTypes,
                                        FloatType donorHydrogenCutoff)
{
    const DataTable* pmfTable = state.data()
        ? static_object_cast<DataTable>(state.data()->getLeafObject(DataTable::OOClass(), HydrogenBondAnalysisModifier::pmfTableId()))
        : nullptr;
    if(!pmfTable)
        throw Exception(HydrogenBondKineticsModifier::tr(
            "PMF-derived hydrogen-bond kinetics requires an upstream 'Hydrogen bond analysis' modifier in PMF-derived mode. "
            "Run that analysis first so the PMF basin and vicinity boundary are available in the pipeline state."));

    const QVariant distanceMaximumVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_distance_maximum"));
    const QVariant distanceBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_distance_bins"));
    const QVariant angleBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_angle_bins"));
    const QVariant boundaryVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
    const QVariant vicinityVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"));
    const QVariant basinBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_basin_bin_count"));
    const QVariant populatedBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_populated_bin_count"));

    if(!distanceMaximumVariant.isValid() || !distanceBinsVariant.isValid() || !angleBinsVariant.isValid() || !vicinityVariant.isValid())
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream 'Hydrogen bond analysis' result does not provide the PMF-derived basin metadata required by hydrogen-bond kinetics."));

    const QString upstreamDonors = canonicalizeTypeList(state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_types")).toString());
    const QString upstreamHydrogens = canonicalizeTypeList(state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_types")).toString());
    const QString upstreamAcceptors = canonicalizeTypeList(state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_types")).toString());
    if(canonicalizeTypeList(donorTypes) != upstreamDonors
       || canonicalizeTypeList(hydrogenTypes) != upstreamHydrogens
       || canonicalizeTypeList(acceptorTypes) != upstreamAcceptors) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The particle types configured for 'Hydrogen bond kinetics' do not match the upstream PMF-derived 'Hydrogen bond analysis'. "
            "Use the same donor, hydrogen, and acceptor atom types in both modifiers."));
    }

    const QVariant upstreamDonorHydrogenCutoff = state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"));
    if(upstreamDonorHydrogenCutoff.isValid()
       && std::abs(upstreamDonorHydrogenCutoff.toDouble() - static_cast<double>(donorHydrogenCutoff)) > 1e-6) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The donor-hydrogen cutoff in 'Hydrogen bond kinetics' does not match the upstream PMF-derived 'Hydrogen bond analysis'. "
            "Use the same donor-hydrogen cutoff in both modifiers."));
    }

    const Property* countProperty = pmfTable->getProperty(QStringLiteral("Count"));
    const Property* freeEnergyProperty = pmfTable->getProperty(QStringLiteral("Free energy"));
    const Property* basinProperty = pmfTable->getProperty(QStringLiteral("In HB basin"));
    if(!countProperty || !freeEnergyProperty || !basinProperty)
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream hydrogen-bond PMF table is missing one or more required columns ('Count', 'Free energy', 'In HB basin')."));

    PmfDefinition pmf;
    pmf.distanceMaximum = distanceMaximumVariant.toDouble();
    pmf.distanceBins = std::max(1, distanceBinsVariant.toInt());
    pmf.angleBins = std::max(1, angleBinsVariant.toInt());
    pmf.boundaryFreeEnergy = boundaryVariant.toDouble();
    pmf.vicinityCutoff = vicinityVariant.toDouble();
    pmf.basinBinCount = static_cast<size_t>(std::max(0LL, basinBinsVariant.toLongLong()));
    pmf.populatedBinCount = static_cast<size_t>(std::max(0LL, populatedBinsVariant.toLongLong()));

    const size_t expectedSize = static_cast<size_t>(pmf.distanceBins) * static_cast<size_t>(pmf.angleBins);
    if(pmfTable->elementCount() != expectedSize)
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream hydrogen-bond PMF table size does not match its reported PMF bin counts."));

    BufferReadAccess<int64_t> counts(countProperty);
    BufferReadAccess<FloatType> freeEnergy(freeEnergyProperty);
    BufferReadAccess<int64_t> basin(basinProperty);

    pmf.counts.resize(expectedSize);
    pmf.freeEnergy.resize(expectedSize);
    pmf.inBasin.resize(expectedSize);
    for(size_t i = 0; i < expectedSize; ++i) {
        pmf.counts[i] = counts[i];
        pmf.freeEnergy[i] = static_cast<double>(freeEnergy[i]);
        pmf.inBasin[i] = basin[i] != 0 ? 1 : 0;
    }

    return pmf;
}

QString donorHydrogenPairingModeLabel(bool useBondTopology)
{
    return useBondTopology
        ? HydrogenBondKineticsModifier::tr("Bond topology")
        : HydrogenBondKineticsModifier::tr("Geometric donor-hydrogen cutoff");
}

DataTable* createMultiCurveLineTable(DataCollection* collection,
                                     const QStringView identifier,
                                     const QString& title,
                                     const std::vector<double>& xValues,
                                     const std::vector<std::vector<double>>& columns,
                                     QStringList componentNames,
                                     const QString& axisLabelX,
                                     const QString& axisLabelY,
                                     const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(columns.empty() || xValues.empty())
        return nullptr;

    const size_t rowCount = xValues.size();
    const int componentCount = static_cast<int>(columns.size());
    OVITO_ASSERT(std::ranges::all_of(columns, [rowCount](const std::vector<double>& c) { return c.size() == rowCount; }));
    if(componentNames.size() != componentCount)
        componentNames.clear();

    PropertyPtr y = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            componentCount,
                                                            axisLabelY,
                                                            0,
                                                            std::move(componentNames));
    BufferWriteAccess<FloatType*, access_mode::discard_write> yAcc(y);
    for(size_t i = 0; i < rowCount; ++i) {
        for(int c = 0; c < componentCount; ++c)
            yAcc.set(i, c, static_cast<FloatType>(columns[c][i]));
    }

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

DataTable* createPmfTable(DataCollection* collection,
                          const QStringView identifier,
                          const PmfDefinition& pmf,
                          const OOWeakRef<const PipelineNode>& createdByNode)
{
    DataTable* table = collection->createObject<DataTable>(identifier.toString(),
                                                           createdByNode,
                                                           DataTable::None,
                                                           HydrogenBondKineticsModifier::tr("Hydrogen-bond PMF"));
    const size_t rowCount = static_cast<size_t>(pmf.distanceBins) * static_cast<size_t>(pmf.angleBins);
    table->setElementCount(rowCount);

    Property* distanceProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Distance"), Property::FloatDefault, 1);
    Property* thetaProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Theta"), Property::FloatDefault, 1);
    Property* countProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Count"), Property::Int64, 1);
    Property* pmfProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Free energy"), Property::FloatDefault, 1);
    Property* basinProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("In HB basin"), Property::Int64, 1);

    BufferWriteAccess<FloatType, access_mode::discard_write> distanceAcc(distanceProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> thetaAcc(thetaProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> countAcc(countProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> pmfAcc(pmfProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> basinAcc(basinProperty);

    const double distanceBinWidth = pmf.distanceMaximum / static_cast<double>(pmf.distanceBins);
    const double angleBinWidth = 180.0 / static_cast<double>(pmf.angleBins);
    size_t row = 0;
    for(int distanceBin = 0; distanceBin < pmf.distanceBins; ++distanceBin) {
        const double distanceCenter = (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        for(int angleBin = 0; angleBin < pmf.angleBins; ++angleBin, ++row) {
            const size_t linearIndex = static_cast<size_t>(distanceBin) * static_cast<size_t>(pmf.angleBins)
                                     + static_cast<size_t>(angleBin);
            distanceAcc[row] = static_cast<FloatType>(distanceCenter);
            thetaAcc[row] = static_cast<FloatType>((static_cast<double>(angleBin) + 0.5) * angleBinWidth);
            countAcc[row] = pmf.counts[linearIndex];
            pmfAcc[row] = std::isfinite(pmf.freeEnergy[linearIndex])
                ? static_cast<FloatType>(pmf.freeEnergy[linearIndex])
                : std::numeric_limits<FloatType>::quiet_NaN();
            basinAcc[row] = pmf.inBasin[linearIndex] ? 1 : 0;
        }
    }

    return table;
}

std::vector<DonorHydrogenPair> collectBondedDonorHydrogenPairs(const Particles* particles,
                                                               const BufferReadAccess<Point3>& positions,
                                                               const BufferReadAccess<int>& particleTypes,
                                                               const std::unordered_set<int>& donorTypes,
                                                               const std::unordered_set<int>& hydrogenTypes,
                                                               const SimulationCellData* cell)
{
    std::vector<DonorHydrogenPair> pairs;
    const Bonds* bonds = particles->bonds();
    if(!bonds)
        return pairs;

    BufferReadAccess<ParticleIndexPair> topology = bonds->getProperty(Bonds::TopologyProperty);
    if(!topology)
        return pairs;
    BufferReadAccess<Vector3I> periodicImages = bonds->getProperty(Bonds::PeriodicImageProperty);

    pairs.reserve(topology.size());
    for(size_t bondIndex = 0; bondIndex < topology.size(); ++bondIndex) {
        const ParticleIndexPair& bond = topology[bondIndex];
        const size_t index1 = static_cast<size_t>(bond[0]);
        const size_t index2 = static_cast<size_t>(bond[1]);
        if(index1 >= positions.size() || index2 >= positions.size())
            continue;

        const bool index1IsDonor = donorTypes.find(particleTypes[index1]) != donorTypes.end();
        const bool index2IsDonor = donorTypes.find(particleTypes[index2]) != donorTypes.end();
        const bool index1IsHydrogen = hydrogenTypes.find(particleTypes[index1]) != hydrogenTypes.end();
        const bool index2IsHydrogen = hydrogenTypes.find(particleTypes[index2]) != hydrogenTypes.end();

        const Vector3I* bondShift = periodicImages ? &periodicImages[bondIndex] : nullptr;
        if(index1IsDonor && index2IsHydrogen) {
            pairs.push_back({index1, index2, donorHydrogenVector(positions[index1], positions[index2], bondShift, cell)});
        }
        else if(index2IsDonor && index1IsHydrogen) {
            const Vector3I reversedShift = bondShift ? -*bondShift : Vector3I::Zero();
            pairs.push_back({index2, index1, donorHydrogenVector(positions[index2], positions[index1], bondShift ? &reversedShift : nullptr, cell)});
        }
    }

    return pairs;
}

std::vector<DonorHydrogenPair> collectGeometricDonorHydrogenPairs(const BufferReadAccess<Point3>& positions,
                                                                  const BufferReadAccess<int>& particleTypes,
                                                                  const std::unordered_set<int>& donorTypes,
                                                                  const std::unordered_set<int>& hydrogenTypes,
                                                                  const SimulationCellData& cellData,
                                                                  FloatType cutoff)
{
    PropertyPtr hydrogenSelectionProperty = createTypeSelectionProperty(particleTypes, hydrogenTypes);
    BufferReadAccess<SelectionIntType> hydrogenSelection(hydrogenSelectionProperty);
    CutoffNeighborFinder hydrogenFinder(cutoff, positions, cellData, hydrogenSelection);

    std::vector<DonorHydrogenPair> pairs;
    for(size_t donorIndex = 0; donorIndex < positions.size(); ++donorIndex) {
        if(donorTypes.find(particleTypes[donorIndex]) == donorTypes.end())
            continue;

        for(CutoffNeighborFinder::Query query(hydrogenFinder, donorIndex); !query.atEnd(); query.next()) {
            const size_t hydrogenIndex = query.current();
            if(hydrogenIndex == donorIndex)
                continue;
            pairs.push_back({donorIndex, hydrogenIndex, query.delta()});
        }
    }

    return pairs;
}

FrameHydrogenBondSnapshot analyzeFrame(const PipelineFlowState& state,
                                       int sourceFrame,
                                       const std::unordered_set<int>& donorTypes,
                                       const std::unordered_set<int>& hydrogenTypes,
                                       const std::unordered_set<int>& acceptorTypes,
                                       FloatType donorHydrogenCutoff,
                                       FloatType donorAcceptorSearchCutoff,
                                       bool useBondTopology)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<int> particleTypes = particles->expectProperty(Particles::TypeProperty);
    BufferReadAccess<IdentifierIntType> identifiers = particles->getProperty(Particles::IdentifierProperty);
    const SimulationCell* simCellObject = state.getObject<SimulationCell>();
    const SimulationCellData cellData = simCellObject
        ? SimulationCellData(*simCellObject)
        : SimulationCellData(positions, false, std::max(donorHydrogenCutoff, donorAcceptorSearchCutoff) / 2);
    const SimulationCellData* cellDataPtr = &cellData;

    FrameHydrogenBondSnapshot result;
    result.frame = sourceFrame;
    result.usedParticleIndices = !identifiers;

    for(size_t particleIndex = 0; particleIndex < particleTypes.size(); ++particleIndex) {
        result.donorCount += donorTypes.find(particleTypes[particleIndex]) != donorTypes.end();
        result.hydrogenCount += hydrogenTypes.find(particleTypes[particleIndex]) != hydrogenTypes.end();
        result.acceptorCount += acceptorTypes.find(particleTypes[particleIndex]) != acceptorTypes.end();
    }

    const auto particleId = [&](size_t particleIndex) -> IdentifierIntType {
        return identifiers ? identifiers[particleIndex] : static_cast<IdentifierIntType>(particleIndex + 1);
    };

    std::vector<DonorHydrogenPair> donorHydrogenPairs = useBondTopology
        ? collectBondedDonorHydrogenPairs(particles, positions, particleTypes, donorTypes, hydrogenTypes, cellDataPtr)
        : collectGeometricDonorHydrogenPairs(positions, particleTypes, donorTypes, hydrogenTypes, cellData, donorHydrogenCutoff);
    result.donorHydrogenPairCount = donorHydrogenPairs.size();

    PropertyPtr acceptorSelectionProperty = createTypeSelectionProperty(particleTypes, acceptorTypes);
    BufferReadAccess<SelectionIntType> acceptorSelection(acceptorSelectionProperty);
    CutoffNeighborFinder acceptorFinder(donorAcceptorSearchCutoff, positions, cellData, acceptorSelection);

    result.candidates.reserve(donorHydrogenPairs.size() * 4);
    for(const DonorHydrogenPair& donorHydrogen : donorHydrogenPairs) {
        this_task::throwIfCanceled();

        const FloatType dhLength = donorHydrogen.donorToHydrogenDelta.length();
        if(dhLength <= FloatType(0))
            continue;

        std::unordered_map<size_t, std::pair<Vector3, FloatType>> bestAcceptorImages;
        for(CutoffNeighborFinder::Query query(acceptorFinder, donorHydrogen.donorIndex); !query.atEnd(); query.next()) {
            const size_t acceptorIndex = query.current();
            if(acceptorIndex == donorHydrogen.donorIndex || acceptorIndex == donorHydrogen.hydrogenIndex)
                continue;
            auto iter = bestAcceptorImages.find(acceptorIndex);
            if(iter == bestAcceptorImages.end() || query.distanceSquared() < iter->second.second)
                bestAcceptorImages[acceptorIndex] = {query.delta(), query.distanceSquared()};
        }

        for(const auto& [acceptorIndex, acceptorInfo] : bestAcceptorImages) {
            const Vector3& donorToAcceptorDelta = acceptorInfo.first;
            const FloatType daLength = std::sqrt(acceptorInfo.second);
            if(daLength <= FloatType(0))
                continue;

            const FloatType theta = qRadiansToDegrees(clampedAcos(donorHydrogen.donorToHydrogenDelta.dot(donorToAcceptorDelta) / (dhLength * daLength)));
            result.candidates.push_back({
                {particleId(donorHydrogen.donorIndex), particleId(donorHydrogen.hydrogenIndex), particleId(acceptorIndex)},
                {particleId(donorHydrogen.donorIndex), particleId(acceptorIndex)},
                static_cast<double>(daLength),
                static_cast<double>(theta)
            });
        }
    }

    return result;
}

size_t pmfLinearIndex(int distanceBin, int angleBin, int angleBins)
{
    return static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin);
}

int clampedBinIndex(double value, double upperBound, int binCount)
{
    if(!(value >= 0.0))
        return -1;
    if(value > upperBound)
        return -1;
    int binIndex = static_cast<int>(std::floor((value / upperBound) * static_cast<double>(binCount)));
    if(binIndex >= binCount)
        binIndex = binCount - 1;
    return binIndex;
}

std::vector<int> connectedComponentAtThreshold(const std::vector<double>& freeEnergy,
                                               int distanceBins,
                                               int angleBins,
                                               int seedIndex,
                                               double threshold)
{
    std::vector<int> component;
    if(seedIndex < 0 || seedIndex >= static_cast<int>(freeEnergy.size()) || !std::isfinite(freeEnergy[seedIndex]) || freeEnergy[seedIndex] > threshold)
        return component;

    std::vector<char> visited(freeEnergy.size(), 0);
    std::queue<int> queue;
    queue.push(seedIndex);
    visited[seedIndex] = 1;

    while(!queue.empty()) {
        const int linearIndex = queue.front();
        queue.pop();
        component.push_back(linearIndex);

        const int distanceBin = linearIndex / angleBins;
        const int angleBin = linearIndex % angleBins;
        for(int dd = -1; dd <= 1; ++dd) {
            for(int da = -1; da <= 1; ++da) {
                if(dd == 0 && da == 0)
                    continue;
                const int neighborDistanceBin = distanceBin + dd;
                const int neighborAngleBin = angleBin + da;
                if(neighborDistanceBin < 0 || neighborDistanceBin >= distanceBins
                   || neighborAngleBin < 0 || neighborAngleBin >= angleBins)
                    continue;

                const int neighborIndex = neighborDistanceBin * angleBins + neighborAngleBin;
                if(visited[neighborIndex] || !std::isfinite(freeEnergy[neighborIndex]) || freeEnergy[neighborIndex] > threshold)
                    continue;
                visited[neighborIndex] = 1;
                queue.push(neighborIndex);
            }
        }
    }

    return component;
}

PmfDefinition buildPmfDefinition(const HydrogenBondAccumulator& accumulator,
                                 double distanceMaximum,
                                 int distanceBins,
                                 int angleBins)
{
    if(distanceMaximum <= 0.0)
        throw Exception(HydrogenBondKineticsModifier::tr("The PMF distance maximum must be positive."));
    if(distanceBins < 4 || angleBins < 4)
        throw Exception(HydrogenBondKineticsModifier::tr("PMF bin counts must be at least 4 in each dimension."));

    PmfDefinition pmf;
    pmf.distanceMaximum = distanceMaximum;
    pmf.distanceBins = distanceBins;
    pmf.angleBins = angleBins;
    pmf.counts.assign(static_cast<size_t>(distanceBins) * static_cast<size_t>(angleBins), 0);
    pmf.freeEnergy.assign(pmf.counts.size(), std::numeric_limits<double>::infinity());
    pmf.inBasin.assign(pmf.counts.size(), 0);

    const double distanceBinWidth = distanceMaximum / static_cast<double>(distanceBins);
    const double angleBinWidth = 180.0 / static_cast<double>(angleBins);

    for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
        for(const CandidateTripletSample& sample : snapshot.candidates) {
            if(sample.distance > distanceMaximum)
                continue;
            const int distanceBin = clampedBinIndex(sample.distance, distanceMaximum, distanceBins);
            const int angleBin = clampedBinIndex(sample.theta, 180.0, angleBins);
            if(distanceBin < 0 || angleBin < 0)
                continue;
            pmf.counts[pmfLinearIndex(distanceBin, angleBin, angleBins)]++;
        }
    }

    double minimumFreeEnergy = std::numeric_limits<double>::infinity();
    int minimumIndex = -1;
    for(int distanceBin = 0; distanceBin < distanceBins; ++distanceBin) {
        const double distanceCenter = (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        for(int angleBin = 0; angleBin < angleBins; ++angleBin) {
            const size_t linearIndex = pmfLinearIndex(distanceBin, angleBin, angleBins);
            const int64_t count = pmf.counts[linearIndex];
            if(count <= 0)
                continue;

            const double thetaRadians = qDegreesToRadians((static_cast<double>(angleBin) + 0.5) * angleBinWidth);
            const double jacobian = std::max(distanceCenter * distanceCenter * std::max(std::sin(thetaRadians), 1e-6), 1e-12);
            const double reducedProbability = static_cast<double>(count) / jacobian;
            const double freeEnergy = -std::log(reducedProbability);
            pmf.freeEnergy[linearIndex] = freeEnergy;
            pmf.populatedBinCount++;
            if(freeEnergy < minimumFreeEnergy) {
                minimumFreeEnergy = freeEnergy;
                minimumIndex = static_cast<int>(linearIndex);
            }
        }
    }

    if(minimumIndex < 0)
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The PMF-derived hydrogen-bond definition found no donor-hydrogen-acceptor triplets within the PMF distance maximum."));

    for(double& value : pmf.freeEnergy) {
        if(std::isfinite(value))
            value -= minimumFreeEnergy;
    }

    std::vector<double> thresholds;
    thresholds.reserve(pmf.populatedBinCount);
    for(double value : pmf.freeEnergy) {
        if(std::isfinite(value))
            thresholds.push_back(value);
    }
    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-12;
    }), thresholds.end());

    double boundaryThreshold = thresholds.back();
    size_t previousArea = 0;
    long long bestJump = std::numeric_limits<long long>::min();
    if(thresholds.size() > 1) {
        for(size_t thresholdIndex = 0; thresholdIndex < thresholds.size(); ++thresholdIndex) {
            const std::vector<int> component = connectedComponentAtThreshold(pmf.freeEnergy,
                                                                             distanceBins,
                                                                             angleBins,
                                                                             minimumIndex,
                                                                             thresholds[thresholdIndex]);
            const size_t area = component.size();
            if(thresholdIndex > 0) {
                const long long jump = static_cast<long long>(area) - static_cast<long long>(previousArea);
                if(jump > bestJump) {
                    bestJump = jump;
                    boundaryThreshold = thresholds[thresholdIndex - 1];
                }
            }
            previousArea = area;
        }
    }

    const std::vector<int> basinComponent = connectedComponentAtThreshold(pmf.freeEnergy,
                                                                          distanceBins,
                                                                          angleBins,
                                                                          minimumIndex,
                                                                          boundaryThreshold);
    pmf.boundaryFreeEnergy = boundaryThreshold;
    pmf.basinBinCount = basinComponent.size();

    double vicinityCutoff = 0.0;
    for(int linearIndex : basinComponent) {
        pmf.inBasin[static_cast<size_t>(linearIndex)] = 1;
        const int distanceBin = linearIndex / angleBins;
        const double distanceCenter = (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        vicinityCutoff = std::max(vicinityCutoff, distanceCenter);
    }
    pmf.vicinityCutoff = vicinityCutoff;
    return pmf;
}

bool pmfTripletIsHydrogenBonded(const CandidateTripletSample& sample, const PmfDefinition& pmf)
{
    if(sample.distance > pmf.distanceMaximum)
        return false;
    const int distanceBin = clampedBinIndex(sample.distance, pmf.distanceMaximum, pmf.distanceBins);
    const int angleBin = clampedBinIndex(sample.theta, 180.0, pmf.angleBins);
    if(distanceBin < 0 || angleBin < 0)
        return false;
    return pmf.inBasin[pmfLinearIndex(distanceBin, angleBin, pmf.angleBins)] != 0;
}

FrameState buildFrameState(const FrameHydrogenBondSnapshot& snapshot,
                           HydrogenBondKineticsModifier::DefinitionMode definitionMode,
                           double fixedHydrogenBondDistanceCutoff,
                           double fixedMaximumTheta,
                           double fixedVicinityCutoff,
                           const PmfDefinition* pmf)
{
    FrameState state;
    state.activeTriplets.reserve(snapshot.candidates.size());
    state.vicinityPairs.reserve(snapshot.candidates.size());

    const double vicinityCutoff = (definitionMode == HydrogenBondKineticsModifier::PMFDerived && pmf)
        ? pmf->vicinityCutoff
        : fixedVicinityCutoff;

    for(const CandidateTripletSample& sample : snapshot.candidates) {
        if(sample.distance <= vicinityCutoff)
            state.vicinityPairs.insert(sample.pair);

        bool active = false;
        if(definitionMode == HydrogenBondKineticsModifier::PMFDerived) {
            active = pmf && pmfTripletIsHydrogenBonded(sample, *pmf);
        }
        else {
            active = (sample.distance <= fixedHydrogenBondDistanceCutoff && sample.theta <= fixedMaximumTheta);
        }

        if(active)
            state.activeTriplets.insert(sample.triplet);
    }

    return state;
}

HydrogenBondKineticsCurves computeKineticsCurves(const std::vector<FrameState>& states,
                                                 const std::vector<int>& frames,
                                                 int requestedMaxLag)
{
    HydrogenBondKineticsCurves curves;
    if(states.empty())
        return curves;

    const size_t frameCount = states.size();
    const size_t maxLagEffective = std::min<size_t>((requestedMaxLag > 0 ? static_cast<size_t>(requestedMaxLag) : frameCount - 1), frameCount - 1);

    curves.lagFrames.assign(maxLagEffective + 1, 0.0);
    curves.c.assign(maxLagEffective + 1, std::numeric_limits<double>::quiet_NaN());
    curves.n.assign(maxLagEffective + 1, std::numeric_limits<double>::quiet_NaN());
    curves.cPlusN.assign(maxLagEffective + 1, std::numeric_limits<double>::quiet_NaN());
    curves.sampleCounts.assign(maxLagEffective + 1, 0.0);

    parallelFor(maxLagEffective + 1, 128, TaskProgress::Ignore, [&states, &frames, &curves](size_t lag) {
        this_task::throwIfCanceled();

        double cCount = 0.0;
        double nCount = 0.0;
        double sampleCount = 0.0;
        const size_t originCount = states.size() - lag;
        for(size_t origin = 0; origin < originCount; ++origin) {
            const FrameState& initialState = states[origin];
            const FrameState& laggedState = states[origin + lag];
            for(const TripletKey& triplet : initialState.activeTriplets) {
                sampleCount += 1.0;
                if(laggedState.activeTriplets.find(triplet) != laggedState.activeTriplets.end()) {
                    cCount += 1.0;
                }
                else {
                    const PairKey pair{triplet.donorId, triplet.acceptorId};
                    if(laggedState.vicinityPairs.find(pair) != laggedState.vicinityPairs.end())
                        nCount += 1.0;
                }
            }
        }

        curves.lagFrames[lag] = static_cast<double>(frames[lag] - frames.front());
        curves.sampleCounts[lag] = sampleCount;
        if(sampleCount > 0.0) {
            curves.c[lag] = cCount / sampleCount;
            curves.n[lag] = nCount / sampleCount;
            curves.cPlusN[lag] = (cCount + nCount) / sampleCount;
        }
    });

    return curves;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondKineticsModifier);
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "DisplayName", "Hydrogen bond kinetics");
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "Description",
                "Compute hydrogen-bond kinetic correlation curves C(t), n(t), and C(t)+n(t) from exact donor-hydrogen-acceptor triplets.");
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, donorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, hydrogenTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, acceptorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, donorHydrogenCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, definitionMode);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, donorAcceptorCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, angleCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, vicinityCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, pmfDistanceMaximum);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, pmfDistanceBins);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, pmfAngleBins);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, intervalStart);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, maxLag);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, donorTypes, "Donor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, hydrogenTypes, "Hydrogen atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, acceptorTypes, "Acceptor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, donorHydrogenCutoff, "Donor-hydrogen cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, definitionMode, "Hydrogen-bond definition");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, donorAcceptorCutoff, "HB donor-acceptor cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, angleCutoff, "HB theta maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, vicinityCutoff, "Vicinity donor-acceptor cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, pmfDistanceMaximum, "PMF distance maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, pmfDistanceBins, "PMF distance bins");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, pmfAngleBins, "PMF angle bins");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, maxLag, "Maximum lag (sampled-frame steps)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondKineticsModifier, donorHydrogenCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondKineticsModifier, donorAcceptorCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, angleCutoff, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondKineticsModifier, vicinityCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondKineticsModifier, pmfDistanceMaximum, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, pmfDistanceBins, IntegerParameterUnit, 4, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, pmfAngleBins, IntegerParameterUnit, 4, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondKineticsModificationNode);
DEFINE_REFERENCE_FIELD(HydrogenBondKineticsModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(HydrogenBondKineticsModifier, HydrogenBondKineticsModificationNode);

bool HydrogenBondKineticsModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void HydrogenBondKineticsModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

QVariant HydrogenBondKineticsModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    const QString donors = donorTypes().trimmed();
    const QString hydrogens = hydrogenTypes().trimmed();
    const QString acceptors = acceptorTypes().trimmed();
    if(donors.isEmpty() || hydrogens.isEmpty() || acceptors.isEmpty())
        return {};
    return tr("D: %1, H: %2, A: %3").arg(donors, hydrogens, acceptors);
}

std::vector<int> HydrogenBondKineticsModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);
    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("Hydrogen-bond kinetics requires an upstream data source with trajectory frames."));

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
        throw Exception(tr("Hydrogen-bond kinetics requires at least two sampled trajectory frames."));

    return result;
}

void HydrogenBondKineticsModifier::inputCachingHints(ModifierEvaluationRequest& request)
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

void HydrogenBondKineticsModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                       PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                       TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

void HydrogenBondKineticsModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

Future<PipelineFlowState> HydrogenBondKineticsModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                         PipelineFlowState&& state)
{
    if(auto* modNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Hydrogen-bond kinetics is idle. Open the Run section and click 'Run hydrogen-bond kinetics' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Hydrogen-bond kinetics is queued. Click 'Run hydrogen-bond kinetics' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeHydrogenBondKinetics(request, std::move(state));
}

Future<PipelineFlowState> HydrogenBondKineticsModifier::computeHydrogenBondKinetics(const ModifierEvaluationRequest& request,
                                                                                     PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    const std::vector<int> donorTypeIds = parseTypeIds(donorTypes(), particleTypeProperty, tr("donor atom type"));
    const std::vector<int> hydrogenTypeIds = parseTypeIds(hydrogenTypes(), particleTypeProperty, tr("hydrogen atom type"));
    const std::vector<int> acceptorTypeIds = parseTypeIds(acceptorTypes(), particleTypeProperty, tr("acceptor atom type"));
    if(donorHydrogenCutoff() <= 0)
        throw Exception(tr("The donor-hydrogen cutoff must be positive."));
    if(definitionMode() == FixedGeometry) {
        if(donorAcceptorCutoff() <= 0)
            throw Exception(tr("The HB donor-acceptor cutoff must be positive."));
        if(vicinityCutoff() <= 0)
            throw Exception(tr("The vicinity donor-acceptor cutoff must be positive."));
        if(angleCutoff() < 0 || angleCutoff() > 180)
            throw Exception(tr("The HB theta maximum must be in the range [0, 180]."));
    }

    const std::unordered_set<int> donorTypeSet(donorTypeIds.begin(), donorTypeIds.end());
    const std::unordered_set<int> hydrogenTypeSet(hydrogenTypeIds.begin(), hydrogenTypeIds.end());
    const std::unordered_set<int> acceptorTypeSet(acceptorTypeIds.begin(), acceptorTypeIds.end());
    const bool useBondTopology = particles->bonds() && particles->bonds()->getProperty(Bonds::TopologyProperty);
    const PmfDefinition upstreamPmf = definitionMode() == PMFDerived
        ? loadUpstreamPmfDefinition(state, donorTypes(), hydrogenTypes(), acceptorTypes(), donorHydrogenCutoff())
        : PmfDefinition{};
    const double donorAcceptorSearchCutoff = definitionMode() == PMFDerived
        ? upstreamPmf.distanceMaximum
        : std::max(static_cast<double>(donorAcceptorCutoff()), static_cast<double>(vicinityCutoff()));

    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode())
        ? dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    HydrogenBondAccumulator accumulator;
    accumulator.snapshots.reserve(frames.size());

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, HydrogenBondAccumulator&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [donorTypeSet,
             hydrogenTypeSet,
             acceptorTypeSet,
             donorHydrogenCutoff = donorHydrogenCutoff(),
             donorAcceptorSearchCutoff,
             useBondTopology](const std::vector<int>& frameBatch,
                              std::vector<SharedFuture<PipelineFlowState>> batchFutures,
                              HydrogenBondAccumulator& accumulator) {
                for(size_t i = 0; i < batchFutures.size(); ++i) {
                    this_task::throwIfCanceled();
                    FrameHydrogenBondSnapshot snapshot = analyzeFrame(batchFutures[i].result(),
                                                                     frameBatch[i],
                                                                     donorTypeSet,
                                                                     hydrogenTypeSet,
                                                                     acceptorTypeSet,
                                                                     donorHydrogenCutoff,
                                                                     static_cast<FloatType>(donorAcceptorSearchCutoff),
                                                                     useBondTopology);
                    accumulator.totalDonorAtoms += snapshot.donorCount;
                    accumulator.totalHydrogenAtoms += snapshot.hydrogenCount;
                    accumulator.totalAcceptorAtoms += snapshot.acceptorCount;
                    accumulator.totalDonorHydrogenPairs += snapshot.donorHydrogenPairCount;
                    accumulator.totalCandidateTriplets += snapshot.candidates.size();
                    accumulator.usedParticleIndices = accumulator.usedParticleIndices || snapshot.usedParticleIndices;
                    accumulator.snapshots.push_back(std::move(snapshot));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this),
              [this, request, state = std::move(state), frames, cacheGenerationId, useBondTopology, upstreamPmf](HydrogenBondAccumulator accumulator) mutable -> Future<PipelineFlowState> {
        OORef<HydrogenBondKineticsModifier> self(this);
        const int completedRunRequestId = runRequestId();

        return asyncLaunch([self = std::move(self),
                            request = ModifierEvaluationRequest(request),
                            state = std::move(state),
                            frames,
                            accumulator = std::move(accumulator),
                            useBondTopology,
                            upstreamPmf,
                            completedRunRequestId,
                            cacheGenerationId]() mutable {
            HydrogenBondKineticsComputationResult computationResult{std::move(state)};

            if(!dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode()))
                return computationResult;

            this_task::throwIfCanceled();

            if(accumulator.snapshots.empty())
                throw Exception(HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics did not sample any trajectory frames."));
            if(accumulator.totalDonorAtoms == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No particles matched the selected donor atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalHydrogenAtoms == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No particles matched the selected hydrogen atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalAcceptorAtoms == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No particles matched the selected acceptor atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalDonorHydrogenPairs == 0) {
                throw Exception(useBondTopology
                    ? HydrogenBondKineticsModifier::tr(
                        "No donor-hydrogen pairs were found in the bond topology for the selected donor and hydrogen atom type(s).")
                    : HydrogenBondKineticsModifier::tr(
                        "No donor-hydrogen pairs were found within the donor-hydrogen cutoff. Increase the cutoff or adjust the atom types."));
            }
            if(accumulator.totalCandidateTriplets == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No donor-hydrogen-acceptor triplets were found within the chosen donor-acceptor search range."));

            const double fixedMaximumTheta = static_cast<double>(self->angleCutoff());
            const PmfDefinition* pmfPtr = self->definitionMode() == PMFDerived ? &upstreamPmf : nullptr;

            std::vector<FrameState> states;
            states.reserve(accumulator.snapshots.size());
            for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
                this_task::throwIfCanceled();
                states.push_back(buildFrameState(snapshot,
                                                 self->definitionMode(),
                                                 static_cast<double>(self->donorAcceptorCutoff()),
                                                 fixedMaximumTheta,
                                                 static_cast<double>(self->vicinityCutoff()),
                                                 pmfPtr));
            }

            const HydrogenBondKineticsCurves curves = computeKineticsCurves(states, frames, self->maxLag());
            if(curves.c.empty())
                throw Exception(HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics could not compute any lag points."));
            if(curves.sampleCounts[0] <= 0.0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No hydrogen bonds satisfied the chosen definition at lag zero, so the kinetics curves cannot be normalized."));

            computationResult.results = DataOORef<DataCollection>::create();
            const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
            createMultiCurveLineTable(computationResult.results,
                                      HydrogenBondKineticsModifier::kineticsTableId(),
                                      HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics"),
                                      curves.lagFrames,
                                      {curves.c, curves.n, curves.cPlusN},
                                      {QStringLiteral("C(t)"), QStringLiteral("n(t)"), QStringLiteral("C(t)+n(t)")},
                                      HydrogenBondKineticsModifier::tr("Lag (source frames)"),
                                      HydrogenBondKineticsModifier::tr("Conditional probability"),
                                      createdByNode);

            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_types"), self->donorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.hydrogen_types"), self->hydrogenTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.acceptor_types"), self->acceptorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.definition_mode"), definitionModeLabel(self->definitionMode()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.total_candidate_triplets"), static_cast<double>(accumulator.totalCandidateTriplets), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_hydrogen_pairing_mode"), donorHydrogenPairingModeLabel(useBondTopology), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_C"), curves.c.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_n"), curves.n.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_C_plus_n"), curves.cPlusN.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_C"), curves.c.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_n"), curves.n.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_C_plus_n"), curves.cPlusN.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.maximum_lag"), static_cast<double>(curves.lagFrames.back()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.initial_triplet_samples"), curves.sampleCounts.front(), createdByNode);
            if(self->definitionMode() == FixedGeometry) {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.hb_donor_acceptor_cutoff"), static_cast<double>(self->donorAcceptorCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.hb_theta_maximum"), static_cast<double>(self->angleCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.vicinity_cutoff"), static_cast<double>(self->vicinityCutoff()), createdByNode);
            }
            else {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_distance_maximum"), upstreamPmf.distanceMaximum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_distance_bins"), static_cast<double>(upstreamPmf.distanceBins), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_angle_bins"), static_cast<double>(upstreamPmf.angleBins), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_boundary_free_energy"), upstreamPmf.boundaryFreeEnergy, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_vicinity_cutoff"), upstreamPmf.vicinityCutoff, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_basin_bin_count"), static_cast<double>(upstreamPmf.basinBinCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_populated_bin_count"), static_cast<double>(upstreamPmf.populatedBinCount), createdByNode);
            }

            QStringList warnings;
            if(accumulator.usedParticleIndices) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "The input does not provide 'Particle Identifier', so hydrogen-bond identities fall back to 1-based particle indices. "
                    "This assumes the particle order stays stable across trajectory frames.");
            }
            if(!useBondTopology) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "No bond topology was available, so donor-hydrogen pairs were identified geometrically using the donor-hydrogen cutoff.");
            }
            if(self->definitionMode() == PMFDerived) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "The PMF-derived mode uses the upstream 'Hydrogen bond analysis' PMF basin and derived vicinity boundary. "
                    "Re-run that modifier if you change the PMF resolution or particle-type definitions.");
            }
            computationResult.warningText = warnings.join(QLatin1Char('\n'));
            computationResult.completedRunRequestId = completedRunRequestId;
            computationResult.cacheGenerationId = cacheGenerationId;
            return computationResult;
        }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](HydrogenBondKineticsComputationResult computationResult) mutable {
            auto* modNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode());
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

PipelineFlowState HydrogenBondKineticsModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                   PipelineFlowState state) const
{
    auto* modNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

void HydrogenBondKineticsModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

bool HydrogenBondKineticsModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
