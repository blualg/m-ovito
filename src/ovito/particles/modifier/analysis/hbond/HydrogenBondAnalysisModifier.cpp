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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include "HydrogenBondAnalysisModifier.h"

namespace Ovito {

namespace {

struct HydrogenBondObservation {
    int frame = 0;
    IdentifierIntType donorId = 0;
    IdentifierIntType hydrogenId = 0;
    IdentifierIntType acceptorId = 0;
    double distance = 0.0;
    double angle = 0.0;
};

struct FrameHydrogenBondResult {
    int frame = 0;
    int64_t count = 0;
    std::vector<HydrogenBondObservation> observations;
    size_t donorCount = 0;
    size_t hydrogenCount = 0;
    size_t acceptorCount = 0;
    size_t donorHydrogenPairCount = 0;
    bool usedParticleIndices = false;
};

struct HydrogenBondAccumulator {
    std::vector<double> frameNumbers;
    std::vector<double> counts;
    std::vector<HydrogenBondObservation> observations;
    size_t totalDonorAtoms = 0;
    size_t totalHydrogenAtoms = 0;
    size_t totalAcceptorAtoms = 0;
    size_t totalDonorHydrogenPairs = 0;
    bool usedParticleIndices = false;
};

struct HydrogenBondComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

struct DonorHydrogenPair {
    size_t donorIndex = 0;
    size_t hydrogenIndex = 0;
    Vector3 donorToHydrogenDelta = Vector3::Zero();
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
        throw Exception(HydrogenBondAnalysisModifier::tr(
            "Hydrogen bond analysis requires a typed 'Particle Type' property with defined element types."));

    const QString trimmedText = typeListText.trimmed();
    if(trimmedText.isEmpty())
        throw Exception(HydrogenBondAnalysisModifier::tr("Please enter at least one %1.").arg(roleDescription));

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
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "Unknown %1 '%2'. Use particle type names or numeric IDs separated by commas.")
                    .arg(roleDescription, token));
            }
        }

        if(std::find(typeIds.begin(), typeIds.end(), typeId) == typeIds.end())
            typeIds.push_back(typeId);
    }

    if(typeIds.empty())
        throw Exception(HydrogenBondAnalysisModifier::tr("Please enter at least one valid %1.").arg(roleDescription));

    return typeIds;
}

PropertyPtr createTypeSelectionProperty(const BufferReadAccess<int>& particleTypes, const std::unordered_set<int>& allowedTypes)
{
    PropertyPtr selectionProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particleTypes.size(), Particles::SelectionProperty);
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> selection(selectionProperty);
    size_t selectedCount = 0;
    for(size_t particleIndex = 0; particleIndex < particleTypes.size(); ++particleIndex) {
        const bool selected = allowedTypes.find(particleTypes[particleIndex]) != allowedTypes.end();
        selection[particleIndex] = selected ? 1 : 0;
        selectedCount += selected;
    }
    Q_UNUSED(selectedCount);
    return selectionProperty;
}

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

inline Vector3 minimumImageVector(const Point3& from, const Point3& to, const SimulationCellData* cell)
{
    Vector3 delta = to - from;
    if(cell)
        delta = cell->wrapVector(delta);
    return delta;
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
    Property* angleProperty = table->createProperty(DataBuffer::Initialized, QStringLiteral("Angle"), Property::FloatDefault, 1);

    BufferWriteAccess<int64_t, access_mode::discard_write> frames(frameProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> donors(donorProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> hydrogens(hydrogenProperty);
    BufferWriteAccess<int64_t, access_mode::discard_write> acceptors(acceptorProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> distances(distanceProperty);
    BufferWriteAccess<FloatType, access_mode::discard_write> angles(angleProperty);

    for(size_t i = 0; i < observations.size(); ++i) {
        frames[i] = observations[i].frame;
        donors[i] = observations[i].donorId;
        hydrogens[i] = observations[i].hydrogenId;
        acceptors[i] = observations[i].acceptorId;
        distances[i] = static_cast<FloatType>(observations[i].distance);
        angles[i] = static_cast<FloatType>(observations[i].angle);
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
            Vector3I reversedShift = bondShift ? -*bondShift : Vector3I::Zero();
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

FrameHydrogenBondResult analyzeFrame(const PipelineFlowState& state,
                                     int sourceFrame,
                                     const std::unordered_set<int>& donorTypes,
                                     const std::unordered_set<int>& hydrogenTypes,
                                     const std::unordered_set<int>& acceptorTypes,
                                     FloatType donorHydrogenCutoff,
                                     FloatType donorAcceptorCutoff,
                                     FloatType angleCutoff,
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
        : SimulationCellData(positions, false, std::max(donorHydrogenCutoff, donorAcceptorCutoff) / 2);
    const SimulationCellData* cellDataPtr = &cellData;

    FrameHydrogenBondResult result;
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
    CutoffNeighborFinder acceptorFinder(donorAcceptorCutoff, positions, cellData, acceptorSelection);

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
            const FloatType daDistance = std::sqrt(acceptorInfo.second);
            const Vector3 hydrogenToDonor = -donorHydrogen.donorToHydrogenDelta;
            const Vector3 hydrogenToAcceptor = donorToAcceptorDelta - donorHydrogen.donorToHydrogenDelta;
            const FloatType haLength = hydrogenToAcceptor.length();
            if(haLength <= FloatType(0))
                continue;

            const FloatType angle = qRadiansToDegrees(
                clampedAcos(hydrogenToDonor.dot(hydrogenToAcceptor) / (dhLength * haLength)));
            if(angle < angleCutoff)
                continue;

            result.observations.push_back({
                sourceFrame,
                particleId(donorHydrogen.donorIndex),
                particleId(donorHydrogen.hydrogenIndex),
                particleId(acceptorIndex),
                static_cast<double>(daDistance),
                static_cast<double>(angle)
            });
        }
    }

    result.count = static_cast<int64_t>(result.observations.size());
    return result;
}

QString donorHydrogenPairingModeLabel(bool useBondTopology)
{
    return useBondTopology
        ? HydrogenBondAnalysisModifier::tr("Bond topology")
        : HydrogenBondAnalysisModifier::tr("Distance cutoff");
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondAnalysisModifier);
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "DisplayName", "Hydrogen bond analysis");
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "Description",
                "Analyze hydrogen bonds over a trajectory using donor-hydrogen-acceptor geometry criteria.");
OVITO_CLASSINFO(HydrogenBondAnalysisModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, hydrogenTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, acceptorTypes);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorHydrogenCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, donorAcceptorCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, angleCutoff);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, intervalStart);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorTypes, "Donor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, hydrogenTypes, "Hydrogen atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, acceptorTypes, "Acceptor atom type(s)");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorHydrogenCutoff, "Donor-hydrogen cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, donorAcceptorCutoff, "Donor-acceptor cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, angleCutoff, "D-H-A angle cutoff");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(HydrogenBondAnalysisModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorHydrogenCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(HydrogenBondAnalysisModifier, donorAcceptorCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, angleCutoff, FloatParameterUnit, 0, 180);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HydrogenBondAnalysisModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondAnalysisModificationNode);
DEFINE_REFERENCE_FIELD(HydrogenBondAnalysisModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(HydrogenBondAnalysisModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(HydrogenBondAnalysisModifier, HydrogenBondAnalysisModificationNode);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool HydrogenBondAnalysisModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Constructor.
 ******************************************************************************/
void HydrogenBondAnalysisModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

/******************************************************************************
 * Returns a concise description for the pipeline editor.
 ******************************************************************************/
QVariant HydrogenBondAnalysisModifier::getPipelineEditorShortInfo(Scene*, ModificationNode*) const
{
    const QString donors = donorTypes().trimmed();
    const QString hydrogens = hydrogenTypes().trimmed();
    const QString acceptors = acceptorTypes().trimmed();
    if(donors.isEmpty() || hydrogens.isEmpty() || acceptors.isEmpty())
        return {};
    return tr("D: %1, H: %2, A: %3").arg(donors, hydrogens, acceptors);
}

/******************************************************************************
 * Builds the sampled frame list.
 ******************************************************************************/
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

/******************************************************************************
 * Asks the modifier for the set of animation time intervals that should be cached.
 ******************************************************************************/
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

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void HydrogenBondAnalysisModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
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
void HydrogenBondAnalysisModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> HydrogenBondAnalysisModifier::evaluateModifier(const ModifierEvaluationRequest& request,
                                                                         PipelineFlowState&& state)
{
    if(auto* modNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Hydrogen bond analysis is idle. Open the Run section and click 'Run hydrogen bond analysis' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Hydrogen bond analysis is queued. Click 'Run hydrogen bond analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeHydrogenBondData(request, std::move(state));
}

/******************************************************************************
 * Computes the cached hydrogen-bond tables by traversing the sampled trajectory.
 ******************************************************************************/
Future<PipelineFlowState> HydrogenBondAnalysisModifier::computeHydrogenBondData(const ModifierEvaluationRequest& request,
                                                                                PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    const std::vector<int> donorTypeIds = parseTypeIds(donorTypes(), particleTypeProperty, tr("donor atom type"));
    const std::vector<int> hydrogenTypeIds = parseTypeIds(hydrogenTypes(), particleTypeProperty, tr("hydrogen atom type"));
    const std::vector<int> acceptorTypeIds = parseTypeIds(acceptorTypes(), particleTypeProperty, tr("acceptor atom type"));
    if(donorHydrogenCutoff() <= 0)
        throw Exception(tr("The donor-hydrogen cutoff must be positive."));
    if(donorAcceptorCutoff() <= 0)
        throw Exception(tr("The donor-acceptor cutoff must be positive."));
    if(angleCutoff() <= 0 || angleCutoff() > 180)
        throw Exception(tr("The D-H-A angle cutoff must be in the range (0, 180]."));

    const std::unordered_set<int> donorTypeSet(donorTypeIds.begin(), donorTypeIds.end());
    const std::unordered_set<int> hydrogenTypeSet(hydrogenTypeIds.begin(), hydrogenTypeIds.end());
    const std::unordered_set<int> acceptorTypeSet(acceptorTypeIds.begin(), acceptorTypeIds.end());
    const bool useBondTopology = particles->bonds() && particles->bonds()->getProperty(Bonds::TopologyProperty);

    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())
        ? dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    HydrogenBondAccumulator accumulator;
    accumulator.frameNumbers.reserve(frames.size());
    accumulator.counts.reserve(frames.size());

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
            [frames = frames,
             donorTypeSet,
             hydrogenTypeSet,
             acceptorTypeSet,
             donorHydrogenCutoff = donorHydrogenCutoff(),
             donorAcceptorCutoff = donorAcceptorCutoff(),
             angleCutoff = angleCutoff(),
             useBondTopology](const std::vector<int>& frameBatch,
                              std::vector<SharedFuture<PipelineFlowState>> batchFutures,
                              HydrogenBondAccumulator& accumulator) {
                for(size_t i = 0; i < batchFutures.size(); ++i) {
                    this_task::throwIfCanceled();
                    FrameHydrogenBondResult frameResult = analyzeFrame(batchFutures[i].result(),
                                                                       frameBatch[i],
                                                                       donorTypeSet,
                                                                       hydrogenTypeSet,
                                                                       acceptorTypeSet,
                                                                       donorHydrogenCutoff,
                                                                       donorAcceptorCutoff,
                                                                       angleCutoff,
                                                                       useBondTopology);
                    accumulator.frameNumbers.push_back(static_cast<double>(frameResult.frame));
                    accumulator.counts.push_back(static_cast<double>(frameResult.count));
                    accumulator.totalDonorAtoms += frameResult.donorCount;
                    accumulator.totalHydrogenAtoms += frameResult.hydrogenCount;
                    accumulator.totalAcceptorAtoms += frameResult.acceptorCount;
                    accumulator.totalDonorHydrogenPairs += frameResult.donorHydrogenPairCount;
                    accumulator.usedParticleIndices = accumulator.usedParticleIndices || frameResult.usedParticleIndices;
                    accumulator.observations.insert(accumulator.observations.end(),
                                                    std::make_move_iterator(frameResult.observations.begin()),
                                                    std::make_move_iterator(frameResult.observations.end()));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this),
              [this, request, state = std::move(state), frames, cacheGenerationId, useBondTopology](HydrogenBondAccumulator accumulator) mutable -> Future<PipelineFlowState> {
        OORef<HydrogenBondAnalysisModifier> self(this);
        const int completedRunRequestId = runRequestId();

        return asyncLaunch([self = std::move(self),
                            request = ModifierEvaluationRequest(request),
                            state = std::move(state),
                            frames,
                            accumulator = std::move(accumulator),
                            useBondTopology,
                            completedRunRequestId,
                            cacheGenerationId]() mutable {
            HydrogenBondComputationResult computationResult{std::move(state)};

            if(!dynamic_object_cast<HydrogenBondAnalysisModificationNode>(request.modificationNode()))
                return computationResult;

            this_task::throwIfCanceled();

            if(frames.empty())
                throw Exception(HydrogenBondAnalysisModifier::tr("Hydrogen bond analysis did not sample any trajectory frames."));
            if(accumulator.totalDonorAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected donor atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalHydrogenAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected hydrogen atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalAcceptorAtoms == 0)
                throw Exception(HydrogenBondAnalysisModifier::tr(
                    "No particles matched the selected acceptor atom type(s) in the sampled trajectory interval."));
            if(accumulator.totalDonorHydrogenPairs == 0) {
                throw Exception(useBondTopology
                    ? HydrogenBondAnalysisModifier::tr(
                        "No donor-hydrogen pairs were found in the bond topology for the selected donor and hydrogen atom type(s).")
                    : HydrogenBondAnalysisModifier::tr(
                        "No donor-hydrogen pairs were found within the donor-hydrogen cutoff. Increase the cutoff or adjust the atom types."));
            }

            computationResult.results = DataOORef<DataCollection>::create();
            const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
            createLineTable(computationResult.results,
                            HydrogenBondAnalysisModifier::countTableId(),
                            HydrogenBondAnalysisModifier::tr("Hydrogen bond count"),
                            accumulator.frameNumbers,
                            accumulator.counts,
                            HydrogenBondAnalysisModifier::tr("Source frame"),
                            HydrogenBondAnalysisModifier::tr("Hydrogen bond count"),
                            createdByNode);
            createObservationTable(computationResult.results,
                                   HydrogenBondAnalysisModifier::observationTableId(),
                                   accumulator.observations,
                                   createdByNode);

            const double sampledFrameCount = static_cast<double>(frames.size());
            const double totalHydrogenBonds = static_cast<double>(accumulator.observations.size());
            const double averageCount = sampledFrameCount > 0 ? (totalHydrogenBonds / sampledFrameCount) : 0.0;
            const double maxCount = accumulator.counts.empty()
                ? 0.0
                : *std::max_element(accumulator.counts.begin(), accumulator.counts.end());

            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_types"), self->donorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.hydrogen_types"), self->hydrogenTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.acceptor_types"), self->acceptorTypes(), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"), static_cast<double>(self->donorHydrogenCutoff()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_acceptor_cutoff"), static_cast<double>(self->donorAcceptorCutoff()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.angle_cutoff"), static_cast<double>(self->angleCutoff()), createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.sampled_frame_count"), sampledFrameCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.total_observations"), totalHydrogenBonds, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.average_count"), averageCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.maximum_count"), maxCount, createdByNode);
            computationResult.results->setAttribute(QStringLiteral("HydrogenBonds.donor_hydrogen_pairing_mode"), donorHydrogenPairingModeLabel(useBondTopology), createdByNode);

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

/******************************************************************************
 * Applies the cached hydrogen-bond data to the current pipeline state.
 ******************************************************************************/
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

/******************************************************************************
 * Clears all cached hydrogen-bond data.
 ******************************************************************************/
void HydrogenBondAnalysisModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

/******************************************************************************
 * Is called when a referenced target generated an event.
 ******************************************************************************/
bool HydrogenBondAnalysisModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
