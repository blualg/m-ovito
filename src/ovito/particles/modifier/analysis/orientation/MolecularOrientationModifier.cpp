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
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "MolecularOrientationModifier.h"

#include <QtMath>
#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_map>

namespace Ovito {

namespace {

struct MoleculeGroup
{
    IdentifierIntType moleculeId = 0;
    std::vector<size_t> indices;
    bool anySelected = false;
};

struct ReferenceHit
{
    FloatType distanceSquared = 0;
    Vector3 referenceToAnchor = Vector3::Zero();
};

struct ReferenceSite
{
    IdentifierIntType moleculeId = 0;
    Point3 position = Point3::Origin();
};

struct DescriptorRecord
{
    IdentifierIntType moleculeId = 0;
    Point3 position = Point3::Origin();
    Vector3 vector = Vector3::Zero();
    FloatType magnitude = 0;
    FloatType angleDegrees = std::numeric_limits<FloatType>::quiet_NaN();
    FloatType cosineToReference = std::numeric_limits<FloatType>::quiet_NaN();
    FloatType distanceToReference = std::numeric_limits<FloatType>::quiet_NaN();
    int32_t overlapCount = 0;
    bool inReferenceShell = false;
};

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

QString directionModeLabel(MolecularOrientationModifier::DirectionMode directionMode)
{
    switch(directionMode) {
    case MolecularOrientationModifier::DipoleDirection:
        return MolecularOrientationModifier::tr("dipole vector");
    case MolecularOrientationModifier::ManualMolecularDirection:
        return MolecularOrientationModifier::tr("atom-type centroid vector");
    case MolecularOrientationModifier::MatchingPairVector:
        return MolecularOrientationModifier::tr("matching pair vector");
    }
    OVITO_ASSERT(false);
    return {};
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(MolecularOrientationModifier);
OVITO_CLASSINFO(MolecularOrientationModifier, "DisplayName", "Molecular orientation around atoms");
OVITO_CLASSINFO(MolecularOrientationModifier, "Description",
                "Generate molecule- or pair-based orientation descriptors around reference atoms and measure their angular distribution.");
OVITO_CLASSINFO(MolecularOrientationModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, directionMode);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, fromTypeId);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, fromExpression);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, toTypeId);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, toExpression);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, referenceTypes);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, referenceExpression);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, anchorTypes);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, anchorExpression);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, cutoff);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, directionMode, "Descriptor");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, fromTypeId, "Direction start atom type");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, fromExpression, "Direction start expression");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, toTypeId, "Direction end atom type");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, toExpression, "Direction end expression");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, referenceTypes, "Orient around atom type(s)");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, referenceExpression, "Reference expression");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, anchorTypes, "Molecule site atom type(s)");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, anchorExpression, "Molecule site expression");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, cutoff, "Distance cutoff");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, numberOfBins, "Angle histogram bins");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(MolecularOrientationModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(MolecularOrientationModifier, numberOfBins, IntegerParameterUnit, 4);

/******************************************************************************
 * Asks the modifier whether it can be applied to the given input data.
 ******************************************************************************/
bool MolecularOrientationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> MolecularOrientationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(tr("This analysis requires the particle property 'Molecule Identifier'. Load molecular topology first."));

    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    if(!particleTypes)
        throw Exception(tr("This analysis requires the particle property 'Particle Type'."));
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
        throw Exception(tr("This analysis requires a typed 'Particle Type' property with defined element types."));

    const DirectionMode selectedDirectionMode = directionMode();
    BufferReadAccess<FloatType> charges(selectedDirectionMode == DipoleDirection ? particles->getProperty(Particles::ChargeProperty) : nullptr);
    if(selectedDirectionMode == DipoleDirection && !charges)
        throw Exception(tr("The dipole direction mode requires the particle property 'Charge'."));

    BufferReadAccess<FloatType> masses = particles->getProperty(Particles::MassProperty);
    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection)
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection."));

    if(selectedDirectionMode == ManualMolecularDirection && fromTypeId() == toTypeId())
        throw Exception(tr("The atom-type centroid vector mode requires two different particle types."));

    const bool referenceUsesPerParticleSites = !referenceExpression().trimmed().isEmpty()
        || parseParticleTypeIds(referenceTypes(), particleTypeProperty, tr("reference atom type"), tr("Molecular orientation analysis")).size() == 1;
    const FloatType cutoffRadius = cutoff();
    if(cutoffRadius <= 0)
        throw Exception(tr("The cutoff radius must be positive."));

    const int histogramBinCount = std::max(numberOfBins(), 4);
    DataTable* table = state.createObject<DataTable>(QString(TableIdentifier), request.modificationNode(), DataTable::Histogram,
                                                     tr("Molecular orientation angle distribution"));
    table->setAxisLabelX(tr("Orientation angle (degrees)"));
    table->setAxisLabelY(tr("Probability density"));
    table->setIntervalStart(FloatType(0));
    table->setIntervalEnd(FloatType(180));

    const SimulationCell* cell = state.getObject<SimulationCell>();
    const int fromParticleType = fromTypeId();
    const int toParticleType = toTypeId();
    const bool selectedOnly = onlySelectedParticles();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            moleculeIds = std::move(moleculeIds),
            particleTypes = std::move(particleTypes),
            charges = std::move(charges),
            masses = std::move(masses),
            selection = std::move(selection),
            particleCount = particles->elementCount(),
            cell,
            selectedDirectionMode,
            fromParticleType,
            toParticleType,
            fromExpression = fromExpression(),
            toExpression = toExpression(),
            referenceTypes = referenceTypes(),
            referenceExpression = referenceExpression(),
            anchorTypes = anchorTypes(),
            anchorExpression = anchorExpression(),
            referenceUsesPerParticleSites,
            cutoffRadius,
            histogramBinCount,
            selectedOnly,
            table,
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        const Particles* particles = state.expectObject<Particles>();
        const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);

        std::unordered_map<IdentifierIntType, size_t> groupLookup;
        groupLookup.reserve(positions.size());
        std::vector<MoleculeGroup> molecules;
        molecules.reserve(positions.size());

        for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
            const IdentifierIntType moleculeId = moleculeIds[particleIndex];
            auto [iter, inserted] = groupLookup.try_emplace(moleculeId, molecules.size());
            if(inserted) {
                molecules.emplace_back();
                molecules.back().moleculeId = moleculeId;
            }

            MoleculeGroup& group = molecules[iter->second];
            group.indices.push_back(particleIndex);
            if(selection && selection[particleIndex])
                group.anySelected = true;
        }

        auto buildWrappedMoleculePositions = [&positions, cell](const MoleculeGroup& group, std::vector<Point3>& wrappedPositions) {
            wrappedPositions.clear();
            wrappedPositions.reserve(group.indices.size());
            if(group.indices.empty())
                return;

            const Point3 referencePosition = positions[group.indices.front()];
            wrappedPositions.push_back(referencePosition);
            for(size_t atomListIndex = 1; atomListIndex < group.indices.size(); ++atomListIndex) {
                const Point3 current = positions[group.indices[atomListIndex]];
                Vector3 delta = current - referencePosition;
                if(cell)
                    delta = cell->wrapVector(delta);
                wrappedPositions.push_back(referencePosition + delta);
            }
        };

        PropertyPtr legacyAngleProperty;
        PropertyPtr legacyDistanceProperty;
        PropertyPtr legacyOverlapProperty;
        std::optional<BufferWriteAccess<FloatType, access_mode::discard_write>> legacyAngleAcc;
        std::optional<BufferWriteAccess<FloatType, access_mode::discard_write>> legacyDistanceAcc;
        std::optional<BufferWriteAccess<int32_t, access_mode::discard_write>> legacyOverlapAcc;
        if(selectedDirectionMode != MatchingPairVector) {
            legacyAngleProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1,
                                                      QStringLiteral("Molecular Orientation Angle"));
            legacyDistanceProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1,
                                                         QStringLiteral("Distance To Nearest Reference"));
            legacyOverlapProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::Int32, 1,
                                                        QStringLiteral("Reference Overlap Count"));
            legacyAngleAcc.emplace(legacyAngleProperty);
            legacyDistanceAcc.emplace(legacyDistanceProperty);
            legacyOverlapAcc.emplace(legacyOverlapProperty);
            std::fill(legacyAngleAcc->begin(), legacyAngleAcc->end(), std::numeric_limits<FloatType>::quiet_NaN());
            std::fill(legacyDistanceAcc->begin(), legacyDistanceAcc->end(), std::numeric_limits<FloatType>::quiet_NaN());
            std::fill(legacyOverlapAcc->begin(), legacyOverlapAcc->end(), 0);
        }

        size_t referenceMatchCount = 0;
        std::vector<uint8_t> referenceMask = evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            referenceTypes, referenceExpression,
            tr("reference atom selector"),
            tr("Molecular orientation analysis"),
            &referenceMatchCount);
        size_t anchorMatchCount = 0;
        std::vector<uint8_t> anchorMask = evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            anchorTypes, anchorExpression,
            tr("molecule site selector"),
            tr("Molecular orientation analysis"),
            &anchorMatchCount);
        size_t fromMatchCount = 0;
        const std::vector<uint8_t> fromMask = selectedDirectionMode != DipoleDirection
            ? evaluateParticleSelector(
                state, particles, particleTypeProperty, particleTypes,
                QString::number(fromParticleType), fromExpression,
                tr("direction start selector"),
                tr("Molecular orientation analysis"),
                &fromMatchCount)
            : std::vector<uint8_t>();
        size_t toMatchCount = 0;
        const std::vector<uint8_t> toMask = selectedDirectionMode != DipoleDirection
            ? evaluateParticleSelector(
                state, particles, particleTypeProperty, particleTypes,
                QString::number(toParticleType), toExpression,
                tr("direction end selector"),
                tr("Molecular orientation analysis"),
                &toMatchCount)
            : std::vector<uint8_t>();
        if(referenceMatchCount == 0)
            throw Exception(tr("No particles matched the reference selector."));
        if(anchorMatchCount == 0)
            throw Exception(tr("No particles matched the molecule site selector."));
        if(selectedDirectionMode != DipoleDirection && fromMatchCount == 0)
            throw Exception(tr("No particles matched the direction start selector."));
        if(selectedDirectionMode != DipoleDirection && toMatchCount == 0)
            throw Exception(tr("No particles matched the direction end selector."));

        std::vector<ReferenceSite> referenceSites;
        referenceSites.reserve(positions.size());
        std::vector<Point3> moleculePositions;
        if(referenceUsesPerParticleSites) {
            for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                if(!referenceMask[particleIndex])
                    continue;
                referenceSites.push_back(ReferenceSite{moleculeIds[particleIndex], positions[particleIndex]});
            }
        }
        else {
            for(const MoleculeGroup& group : molecules) {
                buildWrappedMoleculePositions(group, moleculePositions);
                if(moleculePositions.empty())
                    continue;

                const Point3 referencePosition = moleculePositions.front();
                Vector3 weightedOffsetSum = Vector3::Zero();
                FloatType massSum = FloatType(0);
                size_t matchedCount = 0;
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const size_t particleIndex = group.indices[atomListIndex];
                    if(!referenceMask[particleIndex])
                        continue;

                    FloatType mass = FloatType(1);
                    if(masses && masses[particleIndex] > FloatType(0))
                        mass = masses[particleIndex];
                    weightedOffsetSum += mass * (moleculePositions[atomListIndex] - referencePosition);
                    massSum += mass;
                    matchedCount++;
                }
                if(matchedCount == 0 || massSum <= FloatType(0))
                    continue;

                referenceSites.push_back(ReferenceSite{group.moleculeId, referencePosition + weightedOffsetSum / massSum});
            }
        }
        if(referenceSites.empty())
            throw Exception(tr("No reference sites matched the chosen selector."));

        PropertyPtr referenceSitePositionsProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, referenceSites.size(), Particles::PositionProperty);
        {
            BufferWriteAccess<Point3, access_mode::discard_write> referenceSitePositions(referenceSitePositionsProperty);
            for(size_t siteIndex = 0; siteIndex < referenceSites.size(); ++siteIndex)
                referenceSitePositions[siteIndex] = referenceSites[siteIndex].position;
        }
        PropertyPtr referenceSelectionProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, referenceSites.size(), Particles::SelectionProperty);
        {
            BufferWriteAccess<SelectionIntType, access_mode::discard_write> referenceSelection(referenceSelectionProperty);
            std::fill(referenceSelection.begin(), referenceSelection.end(), 1);
        }

        BufferReadAccess<Point3> referenceSitePositions(referenceSitePositionsProperty);
        const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(referenceSitePositions, false, cutoffRadius / 2);
        BufferReadAccess<SelectionIntType> referenceSelectionRead(referenceSelectionProperty);
        CutoffNeighborFinder neighborFinder(cutoffRadius, referenceSitePositions, cellData, referenceSelectionRead);

        std::vector<int64_t> histogram(histogramBinCount, 0);
        const FloatType angleRangeStart = FloatType(0);
        const FloatType angleRangeEnd = FloatType(180);
        const FloatType binSize = (angleRangeEnd - angleRangeStart) / histogramBinCount;
        std::vector<DescriptorRecord> descriptorRecords;
        descriptorRecords.reserve(molecules.size());

        size_t candidateMoleculeCount = 0;
        size_t generatedDescriptorCount = 0;
        size_t histogramSampleCount = 0;
        size_t missingDirectionCount = 0;
        size_t missingAnchorCount = 0;
        size_t noReferenceCount = 0;
        size_t overlapDescriptorCount = 0;
        size_t zeroDirectionCount = 0;
        size_t zeroRadialCount = 0;

        for(const MoleculeGroup& group : molecules) {
            this_task::throwIfCanceled();

            if(group.indices.empty())
                continue;
            if(selectedOnly && !group.anySelected)
                continue;

            candidateMoleculeCount++;

            buildWrappedMoleculePositions(group, moleculePositions);
            const Point3 reference = moleculePositions.front();

            Vector3 centerOffsetSum = Vector3::Zero();
            for(const Point3& position : moleculePositions)
                centerOffsetSum += (position - reference);
            const Point3 moleculeCenter = reference + centerOffsetSum / static_cast<FloatType>(moleculePositions.size());

            Vector3 anchorWeightedOffsetSum = Vector3::Zero();
            FloatType anchorMassSum = FloatType(0);
            size_t anchorCount = 0;
            for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                const size_t particleIndex = group.indices[atomListIndex];
                if(!anchorMask[particleIndex])
                    continue;

                FloatType mass = FloatType(1);
                if(masses && masses[particleIndex] > FloatType(0))
                    mass = masses[particleIndex];
                anchorWeightedOffsetSum += mass * (moleculePositions[atomListIndex] - reference);
                anchorMassSum += mass;
                anchorCount++;
            }
            if(anchorCount == 0 || anchorMassSum <= FloatType(0)) {
                missingAnchorCount++;
                continue;
            }
            const Point3 anchorPoint = reference + anchorWeightedOffsetSum / anchorMassSum;

            std::unordered_map<size_t, ReferenceHit> referenceHits;
            for(CutoffNeighborFinder::Query neighborQuery(neighborFinder, anchorPoint); !neighborQuery.atEnd(); neighborQuery.next()) {
                const size_t referenceIndex = neighborQuery.current();
                if(referenceIndex >= referenceSites.size())
                    continue;
                if(referenceSites[referenceIndex].moleculeId == group.moleculeId)
                    continue;

                const FloatType distanceSquared = neighborQuery.distanceSquared();
                const Vector3 referenceToAnchor = -neighborQuery.delta();
                auto [iter, inserted] = referenceHits.try_emplace(referenceIndex, ReferenceHit{distanceSquared, referenceToAnchor});
                if(!inserted && distanceSquared < iter->second.distanceSquared) {
                    iter->second.distanceSquared = distanceSquared;
                    iter->second.referenceToAnchor = referenceToAnchor;
                }
            }

            const bool inReferenceShell = !referenceHits.empty();
            FloatType nearestDistance = std::numeric_limits<FloatType>::quiet_NaN();
            Vector3 radialVector = Vector3::Zero();
            FloatType radialMagnitude = FloatType(0);
            int32_t overlapCount = 0;
            if(inReferenceShell) {
                auto nearestIter = std::min_element(referenceHits.begin(), referenceHits.end(),
                                                    [](const auto& left, const auto& right) {
                                                        return left.second.distanceSquared < right.second.distanceSquared;
                                                    });
                OVITO_ASSERT(nearestIter != referenceHits.end());
                nearestDistance = std::sqrt(nearestIter->second.distanceSquared);
                radialVector = nearestIter->second.referenceToAnchor;
                radialMagnitude = radialVector.length();
                overlapCount = static_cast<int32_t>(referenceHits.size());
                if(radialMagnitude <= FloatType(0))
                    zeroRadialCount++;
            }
            else {
                noReferenceCount++;
            }

            std::vector<std::pair<Vector3, FloatType>> descriptorVectors;
            switch(selectedDirectionMode) {
            case DipoleDirection: {
                Vector3 dipoleVector = Vector3::Zero();
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex)
                    dipoleVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - moleculeCenter);
                const FloatType magnitude = dipoleVector.length();
                if(magnitude <= FloatType(0)) {
                    zeroDirectionCount++;
                    continue;
                }
                descriptorVectors.emplace_back(dipoleVector / magnitude, magnitude);
                break;
            }
            case ManualMolecularDirection: {
                Vector3 fromCentroidOffset = Vector3::Zero();
                Vector3 toCentroidOffset = Vector3::Zero();
                size_t fromCount = 0;
                size_t toCount = 0;
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const size_t particleIndex = group.indices[atomListIndex];
                    const Point3& position = moleculePositions[atomListIndex];
                    if(fromMask[particleIndex]) {
                        fromCentroidOffset += (position - reference);
                        fromCount++;
                    }
                    if(toMask[particleIndex]) {
                        toCentroidOffset += (position - reference);
                        toCount++;
                    }
                }
                if(fromCount == 0 || toCount == 0) {
                    missingDirectionCount++;
                    continue;
                }
                const Vector3 directionVector = (toCentroidOffset / static_cast<FloatType>(toCount)) -
                                                (fromCentroidOffset / static_cast<FloatType>(fromCount));
                const FloatType magnitude = directionVector.length();
                if(magnitude <= FloatType(0)) {
                    zeroDirectionCount++;
                    continue;
                }
                descriptorVectors.emplace_back(directionVector / magnitude, magnitude);
                break;
            }
            case MatchingPairVector: {
                std::vector<size_t> fromIndices;
                std::vector<size_t> toIndices;
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const size_t particleIndex = group.indices[atomListIndex];
                    if(fromMask[particleIndex])
                        fromIndices.push_back(atomListIndex);
                    if(toMask[particleIndex])
                        toIndices.push_back(atomListIndex);
                }
                if(fromParticleType == toParticleType) {
                    if(fromIndices.size() < 2) {
                        missingDirectionCount++;
                        continue;
                    }
                    for(size_t i = 0; i < fromIndices.size(); ++i) {
                        for(size_t j = i + 1; j < fromIndices.size(); ++j) {
                            const Vector3 pairVector = moleculePositions[fromIndices[j]] - moleculePositions[fromIndices[i]];
                            const FloatType magnitude = pairVector.length();
                            if(magnitude <= FloatType(0)) {
                                zeroDirectionCount++;
                                continue;
                            }
                            descriptorVectors.emplace_back(pairVector / magnitude, magnitude);
                        }
                    }
                }
                else {
                    if(fromIndices.empty() || toIndices.empty()) {
                        missingDirectionCount++;
                        continue;
                    }
                    for(size_t fromIndex : fromIndices) {
                        for(size_t toIndex : toIndices) {
                            const Vector3 pairVector = moleculePositions[toIndex] - moleculePositions[fromIndex];
                            const FloatType magnitude = pairVector.length();
                            if(magnitude <= FloatType(0)) {
                                zeroDirectionCount++;
                                continue;
                            }
                            descriptorVectors.emplace_back(pairVector / magnitude, magnitude);
                        }
                    }
                }
                if(descriptorVectors.empty()) {
                    missingDirectionCount++;
                    continue;
                }
                break;
            }
            }

            FloatType legacyAngle = std::numeric_limits<FloatType>::quiet_NaN();
            for(const auto& [descriptorVector, descriptorMagnitude] : descriptorVectors) {
                DescriptorRecord record;
                record.moleculeId = group.moleculeId;
                record.position = anchorPoint;
                record.vector = descriptorVector;
                record.magnitude = descriptorMagnitude;
                record.distanceToReference = nearestDistance;
                record.overlapCount = overlapCount;
                record.inReferenceShell = inReferenceShell;

                if(inReferenceShell && radialMagnitude > FloatType(0)) {
                    record.cosineToReference = descriptorVector.dot(radialVector / radialMagnitude);
                    record.angleDegrees = qRadiansToDegrees(clampedAcos(record.cosineToReference));
                    size_t binIndex = static_cast<size_t>((record.angleDegrees - angleRangeStart) / binSize);
                    if(binIndex >= static_cast<size_t>(histogramBinCount))
                        binIndex = static_cast<size_t>(histogramBinCount - 1);
                    histogram[binIndex]++;
                    histogramSampleCount++;
                    legacyAngle = record.angleDegrees;
                }

                if(record.inReferenceShell && record.overlapCount > 1)
                    overlapDescriptorCount++;

                descriptorRecords.push_back(record);
                generatedDescriptorCount++;
            }

            if(legacyAngleAcc && legacyDistanceAcc && legacyOverlapAcc) {
                for(size_t particleIndex : group.indices) {
                    (*legacyAngleAcc)[particleIndex] = legacyAngle;
                    (*legacyDistanceAcc)[particleIndex] = nearestDistance;
                    (*legacyOverlapAcc)[particleIndex] = overlapCount;
                }
            }
        }

        if(selectedOnly && candidateMoleculeCount == 0)
            throw Exception(tr("No molecules were selected. Any selected atom promotes the whole molecule into this analysis."));
        if(descriptorRecords.empty()) {
            if(selectedDirectionMode == ManualMolecularDirection && missingDirectionCount == candidateMoleculeCount)
                throw Exception(tr("No molecules contained both selected atom types for the atom-type centroid vector."));
            if(selectedDirectionMode == MatchingPairVector && missingDirectionCount == candidateMoleculeCount)
                throw Exception(tr("No molecules contained any matching atom pairs for the chosen particle types."));
            if(missingAnchorCount == candidateMoleculeCount)
                throw Exception(tr("No molecules contained the requested molecule site atom type(s)."));
            throw Exception(tr("No molecules satisfied the descriptor-generation criteria."));
        }

        table->setElementCount(histogramBinCount);
        Property* pdfValues = table->createProperty(DataBuffer::Initialized, QStringLiteral("PDF"), Property::FloatDefault);
        BufferWriteAccess<FloatType, access_mode::discard_write> pdf(pdfValues);
        for(int binIndex = 0; binIndex < histogramBinCount; ++binIndex) {
            if(histogramSampleCount != 0)
                pdf[binIndex] = static_cast<FloatType>(histogram[binIndex]) / (static_cast<FloatType>(histogramSampleCount) * binSize);
            else
                pdf[binIndex] = FloatType(0);
        }
        table->setY(pdfValues);

        Particles* descriptorParticles =
            state.createObject<Particles>(DescriptorIdentifier, createdByNode, ObjectInitializationFlag::DontCreateVisElement);
        descriptorParticles->setElementCount(descriptorRecords.size());
        BufferWriteAccess<Point3, access_mode::discard_write> descriptorPositions(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::PositionProperty));
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> descriptorIds(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::IdentifierProperty));
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> descriptorMoleculeIds(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::MoleculeProperty));
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> descriptorSelection(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::SelectionProperty));
        BufferWriteAccess<Vector3, access_mode::discard_write> descriptorVectorsAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Descriptor Vector"), Property::FloatDefault, 3));
        BufferWriteAccess<FloatType, access_mode::discard_write> descriptorMagnitudeAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Descriptor Magnitude"), Property::FloatDefault));
        BufferWriteAccess<FloatType, access_mode::discard_write> descriptorAngleAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Orientation Angle To Reference"), Property::FloatDefault));
        BufferWriteAccess<FloatType, access_mode::discard_write> descriptorCosineAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Orientation Cosine To Reference"), Property::FloatDefault));
        BufferWriteAccess<FloatType, access_mode::discard_write> descriptorDistanceAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Distance To Nearest Reference"), Property::FloatDefault));
        BufferWriteAccess<int32_t, access_mode::discard_write> descriptorOverlapAcc(
            descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Reference Overlap Count"), Property::Int32));

        for(size_t recordIndex = 0; recordIndex < descriptorRecords.size(); ++recordIndex) {
            const DescriptorRecord& record = descriptorRecords[recordIndex];
            descriptorPositions[recordIndex] = record.position;
            descriptorIds[recordIndex] = static_cast<IdentifierIntType>(recordIndex + 1);
            descriptorMoleculeIds[recordIndex] = record.moleculeId;
            descriptorSelection[recordIndex] = record.inReferenceShell ? 1 : 0;
            descriptorVectorsAcc[recordIndex] = record.vector;
            descriptorMagnitudeAcc[recordIndex] = record.magnitude;
            descriptorAngleAcc[recordIndex] = record.angleDegrees;
            descriptorCosineAcc[recordIndex] = record.cosineToReference;
            descriptorDistanceAcc[recordIndex] = record.distanceToReference;
            descriptorOverlapAcc[recordIndex] = record.overlapCount;
        }

        if(legacyAngleProperty) {
            Particles* outputParticles = state.expectMutableObject<Particles>();
            outputParticles->createProperty(std::move(legacyAngleProperty));
            outputParticles->createProperty(std::move(legacyDistanceProperty));
            outputParticles->createProperty(std::move(legacyOverlapProperty));
        }

        QString statusText = tr("Generated %1 descriptor entries from %2 molecules using %3.")
                                 .arg(generatedDescriptorCount)
                                 .arg(candidateMoleculeCount)
                                 .arg(directionModeLabel(selectedDirectionMode));
        statusText += tr(" %1 entries are inside the reference shell.").arg(histogramSampleCount);
        if(overlapDescriptorCount > 0)
            statusText += tr(" %1 descriptor entries had overlapping reference environments.").arg(overlapDescriptorCount);
        if(noReferenceCount > 0)
            statusText += tr(" %1 molecules had no reference site within the cutoff.").arg(noReferenceCount);
        if(missingAnchorCount > 0)
            statusText += tr(" %1 molecules were skipped because they lacked the requested molecule site atoms.").arg(missingAnchorCount);
        if(missingDirectionCount > 0)
            statusText += tr(" %1 molecules were skipped because they lacked the requested descriptor atoms.").arg(missingDirectionCount);
        if(zeroDirectionCount > 0)
            statusText += tr(" %1 descriptor vectors had zero magnitude.").arg(zeroDirectionCount);
        if(zeroRadialCount > 0)
            statusText += tr(" %1 descriptor entries had zero reference distance.").arg(zeroRadialCount);
        if(histogramSampleCount == 0)
            statusText += tr(" No descriptor entry currently lies inside the reference shell.");
        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(generatedDescriptorCount)));
        return std::move(state);
    });
}

}  // namespace Ovito
