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

#include "OrientationTrajectoryAnalysisHelper.h"
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>

#include <QtMath>
#include <algorithm>
#include <limits>
#include <optional>
#include <numeric>
#include <ranges>
#include <unordered_map>

namespace Ovito::OrientationTrajectoryAnalysis {

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
    IdentifierIntType identifier = 0;
    IdentifierIntType moleculeId = 0;
    Vector3 vector = Vector3::Zero();
    bool inReferenceShell = false;
};

struct MembershipRecord
{
    IdentifierIntType identifier = 0;
    IdentifierIntType moleculeId = 0;
    bool inReferenceShell = false;
};

using UnsignedIdType = std::make_unsigned_t<IdentifierIntType>;

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

inline IdentifierIntType makeHashedDescriptorId(std::initializer_list<IdentifierIntType> values)
{
    auto mix = [](UnsignedIdType value) {
        value += 0x9e3779b97f4a7c15ull;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
        return value ^ (value >> 31);
    };

    UnsignedIdType hash = 0x123456789abcdef0ull;
    for(IdentifierIntType value : values)
        hash ^= mix(static_cast<UnsignedIdType>(value)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);

    if(hash == 0)
        hash = 1;
    return static_cast<IdentifierIntType>(hash);
}

bool identifierVectorsMatch(const std::vector<IdentifierIntType>& lhs, const std::vector<IdentifierIntType>& rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

std::vector<size_t> buildIndexMapping(const std::vector<IdentifierIntType>& destinationIds,
                                      const std::vector<IdentifierIntType>& sourceIds,
                                      const QString& elementName,
                                      const QString& destinationLabel,
                                      const QString& sourceLabel,
                                      const QString& analysisLabel)
{
    std::unordered_map<IdentifierIntType, size_t> sourceMap;
    sourceMap.reserve(sourceIds.size());

    size_t index = 0;
    for(const IdentifierIntType id : sourceIds) {
        if(!sourceMap.insert({id, index}).second) {
            throw Exception(QStringLiteral(
                "%1 detected duplicate %2 ID %3 in the %4 configuration. The analysis cannot match elements in this case.")
                .arg(analysisLabel)
                .arg(elementName)
                .arg(id)
                .arg(sourceLabel));
        }
        index++;
    }

    std::vector<size_t> mapping(destinationIds.size());
    size_t destinationIndex = 0;
    for(const IdentifierIntType id : destinationIds) {
        auto iter = sourceMap.find(id);
        if(iter == sourceMap.end()) {
            throw Exception(QStringLiteral(
                "%1 found %2 ID %3 in the %4 configuration but not in the %5 configuration. "
                "The analysis currently requires a stable set of element IDs.")
                .arg(analysisLabel)
                .arg(elementName)
                .arg(id)
                .arg(destinationLabel)
                .arg(sourceLabel));
        }
        mapping[destinationIndex++] = iter->second;
    }

    return mapping;
}

const Property* identifierProperty(const PropertyContainer* container)
{
    if(!container)
        return nullptr;
    if(!container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty))
        return nullptr;
    return container->getProperty(Property::GenericIdentifierProperty);
}

bool identifierBuffersMatch(const BufferReadAccess<IdentifierIntType>& lhs, const BufferReadAccess<IdentifierIntType>& rhs)
{
    if(!lhs || !rhs)
        return false;
    return lhs.size() == rhs.size() && std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin());
}

bool identifierBuffersMatch(const std::vector<IdentifierIntType>& lhs, const BufferReadAccess<IdentifierIntType>& rhs)
{
    if(!rhs)
        return false;
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.cbegin());
}

std::vector<size_t> buildIndexMapping(const std::vector<IdentifierIntType>& destinationIds,
                                      const BufferReadAccess<IdentifierIntType>& sourceIds,
                                      const QString& elementName,
                                      const QString& destinationLabel,
                                      const QString& sourceLabel,
                                      const QString& analysisLabel)
{
    OVITO_ASSERT(sourceIds);

    std::unordered_map<IdentifierIntType, size_t> sourceMap;
    sourceMap.reserve(sourceIds.size());

    size_t index = 0;
    for(const IdentifierIntType id : sourceIds) {
        if(!sourceMap.insert({id, index}).second) {
            throw Exception(QStringLiteral(
                "%1 detected duplicate %2 ID %3 in the %4 configuration. The analysis cannot match elements in this case.")
                .arg(analysisLabel)
                .arg(elementName)
                .arg(id)
                .arg(sourceLabel));
        }
        index++;
    }

    std::vector<size_t> mapping(destinationIds.size());
    size_t destinationIndex = 0;
    for(const IdentifierIntType id : destinationIds) {
        auto iter = sourceMap.find(id);
        if(iter == sourceMap.end()) {
            throw Exception(QStringLiteral(
                "%1 found %2 ID %3 in the %4 configuration but not in the %5 configuration. "
                "The analysis currently requires a stable set of element IDs.")
                .arg(analysisLabel)
                .arg(elementName)
                .arg(id)
                .arg(destinationLabel)
                .arg(sourceLabel));
        }
        mapping[destinationIndex++] = iter->second;
    }

    return mapping;
}

std::vector<double> extractPropertyValues(const Property* property, const std::vector<size_t>* mapping = nullptr)
{
    OVITO_ASSERT(property);
    const size_t outputElementCount = mapping ? mapping->size() : property->size();
    std::vector<double> values(outputElementCount * property->componentCount(), 0.0);
    std::vector<FloatType> componentValues(property->size());

    for(size_t c = 0; c < property->componentCount(); ++c) {
        property->copyComponentTo(componentValues.begin(), c);
        if(mapping) {
            for(size_t dst = 0; dst < mapping->size(); ++dst)
                values[dst * property->componentCount() + c] = static_cast<double>(componentValues[(*mapping)[dst]]);
        }
        else {
            for(size_t i = 0; i < property->size(); ++i)
                values[i * property->componentCount() + c] = static_cast<double>(componentValues[i]);
        }
    }

    return values;
}

std::vector<uint8_t> extractBooleanValues(const Property* property, const std::vector<size_t>* mapping = nullptr)
{
    OVITO_ASSERT(property);
    OVITO_ASSERT(property->componentCount() == 1);

    const size_t outputElementCount = mapping ? mapping->size() : property->size();
    std::vector<uint8_t> values(outputElementCount, 0);
    std::vector<FloatType> componentValues(property->size());
    property->copyComponentTo(componentValues.begin(), 0);

    if(mapping) {
        for(size_t dst = 0; dst < mapping->size(); ++dst)
            values[dst] = (componentValues[(*mapping)[dst]] != FloatType(0)) ? 1 : 0;
    }
    else {
        for(size_t i = 0; i < property->size(); ++i)
            values[i] = (componentValues[i] != FloatType(0)) ? 1 : 0;
    }

    return values;
}

double legendrePolynomial(int order, double x)
{
    x = std::clamp(x, -1.0, 1.0);
    if(order <= 0)
        return 1.0;
    if(order == 1)
        return x;

    double pnm2 = 1.0;
    double pnm1 = x;
    for(int n = 2; n <= order; ++n) {
        const double pn = ((2.0 * n - 1.0) * x * pnm1 - (n - 1.0) * pnm2) / static_cast<double>(n);
        pnm2 = pnm1;
        pnm1 = pn;
    }
    return pnm1;
}

std::vector<std::vector<uint8_t>> applyIntermittencyFilter(const std::vector<std::vector<uint8_t>>& memberships, int intermittency)
{
    if(intermittency <= 0 || memberships.empty())
        return memberships;

    std::vector<std::vector<uint8_t>> filtered = memberships;
    const size_t frameCount = memberships.size();
    const size_t itemCount = memberships.front().size();

    for(size_t item = 0; item < itemCount; ++item) {
        size_t frame = 0;
        while(frame < frameCount) {
            if(filtered[frame][item] != 0) {
                frame++;
                continue;
            }

            const size_t gapStart = frame;
            while(frame < frameCount && filtered[frame][item] == 0)
                frame++;
            const size_t gapEnd = frame;
            const size_t gapLength = gapEnd - gapStart;
            const bool boundedByPresence = gapStart > 0 && gapEnd < frameCount
                && filtered[gapStart - 1][item] != 0
                && filtered[gapEnd][item] != 0;
            if(boundedByPresence && gapLength <= static_cast<size_t>(intermittency)) {
                for(size_t gapFrame = gapStart; gapFrame < gapEnd; ++gapFrame)
                    filtered[gapFrame][item] = 1;
            }
        }
    }

    return filtered;
}

std::vector<MoleculeGroup> buildMoleculeGroups(const BufferReadAccess<IdentifierIntType>& moleculeIds,
                                               const BufferReadAccess<SelectionIntType>& selection)
{
    std::unordered_map<IdentifierIntType, size_t> groupLookup;
    groupLookup.reserve(moleculeIds.size());

    std::vector<MoleculeGroup> molecules;
    molecules.reserve(moleculeIds.size());

    for(size_t particleIndex = 0; particleIndex < moleculeIds.size(); ++particleIndex) {
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

    return molecules;
}

void buildWrappedMoleculePositions(const BufferReadAccess<Point3>& positions,
                                   const SimulationCell* cell,
                                   const MoleculeGroup& group,
                                   std::vector<Point3>& wrappedPositions)
{
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
}

std::vector<ReferenceSite> buildReferenceSites(const PipelineFlowState& state,
                                               const Particles* particles,
                                               const Property* particleTypeProperty,
                                               const BufferReadAccess<int32_t>& particleTypes,
                                               const BufferReadAccess<Point3>& positions,
                                               const BufferReadAccess<IdentifierIntType>& moleculeIds,
                                               const BufferReadAccess<FloatType>& masses,
                                               const SimulationCell* cell,
                                               const std::vector<MoleculeGroup>& molecules,
                                               const QString& referenceTypes,
                                               const QString& referenceExpression,
                                               const QString& analysisLabel,
                                               bool& usesPerParticleSites)
{
    size_t referenceMatchCount = 0;
    std::vector<uint8_t> referenceMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        referenceTypes, referenceExpression,
        QObject::tr("reference atom selector"),
        analysisLabel,
        &referenceMatchCount);
    if(referenceMatchCount == 0)
        throw Exception(QObject::tr("No particles matched the reference selector."));

    usesPerParticleSites = !referenceExpression.trimmed().isEmpty()
        || parseParticleTypeIds(referenceTypes, particleTypeProperty, QObject::tr("reference atom type"), analysisLabel).size() == 1;

    std::vector<ReferenceSite> referenceSites;
    referenceSites.reserve(positions.size());
    std::vector<Point3> moleculePositions;

    if(usesPerParticleSites) {
        for(size_t particleIndex = 0; particleIndex < positions.size(); ++particleIndex) {
            if(!referenceMask[particleIndex])
                continue;
            referenceSites.push_back(ReferenceSite{moleculeIds[particleIndex], positions[particleIndex]});
        }
    }
    else {
        for(const MoleculeGroup& group : molecules) {
            buildWrappedMoleculePositions(positions, cell, group, moleculePositions);
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
        throw Exception(QObject::tr("No reference sites matched the chosen selector."));

    return referenceSites;
}

std::vector<DescriptorRecord> buildDescriptorRecords(const ReferenceShellDescriptorRequest& request,
                                                     const PipelineFlowState& state,
                                                     const Particles* particles,
                                                     const QString& analysisLabel)
{
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(QObject::tr("This analysis requires the particle property 'Molecule Identifier'. Load molecular topology first."));
    BufferReadAccess<IdentifierIntType> particleIds = particles->getProperty(Particles::IdentifierProperty);

    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    if(!particleTypes)
        throw Exception(QObject::tr("This analysis requires the particle property 'Particle Type'."));
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
        throw Exception(QObject::tr("%1 requires a typed 'Particle Type' property with defined element types.").arg(analysisLabel));

    BufferReadAccess<FloatType> charges(request.descriptorMode == DescriptorMode::DipoleVector
                                            ? particles->getProperty(Particles::ChargeProperty)
                                            : nullptr);
    if(request.descriptorMode == DescriptorMode::DipoleVector && !charges)
        throw Exception(QObject::tr("The dipole vector mode requires the particle property 'Charge'."));

    BufferReadAccess<FloatType> masses = particles->getProperty(Particles::MassProperty);
    BufferReadAccess<SelectionIntType> selection(request.onlySelectedParticles ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(request.onlySelectedParticles && !selection)
        throw Exception(QObject::tr("The option 'Use only selected particles' requires a particle selection."));

    if(request.descriptorMode == DescriptorMode::AtomTypeCentroidVector && request.fromTypeId == request.toTypeId)
        throw Exception(QObject::tr("The atom-type centroid vector mode requires two different particle types."));

    const FloatType cutoffRadius = request.cutoff;
    if(cutoffRadius <= 0)
        throw Exception(QObject::tr("The cutoff radius must be positive."));

    const std::vector<MoleculeGroup> molecules = buildMoleculeGroups(moleculeIds, selection);
    const SimulationCell* cell = state.getObject<SimulationCell>();

    bool referenceUsesPerParticleSites = false;
    const std::vector<ReferenceSite> referenceSites = buildReferenceSites(
        state, particles, particleTypeProperty, particleTypes, positions, moleculeIds, masses, cell, molecules,
        request.referenceTypes, request.referenceExpression, analysisLabel, referenceUsesPerParticleSites);

    size_t anchorMatchCount = 0;
    const std::vector<uint8_t> anchorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        request.anchorTypes, request.anchorExpression,
        QObject::tr("molecule site selector"),
        analysisLabel,
        &anchorMatchCount);
    if(anchorMatchCount == 0)
        throw Exception(QObject::tr("No particles matched the molecule site selector."));

    size_t fromMatchCount = 0;
    const std::vector<uint8_t> fromMask = request.descriptorMode != DescriptorMode::DipoleVector
        ? evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            QString::number(request.fromTypeId), request.fromExpression,
            QObject::tr("direction start selector"),
            analysisLabel,
            &fromMatchCount)
        : std::vector<uint8_t>();
    size_t toMatchCount = 0;
    const std::vector<uint8_t> toMask = request.descriptorMode != DescriptorMode::DipoleVector
        ? evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            QString::number(request.toTypeId), request.toExpression,
            QObject::tr("direction end selector"),
            analysisLabel,
            &toMatchCount)
        : std::vector<uint8_t>();
    if(request.descriptorMode != DescriptorMode::DipoleVector && fromMatchCount == 0)
        throw Exception(QObject::tr("No particles matched the direction start selector."));
    if(request.descriptorMode != DescriptorMode::DipoleVector && toMatchCount == 0)
        throw Exception(QObject::tr("No particles matched the direction end selector."));

    PropertyPtr referenceSitePositionsProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, referenceSites.size(), Particles::PositionProperty);
    {
        BufferWriteAccess<Point3, access_mode::discard_write> referenceSitePositions(referenceSitePositionsProperty);
        for(size_t siteIndex = 0; siteIndex < referenceSites.size(); ++siteIndex)
            referenceSitePositions[siteIndex] = referenceSites[siteIndex].position;
    }
    PropertyPtr referenceSelectionProperty = createSelectionPropertyFromMask(std::vector<uint8_t>(referenceSites.size(), 1));
    BufferReadAccess<Point3> referenceSitePositions(referenceSitePositionsProperty);
    BufferReadAccess<SelectionIntType> referenceSelection(referenceSelectionProperty);
    const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(referenceSitePositions, false, cutoffRadius / 2);
    CutoffNeighborFinder neighborFinder(cutoffRadius, referenceSitePositions, cellData, referenceSelection);

    std::vector<DescriptorRecord> descriptorRecords;
    descriptorRecords.reserve(molecules.size());
    std::vector<Point3> moleculePositions;

    for(const MoleculeGroup& group : molecules) {
        this_task::throwIfCanceled();

        if(group.indices.empty())
            continue;
        if(request.onlySelectedParticles && !group.anySelected)
            continue;

        buildWrappedMoleculePositions(positions, cell, group, moleculePositions);
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
        if(anchorCount == 0 || anchorMassSum <= FloatType(0))
            continue;
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
        }

        std::vector<std::pair<Vector3, FloatType>> descriptorVectors;
        std::vector<IdentifierIntType> descriptorIdsForGroup;
        const IdentifierIntType fallbackGroupId = particleIds ? particleIds[group.indices.front()]
                                                              : static_cast<IdentifierIntType>(group.indices.front() + 1);
        switch(request.descriptorMode) {
        case DescriptorMode::DipoleVector: {
            Vector3 dipoleVector = Vector3::Zero();
            for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex)
                dipoleVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - moleculeCenter);
            const FloatType magnitude = dipoleVector.length();
            if(magnitude <= FloatType(0))
                continue;
            descriptorVectors.emplace_back(dipoleVector / magnitude, magnitude);
            descriptorIdsForGroup.push_back(group.moleculeId != 0
                                               ? group.moleculeId
                                               : makeHashedDescriptorId({fallbackGroupId, 1}));
            break;
        }
        case DescriptorMode::AtomTypeCentroidVector: {
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
            if(fromCount == 0 || toCount == 0)
                continue;
            const Vector3 directionVector = (toCentroidOffset / static_cast<FloatType>(toCount)) -
                                            (fromCentroidOffset / static_cast<FloatType>(fromCount));
            const FloatType magnitude = directionVector.length();
            if(magnitude <= FloatType(0))
                continue;
            descriptorVectors.emplace_back(directionVector / magnitude, magnitude);
            descriptorIdsForGroup.push_back(group.moleculeId != 0
                                               ? group.moleculeId
                                               : makeHashedDescriptorId({fallbackGroupId, 1}));
            break;
        }
        case DescriptorMode::MatchingPairVector: {
            std::vector<size_t> fromIndices;
            std::vector<size_t> toIndices;
            for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                const size_t particleIndex = group.indices[atomListIndex];
                if(fromMask[particleIndex])
                    fromIndices.push_back(atomListIndex);
                if(toMask[particleIndex])
                    toIndices.push_back(atomListIndex);
            }
            if(request.fromTypeId == request.toTypeId) {
                if(fromIndices.size() < 2)
                    continue;
                for(size_t i = 0; i < fromIndices.size(); ++i) {
                    for(size_t j = i + 1; j < fromIndices.size(); ++j) {
                        size_t firstIndex = fromIndices[i];
                        size_t secondIndex = fromIndices[j];
                        IdentifierIntType firstId = particleIds ? particleIds[group.indices[firstIndex]]
                                                                : static_cast<IdentifierIntType>(group.indices[firstIndex] + 1);
                        IdentifierIntType secondId = particleIds ? particleIds[group.indices[secondIndex]]
                                                                 : static_cast<IdentifierIntType>(group.indices[secondIndex] + 1);
                        if(firstId > secondId) {
                            std::swap(firstIndex, secondIndex);
                            std::swap(firstId, secondId);
                        }
                        const Vector3 pairVector = moleculePositions[secondIndex] - moleculePositions[firstIndex];
                        const FloatType magnitude = pairVector.length();
                        if(magnitude <= FloatType(0))
                            continue;
                        descriptorVectors.emplace_back(pairVector / magnitude, magnitude);
                        descriptorIdsForGroup.push_back(makeHashedDescriptorId({group.moleculeId, firstId, secondId}));
                    }
                }
            }
            else {
                if(fromIndices.empty() || toIndices.empty())
                    continue;
                for(size_t fromIndex : fromIndices) {
                    for(size_t toIndex : toIndices) {
                        const IdentifierIntType fromId = particleIds ? particleIds[group.indices[fromIndex]]
                                                                     : static_cast<IdentifierIntType>(group.indices[fromIndex] + 1);
                        const IdentifierIntType toId = particleIds ? particleIds[group.indices[toIndex]]
                                                                   : static_cast<IdentifierIntType>(group.indices[toIndex] + 1);
                        const Vector3 pairVector = moleculePositions[toIndex] - moleculePositions[fromIndex];
                        const FloatType magnitude = pairVector.length();
                        if(magnitude <= FloatType(0))
                            continue;
                        descriptorVectors.emplace_back(pairVector / magnitude, magnitude);
                        descriptorIdsForGroup.push_back(makeHashedDescriptorId({group.moleculeId, fromId, toId}));
                    }
                }
            }
            if(descriptorVectors.empty())
                continue;
            break;
        }
        }

        for(size_t descriptorIndex = 0; descriptorIndex < descriptorVectors.size(); ++descriptorIndex) {
            const auto& [descriptorVector, descriptorMagnitude] = descriptorVectors[descriptorIndex];
            DescriptorRecord record;
            record.identifier = descriptorIdsForGroup[descriptorIndex];
            record.moleculeId = group.moleculeId;
            record.vector = descriptorVector;
            record.inReferenceShell = inReferenceShell;
            if(inReferenceShell && radialMagnitude > FloatType(0)) {
                const FloatType cosineToReference = descriptorVector.dot(radialVector / radialMagnitude);
                [[maybe_unused]] const FloatType angleDegrees = qRadiansToDegrees(clampedAcos(cosineToReference));
            }
            descriptorRecords.push_back(record);
        }
    }

    if(request.onlySelectedParticles && std::ranges::none_of(molecules, [](const MoleculeGroup& group) { return group.anySelected; }))
        throw Exception(QObject::tr("No molecules were selected. Any selected atom promotes the whole molecule into this analysis."));
    if(descriptorRecords.empty())
        throw Exception(QObject::tr("No molecules satisfied the descriptor-generation criteria."));

    return descriptorRecords;
}

std::vector<MembershipRecord> buildMembershipRecords(const ReferenceShellMembershipRequest& request,
                                                     const PipelineFlowState& state,
                                                     const Particles* particles,
                                                     const QString& analysisLabel)
{
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(QObject::tr("This analysis requires the particle property 'Molecule Identifier'. Load molecular topology first."));

    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    if(!particleTypes)
        throw Exception(QObject::tr("This analysis requires the particle property 'Particle Type'."));
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
        throw Exception(QObject::tr("%1 requires a typed 'Particle Type' property with defined element types.").arg(analysisLabel));

    BufferReadAccess<FloatType> masses = particles->getProperty(Particles::MassProperty);
    BufferReadAccess<SelectionIntType> selection(request.onlySelectedParticles ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(request.onlySelectedParticles && !selection)
        throw Exception(QObject::tr("The option 'Use only selected particles' requires a particle selection."));

    const FloatType cutoffRadius = request.cutoff;
    if(cutoffRadius <= 0)
        throw Exception(QObject::tr("The cutoff radius must be positive."));

    const std::vector<MoleculeGroup> molecules = buildMoleculeGroups(moleculeIds, selection);
    const SimulationCell* cell = state.getObject<SimulationCell>();

    bool referenceUsesPerParticleSites = false;
    const std::vector<ReferenceSite> referenceSites = buildReferenceSites(
        state, particles, particleTypeProperty, particleTypes, positions, moleculeIds, masses, cell, molecules,
        request.referenceTypes, request.referenceExpression, analysisLabel, referenceUsesPerParticleSites);

    size_t anchorMatchCount = 0;
    const std::vector<uint8_t> anchorMask = evaluateParticleSelector(
        state, particles, particleTypeProperty, particleTypes,
        request.anchorTypes, request.anchorExpression,
        QObject::tr("molecule site selector"),
        analysisLabel,
        &anchorMatchCount);
    if(anchorMatchCount == 0)
        throw Exception(QObject::tr("No particles matched the molecule site selector."));

    PropertyPtr referenceSitePositionsProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, referenceSites.size(), Particles::PositionProperty);
    {
        BufferWriteAccess<Point3, access_mode::discard_write> referenceSitePositions(referenceSitePositionsProperty);
        for(size_t siteIndex = 0; siteIndex < referenceSites.size(); ++siteIndex)
            referenceSitePositions[siteIndex] = referenceSites[siteIndex].position;
    }
    PropertyPtr referenceSelectionProperty = createSelectionPropertyFromMask(std::vector<uint8_t>(referenceSites.size(), 1));
    BufferReadAccess<Point3> referenceSitePositions(referenceSitePositionsProperty);
    BufferReadAccess<SelectionIntType> referenceSelection(referenceSelectionProperty);
    const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(referenceSitePositions, false, cutoffRadius / 2);
    CutoffNeighborFinder neighborFinder(cutoffRadius, referenceSitePositions, cellData, referenceSelection);

    std::vector<MembershipRecord> membershipRecords;
    membershipRecords.reserve(molecules.size());
    std::vector<Point3> moleculePositions;

    for(const MoleculeGroup& group : molecules) {
        this_task::throwIfCanceled();

        if(group.indices.empty())
            continue;
        if(request.onlySelectedParticles && !group.anySelected)
            continue;

        buildWrappedMoleculePositions(positions, cell, group, moleculePositions);
        const Point3 reference = moleculePositions.front();

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
        if(anchorCount == 0 || anchorMassSum <= FloatType(0))
            continue;
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

        membershipRecords.push_back(MembershipRecord{group.moleculeId, group.moleculeId, !referenceHits.empty()});
    }

    if(request.onlySelectedParticles && std::ranges::none_of(molecules, [](const MoleculeGroup& group) { return group.anySelected; }))
        throw Exception(QObject::tr("No molecules were selected. Any selected atom promotes the whole molecule into this analysis."));
    if(membershipRecords.empty())
        throw Exception(QObject::tr("No molecules satisfied the reference-shell membership criteria."));

    return membershipRecords;
}

}  // namespace

bool isNumericDataType(int dataType)
{
    return dataType == DataBuffer::Float32
        || dataType == DataBuffer::Float64
        || dataType == DataBuffer::Int8
        || dataType == DataBuffer::Int32
        || dataType == DataBuffer::Int64;
}

QString vectorSubsetModeLabel(VectorSubsetMode mode)
{
    switch(mode) {
    case VectorSubsetMode::AllElements:
        return QStringLiteral("All elements");
    case VectorSubsetMode::SelectedAtTimeOrigin:
        return QStringLiteral("Selected at time origin");
    case VectorSubsetMode::SelectedAtBothTimes:
        return QStringLiteral("Selected at both times");
    }
    OVITO_ASSERT(false);
    return {};
}

QString descriptorModeLabel(DescriptorMode mode)
{
    switch(mode) {
    case DescriptorMode::DipoleVector:
        return QStringLiteral("Dipole vector");
    case DescriptorMode::AtomTypeCentroidVector:
        return QStringLiteral("Atom-type centroid vector");
    case DescriptorMode::MatchingPairVector:
        return QStringLiteral("Matching pair vector");
    }
    OVITO_ASSERT(false);
    return {};
}

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

void appendVectorSample(const PropertyContainerReference& propertyContainer,
                        const PropertyReference& property,
                        VectorSubsetMode selectionMode,
                        VectorAccumulator& accumulator,
                        const PipelineFlowState& sampleState,
                        const QString& analysisLabel)
{
    if(!propertyContainer)
        throw Exception(QStringLiteral("No input property container selected for %1.").arg(analysisLabel.toLower()));
    if(!property)
        throw Exception(QStringLiteral("No input vector property selected for %1.").arg(analysisLabel.toLower()));
    if(!property.componentName().isEmpty()) {
        throw Exception(QStringLiteral("%1 expects a full vector property, not an individual component.").arg(analysisLabel));
    }

    const PropertyContainer* sampleContainer = sampleState.getLeafObject(propertyContainer);
    if(!sampleContainer) {
        throw Exception(QStringLiteral(
            "The selected property container '%1' is not available in one of the sampled trajectory frames.")
            .arg(propertyContainer.dataTitleOrPath()));
    }
    sampleContainer->verifyIntegrity();

    const Property* sampleProperty = property.findInContainer(sampleContainer);
    if(!sampleProperty) {
        throw Exception(QStringLiteral(
            "Property '%1' is not available in one of the sampled trajectory frames.")
            .arg(property.nameWithComponent()));
    }
    if(!isNumericDataType(sampleProperty->dataType()) || sampleProperty->isTypedProperty()
       || sampleProperty->typeId() == Property::GenericIdentifierProperty || sampleProperty->componentCount() < 2) {
        throw Exception(QStringLiteral(
            "Property '%1' is not a supported vector property for %2.")
            .arg(sampleProperty->name())
            .arg(analysisLabel.toLower()));
    }

    const Property* selectionProperty = nullptr;
    if(selectionMode != VectorSubsetMode::AllElements) {
        if(!sampleContainer->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
            throw Exception(QStringLiteral(
                "The selected property container '%1' does not provide a Selection property needed for the chosen subset mode.")
                .arg(propertyContainer.dataTitleOrPath()));
        selectionProperty = sampleContainer->getProperty(Property::GenericSelectionProperty);
        if(!selectionProperty)
            throw Exception(QStringLiteral("The Selection property is missing in one of the sampled trajectory frames."));
    }

    if(accumulator.frames.empty()) {
        accumulator.targetLabel = property.nameWithComponent();
        accumulator.elementDescriptionName = sampleContainer->getOOMetaClass().elementDescriptionName();
        accumulator.itemCount = sampleProperty->size();
        accumulator.componentCount = static_cast<int>(sampleProperty->componentCount());
        if(BufferReadAccess<IdentifierIntType> referenceIds(identifierProperty(sampleContainer)); referenceIds)
            accumulator.referenceIds.assign(referenceIds.cbegin(), referenceIds.cend());
    }
    else if(sampleProperty->componentCount() != static_cast<size_t>(accumulator.componentCount)) {
        throw Exception(QStringLiteral(
            "Property '%1' changes its vector dimensionality over time and cannot be used for %2.")
            .arg(sampleProperty->name())
            .arg(analysisLabel.toLower()));
    }

    BufferReadAccess<IdentifierIntType> sampleIds(identifierProperty(sampleContainer));
    if(!accumulator.referenceIds.empty()) {
        if(!sampleIds) {
            throw Exception(QStringLiteral(
                "The identifier property of %1 is missing in one of the sampled trajectory frames. "
                "%2 requires stable element IDs when the element order changes.")
                .arg(accumulator.elementDescriptionName)
                .arg(analysisLabel));
        }

        if(identifierBuffersMatch(accumulator.referenceIds, sampleIds)) {
            accumulator.frames.push_back(extractPropertyValues(sampleProperty));
            if(selectionProperty)
                accumulator.selectionFrames.push_back(extractBooleanValues(selectionProperty));
        }
        else {
            const std::vector<size_t> mapping = buildIndexMapping(
                accumulator.referenceIds,
                sampleIds,
                accumulator.elementDescriptionName,
                QStringLiteral("reference"),
                QStringLiteral("sampled"),
                analysisLabel);
            accumulator.frames.push_back(extractPropertyValues(sampleProperty, &mapping));
            if(selectionProperty)
                accumulator.selectionFrames.push_back(extractBooleanValues(selectionProperty, &mapping));
        }
    }
    else {
        if(sampleProperty->size() != accumulator.itemCount) {
            throw Exception(QStringLiteral(
                "Property '%1' changes the number of %2 over time. %3 requires a stable element count or a stable identifier property.")
                .arg(sampleProperty->name())
                .arg(accumulator.elementDescriptionName)
                .arg(analysisLabel));
        }
        accumulator.frames.push_back(extractPropertyValues(sampleProperty));
        if(selectionProperty)
            accumulator.selectionFrames.push_back(extractBooleanValues(selectionProperty));
    }
}

void appendDescriptorVectorSample(const ReferenceShellDescriptorRequest& request,
                                  VectorAccumulator& accumulator,
                                  const PipelineFlowState& sampleState,
                                  const QString& analysisLabel)
{
    const Particles* particles = sampleState.expectObject<Particles>();
    const std::vector<DescriptorRecord> records = buildDescriptorRecords(request, sampleState, particles, analysisLabel);

    std::vector<IdentifierIntType> sampleIds;
    sampleIds.reserve(records.size());
    std::vector<double> values(records.size() * 3, 0.0);
    std::vector<uint8_t> selectionValues(records.size(), 0);
    for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
        const DescriptorRecord& record = records[recordIndex];
        sampleIds.push_back(record.identifier);
        values[recordIndex * 3 + 0] = static_cast<double>(record.vector.x());
        values[recordIndex * 3 + 1] = static_cast<double>(record.vector.y());
        values[recordIndex * 3 + 2] = static_cast<double>(record.vector.z());
        selectionValues[recordIndex] = record.inReferenceShell ? 1 : 0;
    }

    if(accumulator.frames.empty()) {
        accumulator.targetLabel = descriptorModeLabel(request.descriptorMode);
        accumulator.elementDescriptionName = QStringLiteral("descriptor entries");
        accumulator.itemCount = records.size();
        accumulator.componentCount = 3;
        accumulator.referenceIds = std::move(sampleIds);
        accumulator.frames.push_back(std::move(values));
        accumulator.selectionFrames.push_back(std::move(selectionValues));
        return;
    }

    if(identifierVectorsMatch(accumulator.referenceIds, sampleIds)) {
        accumulator.frames.push_back(std::move(values));
        accumulator.selectionFrames.push_back(std::move(selectionValues));
        return;
    }

    const std::vector<size_t> mapping = buildIndexMapping(
        accumulator.referenceIds, sampleIds,
        accumulator.elementDescriptionName,
        QStringLiteral("reference"),
        QStringLiteral("sampled"),
        analysisLabel);

    std::vector<double> reorderedValues(accumulator.referenceIds.size() * 3, 0.0);
    std::vector<uint8_t> reorderedSelection(accumulator.referenceIds.size(), 0);
    for(size_t dst = 0; dst < mapping.size(); ++dst) {
        const size_t src = mapping[dst];
        reorderedValues[dst * 3 + 0] = values[src * 3 + 0];
        reorderedValues[dst * 3 + 1] = values[src * 3 + 1];
        reorderedValues[dst * 3 + 2] = values[src * 3 + 2];
        reorderedSelection[dst] = selectionValues[src];
    }

    accumulator.frames.push_back(std::move(reorderedValues));
    accumulator.selectionFrames.push_back(std::move(reorderedSelection));
}

void appendMembershipSample(const PropertyContainerReference& propertyContainer,
                            const PropertyReference& property,
                            MembershipAccumulator& accumulator,
                            const PipelineFlowState& sampleState,
                            const QString& analysisLabel)
{
    if(!propertyContainer)
        throw Exception(QStringLiteral("No input property container selected for %1.").arg(analysisLabel.toLower()));
    if(!property)
        throw Exception(QStringLiteral("No input membership property selected for %1.").arg(analysisLabel.toLower()));
    if(!property.componentName().isEmpty()) {
        throw Exception(QStringLiteral("%1 expects a scalar membership property, not an individual component.").arg(analysisLabel));
    }

    const PropertyContainer* sampleContainer = sampleState.getLeafObject(propertyContainer);
    if(!sampleContainer) {
        throw Exception(QStringLiteral(
            "The selected property container '%1' is not available in one of the sampled trajectory frames.")
            .arg(propertyContainer.dataTitleOrPath()));
    }
    sampleContainer->verifyIntegrity();

    const Property* sampleProperty = property.findInContainer(sampleContainer);
    if(!sampleProperty) {
        throw Exception(QStringLiteral(
            "Property '%1' is not available in one of the sampled trajectory frames.")
            .arg(property.nameWithComponent()));
    }
    if(!isNumericDataType(sampleProperty->dataType()) || sampleProperty->isTypedProperty()
       || sampleProperty->typeId() == Property::GenericIdentifierProperty || sampleProperty->componentCount() != 1) {
        throw Exception(QStringLiteral(
            "Property '%1' is not a supported scalar membership property for %2.")
            .arg(sampleProperty->name())
            .arg(analysisLabel.toLower()));
    }

    if(accumulator.membershipFrames.empty()) {
        accumulator.targetLabel = property.nameWithComponent();
        accumulator.elementDescriptionName = sampleContainer->getOOMetaClass().elementDescriptionName();
        accumulator.itemCount = sampleProperty->size();
        if(BufferReadAccess<IdentifierIntType> referenceIds(identifierProperty(sampleContainer)); referenceIds)
            accumulator.referenceIds.assign(referenceIds.cbegin(), referenceIds.cend());
    }

    BufferReadAccess<IdentifierIntType> sampleIds(identifierProperty(sampleContainer));
    if(!accumulator.referenceIds.empty()) {
        if(!sampleIds) {
            throw Exception(QStringLiteral(
                "The identifier property of %1 is missing in one of the sampled trajectory frames. "
                "%2 requires stable element IDs when the element order changes.")
                .arg(accumulator.elementDescriptionName)
                .arg(analysisLabel));
        }

        if(identifierBuffersMatch(accumulator.referenceIds, sampleIds)) {
            accumulator.membershipFrames.push_back(extractBooleanValues(sampleProperty));
        }
        else {
            const std::vector<size_t> mapping = buildIndexMapping(
                accumulator.referenceIds,
                sampleIds,
                accumulator.elementDescriptionName,
                QStringLiteral("reference"),
                QStringLiteral("sampled"),
                analysisLabel);
            accumulator.membershipFrames.push_back(extractBooleanValues(sampleProperty, &mapping));
        }
    }
    else {
        if(sampleProperty->size() != accumulator.itemCount) {
            throw Exception(QStringLiteral(
                "Property '%1' changes the number of %2 over time. %3 requires a stable element count or a stable identifier property.")
                .arg(sampleProperty->name())
                .arg(accumulator.elementDescriptionName)
                .arg(analysisLabel));
        }
        accumulator.membershipFrames.push_back(extractBooleanValues(sampleProperty));
    }
}

void appendReferenceShellMembershipSample(const ReferenceShellMembershipRequest& request,
                                          MembershipAccumulator& accumulator,
                                          const PipelineFlowState& sampleState,
                                          const QString& analysisLabel)
{
    const Particles* particles = sampleState.expectObject<Particles>();
    const std::vector<MembershipRecord> records = buildMembershipRecords(request, sampleState, particles, analysisLabel);

    std::vector<IdentifierIntType> sampleIds;
    sampleIds.reserve(records.size());
    std::vector<uint8_t> membershipValues(records.size(), 0);
    for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
        const MembershipRecord& record = records[recordIndex];
        sampleIds.push_back(record.identifier);
        membershipValues[recordIndex] = record.inReferenceShell ? 1 : 0;
    }

    if(accumulator.membershipFrames.empty()) {
        accumulator.targetLabel = QStringLiteral("Reference-shell membership");
        accumulator.elementDescriptionName = QStringLiteral("molecules");
        accumulator.itemCount = records.size();
        accumulator.referenceIds = std::move(sampleIds);
        accumulator.membershipFrames.push_back(std::move(membershipValues));
        return;
    }

    if(identifierVectorsMatch(accumulator.referenceIds, sampleIds)) {
        accumulator.membershipFrames.push_back(std::move(membershipValues));
        return;
    }

    const std::vector<size_t> mapping = buildIndexMapping(
        accumulator.referenceIds, sampleIds,
        accumulator.elementDescriptionName,
        QStringLiteral("reference"),
        QStringLiteral("sampled"),
        analysisLabel);

    std::vector<uint8_t> reorderedMembership(accumulator.referenceIds.size(), 0);
    for(size_t dst = 0; dst < mapping.size(); ++dst)
        reorderedMembership[dst] = membershipValues[mapping[dst]];

    accumulator.membershipFrames.push_back(std::move(reorderedMembership));
}

CorrelationCurves computeVectorReorientationCurves(const VectorAccumulator& samples,
                                                   const std::vector<int>& sampledFrameNumbers,
                                                   int legendreOrder,
                                                   VectorSubsetMode selectionMode,
                                                   int maxLag,
                                                   const QString& analysisLabel)
{
    OVITO_ASSERT(samples.frames.size() == sampledFrameNumbers.size());
    OVITO_ASSERT(samples.selectionFrames.empty() || samples.selectionFrames.size() == sampledFrameNumbers.size());

    CorrelationCurves curves;
    const size_t frameCount = samples.frames.size();
    if(frameCount < 2)
        throw Exception(QStringLiteral("%1 requires at least two sampled trajectory frames.").arg(analysisLabel));

    const size_t componentCount = static_cast<size_t>(samples.componentCount);
    const size_t maxLagEffective = std::min<size_t>((maxLag > 0 ? static_cast<size_t>(maxLag) : frameCount - 1), frameCount - 1);

    curves.lagFrames.assign(maxLagEffective + 1, 0.0);
    curves.overall.assign(maxLagEffective + 1, std::numeric_limits<double>::quiet_NaN());

    parallelForChunks(maxLagEffective + 1, 8, [&](size_t, size_t fromLag, size_t toLag) {
        for(size_t lag = fromLag; lag < toLag; ++lag) {
            this_task::throwIfCanceled();
            const size_t originCount = frameCount - lag;
            double lagFrameAccumulator = 0.0;
            double overallAccumulator = 0.0;
            size_t sampleCount = 0;

            for(size_t origin = 0; origin < originCount; ++origin) {
                const std::vector<double>& frame0 = samples.frames[origin];
                const std::vector<double>& frame1 = samples.frames[origin + lag];
                const std::vector<uint8_t>* selection0 = samples.selectionFrames.empty() ? nullptr : &samples.selectionFrames[origin];
                const std::vector<uint8_t>* selection1 = samples.selectionFrames.empty() ? nullptr : &samples.selectionFrames[origin + lag];
                lagFrameAccumulator += static_cast<double>(sampledFrameNumbers[origin + lag] - sampledFrameNumbers[origin]);

                for(size_t item = 0; item < samples.itemCount; ++item) {
                    if(selectionMode != VectorSubsetMode::AllElements) {
                        const bool originSelected = selection0 && (*selection0)[item] != 0;
                        const bool lagSelected = selection1 && (*selection1)[item] != 0;
                        if(selectionMode == VectorSubsetMode::SelectedAtTimeOrigin && !originSelected)
                            continue;
                        if(selectionMode == VectorSubsetMode::SelectedAtBothTimes && (!originSelected || !lagSelected))
                            continue;
                    }

                    double norm0 = 0.0;
                    double norm1 = 0.0;
                    double dot = 0.0;
                    for(size_t c = 0; c < componentCount; ++c) {
                        const double a = frame0[item * componentCount + c];
                        const double b = frame1[item * componentCount + c];
                        dot += a * b;
                        norm0 += a * a;
                        norm1 += b * b;
                    }
                    if(norm0 <= std::numeric_limits<double>::epsilon() || norm1 <= std::numeric_limits<double>::epsilon())
                        continue;

                    overallAccumulator += legendrePolynomial(legendreOrder, dot / std::sqrt(norm0 * norm1));
                    sampleCount++;
                }
            }

            curves.lagFrames[lag] = lagFrameAccumulator / static_cast<double>(originCount);
            if(sampleCount > 0)
                curves.overall[lag] = overallAccumulator / static_cast<double>(sampleCount);
        }
    });

    return curves;
}

CorrelationCurves computeSurvivalProbabilityCurves(const MembershipAccumulator& samples,
                                                   const std::vector<int>& sampledFrameNumbers,
                                                   int intermittency,
                                                   int maxLag,
                                                   const QString& analysisLabel)
{
    OVITO_ASSERT(samples.membershipFrames.size() == sampledFrameNumbers.size());

    CorrelationCurves curves;
    const size_t frameCount = samples.membershipFrames.size();
    if(frameCount < 2)
        throw Exception(QStringLiteral("%1 requires at least two sampled trajectory frames.").arg(analysisLabel));

    const size_t maxLagEffective = std::min<size_t>((maxLag > 0 ? static_cast<size_t>(maxLag) : frameCount - 1), frameCount - 1);
    curves.lagFrames.assign(maxLagEffective + 1, 0.0);
    curves.overall.assign(maxLagEffective + 1, std::numeric_limits<double>::quiet_NaN());

    const std::vector<std::vector<uint8_t>> memberships = applyIntermittencyFilter(samples.membershipFrames, intermittency);
    const size_t itemCount = samples.itemCount;

    std::vector<std::vector<int>> falsePrefix(itemCount, std::vector<int>(frameCount + 1, 0));
    for(size_t item = 0; item < itemCount; ++item) {
        for(size_t frame = 0; frame < frameCount; ++frame)
            falsePrefix[item][frame + 1] = falsePrefix[item][frame] + (memberships[frame][item] == 0 ? 1 : 0);
    }

    parallelForChunks(maxLagEffective + 1, 8, [&](size_t, size_t fromLag, size_t toLag) {
        for(size_t lag = fromLag; lag < toLag; ++lag) {
            this_task::throwIfCanceled();
            const size_t originCount = frameCount - lag;
            double lagFrameAccumulator = 0.0;
            double originFractionSum = 0.0;
            size_t validOriginCount = 0;

            for(size_t origin = 0; origin < originCount; ++origin) {
                lagFrameAccumulator += static_cast<double>(sampledFrameNumbers[origin + lag] - sampledFrameNumbers[origin]);
                size_t presentCount = 0;
                size_t survivingCount = 0;
                for(size_t item = 0; item < itemCount; ++item) {
                    if(memberships[origin][item] == 0)
                        continue;
                    presentCount++;
                    if(falsePrefix[item][origin + lag + 1] - falsePrefix[item][origin] == 0)
                        survivingCount++;
                }
                if(presentCount > 0) {
                    originFractionSum += static_cast<double>(survivingCount) / static_cast<double>(presentCount);
                    validOriginCount++;
                }
            }

            curves.lagFrames[lag] = lagFrameAccumulator / static_cast<double>(originCount);
            if(validOriginCount > 0)
                curves.overall[lag] = originFractionSum / static_cast<double>(validOriginCount);
        }
    });

    return curves;
}

DataTable* createLineTable(DataCollection* collection,
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

}  // namespace Ovito::OrientationTrajectoryAnalysis
