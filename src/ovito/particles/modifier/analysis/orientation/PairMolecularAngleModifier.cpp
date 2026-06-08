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
#include "PairMolecularAngleModifier.h"

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
    size_t groupIndex = 0;
    std::vector<size_t> indices;
    bool anySelected = false;
};

struct MoleculeGeometry
{
    IdentifierIntType moleculeId = 0;
    size_t groupIndex = 0;
    std::vector<size_t> indices;
    std::vector<Point3> positions;
    bool anySelected = false;
    bool hasSite1 = false;
    bool hasSite2 = false;
    Point3 site1 = Point3::Origin();
    Point3 site2 = Point3::Origin();
};

struct PairSite
{
    size_t geometryIndex = 0;
    IdentifierIntType moleculeId = 0;
    Point3 position = Point3::Origin();
};

struct PairHit
{
    FloatType distanceSquared = 0;
    Vector3 site1ToSite2 = Vector3::Zero();
};

struct EndpointRequest
{
    PairMolecularAngleModifier::EndpointMolecule molecule = PairMolecularAngleModifier::Molecule1;
    std::vector<uint8_t> mask;
};

struct EndpointPosition
{
    size_t particleIndex = 0;
    Point3 position = Point3::Origin();
};

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
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
            molecules.back().groupIndex = molecules.size() - 1;
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
        Vector3 delta = positions[group.indices[atomListIndex]] - referencePosition;
        if(cell)
            delta = cell->wrapVector(delta);
        wrappedPositions.push_back(referencePosition + delta);
    }
}

std::optional<Point3> centroidFromMask(const MoleculeGroup& group,
                                       const std::vector<Point3>& moleculePositions,
                                       const std::vector<uint8_t>& mask,
                                       const BufferReadAccess<FloatType>& masses)
{
    if(group.indices.empty())
        return {};

    const Point3 referencePosition = moleculePositions.front();
    Vector3 weightedOffsetSum = Vector3::Zero();
    FloatType massSum = FloatType(0);
    size_t matchedCount = 0;

    for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
        const size_t particleIndex = group.indices[atomListIndex];
        if(!mask[particleIndex])
            continue;

        FloatType mass = FloatType(1);
        if(masses && masses[particleIndex] > FloatType(0))
            mass = masses[particleIndex];
        weightedOffsetSum += mass * (moleculePositions[atomListIndex] - referencePosition);
        massSum += mass;
        matchedCount++;
    }

    if(matchedCount == 0 || massSum <= FloatType(0))
        return {};

    return referencePosition + weightedOffsetSum / massSum;
}

std::vector<EndpointPosition> collectEndpointPositions(const MoleculeGeometry& molecule1,
                                                       const MoleculeGeometry& molecule2,
                                                       const Vector3& molecule2Shift,
                                                       const EndpointRequest& request)
{
    const MoleculeGeometry& geometry =
        request.molecule == PairMolecularAngleModifier::Molecule1 ? molecule1 : molecule2;
    const Vector3 shift =
        request.molecule == PairMolecularAngleModifier::Molecule1 ? Vector3::Zero() : molecule2Shift;

    std::vector<EndpointPosition> endpoints;
    endpoints.reserve(geometry.indices.size());
    for(size_t atomListIndex = 0; atomListIndex < geometry.indices.size(); ++atomListIndex) {
        const size_t particleIndex = geometry.indices[atomListIndex];
        if(!request.mask[particleIndex])
            continue;
        endpoints.push_back(EndpointPosition{particleIndex, geometry.positions[atomListIndex] + shift});
    }
    return endpoints;
}

std::vector<Vector3> buildVectors(const MoleculeGeometry& molecule1,
                                  const MoleculeGeometry& molecule2,
                                  const Vector3& molecule2Shift,
                                  const EndpointRequest& startRequest,
                                  const EndpointRequest& endRequest)
{
    const std::vector<EndpointPosition> startPositions =
        collectEndpointPositions(molecule1, molecule2, molecule2Shift, startRequest);
    const std::vector<EndpointPosition> endPositions =
        collectEndpointPositions(molecule1, molecule2, molecule2Shift, endRequest);

    std::vector<Vector3> vectors;
    vectors.reserve(startPositions.size() * endPositions.size());
    for(const EndpointPosition& start : startPositions) {
        for(const EndpointPosition& end : endPositions) {
            if(start.particleIndex == end.particleIndex)
                continue;
            const Vector3 vector = end.position - start.position;
            if(vector.length() <= FloatType(0))
                continue;
            vectors.push_back(vector / vector.length());
        }
    }
    return vectors;
}

QString endpointMoleculeLabel(PairMolecularAngleModifier::EndpointMolecule molecule)
{
    return molecule == PairMolecularAngleModifier::Molecule1
        ? PairMolecularAngleModifier::tr("mol1")
        : PairMolecularAngleModifier::tr("mol2");
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(PairMolecularAngleModifier);
OVITO_CLASSINFO(PairMolecularAngleModifier, "DisplayName", "Pair molecular angular distribution");
OVITO_CLASSINFO(PairMolecularAngleModifier, "Description",
                "Measure the angular probability density between two user-defined vectors spanning molecule pairs.");
OVITO_CLASSINFO(PairMolecularAngleModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, molecule1SiteTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, molecule1SiteExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, molecule2SiteTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, molecule2SiteExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, cutoff);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1StartMolecule);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1StartTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1StartExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1EndMolecule);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1EndTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction1EndExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2StartMolecule);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2StartTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2StartExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2EndMolecule);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2EndTypes);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, direction2EndExpression);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, angleSamplingMode);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, pairRoleMode);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(PairMolecularAngleModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, molecule1SiteTypes, "Molecule 1 pair site atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, molecule1SiteExpression, "Molecule 1 pair site expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, molecule2SiteTypes, "Molecule 2 pair site atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, molecule2SiteExpression, "Molecule 2 pair site expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, cutoff, "Pair-site cutoff");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1StartMolecule, "Direction 1 start molecule");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1StartTypes, "Direction 1 start atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1StartExpression, "Direction 1 start expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1EndMolecule, "Direction 1 end molecule");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1EndTypes, "Direction 1 end atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction1EndExpression, "Direction 1 end expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2StartMolecule, "Direction 2 start molecule");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2StartTypes, "Direction 2 start atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2StartExpression, "Direction 2 start expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2EndMolecule, "Direction 2 end molecule");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2EndTypes, "Direction 2 end atom type(s)");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, direction2EndExpression, "Direction 2 end expression");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, angleSamplingMode, "Angle sampling");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, pairRoleMode, "Pair roles");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, numberOfBins, "Angle histogram bins");
SET_PROPERTY_FIELD_LABEL(PairMolecularAngleModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PairMolecularAngleModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(PairMolecularAngleModifier, numberOfBins, IntegerParameterUnit, 4);

bool PairMolecularAngleModifier::PairMolecularAngleModifierClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

Future<PipelineFlowState> PairMolecularAngleModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(tr("Pair molecular angular distribution requires the particle property 'Molecule Identifier'. Load molecular topology first."));

    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    if(!particleTypes)
        throw Exception(tr("Pair molecular angular distribution requires the particle property 'Particle Type'."));
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
        throw Exception(tr("Pair molecular angular distribution requires a typed 'Particle Type' property with defined element types."));

    BufferReadAccess<FloatType> masses = particles->getProperty(Particles::MassProperty);
    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection)
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection."));

    const FloatType cutoffRadius = cutoff();
    if(cutoffRadius <= 0)
        throw Exception(tr("The pair-site cutoff must be positive."));

    const int histogramBinCount = std::max(numberOfBins(), 4);
    DataTable* table = state.createObject<DataTable>(QString(TableIdentifier), request.modificationNode(), DataTable::Histogram,
                                                     tr("Pair molecular angular distribution"));
    table->setAxisLabelX(tr("Angle (degrees)"));
    table->setAxisLabelY(tr("Probability density"));
    table->setIntervalStart(FloatType(0));
    table->setIntervalEnd(FloatType(180));

    const SimulationCell* cell = state.getObject<SimulationCell>();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            moleculeIds = std::move(moleculeIds),
            particleTypes = std::move(particleTypes),
            masses = std::move(masses),
            selection = std::move(selection),
            particleCount = particles->elementCount(),
            cell,
            molecule1SiteTypes = molecule1SiteTypes(),
            molecule1SiteExpression = molecule1SiteExpression(),
            molecule2SiteTypes = molecule2SiteTypes(),
            molecule2SiteExpression = molecule2SiteExpression(),
            cutoffRadius,
            direction1StartMolecule = direction1StartMolecule(),
            direction1StartTypes = direction1StartTypes(),
            direction1StartExpression = direction1StartExpression(),
            direction1EndMolecule = direction1EndMolecule(),
            direction1EndTypes = direction1EndTypes(),
            direction1EndExpression = direction1EndExpression(),
            direction2StartMolecule = direction2StartMolecule(),
            direction2StartTypes = direction2StartTypes(),
            direction2StartExpression = direction2StartExpression(),
            direction2EndMolecule = direction2EndMolecule(),
            direction2EndTypes = direction2EndTypes(),
            direction2EndExpression = direction2EndExpression(),
            angleSamplingMode = angleSamplingMode(),
            pairRoleMode = pairRoleMode(),
            selectedOnly = onlySelectedParticles(),
            histogramBinCount,
            table,
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        Q_UNUSED(createdByNode);
        Q_UNUSED(particleCount);

        const Particles* particles = state.expectObject<Particles>();
        const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);

        size_t molecule1SiteMatchCount = 0;
        const std::vector<uint8_t> molecule1SiteMask = evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            molecule1SiteTypes, molecule1SiteExpression,
            tr("molecule 1 pair site selector"),
            tr("Pair molecular angular distribution"),
            &molecule1SiteMatchCount);
        size_t molecule2SiteMatchCount = 0;
        const std::vector<uint8_t> molecule2SiteMask = evaluateParticleSelector(
            state, particles, particleTypeProperty, particleTypes,
            molecule2SiteTypes, molecule2SiteExpression,
            tr("molecule 2 pair site selector"),
            tr("Pair molecular angular distribution"),
            &molecule2SiteMatchCount);
        if(molecule1SiteMatchCount == 0)
            throw Exception(tr("No particles matched the molecule 1 pair site selector."));
        if(molecule2SiteMatchCount == 0)
            throw Exception(tr("No particles matched the molecule 2 pair site selector."));

        auto evaluateEndpointMask = [&](const QString& types, const QString& expression, const QString& role) {
            size_t matchCount = 0;
            std::vector<uint8_t> mask = evaluateParticleSelector(
                state, particles, particleTypeProperty, particleTypes,
                types, expression,
                role,
                tr("Pair molecular angular distribution"),
                &matchCount);
            if(matchCount == 0)
                throw Exception(tr("No particles matched the %1.").arg(role));
            return mask;
        };

        const EndpointRequest direction1StartRequest{
            direction1StartMolecule,
            evaluateEndpointMask(direction1StartTypes, direction1StartExpression, tr("direction 1 start selector"))};
        const EndpointRequest direction1EndRequest{
            direction1EndMolecule,
            evaluateEndpointMask(direction1EndTypes, direction1EndExpression, tr("direction 1 end selector"))};
        const EndpointRequest direction2StartRequest{
            direction2StartMolecule,
            evaluateEndpointMask(direction2StartTypes, direction2StartExpression, tr("direction 2 start selector"))};
        const EndpointRequest direction2EndRequest{
            direction2EndMolecule,
            evaluateEndpointMask(direction2EndTypes, direction2EndExpression, tr("direction 2 end selector"))};

        const std::vector<MoleculeGroup> moleculeGroups = buildMoleculeGroups(moleculeIds, selection);
        std::vector<MoleculeGeometry> geometries;
        geometries.reserve(moleculeGroups.size());
        std::vector<Point3> wrappedPositions;
        size_t molecule1SiteMoleculeCount = 0;
        size_t molecule2SiteMoleculeCount = 0;
        for(const MoleculeGroup& group : moleculeGroups) {
            this_task::throwIfCanceled();
            if(group.indices.empty())
                continue;

            buildWrappedMoleculePositions(positions, cell, group, wrappedPositions);
            MoleculeGeometry geometry;
            geometry.moleculeId = group.moleculeId;
            geometry.groupIndex = group.groupIndex;
            geometry.indices = group.indices;
            geometry.positions = wrappedPositions;
            geometry.anySelected = group.anySelected;

            if(const std::optional<Point3> site1 = centroidFromMask(group, geometry.positions, molecule1SiteMask, masses)) {
                geometry.hasSite1 = true;
                geometry.site1 = *site1;
                molecule1SiteMoleculeCount++;
            }
            if(const std::optional<Point3> site2 = centroidFromMask(group, geometry.positions, molecule2SiteMask, masses)) {
                geometry.hasSite2 = true;
                geometry.site2 = *site2;
                molecule2SiteMoleculeCount++;
            }

            geometries.push_back(std::move(geometry));
        }
        if(molecule1SiteMoleculeCount == 0)
            throw Exception(tr("No molecule contained the requested molecule 1 pair site atoms."));
        if(molecule2SiteMoleculeCount == 0)
            throw Exception(tr("No molecule contained the requested molecule 2 pair site atoms."));

        std::vector<PairSite> molecule2Sites;
        molecule2Sites.reserve(molecule2SiteMoleculeCount);
        for(size_t geometryIndex = 0; geometryIndex < geometries.size(); ++geometryIndex) {
            if(!geometries[geometryIndex].hasSite2)
                continue;
            molecule2Sites.push_back(PairSite{geometryIndex, geometries[geometryIndex].moleculeId, geometries[geometryIndex].site2});
        }

        PropertyPtr molecule2SitePositionsProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, molecule2Sites.size(), Particles::PositionProperty);
        {
            BufferWriteAccess<Point3, access_mode::discard_write> sitePositions(molecule2SitePositionsProperty);
            for(size_t siteIndex = 0; siteIndex < molecule2Sites.size(); ++siteIndex)
                sitePositions[siteIndex] = molecule2Sites[siteIndex].position;
        }
        PropertyPtr molecule2SiteSelectionProperty = createSelectionPropertyFromMask(std::vector<uint8_t>(molecule2Sites.size(), 1));
        BufferReadAccess<Point3> molecule2SitePositions(molecule2SitePositionsProperty);
        BufferReadAccess<SelectionIntType> molecule2SiteSelection(molecule2SiteSelectionProperty);
        const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(molecule2SitePositions, false, cutoffRadius / 2);
        CutoffNeighborFinder neighborFinder(cutoffRadius, molecule2SitePositions, cellData, molecule2SiteSelection);

        std::vector<int64_t> histogram(histogramBinCount, 0);
        const FloatType angleRangeStart = FloatType(0);
        const FloatType angleRangeEnd = FloatType(180);
        const FloatType binSize = (angleRangeEnd - angleRangeStart) / histogramBinCount;

        size_t candidatePairCount = 0;
        size_t sampledPairCount = 0;
        size_t angleSampleCount = 0;
        size_t skippedSameMoleculeCount = 0;
        size_t skippedSelectionCount = 0;
        size_t skippedUniquePairCount = 0;
        size_t missingVectorCount = 0;

        auto addAngleToHistogram = [&](FloatType angleDegrees) {
            size_t binIndex = static_cast<size_t>((angleDegrees - angleRangeStart) / binSize);
            if(binIndex >= static_cast<size_t>(histogramBinCount))
                binIndex = static_cast<size_t>(histogramBinCount - 1);
            histogram[binIndex]++;
            angleSampleCount++;
        };

        for(size_t geometry1Index = 0; geometry1Index < geometries.size(); ++geometry1Index) {
            this_task::throwIfCanceled();

            const MoleculeGeometry& molecule1 = geometries[geometry1Index];
            if(!molecule1.hasSite1)
                continue;

            std::unordered_map<size_t, PairHit> pairHits;
            for(CutoffNeighborFinder::Query neighborQuery(neighborFinder, molecule1.site1); !neighborQuery.atEnd(); neighborQuery.next()) {
                const size_t site2Index = neighborQuery.current();
                if(site2Index >= molecule2Sites.size())
                    continue;

                const FloatType distanceSquared = neighborQuery.distanceSquared();
                auto [iter, inserted] = pairHits.try_emplace(site2Index, PairHit{distanceSquared, neighborQuery.delta()});
                if(!inserted && distanceSquared < iter->second.distanceSquared) {
                    iter->second.distanceSquared = distanceSquared;
                    iter->second.site1ToSite2 = neighborQuery.delta();
                }
            }

            for(const auto& [site2Index, pairHit] : pairHits) {
                const PairSite& molecule2Site = molecule2Sites[site2Index];
                const MoleculeGeometry& molecule2 = geometries[molecule2Site.geometryIndex];
                if(molecule1.groupIndex == molecule2.groupIndex || molecule1.moleculeId == molecule2.moleculeId) {
                    skippedSameMoleculeCount++;
                    continue;
                }

                if(pairRoleMode == UniqueMoleculePairs && molecule1.groupIndex > molecule2.groupIndex) {
                    skippedUniquePairCount++;
                    continue;
                }

                candidatePairCount++;

                if(selectedOnly && !(molecule1.anySelected || molecule2.anySelected)) {
                    skippedSelectionCount++;
                    continue;
                }

                const Vector3 site1ToNearestSite2 = pairHit.site1ToSite2;
                const Point3 nearestSite2 = molecule1.site1 + site1ToNearestSite2;
                const Vector3 molecule2Shift = nearestSite2 - molecule2.site2;

                const std::vector<Vector3> direction1Vectors =
                    buildVectors(molecule1, molecule2, molecule2Shift, direction1StartRequest, direction1EndRequest);
                const std::vector<Vector3> direction2Vectors =
                    buildVectors(molecule1, molecule2, molecule2Shift, direction2StartRequest, direction2EndRequest);
                if(direction1Vectors.empty() || direction2Vectors.empty()) {
                    missingVectorCount++;
                    continue;
                }

                sampledPairCount++;
                if(angleSamplingMode == SmallestAnglePerPair) {
                    FloatType smallestAngle = std::numeric_limits<FloatType>::max();
                    for(const Vector3& direction1 : direction1Vectors) {
                        for(const Vector3& direction2 : direction2Vectors) {
                            const FloatType angleDegrees = qRadiansToDegrees(clampedAcos(direction1.dot(direction2)));
                            smallestAngle = std::min(smallestAngle, angleDegrees);
                        }
                    }
                    if(smallestAngle != std::numeric_limits<FloatType>::max())
                        addAngleToHistogram(smallestAngle);
                }
                else {
                    for(const Vector3& direction1 : direction1Vectors) {
                        for(const Vector3& direction2 : direction2Vectors)
                            addAngleToHistogram(qRadiansToDegrees(clampedAcos(direction1.dot(direction2))));
                    }
                }
            }
        }

        table->setElementCount(histogramBinCount);
        Property* pdfValues = table->createProperty(DataBuffer::Initialized, QStringLiteral("PDF"), Property::FloatDefault);
        BufferWriteAccess<FloatType, access_mode::discard_write> pdf(pdfValues);
        for(int binIndex = 0; binIndex < histogramBinCount; ++binIndex) {
            if(angleSampleCount != 0)
                pdf[binIndex] = static_cast<FloatType>(histogram[binIndex]) / (static_cast<FloatType>(angleSampleCount) * binSize);
            else
                pdf[binIndex] = FloatType(0);
        }
        table->setY(pdfValues);

        state.setAttribute(QStringLiteral("PairMolecularAngle.candidate_pairs"),
                           QVariant::fromValue(static_cast<double>(candidatePairCount)), createdByNode);
        state.setAttribute(QStringLiteral("PairMolecularAngle.sampled_pairs"),
                           QVariant::fromValue(static_cast<double>(sampledPairCount)), createdByNode);
        state.setAttribute(QStringLiteral("PairMolecularAngle.angle_samples"),
                           QVariant::fromValue(static_cast<double>(angleSampleCount)), createdByNode);

        QString statusText = tr("Pair molecular angular distribution sampled %1 angle values from %2/%3 molecule pairs.")
                                 .arg(angleSampleCount)
                                 .arg(sampledPairCount)
                                 .arg(candidatePairCount);
        statusText += tr(" Direction 1: %1 -> %2; Direction 2: %3 -> %4.")
                          .arg(endpointMoleculeLabel(direction1StartMolecule),
                               endpointMoleculeLabel(direction1EndMolecule),
                               endpointMoleculeLabel(direction2StartMolecule),
                               endpointMoleculeLabel(direction2EndMolecule));
        if(angleSamplingMode == SmallestAnglePerPair)
            statusText += tr(" One smallest angle was used per molecule pair.");
        if(pairRoleMode == OrderedMoleculePairs)
            statusText += tr(" Molecule-pair roles are ordered.");
        else
            statusText += tr(" Each unordered molecule pair is counted once.");
        if(skippedSelectionCount > 0)
            statusText += tr(" %1 pairs were skipped by the selection filter.").arg(skippedSelectionCount);
        if(missingVectorCount > 0)
            statusText += tr(" %1 pairs lacked at least one requested direction vector.").arg(missingVectorCount);
        if(skippedSameMoleculeCount > 0)
            statusText += tr(" %1 same-molecule site hits were ignored.").arg(skippedSameMoleculeCount);
        if(skippedUniquePairCount > 0)
            statusText += tr(" %1 reverse-direction hits were ignored by unique-pair mode.").arg(skippedUniquePairCount);
        if(angleSampleCount == 0)
            statusText += tr(" No valid angle samples were found for the current settings.");

        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(angleSampleCount)));
        return std::move(state);
    });
}

}  // namespace Ovito
