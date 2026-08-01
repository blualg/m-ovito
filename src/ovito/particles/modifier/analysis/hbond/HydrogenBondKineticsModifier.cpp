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
#include <ovito/stdobj/table/StretchedExponentialFit.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "HydrogenBondAnalysisModifier.h"
#include "HydrogenBondKineticsModifier.h"
#include "HydrogenBondSiteEnergy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
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
    double siteEnergy = std::numeric_limits<double>::quiet_NaN();
};

struct FrameHydrogenBondSnapshot {
    int frame = 0;
    double trajectoryCoordinate = 0.0;
    bool usedTimestepAttribute = false;
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
    std::vector<double> freeEnergy;
    std::vector<char> inBasin;
};

struct SiteEnergyDefinition {
    double distanceMaximum = 0.0;
    double thetaMaximum = 180.0;
    double cutoff = std::numeric_limits<double>::quiet_NaN();
    QString cutoffMode;
    QString unitLabel;
    HydrogenBondSiteEnergy::Parameters potential;
};

struct HydrogenBondKineticsCurves {
    struct Counts {
        double samples = 0.0;
        double continuous = 0.0;
        double intermittent = 0.0;
        double nearbyUnbonded = 0.0;
    };

    std::vector<double> lagSourceFrames;
    std::vector<double> lagTimes;
    std::vector<double> continuous;
    std::vector<double> c;
    std::vector<double> n;
    std::vector<double> cPlusN;
    std::vector<double> reactiveFlux;
    std::vector<double> modeledReactiveFlux;
    std::vector<double> sampleCounts;
    std::vector<Counts> blockCounts;
    size_t blockCount = 0;
    size_t blockLength = 1;
};

struct LuzarChandlerFitResult {
    bool valid = false;
    QString status;
    double breakingRate = std::numeric_limits<double>::quiet_NaN();
    double reformationRate = std::numeric_limits<double>::quiet_NaN();
    double breakingLifetime = std::numeric_limits<double>::quiet_NaN();
    double reformationTime = std::numeric_limits<double>::quiet_NaN();
    double rSquared = std::numeric_limits<double>::quiet_NaN();
    double rmse = std::numeric_limits<double>::quiet_NaN();
    double fitStart = std::numeric_limits<double>::quiet_NaN();
    double fitEnd = std::numeric_limits<double>::quiet_NaN();
    int pointCount = 0;
};

struct ConfidenceInterval {
    double lower = std::numeric_limits<double>::quiet_NaN();
    double upper = std::numeric_limits<double>::quiet_NaN();
};

struct BootstrapResult {
    int requestedReplicates = 0;
    int successfulCurveReplicates = 0;
    int successfulRateReplicates = 0;
    int effectiveBlockLength = 1;
    ConfidenceInterval continuousLifetime;
    ConfidenceInterval intermittentCorrelationTime;
    ConfidenceInterval breakingRate;
    ConfidenceInterval reformationRate;
    ConfidenceInterval breakingLifetime;
    ConfidenceInterval reformationTime;
};

struct LifetimeEventStatistics {
    std::vector<double> durations;
    size_t leftCensoredCount = 0;
    size_t rightCensoredCount = 0;
    double mean = std::numeric_limits<double>::quiet_NaN();
    double median = std::numeric_limits<double>::quiet_NaN();
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
    case HydrogenBondKineticsModifier::SiteInteractionEnergy:
        return HydrogenBondKineticsModifier::tr("D/H/A site interaction energy");
    }
    OVITO_ASSERT(false);
    return {};
}

QString timeUnitLabel(HydrogenBondKineticsModifier::TimeUnit unit)
{
    switch(unit) {
    case HydrogenBondKineticsModifier::Femtoseconds:
        return QStringLiteral("fs");
    case HydrogenBondKineticsModifier::Picoseconds:
        return QStringLiteral("ps");
    case HydrogenBondKineticsModifier::Nanoseconds:
        return QStringLiteral("ns");
    }
    OVITO_ASSERT(false);
    return {};
}

PmfDefinition loadUpstreamPmfDefinition(const PipelineFlowState& state,
                                        const QString& donorTypes,
                                        const QString& donorExpression,
                                        const QString& hydrogenTypes,
                                        const QString& hydrogenExpression,
                                        const QString& acceptorTypes,
                                        const QString& acceptorExpression,
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
    const QVariant distanceMinimumVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_distance_minimum"));
    const QVariant distanceBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_distance_bins"));
    const QVariant thetaMinimumVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_theta_minimum"));
    const QVariant thetaMaximumVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_theta_maximum"));
    const QVariant angleBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_angle_bins"));
    const QVariant boundaryVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
    const QVariant vicinityVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"));
    const QVariant basinBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_basin_bin_count"));
    const QVariant populatedBinsVariant = state.getAttributeValue(QStringLiteral("HydrogenBonds.pmf_populated_bin_count"));

    if(!distanceMinimumVariant.isValid() || !distanceMaximumVariant.isValid() || !distanceBinsVariant.isValid()
       || !thetaMinimumVariant.isValid() || !thetaMaximumVariant.isValid()
       || !angleBinsVariant.isValid() || !vicinityVariant.isValid())
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream 'Hydrogen bond analysis' result does not provide the PMF-derived basin metadata required by hydrogen-bond kinetics."));

    const QString upstreamDonors = state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_selector")).toString();
    const QString upstreamHydrogens = state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_selector")).toString();
    const QString upstreamAcceptors = state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_selector")).toString();
    const QString localDonorSelector = canonicalizeParticleSelector(donorTypes, donorExpression);
    const QString localHydrogenSelector = canonicalizeParticleSelector(hydrogenTypes, hydrogenExpression);
    const QString localAcceptorSelector = canonicalizeParticleSelector(acceptorTypes, acceptorExpression);
    const QString fallbackUpstreamDonorSelector = canonicalizeParticleSelector(state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_types")).toString(),
                                                                               state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_expression")).toString());
    const QString fallbackUpstreamHydrogenSelector = canonicalizeParticleSelector(state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_types")).toString(),
                                                                                  state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_expression")).toString());
    const QString fallbackUpstreamAcceptorSelector = canonicalizeParticleSelector(state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_types")).toString(),
                                                                                  state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_expression")).toString());
    if(localDonorSelector != (upstreamDonors.isEmpty() ? fallbackUpstreamDonorSelector : upstreamDonors)
       || localHydrogenSelector != (upstreamHydrogens.isEmpty() ? fallbackUpstreamHydrogenSelector : upstreamHydrogens)
       || localAcceptorSelector != (upstreamAcceptors.isEmpty() ? fallbackUpstreamAcceptorSelector : upstreamAcceptors)) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The donor/hydrogen/acceptor selectors configured for 'Hydrogen bond kinetics' do not match the upstream PMF-derived 'Hydrogen bond analysis'. "
            "Use the same selectors in both modifiers."));
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
    pmf.distanceMinimum = distanceMinimumVariant.toDouble();
    pmf.distanceMaximum = distanceMaximumVariant.toDouble();
    pmf.thetaMinimum = thetaMinimumVariant.toDouble();
    pmf.thetaMaximum = thetaMaximumVariant.toDouble();
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

SiteEnergyDefinition loadUpstreamSiteEnergyDefinition(const PipelineFlowState& state,
                                                      const QString& donorTypes,
                                                      const QString& donorExpression,
                                                      const QString& hydrogenTypes,
                                                      const QString& hydrogenExpression,
                                                      const QString& acceptorTypes,
                                                      const QString& acceptorExpression,
                                                      FloatType donorHydrogenCutoff)
{
    const QVariant distanceMaximumVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_distance_maximum"));
    const QVariant thetaMaximumVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_theta_maximum"));
    const QVariant cutoffVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_cutoff"));
    const QVariant unitVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_unit"));
    const QVariant modelVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_model"));
    const QVariant relativePermittivityVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_relative_permittivity"));
    const QVariant donorEpsilonVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_donor_lj_epsilon"));
    const QVariant donorSigmaVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_donor_lj_sigma"));
    const QVariant hydrogenEpsilonVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_hydrogen_lj_epsilon"));
    const QVariant hydrogenSigmaVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_hydrogen_lj_sigma"));
    const QVariant acceptorEpsilonVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_acceptor_lj_epsilon"));
    const QVariant acceptorSigmaVariant =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_acceptor_lj_sigma"));

    if(!distanceMaximumVariant.isValid() || !thetaMaximumVariant.isValid()
       || !cutoffVariant.isValid() || !unitVariant.isValid() || !modelVariant.isValid()
       || !relativePermittivityVariant.isValid()
       || !donorEpsilonVariant.isValid() || !donorSigmaVariant.isValid()
       || !hydrogenEpsilonVariant.isValid() || !hydrogenSigmaVariant.isValid()
       || !acceptorEpsilonVariant.isValid() || !acceptorSigmaVariant.isValid()) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "D/H/A site-energy hydrogen-bond kinetics requires an upstream 'Hydrogen bond analysis' "
            "modifier in D/H/A site interaction-energy mode. Run that analysis first so its effective "
            "energy cutoff and interaction parameters are available in the pipeline state."));
    }

    const QString upstreamDonors = state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_selector")).toString();
    const QString upstreamHydrogens = state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_selector")).toString();
    const QString upstreamAcceptors = state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_selector")).toString();
    const QString localDonorSelector = canonicalizeParticleSelector(donorTypes, donorExpression);
    const QString localHydrogenSelector = canonicalizeParticleSelector(hydrogenTypes, hydrogenExpression);
    const QString localAcceptorSelector = canonicalizeParticleSelector(acceptorTypes, acceptorExpression);
    const QString fallbackUpstreamDonorSelector = canonicalizeParticleSelector(
        state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_types")).toString(),
        state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_expression")).toString());
    const QString fallbackUpstreamHydrogenSelector = canonicalizeParticleSelector(
        state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_types")).toString(),
        state.getAttributeValue(QStringLiteral("HydrogenBonds.hydrogen_expression")).toString());
    const QString fallbackUpstreamAcceptorSelector = canonicalizeParticleSelector(
        state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_types")).toString(),
        state.getAttributeValue(QStringLiteral("HydrogenBonds.acceptor_expression")).toString());
    if(localDonorSelector != (upstreamDonors.isEmpty() ? fallbackUpstreamDonorSelector : upstreamDonors)
       || localHydrogenSelector != (upstreamHydrogens.isEmpty() ? fallbackUpstreamHydrogenSelector : upstreamHydrogens)
       || localAcceptorSelector != (upstreamAcceptors.isEmpty() ? fallbackUpstreamAcceptorSelector : upstreamAcceptors)) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The donor/hydrogen/acceptor selectors configured for 'Hydrogen bond kinetics' do not match "
            "the upstream site-energy 'Hydrogen bond analysis'. Use the same selectors in both modifiers."));
    }

    const QVariant upstreamDonorHydrogenCutoff =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"));
    if(upstreamDonorHydrogenCutoff.isValid()
       && std::abs(upstreamDonorHydrogenCutoff.toDouble() - static_cast<double>(donorHydrogenCutoff)) > 1e-6) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The donor-hydrogen cutoff in 'Hydrogen bond kinetics' does not match the upstream "
            "site-energy 'Hydrogen bond analysis'. Use the same donor-hydrogen cutoff in both modifiers."));
    }

    SiteEnergyDefinition definition;
    definition.distanceMaximum = distanceMaximumVariant.toDouble();
    definition.thetaMaximum = thetaMaximumVariant.toDouble();
    definition.cutoff = cutoffVariant.toDouble();
    definition.cutoffMode =
        state.getAttributeValue(QStringLiteral("HydrogenBonds.site_energy_cutoff_mode")).toString();
    definition.unitLabel = unitVariant.toString().trimmed();
    definition.potential.relativePermittivity = relativePermittivityVariant.toDouble();
    definition.potential.donorEpsilon = donorEpsilonVariant.toDouble();
    definition.potential.donorSigma = donorSigmaVariant.toDouble();
    definition.potential.hydrogenEpsilon = hydrogenEpsilonVariant.toDouble();
    definition.potential.hydrogenSigma = hydrogenSigmaVariant.toDouble();
    definition.potential.acceptorEpsilon = acceptorEpsilonVariant.toDouble();
    definition.potential.acceptorSigma = acceptorSigmaVariant.toDouble();

    if(definition.unitLabel.compare(QStringLiteral("kcal/mol"), Qt::CaseInsensitive) == 0)
        definition.potential.coulombConstant = 332.063713299;
    else if(definition.unitLabel.compare(QStringLiteral("eV"), Qt::CaseInsensitive) == 0)
        definition.potential.coulombConstant = 14.3996454784255;
    else
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream site-energy definition uses an unsupported energy unit: %1.")
                            .arg(definition.unitLabel));

    if(!(definition.distanceMaximum > 0.0)
       || definition.thetaMaximum < 0.0 || definition.thetaMaximum > 180.0
       || !std::isfinite(definition.cutoff)
       || !(definition.potential.relativePermittivity > 0.0)
       || definition.potential.donorEpsilon < 0.0 || definition.potential.donorSigma < 0.0
       || definition.potential.hydrogenEpsilon < 0.0 || definition.potential.hydrogenSigma < 0.0
       || definition.potential.acceptorEpsilon < 0.0 || definition.potential.acceptorSigma < 0.0) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "The upstream site-energy hydrogen-bond definition contains invalid cutoff or interaction parameters."));
    }

    return definition;
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

DataTable* createLifetimeDistributionTable(DataCollection* collection,
                                           const LifetimeEventStatistics& statistics,
                                           int requestedBinCount,
                                           const QString& timeUnit,
                                           const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(!collection || statistics.durations.empty())
        return nullptr;

    const int binCount = std::max(1, requestedBinCount);
    const double maximumDuration = *std::max_element(statistics.durations.begin(), statistics.durations.end());
    const double intervalEnd = maximumDuration > 0.0 ? maximumDuration * (1.0 + 1e-9) : 1.0;
    const double binWidth = intervalEnd / static_cast<double>(binCount);
    std::vector<size_t> counts(static_cast<size_t>(binCount), 0);
    for(double duration : statistics.durations) {
        const int bin = std::clamp(static_cast<int>(duration / binWidth), 0, binCount - 1);
        counts[static_cast<size_t>(bin)]++;
    }

    DataTable* table = collection->createObject<DataTable>(
        HydrogenBondKineticsModifier::lifetimeDistributionTableId(),
        createdByNode,
        DataTable::Histogram,
        HydrogenBondKineticsModifier::tr("Complete hydrogen-bond event lifetime distribution"));
    table->setElementCount(static_cast<size_t>(binCount));
    table->setIntervalStart(0);
    table->setIntervalEnd(static_cast<FloatType>(intervalEnd));
    table->setAxisLabelX(HydrogenBondKineticsModifier::tr("First-break lifetime (%1)").arg(timeUnit));
    table->setAxisLabelY(HydrogenBondKineticsModifier::tr("Probability density"));

    Property* densityProperty = table->createProperty(
        DataBuffer::Initialized,
        QStringLiteral("Probability density"),
        Property::FloatDefault);
    BufferWriteAccess<FloatType, access_mode::discard_write> density(densityProperty);
    const double normalization = static_cast<double>(statistics.durations.size()) * binWidth;
    for(int bin = 0; bin < binCount; ++bin)
        density[static_cast<size_t>(bin)] = static_cast<FloatType>(static_cast<double>(counts[static_cast<size_t>(bin)]) / normalization);
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
                                       const SiteEnergyDefinition* siteEnergyDefinition)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<IdentifierIntType> identifiers = particles->getProperty(Particles::IdentifierProperty);
    BufferReadAccess<FloatType> charges = particles->getProperty(Particles::ChargeProperty);
    if(siteEnergyDefinition && !charges) {
        throw Exception(HydrogenBondKineticsModifier::tr(
            "D/H/A site-energy hydrogen-bond kinetics requires the particle property 'Charge' "
            "in every sampled trajectory frame."));
    }
    const SimulationCell* simCellObject = state.getObject<SimulationCell>();
    const SimulationCellData cellData = simCellObject
        ? SimulationCellData(*simCellObject)
        : SimulationCellData(positions, false, std::max(donorHydrogenCutoff, donorAcceptorSearchCutoff) / 2);
    const SimulationCellData* cellDataPtr = &cellData;

    FrameHydrogenBondSnapshot result;
    result.frame = sourceFrame;
    bool timestepOk = false;
    const QVariant timestepValue = state.getAttributeValue(QStringLiteral("Timestep"));
    result.trajectoryCoordinate = timestepValue.isValid() ? timestepValue.toDouble(&timestepOk) : 0.0;
    result.usedTimestepAttribute = timestepOk && std::isfinite(result.trajectoryCoordinate);
    if(!result.usedTimestepAttribute)
        result.trajectoryCoordinate = static_cast<double>(sourceFrame);
    result.usedParticleIndices = !identifiers;

    result.donorCount = 0;
    std::vector<uint8_t> donorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        donorTypes, donorExpression,
        HydrogenBondKineticsModifier::tr("donor atom selector"),
        HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics"),
        &result.donorCount);
    result.hydrogenCount = 0;
    std::vector<uint8_t> hydrogenMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        hydrogenTypes, hydrogenExpression,
        HydrogenBondKineticsModifier::tr("hydrogen atom selector"),
        HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics"),
        &result.hydrogenCount);
    result.acceptorCount = 0;
    std::vector<uint8_t> acceptorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        acceptorTypes, acceptorExpression,
        HydrogenBondKineticsModifier::tr("acceptor atom selector"),
        HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics"),
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

            const Vector3 hydrogenToAcceptorDelta =
                donorToAcceptorDelta - donorHydrogen.donorToHydrogenDelta;
            const FloatType haLength = hydrogenToAcceptorDelta.length();
            if(haLength <= FloatType(0))
                continue;

            const FloatType theta = qRadiansToDegrees(clampedAcos(donorHydrogen.donorToHydrogenDelta.dot(donorToAcceptorDelta) / (dhLength * daLength)));
            double siteEnergy = std::numeric_limits<double>::quiet_NaN();
            if(siteEnergyDefinition) {
                const HydrogenBondSiteEnergy::Components components =
                    HydrogenBondSiteEnergy::evaluate(
                        static_cast<double>(daLength),
                        static_cast<double>(haLength),
                        static_cast<double>(charges[donorHydrogen.donorIndex]),
                        static_cast<double>(charges[donorHydrogen.hydrogenIndex]),
                        static_cast<double>(charges[acceptorIndex]),
                        siteEnergyDefinition->potential);
                siteEnergy = components.total;
            }
            result.candidates.push_back({
                {particleId(donorHydrogen.donorIndex), particleId(donorHydrogen.hydrogenIndex), particleId(acceptorIndex)},
                {particleId(donorHydrogen.donorIndex), particleId(acceptorIndex)},
                static_cast<double>(daLength),
                static_cast<double>(theta),
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

int clampedBinIndex(double value, double lowerBound, double upperBound, int binCount)
{
    if(!(value >= lowerBound))
        return -1;
    if(value > upperBound || !(upperBound > lowerBound))
        return -1;
    const double normalized = std::clamp((value - lowerBound) / (upperBound - lowerBound),
                                         0.0,
                                         1.0 - std::numeric_limits<double>::epsilon());
    int binIndex = static_cast<int>(std::floor(normalized * static_cast<double>(binCount)));
    if(binIndex >= binCount)
        binIndex = binCount - 1;
    return binIndex;
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

FrameState buildFrameState(const FrameHydrogenBondSnapshot& snapshot,
                           HydrogenBondKineticsModifier::DefinitionMode definitionMode,
                           double fixedHydrogenBondDistanceCutoff,
                           double fixedMaximumTheta,
                           double fixedVicinityCutoff,
                           const PmfDefinition* pmf,
                           const SiteEnergyDefinition* siteEnergy)
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
        else if(definitionMode == HydrogenBondKineticsModifier::SiteInteractionEnergy) {
            active = siteEnergy
                && sample.distance <= siteEnergy->distanceMaximum
                && sample.theta <= siteEnergy->thetaMaximum
                && std::isfinite(sample.siteEnergy)
                && sample.siteEnergy <= siteEnergy->cutoff;
        }
        else {
            active = (sample.distance <= fixedHydrogenBondDistanceCutoff && sample.theta <= fixedMaximumTheta);
        }

        if(active)
            state.activeTriplets.insert(sample.triplet);
    }

    return state;
}

double trapezoidalIntegral(const std::vector<double>& x, const std::vector<double>& y)
{
    if(x.size() != y.size() || x.size() < 2)
        return std::numeric_limits<double>::quiet_NaN();

    double integral = 0.0;
    bool usedInterval = false;
    for(size_t i = 1; i < x.size(); ++i) {
        const double dx = x[i] - x[i - 1];
        if(!(dx > 0.0) || !std::isfinite(y[i - 1]) || !std::isfinite(y[i]))
            continue;
        integral += 0.5 * dx * (y[i - 1] + y[i]);
        usedInterval = true;
    }
    return usedInterval ? integral : std::numeric_limits<double>::quiet_NaN();
}

std::vector<double> computeReactiveFlux(const std::vector<double>& time,
                                        const std::vector<double>& correlation,
                                        int requestedWindow)
{
    std::vector<double> flux(time.size(), std::numeric_limits<double>::quiet_NaN());
    if(time.size() != correlation.size() || time.size() < 2)
        return flux;

    if(requestedWindow <= 1) {
        for(size_t i = 0; i < time.size(); ++i) {
            const size_t left = i == 0 ? 0 : i - 1;
            const size_t right = i + 1 < time.size() ? i + 1 : i;
            const double dt = time[right] - time[left];
            if(dt > 0.0 && std::isfinite(correlation[left]) && std::isfinite(correlation[right]))
                flux[i] = -(correlation[right] - correlation[left]) / dt;
        }
        return flux;
    }

    int window = std::max(3, requestedWindow);
    if((window % 2) == 0)
        ++window;
    const size_t halfWindow = static_cast<size_t>(window / 2);

    for(size_t i = 0; i < time.size(); ++i) {
        const size_t begin = i > halfWindow ? i - halfWindow : 0;
        const size_t end = std::min(time.size(), i + halfWindow + 1);

        double sumT = 0.0;
        double sumC = 0.0;
        size_t count = 0;
        for(size_t j = begin; j < end; ++j) {
            if(std::isfinite(time[j]) && std::isfinite(correlation[j])) {
                sumT += time[j];
                sumC += correlation[j];
                ++count;
            }
        }
        if(count < 2)
            continue;

        const double meanT = sumT / static_cast<double>(count);
        const double meanC = sumC / static_cast<double>(count);
        double covariance = 0.0;
        double variance = 0.0;
        for(size_t j = begin; j < end; ++j) {
            if(!std::isfinite(time[j]) || !std::isfinite(correlation[j]))
                continue;
            const double dt = time[j] - meanT;
            covariance += dt * (correlation[j] - meanC);
            variance += dt * dt;
        }
        if(variance > 0.0)
            flux[i] = -covariance / variance;
    }

    return flux;
}

LuzarChandlerFitResult fitLuzarChandlerRates(const std::vector<double>& time,
                                             const std::vector<double>& correlation,
                                             const std::vector<double>& nearbyUnbonded,
                                             int requestedStartLag,
                                             int requestedEndLag)
{
    LuzarChandlerFitResult result;
    if(time.size() != correlation.size() || time.size() != nearbyUnbonded.size() || time.size() < 4) {
        result.status = QStringLiteral("insufficient kinetics samples");
        return result;
    }

    std::vector<double> integralC(time.size(), 0.0);
    std::vector<double> integralN(time.size(), 0.0);
    for(size_t i = 1; i < time.size(); ++i) {
        const double dt = time[i] - time[i - 1];
        if(!(dt > 0.0)
           || !std::isfinite(correlation[i - 1]) || !std::isfinite(correlation[i])
           || !std::isfinite(nearbyUnbonded[i - 1]) || !std::isfinite(nearbyUnbonded[i])) {
            integralC[i] = std::numeric_limits<double>::quiet_NaN();
            integralN[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        if(!std::isfinite(integralC[i - 1]) || !std::isfinite(integralN[i - 1])) {
            integralC[i] = std::numeric_limits<double>::quiet_NaN();
            integralN[i] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        integralC[i] = integralC[i - 1] + 0.5 * dt * (correlation[i - 1] + correlation[i]);
        integralN[i] = integralN[i - 1] + 0.5 * dt * (nearbyUnbonded[i - 1] + nearbyUnbonded[i]);
    }

    const size_t start = std::clamp<size_t>(static_cast<size_t>(std::max(1, requestedStartLag)), 1, time.size() - 1);
    const size_t end = requestedEndLag > 0
        ? std::min<size_t>(static_cast<size_t>(requestedEndLag), time.size() - 1)
        : time.size() - 1;
    if(end < start) {
        result.status = QStringLiteral("fit end precedes fit start");
        return result;
    }

    struct Row {
        double xBreaking;
        double xReformation;
        double y;
    };
    std::vector<Row> rows;
    rows.reserve(end - start + 1);
    for(size_t i = start; i <= end; ++i) {
        if(std::isfinite(integralC[i]) && std::isfinite(integralN[i]) && std::isfinite(correlation[i])) {
            rows.push_back({integralC[i], -integralN[i], correlation.front() - correlation[i]});
        }
    }
    if(rows.size() < 3) {
        result.status = QStringLiteral("fewer than three finite fit points");
        return result;
    }

    double a11 = 0.0;
    double a12 = 0.0;
    double a22 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    for(const Row& row : rows) {
        a11 += row.xBreaking * row.xBreaking;
        a12 += row.xBreaking * row.xReformation;
        a22 += row.xReformation * row.xReformation;
        b1 += row.xBreaking * row.y;
        b2 += row.xReformation * row.y;
    }

    struct Candidate {
        double breakingRate = 0.0;
        double reformationRate = 0.0;
        double sse = std::numeric_limits<double>::infinity();
    };
    auto evaluate = [&rows](double breakingRate, double reformationRate) {
        Candidate candidate;
        candidate.breakingRate = breakingRate;
        candidate.reformationRate = reformationRate;
        candidate.sse = 0.0;
        for(const Row& row : rows) {
            const double residual = row.y - breakingRate * row.xBreaking - reformationRate * row.xReformation;
            candidate.sse += residual * residual;
        }
        return candidate;
    };

    std::vector<Candidate> candidates;
    candidates.push_back(evaluate(a11 > 0.0 ? std::max(0.0, b1 / a11) : 0.0, 0.0));
    candidates.push_back(evaluate(0.0, a22 > 0.0 ? std::max(0.0, b2 / a22) : 0.0));
    candidates.push_back(evaluate(0.0, 0.0));

    const double determinant = a11 * a22 - a12 * a12;
    if(determinant > std::numeric_limits<double>::epsilon() * std::max(1.0, a11 * a22)) {
        const double breakingRate = (b1 * a22 - b2 * a12) / determinant;
        const double reformationRate = (a11 * b2 - a12 * b1) / determinant;
        if(breakingRate >= 0.0 && reformationRate >= 0.0)
            candidates.push_back(evaluate(breakingRate, reformationRate));
    }

    const Candidate& best = *std::min_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.sse < b.sse; });
    if(!(best.breakingRate > 0.0) || !std::isfinite(best.sse)) {
        result.status = QStringLiteral("could not determine a positive breaking rate");
        return result;
    }

    const double meanY = std::accumulate(rows.begin(), rows.end(), 0.0,
        [](double sum, const Row& row) { return sum + row.y; }) / static_cast<double>(rows.size());
    double tss = 0.0;
    for(const Row& row : rows) {
        const double delta = row.y - meanY;
        tss += delta * delta;
    }

    result.valid = true;
    result.status = QStringLiteral("ok");
    result.breakingRate = best.breakingRate;
    result.reformationRate = best.reformationRate;
    result.breakingLifetime = 1.0 / best.breakingRate;
    result.reformationTime = best.reformationRate > 0.0
        ? 1.0 / best.reformationRate
        : std::numeric_limits<double>::quiet_NaN();
    result.rSquared = tss > 0.0 ? 1.0 - best.sse / tss : std::numeric_limits<double>::quiet_NaN();
    result.rmse = std::sqrt(best.sse / static_cast<double>(rows.size()));
    result.fitStart = time[start];
    result.fitEnd = time[end];
    result.pointCount = static_cast<int>(rows.size());
    return result;
}

HydrogenBondKineticsCurves computeKineticsCurves(const std::vector<FrameState>& states,
                                                 const std::vector<int>& frames,
                                                 const std::vector<double>& sampleTimes,
                                                 int requestedMaxLag,
                                                 int requestedBlockLength,
                                                 int reactiveFluxSmoothingWindow,
                                                 TaskProgress& progress)
{
    HydrogenBondKineticsCurves curves;
    if(states.empty() || states.size() != frames.size() || states.size() != sampleTimes.size())
        return curves;

    const size_t frameCount = states.size();
    const size_t maxLagEffective = std::min<size_t>(
        requestedMaxLag > 0 ? static_cast<size_t>(requestedMaxLag) : frameCount - 1,
        frameCount - 1);
    curves.blockLength = requestedBlockLength > 0
        ? std::min<size_t>(static_cast<size_t>(requestedBlockLength), frameCount)
        : std::max<size_t>(1, static_cast<size_t>(std::llround(std::sqrt(static_cast<double>(frameCount)))));
    curves.blockCount = (frameCount + curves.blockLength - 1) / curves.blockLength;

    const size_t lagCount = maxLagEffective + 1;
    curves.lagSourceFrames.assign(lagCount, 0.0);
    curves.lagTimes.assign(lagCount, 0.0);
    curves.continuous.assign(lagCount, std::numeric_limits<double>::quiet_NaN());
    curves.c.assign(lagCount, std::numeric_limits<double>::quiet_NaN());
    curves.n.assign(lagCount, std::numeric_limits<double>::quiet_NaN());
    curves.cPlusN.assign(lagCount, std::numeric_limits<double>::quiet_NaN());
    curves.sampleCounts.assign(lagCount, 0.0);
    curves.blockCounts.resize(curves.blockCount * lagCount);

    progress.setText(HydrogenBondKineticsModifier::tr("Computing continuous and intermittent hydrogen-bond kinetics"));
    parallelFor(curves.blockCount, 1, progress, [&states, &curves, lagCount, frameCount](size_t blockIndex) {
        this_task::throwIfCanceled();
        const size_t firstOrigin = blockIndex * curves.blockLength;
        const size_t endOrigin = std::min(frameCount, firstOrigin + curves.blockLength);

        for(size_t origin = firstOrigin; origin < endOrigin; ++origin) {
            const FrameState& initialState = states[origin];
            const size_t availableLag = std::min(lagCount - 1, frameCount - origin - 1);
            for(const TripletKey& triplet : initialState.activeTriplets) {
                const PairKey pair{triplet.donorId, triplet.acceptorId};
                bool survivedContinuously = true;
                for(size_t lag = 0; lag <= availableLag; ++lag) {
                    HydrogenBondKineticsCurves::Counts& counts = curves.blockCounts[blockIndex * lagCount + lag];
                    counts.samples += 1.0;

                    const FrameState& laggedState = states[origin + lag];
                    const bool active =
                        laggedState.activeTriplets.find(triplet) != laggedState.activeTriplets.end();
                    if(active)
                        counts.intermittent += 1.0;
                    else if(laggedState.vicinityPairs.find(pair) != laggedState.vicinityPairs.end())
                        counts.nearbyUnbonded += 1.0;

                    survivedContinuously = survivedContinuously && active;
                    if(survivedContinuously)
                        counts.continuous += 1.0;
                }
            }
        }
    });

    for(size_t lag = 0; lag < lagCount; ++lag) {
        HydrogenBondKineticsCurves::Counts total;
        for(size_t block = 0; block < curves.blockCount; ++block) {
            const HydrogenBondKineticsCurves::Counts& blockValue = curves.blockCounts[block * lagCount + lag];
            total.samples += blockValue.samples;
            total.continuous += blockValue.continuous;
            total.intermittent += blockValue.intermittent;
            total.nearbyUnbonded += blockValue.nearbyUnbonded;
        }

        const size_t originCount = frameCount - lag;
        double frameLagSum = 0.0;
        double timeLagSum = 0.0;
        for(size_t origin = 0; origin < originCount; ++origin) {
            frameLagSum += static_cast<double>(frames[origin + lag] - frames[origin]);
            timeLagSum += sampleTimes[origin + lag] - sampleTimes[origin];
        }
        curves.lagSourceFrames[lag] = frameLagSum / static_cast<double>(originCount);
        curves.lagTimes[lag] = timeLagSum / static_cast<double>(originCount);
        curves.sampleCounts[lag] = total.samples;
        if(total.samples > 0.0) {
            curves.continuous[lag] = total.continuous / total.samples;
            curves.c[lag] = total.intermittent / total.samples;
            curves.n[lag] = total.nearbyUnbonded / total.samples;
            curves.cPlusN[lag] = (total.intermittent + total.nearbyUnbonded) / total.samples;
        }
    }

    curves.reactiveFlux = computeReactiveFlux(curves.lagTimes, curves.c, reactiveFluxSmoothingWindow);
    return curves;
}

LifetimeEventStatistics computeCompleteLifetimeEvents(const std::vector<FrameState>& states,
                                                      const std::vector<double>& sampleTimes)
{
    LifetimeEventStatistics result;
    if(states.size() != sampleTimes.size() || states.size() < 2)
        return result;

    struct ActiveRun {
        size_t startFrame = 0;
        bool leftCensored = false;
    };
    std::unordered_map<TripletKey, ActiveRun, TripletKeyHash> activeRuns;

    for(size_t frame = 0; frame < states.size(); ++frame) {
        for(auto run = activeRuns.begin(); run != activeRuns.end();) {
            if(states[frame].activeTriplets.find(run->first) == states[frame].activeTriplets.end()) {
                if(!run->second.leftCensored) {
                    const double duration = sampleTimes[frame] - sampleTimes[run->second.startFrame];
                    if(duration > 0.0 && std::isfinite(duration))
                        result.durations.push_back(duration);
                }
                run = activeRuns.erase(run);
            }
            else {
                ++run;
            }
        }

        for(const TripletKey& triplet : states[frame].activeTriplets) {
            if(activeRuns.find(triplet) == activeRuns.end()) {
                activeRuns.emplace(triplet, ActiveRun{frame, frame == 0});
                if(frame == 0)
                    result.leftCensoredCount++;
            }
        }
    }
    result.rightCensoredCount = activeRuns.size();

    if(result.durations.empty())
        return result;

    result.mean = std::accumulate(result.durations.begin(), result.durations.end(), 0.0)
                / static_cast<double>(result.durations.size());
    std::vector<double> sorted = result.durations;
    std::sort(sorted.begin(), sorted.end());
    const size_t middle = sorted.size() / 2;
    result.median = (sorted.size() % 2) != 0
        ? sorted[middle]
        : 0.5 * (sorted[middle - 1] + sorted[middle]);
    return result;
}

ConfidenceInterval percentileInterval(std::vector<double> values)
{
    ConfidenceInterval interval;
    values.erase(std::remove_if(values.begin(), values.end(),
                                [](double value) { return !std::isfinite(value); }),
                 values.end());
    if(values.size() < 2)
        return interval;

    std::sort(values.begin(), values.end());
    auto percentile = [&values](double probability) {
        const double position = probability * static_cast<double>(values.size() - 1);
        const size_t lower = static_cast<size_t>(std::floor(position));
        const size_t upper = static_cast<size_t>(std::ceil(position));
        const double fraction = position - static_cast<double>(lower);
        return values[lower] * (1.0 - fraction) + values[upper] * fraction;
    };
    interval.lower = percentile(0.025);
    interval.upper = percentile(0.975);
    return interval;
}

BootstrapResult bootstrapKinetics(const HydrogenBondKineticsCurves& curves,
                                  int requestedReplicates,
                                  int randomSeed,
                                  int fitStartLag,
                                  int fitEndLag,
                                  TaskProgress& progress)
{
    BootstrapResult result;
    result.requestedReplicates = std::max(0, requestedReplicates);
    result.effectiveBlockLength = static_cast<int>(curves.blockLength);
    if(result.requestedReplicates == 0 || curves.blockCount < 2 || curves.lagTimes.empty())
        return result;

    std::mt19937 generator(static_cast<std::mt19937::result_type>(randomSeed));
    std::uniform_int_distribution<size_t> blockDistribution(0, curves.blockCount - 1);
    const size_t lagCount = curves.lagTimes.size();

    std::vector<double> continuousLifetimes;
    std::vector<double> intermittentTimes;
    std::vector<double> breakingRates;
    std::vector<double> reformationRates;
    std::vector<double> breakingLifetimes;
    std::vector<double> reformationTimes;
    continuousLifetimes.reserve(result.requestedReplicates);
    intermittentTimes.reserve(result.requestedReplicates);
    breakingRates.reserve(result.requestedReplicates);
    reformationRates.reserve(result.requestedReplicates);
    breakingLifetimes.reserve(result.requestedReplicates);
    reformationTimes.reserve(result.requestedReplicates);

    progress.setText(HydrogenBondKineticsModifier::tr("Estimating block-bootstrap confidence intervals"));
    progress.setMaximum(result.requestedReplicates);
    for(int replicate = 0; replicate < result.requestedReplicates; ++replicate) {
        this_task::throwIfCanceled();
        std::vector<int> blockWeights(curves.blockCount, 0);
        for(size_t draw = 0; draw < curves.blockCount; ++draw)
            blockWeights[blockDistribution(generator)]++;

        std::vector<double> continuous(lagCount, std::numeric_limits<double>::quiet_NaN());
        std::vector<double> correlation(lagCount, std::numeric_limits<double>::quiet_NaN());
        std::vector<double> nearbyUnbonded(lagCount, std::numeric_limits<double>::quiet_NaN());
        for(size_t lag = 0; lag < lagCount; ++lag) {
            HydrogenBondKineticsCurves::Counts total;
            for(size_t block = 0; block < curves.blockCount; ++block) {
                const int weight = blockWeights[block];
                if(weight == 0)
                    continue;
                const HydrogenBondKineticsCurves::Counts& blockValue =
                    curves.blockCounts[block * lagCount + lag];
                total.samples += static_cast<double>(weight) * blockValue.samples;
                total.continuous += static_cast<double>(weight) * blockValue.continuous;
                total.intermittent += static_cast<double>(weight) * blockValue.intermittent;
                total.nearbyUnbonded += static_cast<double>(weight) * blockValue.nearbyUnbonded;
            }
            if(total.samples > 0.0) {
                continuous[lag] = total.continuous / total.samples;
                correlation[lag] = total.intermittent / total.samples;
                nearbyUnbonded[lag] = total.nearbyUnbonded / total.samples;
            }
        }

        const double continuousLifetime = trapezoidalIntegral(curves.lagTimes, continuous);
        const double intermittentTime = trapezoidalIntegral(curves.lagTimes, correlation);
        if(std::isfinite(continuousLifetime) && std::isfinite(intermittentTime)) {
            continuousLifetimes.push_back(continuousLifetime);
            intermittentTimes.push_back(intermittentTime);
            result.successfulCurveReplicates++;
        }

        const LuzarChandlerFitResult fit =
            fitLuzarChandlerRates(curves.lagTimes, correlation, nearbyUnbonded, fitStartLag, fitEndLag);
        if(fit.valid) {
            breakingRates.push_back(fit.breakingRate);
            reformationRates.push_back(fit.reformationRate);
            breakingLifetimes.push_back(fit.breakingLifetime);
            reformationTimes.push_back(fit.reformationTime);
            result.successfulRateReplicates++;
        }
        progress.setValue(replicate + 1);
    }

    result.continuousLifetime = percentileInterval(std::move(continuousLifetimes));
    result.intermittentCorrelationTime = percentileInterval(std::move(intermittentTimes));
    result.breakingRate = percentileInterval(std::move(breakingRates));
    result.reformationRate = percentileInterval(std::move(reformationRates));
    result.breakingLifetime = percentileInterval(std::move(breakingLifetimes));
    result.reformationTime = percentileInterval(std::move(reformationTimes));
    return result;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondKineticsModifier);
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "DisplayName", "Hydrogen bond kinetics");
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "Description",
                "Compute continuous survival, intermittent relaxation, complete-event lifetimes, and Luzar-Chandler rates from exact donor-hydrogen-acceptor triplets.");
OVITO_CLASSINFO(HydrogenBondKineticsModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, donorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, donorExpression);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, hydrogenTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, hydrogenExpression);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, acceptorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, acceptorExpression);
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
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, timeCoordinateMode);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, timeStep);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, timeUnit);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, kineticFitStartLag);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, kineticFitEndLag);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, reactiveFluxSmoothingWindow);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, bootstrapReplicates);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, bootstrapBlockLength);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, bootstrapSeed);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, lifetimeHistogramBins);
DEFINE_PROPERTY_FIELD(HydrogenBondKineticsModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, donorTypes, "Donor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, donorExpression, "Donor expression");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, hydrogenTypes, "Hydrogen atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, hydrogenExpression, "Hydrogen expression");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, acceptorTypes, "Acceptor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, acceptorExpression, "Acceptor expression");
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
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, timeCoordinateMode, "Time coordinate");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, timeStep, "Time per coordinate unit");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, timeUnit, "Time unit");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, kineticFitStartLag, "Kinetic fit start lag");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, kineticFitEndLag, "Kinetic fit end lag (0 = maximum)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, reactiveFluxSmoothingWindow, "Reactive-flux smoothing window");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, bootstrapReplicates, "Bootstrap replicates (0 = off)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, bootstrapBlockLength, "Bootstrap block length (0 = automatic)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, bootstrapSeed, "Bootstrap random seed");
SET_PROPERTY_FIELD_LABEL(HydrogenBondKineticsModifier, lifetimeHistogramBins, "Lifetime histogram bins");
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
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondKineticsModifier, timeStep, FloatParameterUnit, 1e-12);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, kineticFitStartLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, kineticFitEndLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, reactiveFluxSmoothingWindow, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, bootstrapReplicates, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, bootstrapBlockLength, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, bootstrapSeed, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondKineticsModifier, lifetimeHistogramBins, IntegerParameterUnit, 1, std::numeric_limits<int>::max());

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
    const QString donors = donorExpression().trimmed().isEmpty() ? donorTypes().trimmed() : donorExpression().trimmed();
    const QString hydrogens = hydrogenExpression().trimmed().isEmpty() ? hydrogenTypes().trimmed() : hydrogenExpression().trimmed();
    const QString acceptors = acceptorExpression().trimmed().isEmpty() ? acceptorTypes().trimmed() : acceptorExpression().trimmed();
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
            state.setStatus(PipelineStatus());
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus());
        return std::move(state);
    }

    return computeHydrogenBondKinetics(request, std::move(state));
}

Future<PipelineFlowState> HydrogenBondKineticsModifier::computeHydrogenBondKinetics(const ModifierEvaluationRequest& request,
                                                                                     PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    size_t initialMatchCount = 0;
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             donorTypes(), donorExpression(),
                             tr("donor atom selector"),
                             tr("Hydrogen-bond kinetics"),
                             &initialMatchCount);
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             hydrogenTypes(), hydrogenExpression(),
                             tr("hydrogen atom selector"),
                             tr("Hydrogen-bond kinetics"),
                             &initialMatchCount);
    evaluateParticleSelector(state, particles, particleTypeProperty, particleTypes,
                             acceptorTypes(), acceptorExpression(),
                             tr("acceptor atom selector"),
                             tr("Hydrogen-bond kinetics"),
                             &initialMatchCount);
    if(donorHydrogenCutoff() <= 0)
        throw Exception(tr("The donor-hydrogen cutoff must be positive."));
    if(definitionMode() == FixedGeometry) {
        if(donorAcceptorCutoff() <= 0)
            throw Exception(tr("The HB donor-acceptor cutoff must be positive."));
        if(angleCutoff() < 0 || angleCutoff() > 180)
            throw Exception(tr("The HB theta maximum must be in the range [0, 180]."));
    }
    if((definitionMode() == FixedGeometry || definitionMode() == SiteInteractionEnergy)
       && vicinityCutoff() <= 0) {
        throw Exception(tr("The vicinity donor-acceptor cutoff must be positive."));
    }
    if(timeStep() <= 0)
        throw Exception(tr("The time per simulation step must be positive."));
    if(kineticFitEndLag() > 0 && kineticFitEndLag() < kineticFitStartLag())
        throw Exception(tr("The kinetic fit end lag must be zero or greater than or equal to the fit start lag."));
    if(reactiveFluxSmoothingWindow() < 1)
        throw Exception(tr("The reactive-flux smoothing window must be at least one lag point."));
    if(bootstrapReplicates() < 0 || bootstrapBlockLength() < 0)
        throw Exception(tr("Bootstrap replicates and block length cannot be negative."));
    if(lifetimeHistogramBins() < 1)
        throw Exception(tr("The lifetime histogram must contain at least one bin."));
    const bool useBondTopology = particles->bonds() && particles->bonds()->getProperty(Bonds::TopologyProperty);
    const PmfDefinition upstreamPmf = definitionMode() == PMFDerived
        ? loadUpstreamPmfDefinition(state,
                                    donorTypes(), donorExpression(),
                                    hydrogenTypes(), hydrogenExpression(),
                                    acceptorTypes(), acceptorExpression(),
                                    donorHydrogenCutoff())
        : PmfDefinition{};
    const SiteEnergyDefinition upstreamSiteEnergy = definitionMode() == SiteInteractionEnergy
        ? loadUpstreamSiteEnergyDefinition(state,
                                           donorTypes(), donorExpression(),
                                           hydrogenTypes(), hydrogenExpression(),
                                           acceptorTypes(), acceptorExpression(),
                                           donorHydrogenCutoff())
        : SiteEnergyDefinition{};
    const double donorAcceptorSearchCutoff =
        definitionMode() == PMFDerived
            ? upstreamPmf.distanceMaximum
            : definitionMode() == SiteInteractionEnergy
                ? std::max(upstreamSiteEnergy.distanceMaximum, static_cast<double>(vicinityCutoff()))
                : std::max(static_cast<double>(donorAcceptorCutoff()), static_cast<double>(vicinityCutoff()));

    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode())
        ? dynamic_object_cast<HydrogenBondKineticsModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    HydrogenBondAccumulator accumulator;
    accumulator.snapshots.reserve(frames.size());
    auto progress = std::make_shared<TaskProgress>(this_task::ui());
    progress->setText(tr("Collecting hydrogen-bond kinetics samples"));
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
             definitionMode = definitionMode(),
             upstreamSiteEnergy,
             useBondTopology,
             progress,
             totalFrameCount = frames.size()](const std::vector<int>& frameBatch,
                                              std::vector<SharedFuture<PipelineFlowState>> batchFutures,
                                              HydrogenBondAccumulator& accumulator) {
                for(size_t i = 0; i < batchFutures.size(); ++i) {
                    this_task::throwIfCanceled();
                    const SiteEnergyDefinition* siteEnergyPtr =
                        definitionMode == HydrogenBondKineticsModifier::SiteInteractionEnergy
                            ? &upstreamSiteEnergy
                            : nullptr;
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
                                                                     siteEnergyPtr);
                    accumulator.totalDonorAtoms += snapshot.donorCount;
                    accumulator.totalHydrogenAtoms += snapshot.hydrogenCount;
                    accumulator.totalAcceptorAtoms += snapshot.acceptorCount;
                    accumulator.totalDonorHydrogenPairs += snapshot.donorHydrogenPairCount;
                    accumulator.totalCandidateTriplets += snapshot.candidates.size();
                    accumulator.usedParticleIndices = accumulator.usedParticleIndices || snapshot.usedParticleIndices;
                    accumulator.snapshots.push_back(std::move(snapshot));
                    progress->setText(HydrogenBondKineticsModifier::tr("Collecting hydrogen-bond kinetics samples (%1/%2 frames)")
                                          .arg(accumulator.snapshots.size())
                                          .arg(totalFrameCount));
                    progress->setValue(static_cast<qlonglong>(accumulator.snapshots.size()));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this),
              [this, request, state = std::move(state), frames, cacheGenerationId, useBondTopology,
               upstreamPmf, upstreamSiteEnergy, progress = std::move(progress)](HydrogenBondAccumulator accumulator) mutable -> Future<PipelineFlowState> {
        OORef<HydrogenBondKineticsModifier> self(this);
        const int completedRunRequestId = runRequestId();

        return asyncLaunch([self = std::move(self),
                            request = ModifierEvaluationRequest(request),
                            state = std::move(state),
                            frames,
                            accumulator = std::move(accumulator),
                            useBondTopology,
                            upstreamPmf,
                            upstreamSiteEnergy,
                            progress = std::move(progress),
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
                    "No particles matched the selected donor atom selector in the sampled trajectory interval."));
            if(accumulator.totalHydrogenAtoms == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No particles matched the selected hydrogen atom selector in the sampled trajectory interval."));
            if(accumulator.totalAcceptorAtoms == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No particles matched the selected acceptor atom selector in the sampled trajectory interval."));
            if(accumulator.totalDonorHydrogenPairs == 0) {
                throw Exception(useBondTopology
                    ? HydrogenBondKineticsModifier::tr(
                        "No donor-hydrogen pairs were found in the bond topology for the selected donor and hydrogen atom selectors.")
                    : HydrogenBondKineticsModifier::tr(
                        "No donor-hydrogen pairs were found within the donor-hydrogen cutoff. Increase the cutoff or adjust the donor/hydrogen selectors."));
            }
            if(accumulator.totalCandidateTriplets == 0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No donor-hydrogen-acceptor triplets were found within the chosen donor-acceptor search range."));

            const double fixedMaximumTheta = static_cast<double>(self->angleCutoff());
            const PmfDefinition* pmfPtr = self->definitionMode() == PMFDerived ? &upstreamPmf : nullptr;
            const SiteEnergyDefinition* siteEnergyPtr =
                self->definitionMode() == SiteInteractionEnergy ? &upstreamSiteEnergy : nullptr;

            const bool allSnapshotsUseTimestep = std::ranges::all_of(
                accumulator.snapshots,
                [](const FrameHydrogenBondSnapshot& snapshot) { return snapshot.usedTimestepAttribute; });
            const bool anySnapshotUsesTimestep = std::ranges::any_of(
                accumulator.snapshots,
                [](const FrameHydrogenBondSnapshot& snapshot) { return snapshot.usedTimestepAttribute; });
            const bool timestepCoordinatesStrictlyIncrease =
                allSnapshotsUseTimestep
                && std::adjacent_find(
                    accumulator.snapshots.begin(),
                    accumulator.snapshots.end(),
                    [](const FrameHydrogenBondSnapshot& left, const FrameHydrogenBondSnapshot& right) {
                        return !(right.trajectoryCoordinate > left.trajectoryCoordinate);
                    }) == accumulator.snapshots.end();

            bool useTimestepCoordinates = false;
            switch(self->timeCoordinateMode()) {
                case TimestepAttributeTimeCoordinate:
                    if(!allSnapshotsUseTimestep) {
                        throw Exception(HydrogenBondKineticsModifier::tr(
                            "The selected trajectory-timestep time coordinate is unavailable or invalid in one or more sampled frames. "
                            "Select source-frame time or automatic time."));
                    }
                    if(!timestepCoordinatesStrictlyIncrease) {
                        throw Exception(HydrogenBondKineticsModifier::tr(
                            "The trajectory Timestep values repeat or decrease. "
                            "Select source-frame time to analyze a concatenated or restarted trajectory without editing the dump file."));
                    }
                    useTimestepCoordinates = true;
                    break;
                case SourceFrameTimeCoordinate:
                    useTimestepCoordinates = false;
                    break;
                case AutomaticTimeCoordinate:
                default:
                    useTimestepCoordinates = timestepCoordinatesStrictlyIncrease;
                    break;
            }

            std::vector<FrameState> states;
            std::vector<double> sampleTimes;
            states.reserve(accumulator.snapshots.size());
            sampleTimes.reserve(accumulator.snapshots.size());
            progress->setText(HydrogenBondKineticsModifier::tr("Building hydrogen-bond kinetics frame states"));
            progress->setMaximum(static_cast<qlonglong>(accumulator.snapshots.size()));
            for(size_t snapshotIndex = 0; snapshotIndex < accumulator.snapshots.size(); ++snapshotIndex) {
                this_task::throwIfCanceled();
                const FrameHydrogenBondSnapshot& snapshot = accumulator.snapshots[snapshotIndex];
                states.push_back(buildFrameState(snapshot,
                                                 self->definitionMode(),
                                                 static_cast<double>(self->donorAcceptorCutoff()),
                                                 fixedMaximumTheta,
                                                 static_cast<double>(self->vicinityCutoff()),
                                                 pmfPtr,
                                                 siteEnergyPtr));
                const double trajectoryCoordinate = useTimestepCoordinates
                    ? snapshot.trajectoryCoordinate
                    : static_cast<double>(snapshot.frame);
                sampleTimes.push_back(trajectoryCoordinate * static_cast<double>(self->timeStep()));
                progress->setValue(static_cast<qlonglong>(snapshotIndex + 1));
            }

            bool irregularTimeSpacing = false;
            if(sampleTimes.size() >= 2) {
                const double firstSpacing = sampleTimes[1] - sampleTimes[0];
                if(!(firstSpacing > 0.0))
                    throw Exception(HydrogenBondKineticsModifier::tr(
                        "Sampled trajectory times must be strictly increasing. Check the selected time coordinate and its scale."));
                for(size_t i = 2; i < sampleTimes.size(); ++i) {
                    const double spacing = sampleTimes[i] - sampleTimes[i - 1];
                    if(!(spacing > 0.0))
                        throw Exception(HydrogenBondKineticsModifier::tr(
                            "Sampled trajectory times must be strictly increasing. Check the selected time coordinate and its scale."));
                    const double tolerance = 1e-8 * std::max({1.0, std::abs(firstSpacing), std::abs(spacing)});
                    irregularTimeSpacing = irregularTimeSpacing || std::abs(spacing - firstSpacing) > tolerance;
                }
            }

            HydrogenBondKineticsCurves curves = computeKineticsCurves(
                states,
                frames,
                sampleTimes,
                self->maxLag(),
                self->bootstrapBlockLength(),
                self->reactiveFluxSmoothingWindow(),
                *progress);
            if(curves.c.empty())
                throw Exception(HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics could not compute any lag points."));
            if(curves.sampleCounts[0] <= 0.0)
                throw Exception(HydrogenBondKineticsModifier::tr(
                    "No hydrogen bonds satisfied the chosen definition at lag zero, so the kinetics curves cannot be normalized."));

            const LuzarChandlerFitResult rateFit = fitLuzarChandlerRates(
                curves.lagTimes,
                curves.c,
                curves.n,
                self->kineticFitStartLag(),
                self->kineticFitEndLag());
            curves.modeledReactiveFlux.assign(curves.lagTimes.size(), std::numeric_limits<double>::quiet_NaN());
            if(rateFit.valid) {
                for(size_t i = 0; i < curves.lagTimes.size(); ++i) {
                    if(std::isfinite(curves.c[i]) && std::isfinite(curves.n[i])) {
                        curves.modeledReactiveFlux[i] =
                            rateFit.breakingRate * curves.c[i] - rateFit.reformationRate * curves.n[i];
                    }
                }
            }

            const double continuousLifetime = trapezoidalIntegral(curves.lagTimes, curves.continuous);
            const double intermittentCorrelationTime = trapezoidalIntegral(curves.lagTimes, curves.c);
            const bool continuousIntegralTruncated = curves.continuous.back() > 0.01;
            const bool intermittentIntegralTruncated = curves.c.back() > 0.01;
            const LifetimeEventStatistics lifetimeEvents = computeCompleteLifetimeEvents(states, sampleTimes);
            const BootstrapResult bootstrap = bootstrapKinetics(
                curves,
                self->bootstrapReplicates(),
                self->bootstrapSeed(),
                self->kineticFitStartLag(),
                self->kineticFitEndLag(),
                *progress);

            computationResult.results = DataOORef<DataCollection>::create();
            const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
            const QString selectedTimeUnit = timeUnitLabel(self->timeUnit());
            createMultiCurveLineTable(computationResult.results,
                                      HydrogenBondKineticsModifier::kineticsTableId(),
                                      HydrogenBondKineticsModifier::tr("Hydrogen-bond kinetics"),
                                      curves.lagTimes,
                                      {curves.continuous, curves.c, curves.n, curves.cPlusN},
                                      {QStringLiteral("Continuous S(t)"),
                                       QStringLiteral("Intermittent C(t)"),
                                       QStringLiteral("Nearby unbonded n(t)"),
                                       QStringLiteral("C(t)+n(t)")},
                                      HydrogenBondKineticsModifier::tr("Lag (%1)").arg(selectedTimeUnit),
                                      HydrogenBondKineticsModifier::tr("Conditional probability"),
                                      createdByNode);
            createMultiCurveLineTable(computationResult.results,
                                      HydrogenBondKineticsModifier::reactiveFluxTableId(),
                                      HydrogenBondKineticsModifier::tr("Hydrogen-bond reactive flux"),
                                      curves.lagTimes,
                                      {curves.reactiveFlux, curves.modeledReactiveFlux},
                                      {QStringLiteral("-dC(t)/dt"),
                                       QStringLiteral("k_break C(t) - k_form n(t)")},
                                      HydrogenBondKineticsModifier::tr("Lag (%1)").arg(selectedTimeUnit),
                                      HydrogenBondKineticsModifier::tr("Reactive flux (1/%1)").arg(selectedTimeUnit),
                                      createdByNode);
            createLifetimeDistributionTable(computationResult.results,
                                            lifetimeEvents,
                                            self->lifetimeHistogramBins(),
                                            selectedTimeUnit,
                                            createdByNode);

            setStretchedExponentialFitAttributes(
                computationResult.results,
                QStringLiteral("HBKinetics"),
                fitStretchedExponentialDecay(curves.lagTimes, curves.c, QStringLiteral("Intermittent C(t)")),
                createdByNode);
            setStretchedExponentialFitAttributes(
                computationResult.results,
                QStringLiteral("HBKineticsContinuous"),
                fitStretchedExponentialDecay(curves.lagTimes, curves.continuous, QStringLiteral("Continuous S(t)")),
                createdByNode);

            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_types"), self->donorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_expression"), self->donorExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_selector"), canonicalizeParticleSelector(self->donorTypes(), self->donorExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.hydrogen_types"), self->hydrogenTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.hydrogen_expression"), self->hydrogenExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.hydrogen_selector"), canonicalizeParticleSelector(self->hydrogenTypes(), self->hydrogenExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.acceptor_types"), self->acceptorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.acceptor_expression"), self->acceptorExpression(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.acceptor_selector"), canonicalizeParticleSelector(self->acceptorTypes(), self->acceptorExpression()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.definition_mode"), definitionModeLabel(self->definitionMode()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.total_candidate_triplets"), static_cast<double>(accumulator.totalCandidateTriplets), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.donor_hydrogen_pairing_mode"), donorHydrogenPairingModeLabel(useBondTopology), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.time_coordinate_source"),
                                                     useTimestepCoordinates ? QStringLiteral("Timestep attribute") : QStringLiteral("Source frame"),
                                                     createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.time_step"), static_cast<double>(self->timeStep()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.time_unit"), selectedTimeUnit, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.irregular_time_spacing"), irregularTimeSpacing ? 1.0 : 0.0, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_S"), curves.continuous.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_C"), curves.c.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_n"), curves.n.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.zero_lag_C_plus_n"), curves.cPlusN.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_S"), curves.continuous.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_C"), curves.c.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_n"), curves.n.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.final_C_plus_n"), curves.cPlusN.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.maximum_lag"), curves.lagTimes.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.maximum_lag_source_frames"), curves.lagSourceFrames.back(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.initial_triplet_samples"), curves.sampleCounts.front(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.continuous_lifetime_observed"), continuousLifetime, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.intermittent_correlation_time_observed"), intermittentCorrelationTime, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.continuous_lifetime_integral_truncated"),
                                                     continuousIntegralTruncated ? 1.0 : 0.0, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.intermittent_correlation_integral_truncated"),
                                                     intermittentIntegralTruncated ? 1.0 : 0.0, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.reactive_flux_smoothing_window"),
                                                     static_cast<double>(self->reactiveFluxSmoothingWindow()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.complete_event_count"),
                                                     static_cast<double>(lifetimeEvents.durations.size()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.left_censored_event_count"),
                                                     static_cast<double>(lifetimeEvents.leftCensoredCount), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.right_censored_event_count"),
                                                     static_cast<double>(lifetimeEvents.rightCensoredCount), createdByNode);
            if(!lifetimeEvents.durations.empty()) {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.complete_event_mean_lifetime"), lifetimeEvents.mean, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.complete_event_median_lifetime"), lifetimeEvents.median, createdByNode);
            }

            computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_valid"), rateFit.valid ? 1.0 : 0.0, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_status"), rateFit.status, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_point_count"), static_cast<double>(rateFit.pointCount), createdByNode);
            if(rateFit.valid) {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.breaking_rate"), rateFit.breakingRate, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.reformation_rate"), rateFit.reformationRate, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.luzar_chandler_lifetime"), rateFit.breakingLifetime, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.reformation_time"), rateFit.reformationTime, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_r_squared"), rateFit.rSquared, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_rmse"), rateFit.rmse, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_start"), rateFit.fitStart, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.lc_fit_end"), rateFit.fitEnd, createdByNode);
            }

            computationResult.results->setAttribute(QStringLiteral("HBKinetics.bootstrap_requested_replicates"),
                                                     static_cast<double>(bootstrap.requestedReplicates), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.bootstrap_successful_curve_replicates"),
                                                     static_cast<double>(bootstrap.successfulCurveReplicates), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.bootstrap_successful_rate_replicates"),
                                                     static_cast<double>(bootstrap.successfulRateReplicates), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HBKinetics.bootstrap_block_length"),
                                                     static_cast<double>(bootstrap.effectiveBlockLength), createdByNode);
            const auto setConfidenceInterval = [&](const QString& prefix, const ConfidenceInterval& interval) {
                if(std::isfinite(interval.lower) && std::isfinite(interval.upper)) {
                    const QString lowerKey = prefix + QStringLiteral("_ci95_lower");
                    const QString upperKey = prefix + QStringLiteral("_ci95_upper");
                    computationResult.results->setAttribute(QStringView(lowerKey), interval.lower, createdByNode);
                    computationResult.results->setAttribute(QStringView(upperKey), interval.upper, createdByNode);
                }
            };
            setConfidenceInterval(QStringLiteral("HBKinetics.continuous_lifetime"), bootstrap.continuousLifetime);
            setConfidenceInterval(QStringLiteral("HBKinetics.intermittent_correlation_time"), bootstrap.intermittentCorrelationTime);
            setConfidenceInterval(QStringLiteral("HBKinetics.breaking_rate"), bootstrap.breakingRate);
            setConfidenceInterval(QStringLiteral("HBKinetics.reformation_rate"), bootstrap.reformationRate);
            setConfidenceInterval(QStringLiteral("HBKinetics.luzar_chandler_lifetime"), bootstrap.breakingLifetime);
            setConfidenceInterval(QStringLiteral("HBKinetics.reformation_time"), bootstrap.reformationTime);
            if(self->definitionMode() == FixedGeometry) {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.hb_donor_acceptor_cutoff"), static_cast<double>(self->donorAcceptorCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.hb_theta_maximum"), static_cast<double>(self->angleCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.vicinity_cutoff"), static_cast<double>(self->vicinityCutoff()), createdByNode);
            }
            else if(self->definitionMode() == PMFDerived) {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_distance_minimum"), upstreamPmf.distanceMinimum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_distance_maximum"), upstreamPmf.distanceMaximum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_theta_minimum"), upstreamPmf.thetaMinimum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_theta_maximum"), upstreamPmf.thetaMaximum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_distance_bins"), static_cast<double>(upstreamPmf.distanceBins), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_angle_bins"), static_cast<double>(upstreamPmf.angleBins), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_boundary_free_energy"), upstreamPmf.boundaryFreeEnergy, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_vicinity_cutoff"), upstreamPmf.vicinityCutoff, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_basin_bin_count"), static_cast<double>(upstreamPmf.basinBinCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.pmf_populated_bin_count"), static_cast<double>(upstreamPmf.populatedBinCount), createdByNode);
            }
            else {
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_distance_maximum"), upstreamSiteEnergy.distanceMaximum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_theta_maximum"), upstreamSiteEnergy.thetaMaximum, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_cutoff"), upstreamSiteEnergy.cutoff, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_cutoff_mode"), upstreamSiteEnergy.cutoffMode, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_unit"), upstreamSiteEnergy.unitLabel, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.vicinity_cutoff"), static_cast<double>(self->vicinityCutoff()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_relative_permittivity"), upstreamSiteEnergy.potential.relativePermittivity, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_donor_lj_epsilon"), upstreamSiteEnergy.potential.donorEpsilon, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_donor_lj_sigma"), upstreamSiteEnergy.potential.donorSigma, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_hydrogen_lj_epsilon"), upstreamSiteEnergy.potential.hydrogenEpsilon, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_hydrogen_lj_sigma"), upstreamSiteEnergy.potential.hydrogenSigma, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_acceptor_lj_epsilon"), upstreamSiteEnergy.potential.acceptorEpsilon, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("HBKinetics.site_energy_acceptor_lj_sigma"), upstreamSiteEnergy.potential.acceptorSigma, createdByNode);
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
            if(self->timeCoordinateMode() == AutomaticTimeCoordinate
               && anySnapshotUsesTimestep
               && !allSnapshotsUseTimestep) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "The Timestep attribute was missing or invalid in some sampled frames. "
                    "The time axis therefore falls back to source-frame numbers for the entire analysis.");
            }
            if(self->timeCoordinateMode() == AutomaticTimeCoordinate
               && allSnapshotsUseTimestep
               && !timestepCoordinatesStrictlyIncrease) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "The Timestep attribute repeats or decreases in the sampled trajectory. "
                    "The time axis therefore uses source-frame numbers without modifying the trajectory file.");
            }
            if(irregularTimeSpacing) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "The sampled trajectory has nonuniform time spacing. Lag times are averaged over all available time origins.");
            }
            if(continuousIntegralTruncated) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "Continuous S(t) remains above 0.01 at the maximum lag. Its observed integral is truncated and should be treated as a lower-bound estimate.");
            }
            if(intermittentIntegralTruncated) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "Intermittent C(t) remains above 0.01 at the maximum lag. Its observed correlation-time integral has not converged.");
            }
            if(lifetimeEvents.durations.empty()) {
                warnings << HydrogenBondKineticsModifier::tr(
                    "No complete uncensored bond episode was observed in the selected interval. Boundary-censored episodes are excluded from the event-lifetime histogram.");
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
