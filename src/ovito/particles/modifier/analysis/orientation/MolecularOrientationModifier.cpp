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
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "MolecularOrientationModifier.h"

#include <QHash>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>
#include <limits>
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

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

QString directionModeLabel(MolecularOrientationModifier::DirectionMode directionMode)
{
    switch(directionMode) {
    case MolecularOrientationModifier::DipoleDirection:
        return MolecularOrientationModifier::tr("dipole direction");
    case MolecularOrientationModifier::ManualMolecularDirection:
        return MolecularOrientationModifier::tr("manual molecular direction");
    }
    OVITO_ASSERT(false);
    return {};
}

std::vector<int> parseAnchorTypeIds(const QString& anchorTypesText, const Property* typeProperty)
{
    if(!typeProperty || !typeProperty->isTypedProperty())
        throw Exception(MolecularOrientationModifier::tr("This analysis requires a typed 'Particle Type' property with defined element types."));

    const QString trimmedText = anchorTypesText.trimmed();
    if(trimmedText.isEmpty())
        throw Exception(MolecularOrientationModifier::tr("Please enter at least one anchor atom type."));

    QHash<QString, int> nameToId;
    for(const ElementType* type : typeProperty->elementTypes()) {
        if(!type->name().isEmpty())
            nameToId.insert(type->name(), type->numericId());
        nameToId.insert(type->nameOrNumericId(), type->numericId());
    }

    std::vector<int> anchorTypeIds;
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
            if(!ok || !typeProperty->elementType(typeId))
                throw Exception(MolecularOrientationModifier::tr("Unknown anchor atom type '%1'. Use particle type names or numeric IDs separated by commas.").arg(token));
        }

        if(std::find(anchorTypeIds.begin(), anchorTypeIds.end(), typeId) == anchorTypeIds.end())
            anchorTypeIds.push_back(typeId);
    }

    if(anchorTypeIds.empty())
        throw Exception(MolecularOrientationModifier::tr("Please enter at least one valid anchor atom type."));

    return anchorTypeIds;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(MolecularOrientationModifier);
OVITO_CLASSINFO(MolecularOrientationModifier, "DisplayName", "Molecular orientation around atoms");
OVITO_CLASSINFO(MolecularOrientationModifier, "Description",
                "Measure how molecular orientation vectors align relative to the nearest reference atom of a chosen type.");
OVITO_CLASSINFO(MolecularOrientationModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, directionMode);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, fromTypeId);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, toTypeId);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, referenceTypeId);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, anchorTypes);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, cutoff);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(MolecularOrientationModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, directionMode, "Direction mode");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, fromTypeId, "From atom type");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, toTypeId, "To atom type");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, referenceTypeId, "Reference atom type");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, anchorTypes, "Anchor atom types");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, cutoff, "Cutoff radius");
SET_PROPERTY_FIELD_LABEL(MolecularOrientationModifier, numberOfBins, "Number of bins");
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
        throw Exception(tr("The manual molecular direction mode requires two different particle types."));

    const std::vector<int> anchorTypeIds = parseAnchorTypeIds(anchorTypes(), particleTypeProperty);
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
    const int referenceParticleType = referenceTypeId();
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
            referenceParticleType,
            anchorTypeIds,
            cutoffRadius,
            histogramBinCount,
            selectedOnly,
            table]() mutable
    {
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

        PropertyPtr angleProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1,
                                                        QStringLiteral("Molecular Orientation Angle"));
        PropertyPtr distanceProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1,
                                                           QStringLiteral("Distance To Nearest Reference"));
        PropertyPtr overlapProperty = PropertyPtr::create(DataBuffer::Initialized, particleCount, Property::Int32, 1,
                                                          QStringLiteral("Reference Overlap Count"));

        BufferWriteAccess<FloatType, access_mode::discard_write> angleAcc(angleProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> distanceAcc(distanceProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> overlapAcc(overlapProperty);
        std::fill(angleAcc.begin(), angleAcc.end(), std::numeric_limits<FloatType>::quiet_NaN());
        std::fill(distanceAcc.begin(), distanceAcc.end(), std::numeric_limits<FloatType>::quiet_NaN());
        std::fill(overlapAcc.begin(), overlapAcc.end(), 0);

        PropertyPtr referenceSelectionProperty =
            Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particleCount, Particles::SelectionProperty);
        {
            BufferWriteAccess<SelectionIntType, access_mode::discard_write> referenceSelection(referenceSelectionProperty);
            for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
                referenceSelection[particleIndex] = (particleTypes[particleIndex] == referenceParticleType) ? 1 : 0;
        }

        const SimulationCellData cellData = cell ? SimulationCellData(cell) : SimulationCellData(positions, false, cutoffRadius / 2);
        BufferReadAccess<SelectionIntType> referenceSelectionRead(referenceSelectionProperty);
        CutoffNeighborFinder neighborFinder(cutoffRadius, positions, cellData, referenceSelectionRead);

        std::vector<int64_t> histogram(histogramBinCount, 0);
        const FloatType angleRangeStart = FloatType(0);
        const FloatType angleRangeEnd = FloatType(180);
        const FloatType binSize = (angleRangeEnd - angleRangeStart) / histogramBinCount;

        std::vector<Point3> moleculePositions;
        size_t candidateMoleculeCount = 0;
        size_t analyzedMoleculeCount = 0;
        size_t missingDirectionCount = 0;
        size_t missingAnchorCount = 0;
        size_t noReferenceCount = 0;
        size_t overlapMoleculeCount = 0;
        size_t zeroDirectionCount = 0;
        size_t zeroRadialCount = 0;

        for(const MoleculeGroup& group : molecules) {
            this_task::throwIfCanceled();

            if(group.indices.empty())
                continue;
            if(selectedOnly && !group.anySelected)
                continue;

            candidateMoleculeCount++;

            moleculePositions.clear();
            moleculePositions.reserve(group.indices.size());
            const Point3 reference = positions[group.indices.front()];
            moleculePositions.push_back(reference);
            for(size_t atomListIndex = 1; atomListIndex < group.indices.size(); ++atomListIndex) {
                const Point3 current = positions[group.indices[atomListIndex]];
                Vector3 delta = current - reference;
                if(cell)
                    delta = cell->wrapVector(delta);
                moleculePositions.push_back(reference + delta);
            }

            Vector3 centerOffsetSum = Vector3::Zero();
            for(const Point3& position : moleculePositions)
                centerOffsetSum += (position - reference);
            const Point3 moleculeCenter = reference + centerOffsetSum / static_cast<FloatType>(moleculePositions.size());

            Vector3 orientationVector = Vector3::Zero();
            switch(selectedDirectionMode) {
            case DipoleDirection:
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex)
                    orientationVector += charges[group.indices[atomListIndex]] * (moleculePositions[atomListIndex] - moleculeCenter);
                break;
            case ManualMolecularDirection: {
                Vector3 fromCentroidOffset = Vector3::Zero();
                Vector3 toCentroidOffset = Vector3::Zero();
                size_t fromCount = 0;
                size_t toCount = 0;
                for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                    const size_t particleIndex = group.indices[atomListIndex];
                    const Point3& position = moleculePositions[atomListIndex];
                    if(particleTypes[particleIndex] == fromParticleType) {
                        fromCentroidOffset += (position - reference);
                        fromCount++;
                    }
                    if(particleTypes[particleIndex] == toParticleType) {
                        toCentroidOffset += (position - reference);
                        toCount++;
                    }
                }
                if(fromCount == 0 || toCount == 0) {
                    missingDirectionCount++;
                    continue;
                }
                const Point3 fromCentroid = reference + fromCentroidOffset / static_cast<FloatType>(fromCount);
                const Point3 toCentroid = reference + toCentroidOffset / static_cast<FloatType>(toCount);
                orientationVector = toCentroid - fromCentroid;
                break;
            }
            }

            const FloatType directionMagnitude = orientationVector.length();
            if(directionMagnitude <= FloatType(0)) {
                zeroDirectionCount++;
                continue;
            }
            const Vector3 orientationDirection = orientationVector / directionMagnitude;

            Vector3 anchorWeightedOffsetSum = Vector3::Zero();
            FloatType anchorMassSum = FloatType(0);
            size_t anchorCount = 0;
            for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
                const size_t particleIndex = group.indices[atomListIndex];
                if(std::find(anchorTypeIds.begin(), anchorTypeIds.end(), particleTypes[particleIndex]) == anchorTypeIds.end())
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
                if(referenceIndex >= particleCount)
                    continue;
                if(moleculeIds[referenceIndex] == group.moleculeId)
                    continue;

                const FloatType distanceSquared = neighborQuery.distanceSquared();
                const Vector3 referenceToAnchor = -neighborQuery.delta();
                auto [iter, inserted] = referenceHits.try_emplace(referenceIndex, ReferenceHit{distanceSquared, referenceToAnchor});
                if(!inserted && distanceSquared < iter->second.distanceSquared) {
                    iter->second.distanceSquared = distanceSquared;
                    iter->second.referenceToAnchor = referenceToAnchor;
                }
            }

            if(referenceHits.empty()) {
                noReferenceCount++;
                continue;
            }

            auto nearestIter = std::min_element(referenceHits.begin(), referenceHits.end(),
                                                [](const auto& left, const auto& right) {
                                                    return left.second.distanceSquared < right.second.distanceSquared;
                                                });
            OVITO_ASSERT(nearestIter != referenceHits.end());
            const FloatType nearestDistance = std::sqrt(nearestIter->second.distanceSquared);
            const Vector3 radialVector = nearestIter->second.referenceToAnchor;
            const FloatType radialMagnitude = radialVector.length();
            if(radialMagnitude <= FloatType(0)) {
                zeroRadialCount++;
                continue;
            }

            const FloatType orientationAngle =
                qRadiansToDegrees(clampedAcos(orientationDirection.dot(radialVector / radialMagnitude)));
            size_t binIndex = static_cast<size_t>((orientationAngle - angleRangeStart) / binSize);
            if(binIndex >= static_cast<size_t>(histogramBinCount))
                binIndex = static_cast<size_t>(histogramBinCount - 1);
            histogram[binIndex]++;

            const int32_t overlapCount = static_cast<int32_t>(referenceHits.size());
            if(overlapCount > 1)
                overlapMoleculeCount++;

            for(size_t particleIndex : group.indices) {
                angleAcc[particleIndex] = orientationAngle;
                distanceAcc[particleIndex] = nearestDistance;
                overlapAcc[particleIndex] = overlapCount;
            }
            analyzedMoleculeCount++;
        }

        if(selectedOnly && candidateMoleculeCount == 0)
            throw Exception(tr("No molecules were selected. Any selected atom promotes the whole molecule into this analysis."));
        if(analyzedMoleculeCount == 0) {
            if(selectedDirectionMode == ManualMolecularDirection && missingDirectionCount == candidateMoleculeCount)
                throw Exception(tr("No molecules contained both selected atom types for the manual molecular direction."));
            if(missingAnchorCount == candidateMoleculeCount)
                throw Exception(tr("No molecules contained the requested anchor atom types."));
            if(noReferenceCount == candidateMoleculeCount)
                throw Exception(tr("No molecules had a reference atom of the chosen type within the cutoff radius."));
            throw Exception(tr("No molecules satisfied the molecular orientation analysis criteria."));
        }

        table->setElementCount(histogramBinCount);
        Property* pdfValues = table->createProperty(DataBuffer::Initialized, QStringLiteral("PDF"), Property::FloatDefault);
        BufferWriteAccess<FloatType, access_mode::discard_write> pdf(pdfValues);
        for(int binIndex = 0; binIndex < histogramBinCount; ++binIndex)
            pdf[binIndex] = static_cast<FloatType>(histogram[binIndex]) / (static_cast<FloatType>(analyzedMoleculeCount) * binSize);
        table->setY(pdfValues);

        Particles* outputParticles = state.expectMutableObject<Particles>();
        outputParticles->createProperty(std::move(angleProperty));
        outputParticles->createProperty(std::move(distanceProperty));
        outputParticles->createProperty(std::move(overlapProperty));

        QString statusText = tr("Analyzed %1 molecules using %2.")
                                 .arg(analyzedMoleculeCount)
                                 .arg(directionModeLabel(selectedDirectionMode));
        if(overlapMoleculeCount > 0)
            statusText += tr(" %1 molecules had overlapping reference environments.").arg(overlapMoleculeCount);
        if(noReferenceCount > 0)
            statusText += tr(" %1 molecules had no reference atom within the cutoff.").arg(noReferenceCount);
        if(missingAnchorCount > 0)
            statusText += tr(" %1 molecules were skipped because they lacked the requested anchor atoms.").arg(missingAnchorCount);
        if(missingDirectionCount > 0)
            statusText += tr(" %1 molecules were skipped because they lacked the requested direction atom types.").arg(missingDirectionCount);
        if(zeroDirectionCount > 0)
            statusText += tr(" %1 molecules had zero direction magnitude.").arg(zeroDirectionCount);
        if(zeroRadialCount > 0)
            statusText += tr(" %1 molecules had zero reference distance.").arg(zeroRadialCount);
        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(analyzedMoleculeCount)));
        return std::move(state);
    });
}

}  // namespace Ovito
