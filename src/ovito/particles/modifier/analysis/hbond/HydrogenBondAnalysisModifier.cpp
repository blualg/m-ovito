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
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "HydrogenBondAnalysisModifier.h"
#include "HydrogenBondPmf.h"
#include "HydrogenBondSiteEnergy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Ovito {

namespace {

struct TripletKey {
    IdentifierIntType donorId = 0;
    IdentifierIntType hydrogenId = 0;
    IdentifierIntType acceptorId = 0;

    auto operator<=>(const TripletKey&) const = default;
};

struct DonorHydrogenPair {
    size_t donorIndex = 0;
    size_t hydrogenIndex = 0;
    Vector3 donorToHydrogenDelta = Vector3::Zero();
};

struct CandidateTripletSample {
    TripletKey triplet;
    double distance = 0.0;
    double hydrogenAcceptorDistance = 0.0;
    double theta = 0.0;
    double coulombEnergy = std::numeric_limits<double>::quiet_NaN();
    double lennardJonesEnergy = std::numeric_limits<double>::quiet_NaN();
    double siteEnergy = std::numeric_limits<double>::quiet_NaN();
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

struct HydrogenBondObservation {
    int frame = 0;
    IdentifierIntType donorId = 0;
    IdentifierIntType hydrogenId = 0;
    IdentifierIntType acceptorId = 0;
    double distance = 0.0;
    double hydrogenAcceptorDistance = 0.0;
    double theta = 0.0;
    double angle = 0.0;
    double coulombEnergy = std::numeric_limits<double>::quiet_NaN();
    double lennardJonesEnergy = std::numeric_limits<double>::quiet_NaN();
    double siteEnergy = std::numeric_limits<double>::quiet_NaN();
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

struct PmfDefinition {
    double distanceMinimum = 0.0;
    double distanceMaximum = 0.0;
    double thetaMinimum = 0.0;
    double thetaMaximum = 180.0;
    int distanceBins = 0;
    int angleBins = 0;
    double boundaryFreeEnergy = 0.0;
    double vicinityCutoff = 0.0;
    size_t basinBinCount = 0;
    size_t populatedBinCount = 0;
    std::vector<int64_t> counts;
    std::vector<double> smoothedReducedDensity;
    std::vector<double> freeEnergy;
    std::vector<char> inBasin;
    double distanceBandwidth = 0.0;
    double angleBandwidth = 0.0;
    double referenceShellFraction = 0.0;
    double referenceDistanceMinimum = 0.0;
    double referenceDensity = 0.0;
    double minimumFreeEnergy = 0.0;
    double minimumDistance = 0.0;
    double minimumTheta = 0.0;
    double minimumRequiredWellDepth = 0.0;
};

struct HydrogenBondComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

struct SiteEnergyParameters {
    bool enabled = false;
    HydrogenBondSiteEnergy::Parameters potential;
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
            throw Exception(HydrogenBondAnalysisModifier::tr(
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

QString definitionModeLabel(HydrogenBondAnalysisModifier::DefinitionMode mode)
{
    switch(mode) {
    case HydrogenBondAnalysisModifier::FixedGeometry:
        return HydrogenBondAnalysisModifier::tr("Fixed geometry");
    case HydrogenBondAnalysisModifier::PMFDerived:
        return HydrogenBondAnalysisModifier::tr("PMF-derived");
    case HydrogenBondAnalysisModifier::SiteInteractionEnergy:
        return HydrogenBondAnalysisModifier::tr("D/H/A site interaction energy");
    }
    OVITO_ASSERT(false);
    return {};
}

QString siteEnergyUnitLabel(HydrogenBondAnalysisModifier::SiteEnergyUnit unit)
{
    switch(unit) {
    case HydrogenBondAnalysisModifier::KcalPerMol:
        return HydrogenBondAnalysisModifier::tr("kcal/mol");
    case HydrogenBondAnalysisModifier::ElectronVolt:
        return HydrogenBondAnalysisModifier::tr("eV");
    }
    OVITO_ASSERT(false);
    return {};
}

QString siteEnergyCutoffModeLabel(HydrogenBondAnalysisModifier::SiteEnergyCutoffMode mode)
{
    switch(mode) {
    case HydrogenBondAnalysisModifier::AutomaticEnergyMinimum:
        return HydrogenBondAnalysisModifier::tr("Automatic distribution minimum");
    case HydrogenBondAnalysisModifier::ManualEnergyCutoff:
        return HydrogenBondAnalysisModifier::tr("Manual");
    }
    OVITO_ASSERT(false);
    return {};
}

QString automaticCutoffFailureMessage(HydrogenBondSiteEnergy::AutomaticCutoffStatus status)
{
    switch(status) {
    case HydrogenBondSiteEnergy::AutomaticCutoffStatus::Success:
        return {};
    case HydrogenBondSiteEnergy::AutomaticCutoffStatus::TooFewSamples:
        return HydrogenBondAnalysisModifier::tr(
            "Automatic energy-cutoff calibration requires at least 500 finite D/H/A candidate energies. "
            "Sample more frames, enlarge the candidate range, or use a manual cutoff.");
    case HydrogenBondSiteEnergy::AutomaticCutoffStatus::DegenerateDistribution:
        return HydrogenBondAnalysisModifier::tr(
            "The D/H/A candidate-energy distribution has no usable finite range. "
            "Check the charges, Lennard-Jones parameters, and candidate selectors.");
    case HydrogenBondSiteEnergy::AutomaticCutoffStatus::NoResolvedMinimum:
        return HydrogenBondAnalysisModifier::tr(
            "Automatic cutoff not found: the energy PDF has no stable minimum between two resolved populations; "
            "no hydrogen bonds were classified.");
    }
    OVITO_ASSERT(false);
    return {};
}

double siteEnergyCoulombConstant(HydrogenBondAnalysisModifier::SiteEnergyUnit unit)
{
    // Charges are in elementary-charge units and distances are in angstroms.
    switch(unit) {
    case HydrogenBondAnalysisModifier::KcalPerMol:
        return 332.063713299;
    case HydrogenBondAnalysisModifier::ElectronVolt:
        return 14.3996454784255;
    }
    OVITO_ASSERT(false);
    return 0.0;
}

QString donorHydrogenPairingModeLabel(bool useBondTopology)
{
    return useBondTopology
        ? HydrogenBondAnalysisModifier::tr("Bond topology")
        : HydrogenBondAnalysisModifier::tr("Distance cutoff");
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
                                                            QStringLiteral("Frame"));
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

DataTable* createObservationTable(DataCollection* collection,
                                  const QStringView identifier,
                                  const std::vector<HydrogenBondObservation>& observations,
                                  bool includeSiteEnergies,
                                  const OOWeakRef<const PipelineNode>& createdByNode)
{
    DataTable* table = collection->createObject<DataTable>(identifier.toString(),
                                                           createdByNode,
                                                           DataTable::None,
                                                           HydrogenBondAnalysisModifier::tr("Hydrogen bond observations"));
    table->setElementCount(observations.size());

    Property* frameProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Frame"), Property::Int64, 1);
    Property* donorProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Donor"), Property::Int64, 1);
    Property* hydrogenProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Hydrogen"), Property::Int64, 1);
    Property* acceptorProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Acceptor"), Property::Int64, 1);
    Property* distanceProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Distance"), Property::FloatDefault, 1);
    Property* hydrogenAcceptorDistanceProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Hydrogen-acceptor distance"), Property::FloatDefault, 1);
    Property* thetaProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Theta"), Property::FloatDefault, 1);
    Property* angleProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Angle"), Property::FloatDefault, 1);

    BufferWriteAccess<int64_t, access_mode::discard_write> frames(frameProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> donors(donorProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> hydrogens(hydrogenProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> acceptors(acceptorProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> distances(distanceProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> hydrogenAcceptorDistances(hydrogenAcceptorDistanceProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> thetas(thetaProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> angles(angleProperty);

    for(size_t i = 0; i < observations.size(); ++i) {
        frames[i] = observations[i].frame;
        donors[i] = observations[i].donorId;
        hydrogens[i] = observations[i].hydrogenId;
        acceptors[i] = observations[i].acceptorId;
        distances[i] = static_cast<FloatType>(observations[i].distance);
        hydrogenAcceptorDistances[i] = static_cast<FloatType>(observations[i].hydrogenAcceptorDistance);
        thetas[i] = static_cast<FloatType>(observations[i].theta);
        angles[i] = static_cast<FloatType>(observations[i].angle);
    }

    if(includeSiteEnergies) {
        Property* coulombProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Coulomb energy"), Property::FloatDefault, 1);
        Property* lennardJonesProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("LJ energy"), Property::FloatDefault, 1);
        Property* siteEnergyProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("D/H/A site energy"), Property::FloatDefault, 1);
        BufferWriteAccess<FloatType, access_mode::discard_write> coulombEnergies(coulombProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> lennardJonesEnergies(lennardJonesProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> siteEnergies(siteEnergyProperty);
        for(size_t i = 0; i < observations.size(); ++i) {
            coulombEnergies[i] = static_cast<FloatType>(observations[i].coulombEnergy);
            lennardJonesEnergies[i] = static_cast<FloatType>(observations[i].lennardJonesEnergy);
            siteEnergies[i] = static_cast<FloatType>(observations[i].siteEnergy);
        }
    }

    return table;
}

DataTable* createSiteEnergyDistributionTable(
    DataCollection* collection,
    const QStringView identifier,
    const HydrogenBondSiteEnergy::AutomaticCutoffResult& distribution,
    const QString& energyUnit,
    const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(distribution.binCenters.empty()
       || distribution.binCenters.size() != distribution.probabilityDensity.size())
        return nullptr;

    PropertyPtr density = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized,
        distribution.probabilityDensity.size(),
        Property::FloatDefault,
        1,
        HydrogenBondAnalysisModifier::tr("Probability density"));
    BufferWriteAccess<FloatType, access_mode::discard_write> densityAcc(density);
    for(size_t i = 0; i < distribution.probabilityDensity.size(); ++i)
        densityAcc[i] = static_cast<FloatType>(distribution.probabilityDensity[i]);

    PropertyPtr energy = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized,
        distribution.binCenters.size(),
        Property::FloatDefault,
        1,
        HydrogenBondAnalysisModifier::tr("D/H/A site energy"));
    BufferWriteAccess<FloatType, access_mode::discard_write> energyAcc(energy);
    for(size_t i = 0; i < distribution.binCenters.size(); ++i)
        energyAcc[i] = static_cast<FloatType>(distribution.binCenters[i]);

    DataTable* table = collection->createObject<DataTable>(
        identifier.toString(),
        createdByNode,
        DataTable::Line,
        HydrogenBondAnalysisModifier::tr("D/H/A candidate site-energy distribution"),
        std::move(density),
        std::move(energy));
    table->setAxisLabelX(HydrogenBondAnalysisModifier::tr("D/H/A site energy (%1)").arg(energyUnit));
    table->setAxisLabelY(HydrogenBondAnalysisModifier::tr("Probability density"));
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
                                                           HydrogenBondAnalysisModifier::tr("Hydrogen-bond PMF W(r, theta) / kBT"));
    const size_t rowCount = static_cast<size_t>(pmf.distanceBins) * static_cast<size_t>(pmf.angleBins);
    table->setElementCount(rowCount);

    Property* distanceProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Distance"), Property::FloatDefault, 1);
    Property* thetaProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Theta"), Property::FloatDefault, 1);
    Property* countProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Count"), Property::Int64, 1);
    Property* densityProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Smoothed reduced density"), Property::FloatDefault, 1);
    Property* pmfProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Free energy"), Property::FloatDefault, 1);
    Property* basinProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("In HB basin"), Property::Int64, 1);

    BufferWriteAccess<FloatType, access_mode::discard_write> distanceAcc(distanceProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> thetaAcc(thetaProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> countAcc(countProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> densityAcc(densityProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> pmfAcc(pmfProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> basinAcc(basinProperty);

    const double distanceBinWidth = (pmf.distanceMaximum - pmf.distanceMinimum) / static_cast<double>(pmf.distanceBins);
    const double angleBinWidth = (pmf.thetaMaximum - pmf.thetaMinimum) / static_cast<double>(pmf.angleBins);
    size_t row = 0;
    for(int distanceBin = 0; distanceBin < pmf.distanceBins; ++distanceBin) {
        const double distanceCenter = pmf.distanceMinimum + (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        for(int angleBin = 0; angleBin < pmf.angleBins; ++angleBin, ++row) {
            const size_t linearIndex = static_cast<size_t>(distanceBin) * static_cast<size_t>(pmf.angleBins)
                                     + static_cast<size_t>(angleBin);
            distanceAcc[row] = static_cast<FloatType>(distanceCenter);
            thetaAcc[row] = static_cast<FloatType>(pmf.thetaMinimum + (static_cast<double>(angleBin) + 0.5) * angleBinWidth);
            countAcc[row] = pmf.counts[linearIndex];
            densityAcc[row] = static_cast<FloatType>(pmf.smoothedReducedDensity[linearIndex]);
            pmfAcc[row] = std::isfinite(pmf.freeEnergy[linearIndex])
                ? static_cast<FloatType>(pmf.freeEnergy[linearIndex])
                : std::numeric_limits<FloatType>::quiet_NaN();
            basinAcc[row] = pmf.inBasin[linearIndex] ? 1 : 0;
        }
    }

    return table;
}

std::vector<double> collectSiteEnergySamples(const HydrogenBondAccumulator& accumulator,
                                             double maximumTheta)
{
    std::vector<double> energies;
    energies.reserve(accumulator.totalCandidateTriplets);
    for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
        for(const CandidateTripletSample& sample : snapshot.candidates) {
            if(sample.theta > maximumTheta || !std::isfinite(sample.siteEnergy))
                continue;
            energies.push_back(sample.siteEnergy);
        }
    }
    return energies;
}

std::vector<DonorHydrogenPair> collectBondedDonorHydrogenPairs(const Particles* particles,
                                                               const BufferReadAccess<Point3>& positions,
                                                               const std::vector<uint8_t>& donorMask,
                                                               const std::vector<uint8_t>& hydrogenMask,
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

        const bool index1IsDonor = donorMask[index1] != 0;
        const bool index2IsDonor = donorMask[index2] != 0;
        const bool index1IsHydrogen = hydrogenMask[index1] != 0;
        const bool index2IsHydrogen = hydrogenMask[index2] != 0;

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
                                                                  const std::vector<uint8_t>& donorMask,
                                                                  const std::vector<uint8_t>& hydrogenMask,
                                                                  const SimulationCellData& cellData,
                                                                  FloatType cutoff)
{
    PropertyPtr hydrogenSelectionProperty = createSelectionPropertyFromMask(hydrogenMask);
    BufferReadAccess<SelectionIntType> hydrogenSelection(hydrogenSelectionProperty);
    CutoffNeighborFinder hydrogenFinder(cutoff, positions, cellData, hydrogenSelection);

    std::vector<DonorHydrogenPair> pairs;
    for(size_t donorIndex = 0; donorIndex < positions.size(); ++donorIndex) {
        if(!donorMask[donorIndex])
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
                                       const QString& donorTypes,
                                       const QString& donorExpression,
                                       const QString& hydrogenTypes,
                                       const QString& hydrogenExpression,
                                       const QString& acceptorTypes,
                                       const QString& acceptorExpression,
                                       FloatType donorHydrogenCutoff,
                                       FloatType donorAcceptorSearchCutoff,
                                       bool useBondTopology,
                                       const SiteEnergyParameters& siteEnergyParameters)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<IdentifierIntType> identifiers = particles->getProperty(Particles::IdentifierProperty);
    BufferReadAccess<FloatType> charges = particles->getProperty(Particles::ChargeProperty);
    if(siteEnergyParameters.enabled && !charges) {
        throw Exception(HydrogenBondAnalysisModifier::tr(
            "The D/H/A site-energy definition requires the particle property 'Charge' in every sampled frame."));
    }
    const SimulationCell* simCellObject = state.getObject<SimulationCell>();
    const SimulationCellData cellData = simCellObject
        ? SimulationCellData(*simCellObject)
        : SimulationCellData(positions, false, std::max(donorHydrogenCutoff, donorAcceptorSearchCutoff) / 2);
    const SimulationCellData* cellDataPtr = &cellData;

    FrameHydrogenBondSnapshot result;
    result.frame = sourceFrame;
    result.usedParticleIndices = !identifiers;

    result.donorCount = 0;
    std::vector<uint8_t> donorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        donorTypes, donorExpression,
        HydrogenBondAnalysisModifier::tr("donor atom selector"),
        HydrogenBondAnalysisModifier::tr("Hydrogen bond analysis"),
        &result.donorCount);
    result.hydrogenCount = 0;
    std::vector<uint8_t> hydrogenMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        hydrogenTypes, hydrogenExpression,
        HydrogenBondAnalysisModifier::tr("hydrogen atom selector"),
        HydrogenBondAnalysisModifier::tr("Hydrogen bond analysis"),
        &result.hydrogenCount);
    result.acceptorCount = 0;
    std::vector<uint8_t> acceptorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        acceptorTypes, acceptorExpression,
        HydrogenBondAnalysisModifier::tr("acceptor atom selector"),
        HydrogenBondAnalysisModifier::tr("Hydrogen bond analysis"),
        &result.acceptorCount);

    const auto particleId = [&](size_t particleIndex) -> IdentifierIntType {
        return identifiers ? identifiers[particleIndex] : static_cast<IdentifierIntType>(particleIndex + 1);
    };

    std::vector<DonorHydrogenPair> donorHydrogenPairs = useBondTopology
        ? collectBondedDonorHydrogenPairs(particles, positions, donorMask, hydrogenMask, cellDataPtr)
        : collectGeometricDonorHydrogenPairs(positions, donorMask, hydrogenMask, cellData, donorHydrogenCutoff);
    result.donorHydrogenPairCount = donorHydrogenPairs.size();

    PropertyPtr acceptorSelectionProperty = createSelectionPropertyFromMask(acceptorMask);
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

            const Vector3 hydrogenToAcceptorDelta = donorToAcceptorDelta - donorHydrogen.donorToHydrogenDelta;
            const FloatType haLength = hydrogenToAcceptorDelta.length();
            if(haLength <= FloatType(0))
                continue;

            const FloatType theta = qRadiansToDegrees(
                clampedAcos(donorHydrogen.donorToHydrogenDelta.dot(donorToAcceptorDelta) / (dhLength * daLength)));
            double coulombEnergy = std::numeric_limits<double>::quiet_NaN();
            double lennardJonesEnergy = std::numeric_limits<double>::quiet_NaN();
            double siteEnergy = std::numeric_limits<double>::quiet_NaN();
            if(siteEnergyParameters.enabled) {
                const HydrogenBondSiteEnergy::Components components =
                    HydrogenBondSiteEnergy::evaluate(
                        static_cast<double>(daLength),
                        static_cast<double>(haLength),
                        static_cast<double>(charges[donorHydrogen.donorIndex]),
                        static_cast<double>(charges[donorHydrogen.hydrogenIndex]),
                        static_cast<double>(charges[acceptorIndex]),
                        siteEnergyParameters.potential);
                coulombEnergy = components.coulomb;
                lennardJonesEnergy = components.lennardJones;
                siteEnergy = components.total;
            }
            result.candidates.push_back({
                {particleId(donorHydrogen.donorIndex), particleId(donorHydrogen.hydrogenIndex), particleId(acceptorIndex)},
                static_cast<double>(daLength),
                static_cast<double>(haLength),
                static_cast<double>(theta),
                coulombEnergy,
                lennardJonesEnergy,
                siteEnergy
            });
        }
    }

    return result;
}

size_t pmfLinearIndex(int distanceBin, int angleBin, int angleBins)
{
    return static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin);
}

int clampedBinIndex(double value, double minimum, double maximum, int binCount)
{
    if(!(value >= minimum) || !(maximum > minimum) || binCount <= 0)
        return -1;
    if(value > maximum)
        return -1;
    const double normalized = std::clamp((value - minimum) / (maximum - minimum), 0.0, 1.0 - std::numeric_limits<double>::epsilon());
    return std::clamp(static_cast<int>(std::floor(normalized * static_cast<double>(binCount))), 0, binCount - 1);
}

PmfDefinition buildPmfDefinition(const HydrogenBondAccumulator& accumulator,
                                 double distanceMinimum,
                                 double distanceMaximum,
                                 double thetaMinimum,
                                 double thetaMaximum,
                                 int distanceBins,
                                 int angleBins,
                                 double distanceBandwidth,
                                 double angleBandwidth,
                                 double referenceShellFraction)
{
    PmfDefinition pmf;
    pmf.distanceMinimum = distanceMinimum;
    pmf.distanceMaximum = distanceMaximum;
    pmf.thetaMinimum = thetaMinimum;
    pmf.thetaMaximum = thetaMaximum;
    pmf.distanceBins = distanceBins;
    pmf.angleBins = angleBins;
    pmf.counts.assign(static_cast<size_t>(distanceBins) * static_cast<size_t>(angleBins), 0);

    for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
        for(const CandidateTripletSample& sample : snapshot.candidates) {
            if(sample.distance < distanceMinimum || sample.distance > distanceMaximum
               || sample.theta < thetaMinimum || sample.theta > thetaMaximum)
                continue;
            const int distanceBin = clampedBinIndex(sample.distance, distanceMinimum, distanceMaximum, distanceBins);
            const int angleBin = clampedBinIndex(sample.theta, thetaMinimum, thetaMaximum, angleBins);
            if(distanceBin < 0 || angleBin < 0)
                continue;
            pmf.counts[pmfLinearIndex(distanceBin, angleBin, angleBins)]++;
        }
    }

    HydrogenBondPmf::Parameters parameters;
    parameters.distanceMinimum = distanceMinimum;
    parameters.distanceMaximum = distanceMaximum;
    parameters.thetaMinimum = thetaMinimum;
    parameters.thetaMaximum = thetaMaximum;
    parameters.distanceBins = distanceBins;
    parameters.angleBins = angleBins;
    parameters.distanceBandwidth = distanceBandwidth;
    parameters.angleBandwidth = angleBandwidth;
    parameters.referenceShellFraction = referenceShellFraction;

    try {
        HydrogenBondPmf::Definition definition =
            HydrogenBondPmf::buildDefinition(std::move(pmf.counts), parameters);
        pmf.counts = std::move(definition.counts);
        pmf.smoothedReducedDensity = std::move(definition.smoothedReducedDensity);
        pmf.freeEnergy = std::move(definition.freeEnergy);
        pmf.inBasin = std::move(definition.inBasin);
        pmf.boundaryFreeEnergy = definition.boundaryFreeEnergy;
        pmf.vicinityCutoff = definition.vicinityCutoff;
        pmf.basinBinCount = definition.basinBinCount;
        pmf.populatedBinCount = definition.populatedBinCount;
        pmf.distanceBandwidth = parameters.distanceBandwidth;
        pmf.angleBandwidth = parameters.angleBandwidth;
        pmf.referenceShellFraction = parameters.referenceShellFraction;
        pmf.referenceDistanceMinimum = definition.referenceDistanceMinimum;
        pmf.referenceDensity = definition.referenceDensity;
        pmf.minimumFreeEnergy = definition.minimumFreeEnergy;
        pmf.minimumDistance = definition.minimumDistance;
        pmf.minimumTheta = definition.minimumTheta;
        pmf.minimumRequiredWellDepth = definition.minimumRequiredWellDepth;
    }
    catch(const std::exception& error) {
        throw Exception(HydrogenBondAnalysisModifier::tr(
            "The PMF-derived hydrogen-bond definition failed: %1").arg(QString::fromLocal8Bit(error.what())));
    }
    return pmf;
}

bool pmfTripletIsHydrogenBonded(const CandidateTripletSample& sample, const PmfDefinition& pmf)
{
    if(sample.distance < pmf.distanceMinimum || sample.distance > pmf.distanceMaximum
       || sample.theta < pmf.thetaMinimum || sample.theta > pmf.thetaMaximum)
        return false;
    const int distanceBin = clampedBinIndex(sample.distance, pmf.distanceMinimum, pmf.distanceMaximum, pmf.distanceBins);
    const int angleBin = clampedBinIndex(sample.theta, pmf.thetaMinimum, pmf.thetaMaximum, pmf.angleBins);
    if(distanceBin < 0 || angleBin < 0)
        return false;
    return pmf.inBasin[pmfLinearIndex(distanceBin, angleBin, pmf.angleBins)] != 0;
}

bool candidateIsHydrogenBonded(const CandidateTripletSample& sample,
                               HydrogenBondAnalysisModifier::DefinitionMode definitionMode,
                               double fixedHydrogenBondDistanceCutoff,
                               double fixedMaximumTheta,
                               double siteEnergyCutoff,
                               double siteEnergyMaximumTheta,
                               const PmfDefinition* pmf)
{
    if(definitionMode == HydrogenBondAnalysisModifier::PMFDerived)
        return pmf && pmfTripletIsHydrogenBonded(sample, *pmf);
    if(definitionMode == HydrogenBondAnalysisModifier::SiteInteractionEnergy) {
        return std::isfinite(sample.siteEnergy)
            && sample.siteEnergy <= siteEnergyCutoff
            && sample.theta <= siteEnergyMaximumTheta;
    }
    return sample.distance <= fixedHydrogenBondDistanceCutoff && sample.theta <= fixedMaximumTheta;
}

std::vector<HydrogenBondObservation> buildObservations(const FrameHydrogenBondSnapshot& snapshot,
                                                       HydrogenBondAnalysisModifier::DefinitionMode definitionMode,
                                                       double fixedHydrogenBondDistanceCutoff,
                                                       double fixedMaximumTheta,
                                                       double siteEnergyCutoff,
                                                       double siteEnergyMaximumTheta,
                                                       const PmfDefinition* pmf)
{
    std::vector<HydrogenBondObservation> observations;
    observations.reserve(snapshot.candidates.size());
    for(const CandidateTripletSample& sample : snapshot.candidates) {
        if(!candidateIsHydrogenBonded(sample,
                                      definitionMode,
                                      fixedHydrogenBondDistanceCutoff,
                                      fixedMaximumTheta,
                                      siteEnergyCutoff,
                                      siteEnergyMaximumTheta,
                                      pmf))
            continue;

        observations.push_back({
            snapshot.frame,
            sample.triplet.donorId,
            sample.triplet.hydrogenId,
            sample.triplet.acceptorId,
            sample.distance,
            sample.hydrogenAcceptorDistance,
            sample.theta,
            180.0 - sample.theta,
            sample.coulombEnergy,
            sample.lennardJonesEnergy,
            sample.siteEnergy
        });
    }
    return observations;
}

DataTable* createGeometryClassificationTable(
    DataCollection* collection,
    const QStringView identifier,
    const HydrogenBondAccumulator& accumulator,
    HydrogenBondAnalysisModifier::DefinitionMode definitionMode,
    double fixedHydrogenBondDistanceCutoff,
    double fixedMaximumTheta,
    double siteEnergyCutoff,
    double siteEnergyMaximumTheta,
    const PmfDefinition* pmf,
    const OOWeakRef<const PipelineNode>& createdByNode)
{
    const size_t rowCount = accumulator.totalCandidateTriplets;
    PropertyPtr angles = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized,
        rowCount,
        Property::FloatDefault,
        1,
        HydrogenBondAnalysisModifier::tr("D-H-A angle"));
    PropertyPtr distances = DataTable::OOClass().createUserProperty(
        DataBuffer::Initialized,
        rowCount,
        Property::FloatDefault,
        1,
        HydrogenBondAnalysisModifier::tr("D-A distance"));
    BufferWriteAccess<FloatType, access_mode::discard_write> angleValues(angles);
    BufferWriteAccess<FloatType, access_mode::discard_write> distanceValues(distances);

    DataTable* table = collection->createObject<DataTable>(
        identifier.toString(),
        createdByNode,
        DataTable::Scatter,
        HydrogenBondAnalysisModifier::tr("D-A distance vs. D-H-A angle"),
        std::move(angles),
        std::move(distances));
    table->setAxisLabelX(HydrogenBondAnalysisModifier::tr("D-A distance"));
    table->setAxisLabelY(HydrogenBondAnalysisModifier::tr("D-H-A angle (degrees)"));

    Property* frameProperty =
        table->createProperty(DataBuffer::Initialized, QStringLiteral("Frame"), Property::Int64, 1);
    Property* donorProperty =
        table->createProperty(DataBuffer::Initialized, QStringLiteral("Donor"), Property::Int64, 1);
    Property* hydrogenProperty =
        table->createProperty(DataBuffer::Initialized, QStringLiteral("Hydrogen"), Property::Int64, 1);
    Property* acceptorProperty =
        table->createProperty(DataBuffer::Initialized, QStringLiteral("Acceptor"), Property::Int64, 1);
    Property* hydrogenBondProperty =
        table->createProperty(DataBuffer::Initialized, QStringLiteral("Is hydrogen bond"), Property::Int64, 1);

    BufferWriteAccess<int64_t, access_mode::discard_write> frames(frameProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> donors(donorProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> hydrogens(hydrogenProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> acceptors(acceptorProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> isHydrogenBond(hydrogenBondProperty);

    size_t row = 0;
    for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
        for(const CandidateTripletSample& sample : snapshot.candidates) {
            distanceValues[row] = static_cast<FloatType>(sample.distance);
            angleValues[row] = static_cast<FloatType>(180.0 - sample.theta);
            frames[row] = snapshot.frame;
            donors[row] = sample.triplet.donorId;
            hydrogens[row] = sample.triplet.hydrogenId;
            acceptors[row] = sample.triplet.acceptorId;
            isHydrogenBond[row] = candidateIsHydrogenBonded(
                sample,
                definitionMode,
                fixedHydrogenBondDistanceCutoff,
                fixedMaximumTheta,
                siteEnergyCutoff,
                siteEnergyMaximumTheta,
                pmf);
            ++row;
        }
    }
    OVITO_ASSERT(row == rowCount);
    return table;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondAnalysisModifier);
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "DisplayName", "Hydrogen bond analysis");
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "Description",
                "Analyze hydrogen bonds over a trajectory using geometry, PMF, or D/H/A site interaction energy.");
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorExpression);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, hydrogenTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, hydrogenExpression);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, acceptorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, acceptorExpression);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorHydrogenCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, definitionMode);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorAcceptorCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, angleCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, siteEnergyDistanceMaximum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, siteEnergyThetaMaximum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, siteEnergyCutoffMode);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, siteEnergyCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, siteEnergyUnit);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, relativePermittivity);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorLJEpsilon);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorLJSigma);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, hydrogenLJEpsilon);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, hydrogenLJSigma);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, acceptorLJEpsilon);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, acceptorLJSigma);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfDistanceMinimum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfDistanceMaximum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfThetaMinimum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfThetaMaximum);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfDistanceBins);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfAngleBins);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfDistanceBandwidth);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfAngleBandwidth);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, pmfReferenceShellFraction);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, intervalStart);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorTypes, "Donor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorExpression, "Donor expression");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, hydrogenTypes, "Hydrogen atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, hydrogenExpression, "Hydrogen expression");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, acceptorTypes, "Acceptor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, acceptorExpression, "Acceptor expression");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorHydrogenCutoff, "Donor-hydrogen cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, definitionMode, "Hydrogen-bond definition");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorAcceptorCutoff, "HB donor-acceptor cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, angleCutoff, "HB theta maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, siteEnergyDistanceMaximum, "Candidate D-A cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, siteEnergyThetaMaximum, "Candidate theta maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, siteEnergyCutoffMode, "Energy cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, siteEnergyCutoff, "Maximum D/H/A site energy");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, siteEnergyUnit, "Energy unit");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, relativePermittivity, "Relative permittivity");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorLJEpsilon, "Donor LJ epsilon");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorLJSigma, "Donor LJ sigma");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, hydrogenLJEpsilon, "Hydrogen LJ epsilon");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, hydrogenLJSigma, "Hydrogen LJ sigma");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, acceptorLJEpsilon, "Acceptor LJ epsilon");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, acceptorLJSigma, "Acceptor LJ sigma");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfDistanceMinimum, "PMF distance minimum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfDistanceMaximum, "PMF distance maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfThetaMinimum, "PMF theta minimum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfThetaMaximum, "PMF theta maximum");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfDistanceBins, "PMF distance bins");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfAngleBins, "PMF angle bins");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfDistanceBandwidth, "Distance smoothing bandwidth");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfAngleBandwidth, "Angle smoothing bandwidth");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, pmfReferenceShellFraction, "Outer reference fraction");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorHydrogenCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorAcceptorCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, angleCutoff, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, siteEnergyDistanceMaximum, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, siteEnergyThetaMaximum, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, relativePermittivity, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorLJEpsilon, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorLJSigma, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, hydrogenLJEpsilon, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, hydrogenLJSigma, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, acceptorLJEpsilon, FloatParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, acceptorLJSigma, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, pmfDistanceMinimum, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, pmfDistanceMaximum, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, pmfThetaMinimum, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, pmfThetaMaximum, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, pmfDistanceBins, IntegerParameterUnit, 4, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, pmfAngleBins, IntegerParameterUnit, 4, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, pmfDistanceBandwidth, WorldParameterUnit, 1e-6);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, pmfAngleBandwidth, FloatParameterUnit, 1e-6);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, pmfReferenceShellFraction, FloatParameterUnit, 0.05, 0.5);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondAnalysisModificationNode);
DEFINE_REFERENCE_FIELD(HydrogenBondAnalysisModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(HydrogenBondAnalysisModifier, HydrogenBondAnalysisModificationNode);

bool HydrogenBondAnalysisModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void HydrogenBondAnalysisModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

QVariant HydrogenBondAnalysisModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    const QString donors = donorExpression().trimmed().isEmpty() ? donorTypes().trimmed() : donorExpression().trimmed();
    const QString hydrogens = hydrogenExpression().trimmed().isEmpty() ? hydrogenTypes().trimmed() : hydrogenExpression().trimmed();
    const QString acceptors = acceptorExpression().trimmed().isEmpty() ? acceptorTypes().trimmed() : acceptorExpression().trimmed();
    if(donors.isEmpty() || hydrogens.isEmpty() || acceptors.isEmpty())
        return {};
    return tr("D: %1, H: %2, A: %3").arg(donors, hydrogens, acceptors);
}

std::vector<int> HydrogenBondAnalysisModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);
    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("Hydrogen bond analysis requires an upstream data source with trajectory frames."));

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

    if(result.empty())
        throw Exception(tr("Hydrogen bond analysis requires at least one sampled trajectory frame."));

    return result;
}

void HydrogenBondAnalysisModifier::inputCachingHints(ModifierEvaluationRequest& request)
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

void HydrogenBondAnalysisModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                       PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                       TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

void HydrogenBondAnalysisModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

Future<PipelineFlowState> HydrogenBondAnalysisModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                         PipelineFlowState&& state)
{
    if(auto* modNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus());
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus());
        return std::move(state);
    }

    return computeHydrogenBondData(request, std::move(state));
}

Future<PipelineFlowState> HydrogenBondAnalysisModifier::computeHydrogenBondData(const ModifierEvaluationRequest& request,
                                                                                PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    size_t initialMatchCount = 0;
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             donorTypes(), donorExpression(),
                             tr("donor atom selector"),
                             tr("Hydrogen bond analysis"),
                             &initialMatchCount);
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             hydrogenTypes(), hydrogenExpression(),
                             tr("hydrogen atom selector"),
                             tr("Hydrogen bond analysis"),
                             &initialMatchCount);
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             acceptorTypes(), acceptorExpression(),
                             tr("acceptor atom selector"),
                             tr("Hydrogen bond analysis"),
                             &initialMatchCount);
    if(donorHydrogenCutoff() <= 0)
        throw Exception(tr("The donor-hydrogen cutoff must be positive."));
    SiteEnergyParameters siteEnergyParameters;
    if(definitionMode() == FixedGeometry) {
        if(donorAcceptorCutoff() <= 0)
            throw Exception(tr("The HB donor-acceptor cutoff must be positive."));
        if(angleCutoff() < 0 || angleCutoff() > 180)
            throw Exception(tr("The HB theta maximum must be in the range [0, 180]."));
    }
    else if(definitionMode() == PMFDerived) {
        if(pmfDistanceMinimum() < 0)
            throw Exception(tr("The PMF distance minimum must be non-negative."));
        if(pmfDistanceMaximum() <= 0)
            throw Exception(tr("The PMF distance maximum must be positive."));
        if(pmfDistanceMaximum() <= pmfDistanceMinimum())
            throw Exception(tr("The PMF distance maximum must be greater than the PMF distance minimum."));
        if(pmfThetaMinimum() < 0 || pmfThetaMinimum() > 180)
            throw Exception(tr("The PMF theta minimum must be in the range [0, 180]."));
        if(pmfThetaMaximum() < 0 || pmfThetaMaximum() > 180)
            throw Exception(tr("The PMF theta maximum must be in the range [0, 180]."));
        if(pmfThetaMaximum() <= pmfThetaMinimum())
            throw Exception(tr("The PMF theta maximum must be greater than the PMF theta minimum."));
        if(pmfDistanceBandwidth() <= 0 || pmfAngleBandwidth() <= 0)
            throw Exception(tr("The PMF smoothing bandwidths must be positive."));
        if(pmfReferenceShellFraction() < 0.05 || pmfReferenceShellFraction() > 0.5)
            throw Exception(tr("The PMF outer reference fraction must be in the range [0.05, 0.5]."));
    }
    else {
        if(siteEnergyDistanceMaximum() <= 0)
            throw Exception(tr("The candidate donor-acceptor cutoff must be positive."));
        if(siteEnergyThetaMaximum() < 0 || siteEnergyThetaMaximum() > 180)
            throw Exception(tr("The candidate theta maximum must be in the range [0, 180]."));
        if(siteEnergyCutoffMode() == ManualEnergyCutoff
           && !std::isfinite(static_cast<double>(siteEnergyCutoff())))
            throw Exception(tr("The maximum D/H/A site energy must be finite."));
        if(relativePermittivity() <= 0)
            throw Exception(tr("The relative permittivity must be positive."));
        if(donorLJEpsilon() < 0 || donorLJSigma() < 0
           || hydrogenLJEpsilon() < 0 || hydrogenLJSigma() < 0
           || acceptorLJEpsilon() < 0 || acceptorLJSigma() < 0) {
            throw Exception(tr("Lennard-Jones epsilon and sigma parameters must be non-negative."));
        }
        if(!particles->getProperty(Particles::ChargeProperty)) {
            throw Exception(tr(
                "The D/H/A site-energy definition requires the particle property 'Charge'."));
        }

        siteEnergyParameters.enabled = true;
        siteEnergyParameters.potential.coulombConstant = siteEnergyCoulombConstant(siteEnergyUnit());
        siteEnergyParameters.potential.relativePermittivity = static_cast<double>(relativePermittivity());
        siteEnergyParameters.potential.donorEpsilon = static_cast<double>(donorLJEpsilon());
        siteEnergyParameters.potential.donorSigma = static_cast<double>(donorLJSigma());
        siteEnergyParameters.potential.hydrogenEpsilon = static_cast<double>(hydrogenLJEpsilon());
        siteEnergyParameters.potential.hydrogenSigma = static_cast<double>(hydrogenLJSigma());
        siteEnergyParameters.potential.acceptorEpsilon = static_cast<double>(acceptorLJEpsilon());
        siteEnergyParameters.potential.acceptorSigma = static_cast<double>(acceptorLJSigma());
    }
    const bool useBondTopology = particles->bonds() && particles->bonds()->getProperty(Bonds::TopologyProperty);
    const double donorAcceptorSearchCutoff =
        definitionMode() == PMFDerived
            ? static_cast<double>(pmfDistanceMaximum())
            : (definitionMode() == SiteInteractionEnergy
                   ? static_cast<double>(siteEnergyDistanceMaximum())
                   : static_cast<double>(donorAcceptorCutoff()));

    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())
        ? dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    HydrogenBondAccumulator accumulator;
    accumulator.snapshots.reserve(frames.size());
    auto progress = std::make_shared<TaskProgress>(this_task::ui());
    progress->setText(tr("Collecting hydrogen-bond samples"));
    progress->setMaximum(static_cast<qlonglong>(frames.size()));

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
            [donorTypes = donorTypes(),
             donorExpression = donorExpression(),
             hydrogenTypes = hydrogenTypes(),
             hydrogenExpression = hydrogenExpression(),
             acceptorTypes = acceptorTypes(),
             acceptorExpression = acceptorExpression(),
              donorHydrogenCutoff = donorHydrogenCutoff(),
              donorAcceptorSearchCutoff,
              useBondTopology,
              siteEnergyParameters,
              progress,
             totalFrameCount = frames.size()](const std::vector<int>& frameBatch,
                                              std::vector<SharedFuture<PipelineFlowState>> batchFutures,
                                              HydrogenBondAccumulator& accumulator) {
                for(size_t i = 0; i < batchFutures.size(); ++i) {
                    this_task::throwIfCanceled();
                    FrameHydrogenBondSnapshot snapshot = analyzeFrame(batchFutures[i].result(),
                                                                     frameBatch[i],
                                                                     donorTypes,
                                                                     donorExpression,
                                                                     hydrogenTypes,
                                                                     hydrogenExpression,
                                                                     acceptorTypes,
                                                                     acceptorExpression,
                                                                     donorHydrogenCutoff,
                                                                     static_cast<FloatType>(donorAcceptorSearchCutoff),
                                                                     useBondTopology,
                                                                     siteEnergyParameters);
                    accumulator.totalDonorAtoms += snapshot.donorCount;
                    accumulator.totalHydrogenAtoms += snapshot.hydrogenCount;
                    accumulator.totalAcceptorAtoms += snapshot.acceptorCount;
                    accumulator.totalDonorHydrogenPairs += snapshot.donorHydrogenPairCount;
                    accumulator.totalCandidateTriplets += snapshot.candidates.size();
                    accumulator.usedParticleIndices = accumulator.usedParticleIndices || snapshot.usedParticleIndices;
                    accumulator.snapshots.push_back(std::move(snapshot));
                    progress->setText(HydrogenBondAnalysisModifier::tr("Collecting hydrogen-bond samples (%1/%2 frames)")
                                          .arg(accumulator.snapshots.size())
                                          .arg(totalFrameCount));
                    progress->setValue(static_cast<qlonglong>(accumulator.snapshots.size()));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this),
              [this, request, state = std::move(state), frames, cacheGenerationId, useBondTopology, progress = std::move(progress)](HydrogenBondAccumulator accumulator) mutable -> Future<PipelineFlowState> {
        OORef<HydrogenBondAnalysisModifier> self(this);
        const int completedRunRequestId = runRequestId();

        return asyncLaunch([self = std::move(self),
                            request = ModifierEvaluationRequest(request),
                            state = std::move(state),
                            frames,
                            accumulator = std::move(accumulator),
                            useBondTopology,
                            progress = std::move(progress),
                            completedRunRequestId,
                            cacheGenerationId]() mutable {
            HydrogenBondComputationResult computationResult{std::move(state)};

            if(!dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode()))
                return computationResult;

            this_task::throwIfCanceled();
            progress->setText(HydrogenBondAnalysisModifier::tr("Computing hydrogen-bond results"));

            if(accumulator.snapshots.empty())
                throw Exception(HydrogenBondAnalysisModifier::tr("Hydrogen bond analysis did not sample any trajectory frames."));
            if(accumulator.totalDonorAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected donor atom selector in the sampled trajectory interval."));
            if(accumulator.totalHydrogenAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected hydrogen atom selector in the sampled trajectory interval."));
            if(accumulator.totalAcceptorAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected acceptor atom selector in the sampled trajectory interval."));
            if(accumulator.totalDonorHydrogenPairs == 0) {
                throw Exception(useBondTopology
                    ? HydrogenBondAnalysisModifier::tr(
                        "No donor-hydrogen pairs were found in the bond topology for the selected donor and hydrogen atom selectors.")
                    : HydrogenBondAnalysisModifier::tr(
                        "No donor-hydrogen pairs were found within the donor-hydrogen cutoff. Increase the cutoff or adjust the donor/hydrogen selectors."));
            }
            if(accumulator.totalCandidateTriplets == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No donor-hydrogen-acceptor triplets were found within the chosen donor-acceptor search range."));

            const double fixedMaximumTheta = static_cast<double>(self->angleCutoff());
            PmfDefinition pmf;
            const PmfDefinition* pmfPtr = nullptr;
            if(self->definitionMode() == PMFDerived) {
                pmf = buildPmfDefinition(accumulator,
                                         static_cast<double>(self->pmfDistanceMinimum()),
                                         static_cast<double>(self->pmfDistanceMaximum()),
                                         static_cast<double>(self->pmfThetaMinimum()),
                                         static_cast<double>(self->pmfThetaMaximum()),
                                         std::max(4, self->pmfDistanceBins()),
                                         std::max(4, self->pmfAngleBins()),
                                         static_cast<double>(self->pmfDistanceBandwidth()),
                                         static_cast<double>(self->pmfAngleBandwidth()),
                                         static_cast<double>(self->pmfReferenceShellFraction()));
                pmfPtr = &pmf;
            }

            HydrogenBondSiteEnergy::AutomaticCutoffResult siteEnergyDistribution;
            std::vector<double> siteEnergySamples;
            double effectiveSiteEnergyCutoff = static_cast<double>(self->siteEnergyCutoff());
            if(self->definitionMode() == SiteInteractionEnergy) {
                siteEnergySamples =
                    collectSiteEnergySamples(accumulator, static_cast<double>(self->siteEnergyThetaMaximum()));
                siteEnergyDistribution =
                    HydrogenBondSiteEnergy::findAutomaticCutoff(siteEnergySamples);
                if(self->siteEnergyCutoffMode() == AutomaticEnergyMinimum) {
                    effectiveSiteEnergyCutoff =
                        siteEnergyDistribution.status == HydrogenBondSiteEnergy::AutomaticCutoffStatus::Success
                            ? siteEnergyDistribution.cutoff
                            : std::numeric_limits<double>::quiet_NaN();
                }
            }

            std::vector<double> frameNumbers;
            std::vector<double> counts;
            std::vector<HydrogenBondObservation> observations;
            frameNumbers.reserve(accumulator.snapshots.size());
            counts.reserve(accumulator.snapshots.size());
            observations.reserve(accumulator.totalCandidateTriplets);

            for(const FrameHydrogenBondSnapshot& snapshot : accumulator.snapshots) {
                this_task::throwIfCanceled();
                std::vector<HydrogenBondObservation> frameObservations =
                    buildObservations(snapshot,
                                      self->definitionMode(),
                                      static_cast<double>(self->donorAcceptorCutoff()),
                                      fixedMaximumTheta,
                                      effectiveSiteEnergyCutoff,
                                      static_cast<double>(self->siteEnergyThetaMaximum()),
                                      pmfPtr);
                frameNumbers.push_back(static_cast<double>(snapshot.frame));
                counts.push_back(static_cast<double>(frameObservations.size()));
                observations.insert(observations.end(),
                                    std::make_move_iterator(frameObservations.begin()),
                                    std::make_move_iterator(frameObservations.end()));
            }

            computationResult.results = DataOORef<DataCollection>::create();
            const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
            createLineTable(computationResult.results,
                            HydrogenBondAnalysisModifier::countTableId(),
                            HydrogenBondAnalysisModifier::tr("Hydrogen bond count"),
                            frameNumbers,
                            counts,
                            HydrogenBondAnalysisModifier::tr("Source frame"),
                            HydrogenBondAnalysisModifier::tr("Hydrogen bond count"),
                            createdByNode);
            createObservationTable(computationResult.results,
                                   HydrogenBondAnalysisModifier::observationTableId(),
                                   observations,
                                   self->definitionMode() == SiteInteractionEnergy,
                                   createdByNode);
            createGeometryClassificationTable(
                computationResult.results,
                HydrogenBondAnalysisModifier::geometryClassificationTableId(),
                accumulator,
                self->definitionMode(),
                static_cast<double>(self->donorAcceptorCutoff()),
                fixedMaximumTheta,
                effectiveSiteEnergyCutoff,
                static_cast<double>(self->siteEnergyThetaMaximum()),
                pmfPtr,
                createdByNode);
            if(self->definitionMode() == PMFDerived)
                createPmfTable(computationResult.results, HydrogenBondAnalysisModifier::pmfTableId(), pmf, createdByNode);
            else if(self->definitionMode() == SiteInteractionEnergy) {
                createSiteEnergyDistributionTable(
                    computationResult.results,
                    HydrogenBondAnalysisModifier::siteEnergyDistributionTableId(),
                    siteEnergyDistribution,
                    siteEnergyUnitLabel(self->siteEnergyUnit()),
                    createdByNode);
            }

            const double sampledFrameCount = static_cast<double>(frames.size());
            const double totalHydrogenBonds = static_cast<double>(observations.size());
            const double averageCount = sampledFrameCount > 0 ? (totalHydrogenBonds / sampledFrameCount) : 0.0;
            const double maxCount = counts.empty() ? 0.0 : *std::max_element(counts.begin(), counts.end());
            double averageSiteEnergy = std::numeric_limits<double>::quiet_NaN();
            double minimumSiteEnergy = std::numeric_limits<double>::quiet_NaN();
            double maximumSiteEnergy = std::numeric_limits<double>::quiet_NaN();
            if(self->definitionMode() == SiteInteractionEnergy && !observations.empty()) {
                double siteEnergySum = 0.0;
                minimumSiteEnergy = std::numeric_limits<double>::infinity();
                maximumSiteEnergy = -std::numeric_limits<double>::infinity();
                for(const HydrogenBondObservation& observation : observations) {
                    siteEnergySum += observation.siteEnergy;
                    minimumSiteEnergy = std::min(minimumSiteEnergy, observation.siteEnergy);
                    maximumSiteEnergy = std::max(maximumSiteEnergy, observation.siteEnergy);
                }
                averageSiteEnergy = siteEnergySum / static_cast<double>(observations.size());
            }

            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_types"), self->donorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_expression"), self->donorExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_selector"), canonicalizeParticleSelector(self->donorTypes(), self->donorExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hydrogen_types"), self->hydrogenTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hydrogen_expression"), self->hydrogenExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hydrogen_selector"), canonicalizeParticleSelector(self->hydrogenTypes(), self->hydrogenExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.acceptor_types"), self->acceptorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.acceptor_expression"), self->acceptorExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.acceptor_selector"), canonicalizeParticleSelector(self->acceptorTypes(), self->acceptorExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.definition_mode"), definitionModeLabel(self->definitionMode()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"), static_cast<double>(self->donorHydrogenCutoff()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.sampled_frame_count"), sampledFrameCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.total_observations"), totalHydrogenBonds, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.average_count"), averageCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.maximum_count"), maxCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.total_candidate_triplets"), static_cast<double>(accumulator.totalCandidateTriplets), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_hydrogen_pairing_mode"), donorHydrogenPairingModeLabel(useBondTopology), createdByNode);
            if(self->definitionMode() == FixedGeometry) {
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hb_donor_acceptor_cutoff"), static_cast<double>(self->donorAcceptorCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hb_theta_maximum"), static_cast<double>(self->angleCutoff()), createdByNode);
            }
            else if(self->definitionMode() == PMFDerived) {
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_distance_minimum"), static_cast<double>(self->pmfDistanceMinimum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_distance_maximum"), static_cast<double>(self->pmfDistanceMaximum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_theta_minimum"), static_cast<double>(self->pmfThetaMinimum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_theta_maximum"), static_cast<double>(self->pmfThetaMaximum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_distance_bins"), static_cast<double>(self->pmfDistanceBins()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_angle_bins"), static_cast<double>(self->pmfAngleBins()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_distance_bandwidth"), pmf.distanceBandwidth, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_angle_bandwidth"), pmf.angleBandwidth, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_reference_shell_fraction"), pmf.referenceShellFraction, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_reference_distance_minimum"), pmf.referenceDistanceMinimum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_reference_density"), pmf.referenceDensity, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"), pmf.boundaryFreeEnergy, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"), pmf.vicinityCutoff, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_basin_bin_count"), static_cast<double>(pmf.basinBinCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_populated_bin_count"), static_cast<double>(pmf.populatedBinCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_minimum_free_energy"), pmf.minimumFreeEnergy, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_minimum_distance"), pmf.minimumDistance, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_minimum_theta"), pmf.minimumTheta, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_minimum_required_well_depth"), pmf.minimumRequiredWellDepth, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.pmf_energy_unit"), QStringLiteral("kBT"), createdByNode);
            }
            else {
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_distance_maximum"), static_cast<double>(self->siteEnergyDistanceMaximum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_theta_maximum"), static_cast<double>(self->siteEnergyThetaMaximum()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_cutoff_mode"), siteEnergyCutoffModeLabel(self->siteEnergyCutoffMode()), createdByNode);
                if(std::isfinite(effectiveSiteEnergyCutoff))
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_cutoff"), effectiveSiteEnergyCutoff, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_unit"), siteEnergyUnitLabel(self->siteEnergyUnit()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_calibration_sample_count"), static_cast<double>(siteEnergyDistribution.sampleCount), createdByNode);
                if(std::isfinite(siteEnergyDistribution.bandwidth))
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_calibration_bandwidth"), siteEnergyDistribution.bandwidth, createdByNode);
                if(siteEnergyDistribution.status == HydrogenBondSiteEnergy::AutomaticCutoffStatus::Success) {
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_lower_peak"), siteEnergyDistribution.lowerEnergyPeak, createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_upper_peak"), siteEnergyDistribution.upperEnergyPeak, createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_valley_ratio"), siteEnergyDistribution.valleyToPeakRatio, createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_lower_fraction"), siteEnergyDistribution.lowerEnergyFraction, createdByNode);
                }
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_relative_permittivity"), static_cast<double>(self->relativePermittivity()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_donor_lj_epsilon"), static_cast<double>(self->donorLJEpsilon()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_donor_lj_sigma"), static_cast<double>(self->donorLJSigma()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_hydrogen_lj_epsilon"), static_cast<double>(self->hydrogenLJEpsilon()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_hydrogen_lj_sigma"), static_cast<double>(self->hydrogenLJSigma()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_acceptor_lj_epsilon"), static_cast<double>(self->acceptorLJEpsilon()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_acceptor_lj_sigma"), static_cast<double>(self->acceptorLJSigma()), createdByNode);
                if(!observations.empty()) {
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_average"), averageSiteEnergy, createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_minimum"), minimumSiteEnergy, createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.site_energy_maximum"), maximumSiteEnergy, createdByNode);
                }
                computationResult.results->setAttribute(
                    QStringLiteral("HydrogenBonds.site_energy_model"),
                    QStringLiteral("Unshifted direct Coulomb + 12-6 LJ; Lorentz-Berthelot mixing; U_DA + U_HA"),
                    createdByNode);
            }

            QStringList warnings;
            if(accumulator.usedParticleIndices) {
                warnings << HydrogenBondAnalysisModifier::tr(
                    "The input does not provide 'Particle Identifier', so hydrogen-bond observations report 1-based particle indices. "
                    "This assumes the particle order stays stable across trajectory frames.");
            }
            if(!useBondTopology) {
                warnings << HydrogenBondAnalysisModifier::tr(
                    "No bond topology was available, so donor-hydrogen pairs were identified geometrically using the donor-hydrogen cutoff.");
            }
            if(self->definitionMode() == SiteInteractionEnergy) {
                if(self->siteEnergyCutoffMode() == AutomaticEnergyMinimum) {
                    if(siteEnergyDistribution.status != HydrogenBondSiteEnergy::AutomaticCutoffStatus::Success)
                        warnings << automaticCutoffFailureMessage(siteEnergyDistribution.status);
                }
            }
            computationResult.warningText = warnings.join(QLatin1Char('\n'));
            computationResult.completedRunRequestId = completedRunRequestId;
            computationResult.cacheGenerationId = cacheGenerationId;
            return computationResult;
        }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](HydrogenBondComputationResult computationResult) mutable {
            auto* modNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode());
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

PipelineFlowState HydrogenBondAnalysisModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                  PipelineFlowState state) const
{
    auto* modNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

void HydrogenBondAnalysisModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

bool HydrogenBondAnalysisModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
