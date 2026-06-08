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
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/NearestNeighborFinder.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "StructuralOrderModifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(StructuralOrderModifier);
OVITO_CLASSINFO(StructuralOrderModifier, "Description", "Compute entropy-based and related structural order parameters.");
OVITO_CLASSINFO(StructuralOrderModifier, "DisplayName", "Structural order");
OVITO_CLASSINFO(StructuralOrderModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, orderParameter);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, cutoff);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, radialBins);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, angularBins);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, distributionBins);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, tetrahedralReferenceDistance);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, localStructureIndexCutoff);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, localOrderTargetMode);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, referenceTypes);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, referenceExpression);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, localSiteTypes);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, localSiteExpression);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, localShellCutoff);
DEFINE_PROPERTY_FIELD(StructuralOrderModifier, onlySelected);
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, orderParameter, "Order parameter");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, cutoff, "Cutoff radius");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, radialBins, "Number of radial bins");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, angularBins, "Number of angular bins");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, distributionBins, "Number of histogram bins");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, tetrahedralReferenceDistance, "Ideal tetrahedral distance");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, localStructureIndexCutoff, "LSI shell cutoff");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, localOrderTargetMode, "Local-order target");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, referenceTypes, "Reference atom type(s)");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, referenceExpression, "Reference expression");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, localSiteTypes, "Local site atom type(s)");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, localSiteExpression, "Local site expression");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, localShellCutoff, "Reference cutoff");
SET_PROPERTY_FIELD_LABEL(StructuralOrderModifier, onlySelected, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, radialBins, IntegerParameterUnit, 4);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, angularBins, IntegerParameterUnit, 4);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, distributionBins, IntegerParameterUnit, 4);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, tetrahedralReferenceDistance, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, localStructureIndexCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(StructuralOrderModifier, localShellCutoff, WorldParameterUnit, 0);

namespace {

struct MoleculeGroup
{
    IdentifierIntType moleculeId = 0;
    std::vector<size_t> indices;
    bool anySelected = false;
};

struct MoleculeRecord
{
    IdentifierIntType moleculeId = 0;
    Point3 center = Point3::Origin();
    Vector3 dipoleDirection = Vector3::Zero();
};

struct LocalOrderStats
{
    double mean = 0.0;
    double stddev = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
    size_t targetValueCount = 0;
    size_t finiteValueCount = 0;
};

constexpr double GasConstantJPerMolK = 8.31446261815324;
constexpr double JoulesPerCalorie = 4.184;

double entropyKbToJPerMolK(double value)
{
    return value * GasConstantJPerMolK;
}

double entropyKbToCalPerMolK(double value)
{
    return entropyKbToJPerMolK(value) / JoulesPerCalorie;
}

double entropyIntegrand(double g)
{
    if(!(g > 0.0))
        return 1.0;
    return g * std::log(g) - g + 1.0;
}

double shellVolume(int bin, double binSize, bool is2D)
{
    const double r1 = static_cast<double>(bin) * binSize;
    const double r2 = r1 + binSize;
    return is2D
        ? Ovito::pi * (r2 * r2 - r1 * r1)
        : (4.0 / 3.0) * Ovito::pi * (r2 * r2 * r2 - r1 * r1 * r1);
}

QString orderParameterLabel(StructuralOrderModifier::OrderParameter orderParameter)
{
    switch(orderParameter) {
    case StructuralOrderModifier::TranslationalEntropyOrder:
        return StructuralOrderModifier::tr("Translational entropy order (-s2^tr)");
    case StructuralOrderModifier::OrientationalEntropyOrder:
        return StructuralOrderModifier::tr("Orientational entropy order (-s2^or, dipole approximation)");
    case StructuralOrderModifier::TetrahedralOrderParameter:
        return StructuralOrderModifier::tr("Tetrahedral order parameter (q)");
    case StructuralOrderModifier::RadialTetrahedralOrderParameter:
        return StructuralOrderModifier::tr("Radial tetrahedral order (S_k)");
    case StructuralOrderModifier::LocalStructureIndexOrderParameter:
        return StructuralOrderModifier::tr("Local structure index (LSI)");
    case StructuralOrderModifier::VoronoiLocalDensityOrderParameter:
        return StructuralOrderModifier::tr("Voronoi local density (1/V)");
    }
    return {};
}

bool isLocalScalarOrder(StructuralOrderModifier::OrderParameter orderParameter)
{
    return orderParameter == StructuralOrderModifier::TetrahedralOrderParameter
        || orderParameter == StructuralOrderModifier::RadialTetrahedralOrderParameter
        || orderParameter == StructuralOrderModifier::LocalStructureIndexOrderParameter
        || orderParameter == StructuralOrderModifier::VoronoiLocalDensityOrderParameter;
}

QString localOrderPropertyName(StructuralOrderModifier::OrderParameter orderParameter)
{
    switch(orderParameter) {
    case StructuralOrderModifier::TetrahedralOrderParameter:
        return QStringLiteral("Tetrahedral Order Parameter");
    case StructuralOrderModifier::RadialTetrahedralOrderParameter:
        return QStringLiteral("Radial Tetrahedral Order Parameter");
    case StructuralOrderModifier::LocalStructureIndexOrderParameter:
        return QStringLiteral("Local Structure Index");
    case StructuralOrderModifier::VoronoiLocalDensityOrderParameter:
        return QStringLiteral("Voronoi Local Density");
    default:
        return {};
    }
}

QString localOrderAxisLabel(StructuralOrderModifier::OrderParameter orderParameter)
{
    switch(orderParameter) {
    case StructuralOrderModifier::TetrahedralOrderParameter:
        return StructuralOrderModifier::tr("Tetrahedral order parameter q");
    case StructuralOrderModifier::RadialTetrahedralOrderParameter:
        return StructuralOrderModifier::tr("Radial tetrahedral order S_k");
    case StructuralOrderModifier::LocalStructureIndexOrderParameter:
        return StructuralOrderModifier::tr("Local structure index LSI (length^2)");
    case StructuralOrderModifier::VoronoiLocalDensityOrderParameter:
        return StructuralOrderModifier::tr("Voronoi local density 1/V (1/length^3)");
    default:
        return {};
    }
}

FloatType computeTetrahedralOrder(const NearestNeighborFinder::Query<4>& neighborQuery)
{
    if(neighborQuery.results().size() < 4)
        return std::numeric_limits<FloatType>::quiet_NaN();

    std::array<Vector3, 4> neighborDirections;
    for(int neighborIndex = 0; neighborIndex < 4; ++neighborIndex) {
        const Vector3& delta = neighborQuery.results()[neighborIndex].delta;
        const FloatType length = delta.length();
        if(!(length > FloatType(0)))
            return std::numeric_limits<FloatType>::quiet_NaN();
        neighborDirections[neighborIndex] = delta / length;
    }

    FloatType angleSum = FloatType(0);
    for(int j = 0; j < 3; ++j) {
        for(int k = j + 1; k < 4; ++k) {
            const FloatType cosTheta = std::clamp(neighborDirections[j].dot(neighborDirections[k]), FloatType(-1), FloatType(1));
            const FloatType deviation = cosTheta + FloatType(1.0 / 3.0);
            angleSum += deviation * deviation;
        }
    }
    return FloatType(1) - FloatType(3.0 / 8.0) * angleSum;
}

FloatType computeRadialTetrahedralOrder(const NearestNeighborFinder::Query<4>& neighborQuery, FloatType referenceDistance)
{
    if(neighborQuery.results().size() < 4 || !(referenceDistance > FloatType(0)))
        return std::numeric_limits<FloatType>::quiet_NaN();

    std::array<FloatType, 4> distances;
    FloatType meanDistance = FloatType(0);
    for(int neighborIndex = 0; neighborIndex < 4; ++neighborIndex) {
        distances[neighborIndex] = std::sqrt(neighborQuery.results()[neighborIndex].distanceSq);
        if(!(distances[neighborIndex] > FloatType(0)))
            return std::numeric_limits<FloatType>::quiet_NaN();
        meanDistance += distances[neighborIndex];
    }
    meanDistance /= FloatType(4);

    FloatType varianceSum = FloatType(0);
    for(FloatType distance : distances) {
        const FloatType deviation = distance - meanDistance;
        varianceSum += deviation * deviation;
    }

    const FloatType denominator = FloatType(12) * referenceDistance * referenceDistance;
    return FloatType(1) - varianceSum / denominator;
}

FloatType computeLocalStructureIndex(std::vector<FloatType>& neighborDistances)
{
    if(neighborDistances.size() < 2)
        return std::numeric_limits<FloatType>::quiet_NaN();

    std::sort(neighborDistances.begin(), neighborDistances.end());
    std::vector<FloatType> gaps;
    gaps.reserve(neighborDistances.size() - 1);
    FloatType meanGap = FloatType(0);
    for(size_t index = 0; index + 1 < neighborDistances.size(); ++index) {
        const FloatType gap = neighborDistances[index + 1] - neighborDistances[index];
        gaps.push_back(gap);
        meanGap += gap;
    }
    meanGap /= static_cast<FloatType>(gaps.size());

    FloatType variance = FloatType(0);
    for(FloatType gap : gaps) {
        const FloatType deviation = gap - meanGap;
        variance += deviation * deviation;
    }
    return variance / static_cast<FloatType>(gaps.size());
}

LocalOrderStats createLocalOrderDistributionTable(PipelineFlowState& state,
                                                  const Property* localOrderProperty,
                                                  const std::vector<uint8_t>& targetMask,
                                                  int histogramBinCount,
                                                  StructuralOrderModifier::OrderParameter orderParameter,
                                                  const OOWeakRef<const PipelineNode>& createdByNode)
{
    BufferReadAccess<FloatType> localOrderValues(localOrderProperty);

    LocalOrderStats stats;
    double valueSum = 0.0;
    double valueSquareSum = 0.0;
    bool haveFiniteValue = false;

    for(size_t particleIndex = 0; particleIndex < localOrderValues.size(); ++particleIndex) {
        if(!targetMask[particleIndex])
            continue;

        stats.targetValueCount++;
        const double value = localOrderValues[particleIndex];
        if(!std::isfinite(value))
            continue;

        if(!haveFiniteValue) {
            stats.minValue = value;
            stats.maxValue = value;
            haveFiniteValue = true;
        }
        else {
            stats.minValue = std::min(stats.minValue, value);
            stats.maxValue = std::max(stats.maxValue, value);
        }

        valueSum += value;
        valueSquareSum += value * value;
        stats.finiteValueCount++;
    }

    if(stats.finiteValueCount == 0)
        throw Exception(StructuralOrderModifier::tr("No finite local structural order values were computed."));

    stats.mean = valueSum / static_cast<double>(stats.finiteValueCount);
    const double variance = std::max(0.0, valueSquareSum / static_cast<double>(stats.finiteValueCount) - stats.mean * stats.mean);
    stats.stddev = std::sqrt(variance);

    double rangeMin = stats.minValue;
    double rangeMax = stats.maxValue;
    if(orderParameter == StructuralOrderModifier::TetrahedralOrderParameter) {
        const double qMin = -3.0;
        const double qMax = 1.0;
        const double qBinSpacing = (qMax - qMin) / static_cast<double>(std::max(histogramBinCount - 1, 1));
        rangeMin = qMin - 0.5 * qBinSpacing;
        rangeMax = qMax + 0.5 * qBinSpacing;
    }
    else if(!(rangeMax > rangeMin)) {
        const double padding = std::max(std::abs(rangeMin), 1.0) * 0.5;
        rangeMin -= padding;
        rangeMax += padding;
    }
    else {
        const double padding = (rangeMax - rangeMin) * 0.025;
        rangeMin -= padding;
        rangeMax += padding;
    }

    const double binSize = (rangeMax - rangeMin) / static_cast<double>(histogramBinCount);
    std::vector<size_t> histogram(static_cast<size_t>(histogramBinCount), 0);
    for(size_t particleIndex = 0; particleIndex < localOrderValues.size(); ++particleIndex) {
        if(!targetMask[particleIndex])
            continue;

        const double value = localOrderValues[particleIndex];
        if(!std::isfinite(value))
            continue;

        size_t histogramBin = 0;
        if(value >= rangeMax)
            histogramBin = static_cast<size_t>(histogramBinCount - 1);
        else if(value > rangeMin)
            histogramBin = static_cast<size_t>((value - rangeMin) / binSize);
        if(histogramBin >= static_cast<size_t>(histogramBinCount))
            histogramBin = static_cast<size_t>(histogramBinCount - 1);
        histogram[histogramBin]++;
    }

    DataTable* table = state.createObject<DataTable>(StructuralOrderModifier::ProfileTableIdentifier.toString(),
                                                     createdByNode,
                                                     DataTable::Histogram,
                                                     StructuralOrderModifier::tr("Local structural order distribution"));
    table->setElementCount(histogramBinCount);
    table->setIntervalStart(static_cast<FloatType>(rangeMin));
    table->setIntervalEnd(static_cast<FloatType>(rangeMax));
    table->setAxisLabelX(localOrderAxisLabel(orderParameter));
    table->setAxisLabelY(StructuralOrderModifier::tr("Probability density"));
    Property* distributionValues = table->createProperty(DataBuffer::Initialized,
                                                         QStringLiteral("Probability density"),
                                                         Property::FloatDefault);
    BufferWriteAccess<FloatType, access_mode::discard_write> distribution(distributionValues);
    for(int bin = 0; bin < histogramBinCount; ++bin) {
        distribution[bin] = static_cast<FloatType>(histogram[bin])
                          / (static_cast<FloatType>(stats.finiteValueCount) * static_cast<FloatType>(binSize));
    }
    distribution.reset();
    table->setY(distributionValues);

    return stats;
}

DataTable* createProfileTable(PipelineFlowState& state,
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
    OVITO_ASSERT(std::ranges::all_of(columns, [rowCount](const std::vector<double>& column) {
        return column.size() == rowCount;
    }));

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
    for(size_t row = 0; row < rowCount; ++row) {
        for(int component = 0; component < componentCount; ++component)
            yAcc.set(row, component, static_cast<FloatType>(columns[component][row]));
    }

    PropertyPtr x = DataTable::OOClass().createUserProperty(DataBuffer::Initialized,
                                                            rowCount,
                                                            Property::FloatDefault,
                                                            1,
                                                            QStringLiteral("r"));
    BufferWriteAccess<FloatType, access_mode::discard_write> xAcc(x);
    for(size_t row = 0; row < rowCount; ++row)
        xAcc[row] = static_cast<FloatType>(xValues[row]);

    DataTable* table = state.createObject<DataTable>(identifier.toString(),
                                                     createdByNode,
                                                     DataTable::Line,
                                                     title,
                                                     std::move(y),
                                                     std::move(x));
    table->setAxisLabelX(axisLabelX);
    table->setAxisLabelY(axisLabelY);
    return table;
}

std::vector<MoleculeRecord> buildDipoleMoleculeRecords(const Particles* particles,
                                                       const SimulationCell* simulationCell,
                                                       const Property* selectionProperty,
                                                       bool onlySelected)
{
    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(StructuralOrderModifier::tr(
            "The orientational entropy order requires the particle property 'Molecule Identifier'. Load molecular topology first."));

    BufferReadAccess<FloatType> charges = particles->getProperty(Particles::ChargeProperty);
    if(!charges)
        throw Exception(StructuralOrderModifier::tr(
            "The orientational entropy order currently uses molecular dipole vectors and requires the particle property 'Charge'."));

    BufferReadAccess<FloatType> masses = particles->getProperty(Particles::MassProperty);
    BufferReadAccess<SelectionIntType> selection(selectionProperty);

    std::unordered_map<IdentifierIntType, size_t> groupLookup;
    groupLookup.reserve(positions.size());
    std::vector<MoleculeGroup> groups;
    groups.reserve(positions.size());

    for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
        const IdentifierIntType moleculeId = moleculeIds[particleIndex];
        auto [iter, inserted] = groupLookup.try_emplace(moleculeId, groups.size());
        if(inserted) {
            groups.emplace_back();
            groups.back().moleculeId = moleculeId;
        }

        MoleculeGroup& group = groups[iter->second];
        group.indices.push_back(particleIndex);
        if(selection && selection[particleIndex])
            group.anySelected = true;
    }

    std::vector<MoleculeRecord> records;
    records.reserve(groups.size());
    std::vector<Point3> wrappedPositions;

    for(const MoleculeGroup& group : groups) {
        if(group.indices.empty())
            continue;
        if(onlySelected && !group.anySelected)
            continue;

        wrappedPositions.clear();
        wrappedPositions.reserve(group.indices.size());
        const Point3 referencePosition = positions[group.indices.front()];
        wrappedPositions.push_back(referencePosition);
        for(size_t atomListIndex = 1; atomListIndex < group.indices.size(); ++atomListIndex) {
            const Point3 currentPosition = positions[group.indices[atomListIndex]];
            Vector3 delta = currentPosition - referencePosition;
            if(simulationCell)
                delta = simulationCell->wrapVector(delta);
            wrappedPositions.push_back(referencePosition + delta);
        }

        Vector3 weightedOffsetSum = Vector3::Zero();
        FloatType massSum = FloatType(0);
        for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
            const size_t particleIndex = group.indices[atomListIndex];
            FloatType mass = FloatType(1);
            if(masses && masses[particleIndex] > FloatType(0))
                mass = masses[particleIndex];
            weightedOffsetSum += mass * (wrappedPositions[atomListIndex] - referencePosition);
            massSum += mass;
        }
        if(!(massSum > FloatType(0)))
            continue;

        const Point3 moleculeCenter = referencePosition + weightedOffsetSum / massSum;
        Vector3 dipole = Vector3::Zero();
        for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
            const size_t particleIndex = group.indices[atomListIndex];
            dipole += charges[particleIndex] * (wrappedPositions[atomListIndex] - moleculeCenter);
        }

        const FloatType magnitude = dipole.length();
        if(!(magnitude > FloatType(0)))
            continue;

        records.push_back(MoleculeRecord{group.moleculeId, moleculeCenter, dipole / magnitude});
    }

    return records;
}

}  // namespace

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool StructuralOrderModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void StructuralOrderModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void StructuralOrderModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                  PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                  TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
 * Returns a short piece of information for the pipeline editor list.
 ******************************************************************************/
QVariant StructuralOrderModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    return orderParameterLabel(orderParameter());
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> StructuralOrderModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), false)) {
            bool cachedOwnOutput = false;
            if(const DataTable* cachedTable = cachedState.getObjectBy<DataTable>(
                   request.modificationNode(), StructuralOrderModifier::ProfileTableIdentifier)) {
                state.addObject(cachedTable);
                cachedOwnOutput = true;
            }
            if(const Particles* cachedParticles = cachedState.getObject<Particles>()) {
                if(isLocalScalarOrder(orderParameter())) {
                    particles->tryToAdoptProperties(cachedParticles, {
                        cachedParticles->getProperty(localOrderPropertyName(orderParameter()))
                    }, {particles});
                    if(cachedParticles->getProperty(localOrderPropertyName(orderParameter())))
                        cachedOwnOutput = true;
                }
            }
            if(cachedOwnOutput)
                state.adoptAttributesFrom(cachedState, request.modificationNodeWeak());
        }
        return std::move(state);
    }

    const OrderParameter selectedOrderParameter = orderParameter();
    const LocalOrderTargetMode selectedLocalTargetMode = localOrderTargetMode();
    const bool localScalarOrder = isLocalScalarOrder(selectedOrderParameter);

    if(!localScalarOrder && cutoff() <= 0)
        throw Exception(tr("Invalid cutoff range value. Cutoff must be positive."));
    if(localScalarOrder
            && selectedLocalTargetMode == SitesWithinReferenceCutoff
            && localShellCutoff() <= 0)
        throw Exception(tr("Invalid reference cutoff value. Cutoff must be positive."));
    if(selectedOrderParameter == RadialTetrahedralOrderParameter && tetrahedralReferenceDistance() <= 0)
        throw Exception(tr("Invalid ideal tetrahedral distance. Distance must be positive."));
    if(selectedOrderParameter == LocalStructureIndexOrderParameter && localStructureIndexCutoff() <= 0)
        throw Exception(tr("Invalid LSI shell cutoff. Cutoff must be positive."));

    const int binCount = std::max(radialBins(), 4);
    const int angularBinCount = std::max(angularBins(), 4);
    const int histogramBinCount = std::max(distributionBins(), 4);
    const FloatType cutoffRadius = cutoff();
    const FloatType binSize = cutoffRadius / static_cast<FloatType>(binCount);
    const Property* selection = onlySelected() ? particles->expectProperty(Particles::SelectionProperty) : nullptr;
    const SimulationCell* simulationCell = state.getObject<SimulationCell>();

    if(!simulationCell)
        throw Exception(tr("The entropy-based structural order parameters require a simulation cell to determine the number density."));
    if(simulationCell->isDegenerate())
        throw Exception(tr("The entropy-based structural order parameters require a non-zero simulation cell volume."));

    return asyncLaunch([state = std::move(state),
                        particles,
                        selection,
                        simulationCell,
                        selectedOrderParameter,
                        selectedLocalTargetMode,
                        cutoffRadius,
                        localShellCutoff = localShellCutoff(),
                        tetrahedralReferenceDistance = tetrahedralReferenceDistance(),
                        localStructureIndexCutoff = localStructureIndexCutoff(),
                        binSize,
                        binCount,
                        angularBinCount,
                        histogramBinCount,
                        selectedOnly = onlySelected(),
                        referenceTypes = referenceTypes(),
                        referenceExpression = referenceExpression(),
                        localSiteTypes = localSiteTypes(),
                        localSiteExpression = localSiteExpression(),
                        createdByNode = request.modificationNodeWeak()]() mutable {
        TaskProgress progress(this_task::ui());
        progress.setText(tr("Computing structural order"));

        if(selectedOrderParameter == TranslationalEntropyOrder) {
            const size_t particleCount = particles->elementCount();
            const size_t selectedParticleCount = selection ? selection->nonzeroCount() : particleCount;
            if(selectedParticleCount == 0)
                throw Exception(tr("The selected particle set is empty."));

            CutoffNeighborFinder neighborFinder(cutoffRadius,
                                                particles->expectProperty(Particles::PositionProperty),
                                                simulationCell,
                                                selection);

            const double volume = neighborFinder.simulationCellVolume();
            if(!(volume > 0.0))
                throw Exception(tr("The translational entropy order requires a non-zero simulation cell volume."));

            const double density = static_cast<double>(selectedParticleCount) / volume;
            double estimatedNeighborCount = density *
                (!neighborFinder.simCell().is2D()
                     ? (Ovito::pi * cutoffRadius * cutoffRadius * cutoffRadius * (4.0 / 3.0))
                     : (Ovito::pi * cutoffRadius * cutoffRadius));
            size_t chunkSize = 4096;
            if(estimatedNeighborCount > 1.0)
                chunkSize = std::clamp<size_t>(static_cast<size_t>((4096 * 32) / estimatedNeighborCount), 8, chunkSize);

            BufferReadAccess<SelectionIntType> selectionData(selection);
            EnumerableThreadSpecific<std::vector<size_t>> threadLocalHistograms;

            parallelForInnerOuter(particleCount, chunkSize, progress, [&](auto&& iterate) {
                std::vector<size_t>& threadLocalHistogram = threadLocalHistograms.create(binCount, 0);
                iterate([&](size_t i) {
                    if(selectionData && !selectionData[i])
                        return;

                    for(CutoffNeighborFinder::Query neighQuery(neighborFinder, i); !neighQuery.atEnd(); neighQuery.next()) {
                        const size_t rdfBin = std::min(static_cast<size_t>(neighQuery.distance() / binSize), static_cast<size_t>(binCount - 1));
                        threadLocalHistogram[rdfBin]++;
                    }
                });
            });

            std::vector<double> rdfHistogram(binCount, 0.0);
            threadLocalHistograms.visitEach([&](const std::vector<size_t>& histogram) {
                OVITO_ASSERT(histogram.size() == rdfHistogram.size());
                for(size_t i = 0; i < histogram.size(); ++i)
                    rdfHistogram[i] += static_cast<double>(histogram[i]);
            });

            this_task::throwIfCanceled();

            std::vector<double> rValues(binCount, 0.0);
            std::vector<double> cumulativeOrder(binCount, 0.0);
            std::vector<double> orderDensity(binCount, 0.0);
            std::vector<double> rdfValues(binCount, 0.0);

            double translationalOrder = 0.0;
            const double radialStep = static_cast<double>(binSize);
            for(int bin = 0; bin < binCount; ++bin) {
                const double shell = shellVolume(bin, radialStep, neighborFinder.simCell().is2D());
                const double expectedCount = static_cast<double>(selectedParticleCount) * density * shell;
                const double g = expectedCount > 0.0 ? rdfHistogram[bin] / expectedCount : 0.0;
                const double contribution = 0.5 * density * entropyIntegrand(g) * shell;

                translationalOrder += contribution;
                rValues[bin] = (static_cast<double>(bin) + 0.5) * radialStep;
                cumulativeOrder[bin] = translationalOrder;
                orderDensity[bin] = contribution / radialStep;
                rdfValues[bin] = g;
            }

            createProfileTable(state,
                               StructuralOrderModifier::ProfileTableIdentifier,
                               tr("Structural order profile"),
                               rValues,
                               {cumulativeOrder, orderDensity, rdfValues},
                               {tr("Cumulative -s2^tr (kB)"), tr("d(-s2^tr)/dr (kB/length)"), tr("g(r)")},
                               tr("Pair separation distance"),
                               tr("Structural order profile (native kB units)"),
                               createdByNode);

            const double translationalPairEntropy = -translationalOrder;
            const double translationalOrderJ = entropyKbToJPerMolK(translationalOrder);
            const double translationalOrderCal = entropyKbToCalPerMolK(translationalOrder);
            const double translationalPairEntropyJ = entropyKbToJPerMolK(translationalPairEntropy);
            const double translationalPairEntropyCal = entropyKbToCalPerMolK(translationalPairEntropy);
            state.setAttribute(QStringLiteral("StructuralOrder.order_parameter"),
                               QVariant::fromValue(QStringLiteral("Translational entropy order (-s2^tr)")),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_order"),
                               QVariant::fromValue(translationalOrder),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_order_J_per_mol_K"),
                               QVariant::fromValue(translationalOrderJ),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_order_cal_per_mol_K"),
                               QVariant::fromValue(translationalOrderCal),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_pair_entropy"),
                               QVariant::fromValue(translationalPairEntropy),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_pair_entropy_J_per_mol_K"),
                               QVariant::fromValue(translationalPairEntropyJ),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.translational_pair_entropy_cal_per_mol_K"),
                               QVariant::fromValue(translationalPairEntropyCal),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.number_density"),
                               QVariant::fromValue(density),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.sampled_particle_count"),
                               QVariant::fromValue(static_cast<qlonglong>(selectedParticleCount)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.cutoff"),
                               QVariant::fromValue(static_cast<double>(cutoffRadius)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.radial_bins"),
                               QVariant::fromValue(binCount),
                               createdByNode);

            state.combineStatus(PipelineStatus::Success,
                                tr("Translational entropy order (-s2^tr): %1 kB = %2 J/mol K = %3 cal/mol K; profile computed from %4 particles.")
                                    .arg(translationalOrder, 0, 'g', 8)
                                    .arg(translationalOrderJ, 0, 'g', 8)
                                    .arg(translationalOrderCal, 0, 'g', 8)
                                    .arg(selectedParticleCount));
            return std::move(state);
        }

        if(isLocalScalarOrder(selectedOrderParameter)) {
            const size_t particleCount = particles->elementCount();
            BufferReadAccess<SelectionIntType> selectionData(selection);
            std::vector<uint8_t> targetMask(particleCount, 1);
            size_t targetParticleCount = particleCount;
            size_t neighborCandidateCount = particleCount;
            QString targetModeLabel = tr("current particles");

            PropertyPtr neighborSelectionStorage;
            const Property* neighborSelection = selection;

            if(selectedLocalTargetMode == CurrentParticles) {
                if(selectionData) {
                    targetParticleCount = 0;
                    for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                        const bool selected = selectionData[particleIndex] != 0;
                        targetMask[particleIndex] = selected ? 1 : 0;
                        if(selected)
                            targetParticleCount++;
                    }
                    neighborCandidateCount = targetParticleCount;
                    targetModeLabel = tr("selected particles");
                }
            }
            else {
                BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
                BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
                const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
                if(!particleTypes || !particleTypeProperty || !particleTypeProperty->isTypedProperty())
                    throw Exception(tr("The reference-shell local-order mode requires a typed 'Particle Type' property with defined element types."));

                size_t referenceMatchCount = 0;
                std::vector<uint8_t> referenceMask = evaluateParticleSelector(
                    state,
                    particles,
                    particleTypeProperty,
                    particleTypes,
                    referenceTypes,
                    referenceExpression,
                    tr("reference atom type"),
                    tr("Structural order"),
                    &referenceMatchCount);
                if(referenceMatchCount == 0)
                    throw Exception(tr("No particles matched the reference selector."));

                size_t localSiteMatchCount = 0;
                std::vector<uint8_t> localSiteMask = evaluateParticleSelector(
                    state,
                    particles,
                    particleTypeProperty,
                    particleTypes,
                    localSiteTypes,
                    localSiteExpression,
                    tr("local site atom type"),
                    tr("Structural order"),
                    &localSiteMatchCount);
                if(localSiteMatchCount == 0)
                    throw Exception(tr("No particles matched the local site selector."));

                neighborCandidateCount = 0;
                for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                    if(selectionData && !selectionData[particleIndex])
                        localSiteMask[particleIndex] = 0;
                    if(localSiteMask[particleIndex])
                        neighborCandidateCount++;
                }

                neighborSelectionStorage = createSelectionPropertyFromMask(localSiteMask);
                neighborSelection = neighborSelectionStorage.get();

                PropertyPtr referenceSelectionStorage = createSelectionPropertyFromMask(referenceMask);
                BufferReadAccess<SelectionIntType> referenceSelection(referenceSelectionStorage);
                CutoffNeighborFinder referenceFinder(localShellCutoff,
                                                     positions,
                                                     simulationCell,
                                                     referenceSelection);

                std::ranges::fill(targetMask, 0);
                targetParticleCount = 0;
                for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                    if(!localSiteMask[particleIndex])
                        continue;
                    CutoffNeighborFinder::Query referenceQuery(referenceFinder, positions[particleIndex]);
                    if(!referenceQuery.atEnd()) {
                        targetMask[particleIndex] = 1;
                        targetParticleCount++;
                    }
                }

                if(targetParticleCount == 0)
                    throw Exception(tr("No local site atoms were found within the reference cutoff."));

                targetModeLabel = tr("local sites within reference cutoff");
            }

            if((selectedOrderParameter == TetrahedralOrderParameter || selectedOrderParameter == RadialTetrahedralOrderParameter)
                    && neighborCandidateCount < 5)
                throw Exception(tr("The tetrahedral order parameters require at least five neighbor-candidate particles."));
            if(selectedOrderParameter == LocalStructureIndexOrderParameter && neighborCandidateCount < 3)
                throw Exception(tr("The LSI requires at least three neighbor-candidate particles."));
            if(targetParticleCount == 0)
                throw Exception(tr("The local structural order parameter has no target particles to evaluate."));

            Property* localOrderProperty = particles->createProperty(DataBuffer::Uninitialized,
                                                                     localOrderPropertyName(selectedOrderParameter),
                                                                     Property::FloatDefault,
                                                                     1);
            BufferWriteAccess<FloatType, access_mode::discard_read_write> localOrderValues(localOrderProperty);

            if(selectedOrderParameter == TetrahedralOrderParameter || selectedOrderParameter == RadialTetrahedralOrderParameter) {
                NearestNeighborFinder neighborFinder(4,
                                                     particles->expectProperty(Particles::PositionProperty),
                                                     simulationCell,
                                                     neighborSelection);

                parallelFor(particleCount, 4096, progress, [&](size_t particleIndex) {
                    if(!targetMask[particleIndex]) {
                        localOrderValues[particleIndex] = std::numeric_limits<FloatType>::quiet_NaN();
                        return;
                    }

                    NearestNeighborFinder::Query<4> neighborQuery(neighborFinder);
                    neighborQuery.findNeighbors(particleIndex);
                    localOrderValues[particleIndex] = selectedOrderParameter == TetrahedralOrderParameter
                        ? computeTetrahedralOrder(neighborQuery)
                        : computeRadialTetrahedralOrder(neighborQuery, tetrahedralReferenceDistance);
                });
            }
            else if(selectedOrderParameter == LocalStructureIndexOrderParameter) {
                CutoffNeighborFinder neighborFinder(localStructureIndexCutoff,
                                                    particles->expectProperty(Particles::PositionProperty),
                                                    simulationCell,
                                                    neighborSelection);

                parallelFor(particleCount, 4096, progress, [&](size_t particleIndex) {
                    if(!targetMask[particleIndex]) {
                        localOrderValues[particleIndex] = std::numeric_limits<FloatType>::quiet_NaN();
                        return;
                    }

                    std::vector<FloatType> neighborDistances;
                    for(CutoffNeighborFinder::Query neighborQuery(neighborFinder, particleIndex);
                            !neighborQuery.atEnd(); neighborQuery.next()) {
                        if(neighborQuery.distance() > FloatType(0))
                            neighborDistances.push_back(neighborQuery.distance());
                    }
                    localOrderValues[particleIndex] = computeLocalStructureIndex(neighborDistances);
                });
            }
            else if(selectedOrderParameter == VoronoiLocalDensityOrderParameter) {
                BufferReadAccess<FloatType> voronoiLocalDensities = particles->getProperty(QStringLiteral("Voronoi Local Density"));
                BufferReadAccess<FloatType> atomicVolumes(voronoiLocalDensities ? nullptr : particles->getProperty(QStringLiteral("Atomic Volume")));
                if(!voronoiLocalDensities && !atomicVolumes)
                    throw Exception(tr("Voronoi local density requires an upstream Voronoi analysis modifier that outputs 'Voronoi Local Density' or 'Atomic Volume'."));

                parallelFor(particleCount, 4096, progress, [&](size_t particleIndex) {
                    if(!targetMask[particleIndex]) {
                        localOrderValues[particleIndex] = std::numeric_limits<FloatType>::quiet_NaN();
                        return;
                    }

                    if(voronoiLocalDensities) {
                        const FloatType density = voronoiLocalDensities[particleIndex];
                        localOrderValues[particleIndex] = density > FloatType(0)
                            ? density
                            : std::numeric_limits<FloatType>::quiet_NaN();
                    }
                    else {
                        const FloatType volume = atomicVolumes[particleIndex];
                        localOrderValues[particleIndex] = volume > FloatType(0)
                            ? FloatType(1) / volume
                            : std::numeric_limits<FloatType>::quiet_NaN();
                    }
                });
            }

            localOrderValues.reset();
            const LocalOrderStats stats = createLocalOrderDistributionTable(state,
                                                                           localOrderProperty,
                                                                           targetMask,
                                                                           histogramBinCount,
                                                                           selectedOrderParameter,
                                                                           createdByNode);

            state.setAttribute(QStringLiteral("StructuralOrder.order_parameter"),
                               QVariant::fromValue(orderParameterLabel(selectedOrderParameter)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_mean"),
                               QVariant::fromValue(stats.mean),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_stddev"),
                               QVariant::fromValue(stats.stddev),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_sampled_particle_count"),
                               QVariant::fromValue(static_cast<qlonglong>(stats.finiteValueCount)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_skipped_particle_count"),
                               QVariant::fromValue(static_cast<qlonglong>(stats.targetValueCount - stats.finiteValueCount)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_target_particle_count"),
                               QVariant::fromValue(static_cast<qlonglong>(targetParticleCount)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_neighbor_candidate_count"),
                               QVariant::fromValue(static_cast<qlonglong>(neighborCandidateCount)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_target_mode"),
                               QVariant::fromValue(targetModeLabel),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_histogram_bins"),
                               QVariant::fromValue(histogramBinCount),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_property"),
                               QVariant::fromValue(localOrderPropertyName(selectedOrderParameter)),
                               createdByNode);
            state.setAttribute(QStringLiteral("StructuralOrder.local_order_axis_label"),
                               QVariant::fromValue(localOrderAxisLabel(selectedOrderParameter)),
                               createdByNode);
            if(selectedLocalTargetMode == SitesWithinReferenceCutoff) {
                state.setAttribute(QStringLiteral("StructuralOrder.local_order_reference_selector"),
                                   QVariant::fromValue(canonicalizeParticleSelector(referenceTypes, referenceExpression)),
                                   createdByNode);
                state.setAttribute(QStringLiteral("StructuralOrder.local_order_local_site_selector"),
                                   QVariant::fromValue(canonicalizeParticleSelector(localSiteTypes, localSiteExpression)),
                                   createdByNode);
                state.setAttribute(QStringLiteral("StructuralOrder.local_order_reference_cutoff"),
                                   QVariant::fromValue(static_cast<double>(localShellCutoff)),
                                   createdByNode);
            }
            if(selectedOrderParameter == TetrahedralOrderParameter) {
                state.setAttribute(QStringLiteral("StructuralOrder.tetrahedral_order_mean"),
                                   QVariant::fromValue(stats.mean),
                                   createdByNode);
                state.setAttribute(QStringLiteral("StructuralOrder.tetrahedral_order_stddev"),
                                   QVariant::fromValue(stats.stddev),
                                   createdByNode);
            }
            if(selectedOrderParameter == RadialTetrahedralOrderParameter) {
                state.setAttribute(QStringLiteral("StructuralOrder.radial_tetrahedral_reference_distance"),
                                   QVariant::fromValue(static_cast<double>(tetrahedralReferenceDistance)),
                                   createdByNode);
            }
            if(selectedOrderParameter == LocalStructureIndexOrderParameter) {
                state.setAttribute(QStringLiteral("StructuralOrder.local_structure_index_cutoff"),
                                   QVariant::fromValue(static_cast<double>(localStructureIndexCutoff)),
                                   createdByNode);
            }

            state.combineStatus(PipelineStatus::Success,
                                tr("%1: mean %2; distribution computed from %3/%4 target particles.")
                                    .arg(orderParameterLabel(selectedOrderParameter))
                                    .arg(stats.mean, 0, 'g', 8)
                                    .arg(stats.finiteValueCount)
                                    .arg(stats.targetValueCount));
            return std::move(state);
        }

        std::vector<MoleculeRecord> moleculeRecords =
            buildDipoleMoleculeRecords(particles, simulationCell, selection, selectedOnly);
        if(moleculeRecords.empty())
            throw Exception(tr("No molecules with finite dipole vectors are available for the orientational entropy order."));

        PropertyPtr moleculeCenterProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, moleculeRecords.size(), Particles::PositionProperty);
        {
            BufferWriteAccess<Point3, access_mode::discard_write> moleculeCenters(moleculeCenterProperty);
            for(size_t moleculeIndex = 0; moleculeIndex < moleculeRecords.size(); ++moleculeIndex)
                moleculeCenters[moleculeIndex] = moleculeRecords[moleculeIndex].center;
        }

        PropertyPtr moleculeSelectionProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, moleculeRecords.size(), Particles::SelectionProperty);
        {
            BufferWriteAccess<SelectionIntType, access_mode::discard_write> moleculeSelection(moleculeSelectionProperty);
            std::fill(moleculeSelection.begin(), moleculeSelection.end(), 1);
        }

        BufferReadAccess<Point3> moleculeCenters(moleculeCenterProperty);
        BufferReadAccess<SelectionIntType> moleculeSelection(moleculeSelectionProperty);
        const SimulationCellData cellData(simulationCell);
        CutoffNeighborFinder neighborFinder(cutoffRadius, moleculeCenters, cellData, moleculeSelection);

        const double volume = neighborFinder.simulationCellVolume();
        if(!(volume > 0.0))
            throw Exception(tr("The orientational entropy order requires a non-zero simulation cell volume."));

        const size_t moleculeCount = moleculeRecords.size();
        const double density = static_cast<double>(moleculeCount) / volume;
        const double radialStep = static_cast<double>(binSize);
        const double angularBinSize = 2.0 / static_cast<double>(angularBinCount);

        std::vector<double> radialHistogram(binCount, 0.0);
        std::vector<double> angularHistogram(static_cast<size_t>(binCount) * static_cast<size_t>(angularBinCount), 0.0);

        for(size_t moleculeIndex = 0; moleculeIndex < moleculeCount; ++moleculeIndex) {
            this_task::throwIfCanceled();
            for(CutoffNeighborFinder::Query neighQuery(neighborFinder, moleculeIndex); !neighQuery.atEnd(); neighQuery.next()) {
                const size_t neighborIndex = neighQuery.current();
                if(neighborIndex == moleculeIndex)
                    continue;

                const size_t radialBin = std::min(static_cast<size_t>(neighQuery.distance() / binSize), static_cast<size_t>(binCount - 1));
                radialHistogram[radialBin] += 1.0;

                const double cosTheta = std::clamp(
                    static_cast<double>(moleculeRecords[moleculeIndex].dipoleDirection.dot(moleculeRecords[neighborIndex].dipoleDirection)),
                    -1.0,
                    1.0);
                size_t angularBin = static_cast<size_t>((cosTheta + 1.0) / angularBinSize);
                if(angularBin >= static_cast<size_t>(angularBinCount))
                    angularBin = static_cast<size_t>(angularBinCount - 1);
                angularHistogram[radialBin * static_cast<size_t>(angularBinCount) + angularBin] += 1.0;
            }
        }

        std::vector<double> rValues(binCount, 0.0);
        std::vector<double> cumulativeOrder(binCount, 0.0);
        std::vector<double> orderDensity(binCount, 0.0);
        std::vector<double> rdfValues(binCount, 0.0);
        std::vector<double> angularKlValues(binCount, 0.0);

        double orientationalOrder = 0.0;
        for(int bin = 0; bin < binCount; ++bin) {
            const double pairCount = radialHistogram[bin];
            double angularKl = 0.0;
            if(pairCount > 0.0) {
                for(int angularBin = 0; angularBin < angularBinCount; ++angularBin) {
                    const double count = angularHistogram[static_cast<size_t>(bin) * static_cast<size_t>(angularBinCount) + angularBin];
                    if(!(count > 0.0))
                        continue;
                    const double probability = count / pairCount;
                    angularKl += probability * std::log(probability * static_cast<double>(angularBinCount));
                }
            }

            const double shell = shellVolume(bin, radialStep, neighborFinder.simCell().is2D());
            const double expectedCount = static_cast<double>(moleculeCount) * density * shell;
            const double g = expectedCount > 0.0 ? pairCount / expectedCount : 0.0;
            const double contribution = 0.5 * density * g * angularKl * shell;

            orientationalOrder += contribution;
            rValues[bin] = (static_cast<double>(bin) + 0.5) * radialStep;
            cumulativeOrder[bin] = orientationalOrder;
            orderDensity[bin] = contribution / radialStep;
            rdfValues[bin] = g;
            angularKlValues[bin] = angularKl;
        }

        createProfileTable(state,
                           StructuralOrderModifier::ProfileTableIdentifier,
                           tr("Structural order profile"),
                           rValues,
                           {cumulativeOrder, orderDensity, rdfValues, angularKlValues},
                           {tr("Cumulative -s2^or approx. (kB)"), tr("d(-s2^or)/dr approx. (kB/length)"), tr("Molecular g(r)"), tr("Angular KL")},
                           tr("Molecular center separation distance"),
                           tr("Orientational order profile (native kB units)"),
                           createdByNode);

        const double orientationalOrderJ = entropyKbToJPerMolK(orientationalOrder);
        const double orientationalOrderCal = entropyKbToCalPerMolK(orientationalOrder);
        const double orientationalPairEntropy = -orientationalOrder;
        const double orientationalPairEntropyJ = entropyKbToJPerMolK(orientationalPairEntropy);
        const double orientationalPairEntropyCal = entropyKbToCalPerMolK(orientationalPairEntropy);
        state.setAttribute(QStringLiteral("StructuralOrder.order_parameter"),
                           QVariant::fromValue(QStringLiteral("Orientational entropy order (-s2^or, dipole approximation)")),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_order"),
                           QVariant::fromValue(orientationalOrder),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_order_J_per_mol_K"),
                           QVariant::fromValue(orientationalOrderJ),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_order_cal_per_mol_K"),
                           QVariant::fromValue(orientationalOrderCal),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_pair_entropy"),
                           QVariant::fromValue(orientationalPairEntropy),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_pair_entropy_J_per_mol_K"),
                           QVariant::fromValue(orientationalPairEntropyJ),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_pair_entropy_cal_per_mol_K"),
                           QVariant::fromValue(orientationalPairEntropyCal),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.number_density"),
                           QVariant::fromValue(density),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.sampled_molecule_count"),
                           QVariant::fromValue(static_cast<qlonglong>(moleculeCount)),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.cutoff"),
                           QVariant::fromValue(static_cast<double>(cutoffRadius)),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.radial_bins"),
                           QVariant::fromValue(binCount),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.angular_bins"),
                           QVariant::fromValue(angularBinCount),
                           createdByNode);
        state.setAttribute(QStringLiteral("StructuralOrder.orientational_approximation"),
                           QVariant::fromValue(QStringLiteral("dipole-vector cos(theta) histogram")),
                           createdByNode);

        state.combineStatus(PipelineStatus::Success,
                            tr("Orientational entropy order (-s2^or, dipole approximation): %1 kB = %2 J/mol K = %3 cal/mol K; profile computed from %4 molecules.")
                                .arg(orientationalOrder, 0, 'g', 8)
                                .arg(orientationalOrderJ, 0, 'g', 8)
                                .arg(orientationalOrderCal, 0, 'g', 8)
                                .arg(moleculeCount));
        return std::move(state);
    });
}

}  // namespace Ovito
