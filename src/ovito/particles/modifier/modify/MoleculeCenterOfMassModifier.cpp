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
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include "MoleculeCenterOfMassModifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ovito {

namespace {

struct MoleculeGroup
{
    IdentifierIntType moleculeId = 0;
    std::vector<size_t> particleIndices;
    bool selected = false;
};

struct CenterOfMassRecord
{
    IdentifierIntType moleculeId = 0;
    IdentifierIntType outputIdentifier = 0;
    Point3 position = Point3::Origin();
    Vector3 velocity = Vector3::Zero();
    FloatType mass = 0;
    FloatType charge = 0;
    ColorG color = ColorG(1, 0.65f, 0);
    GraphicsFloatType radius = 0;
    int32_t atomCount = 0;
};

struct KeptParticleRecord
{
    Point3 position = Point3::Origin();
    IdentifierIntType moleculeId = 0;
    IdentifierIntType identifier = 0;
    int32_t typeId = 0;
    Vector3 velocity = Vector3::Zero();
    FloatType charge = 0;
    FloatType mass = 0;
    SelectionIntType selection = 0;
    ColorG color = ColorG(1, 1, 1);
    GraphicsFloatType radius = 0;
};

bool isPositiveFinite(FloatType value)
{
    return std::isfinite(value) && value > 0;
}

QString makeUniqueCOMTypeName(const Property* typeProperty)
{
    QString candidate = QStringLiteral("COM");
    if(!typeProperty)
        return candidate;

    auto nameExists = [&](const QString& name) {
        return std::ranges::any_of(typeProperty->elementTypes(), [&](const ElementType* type) {
            return type && type->name() == name;
        });
    };

    if(!nameExists(candidate))
        return candidate;

    int suffix = 2;
    while(nameExists(QStringLiteral("COM %1").arg(suffix)))
        ++suffix;
    return QStringLiteral("COM %1").arg(suffix);
}

int makeUniqueCOMTypeId(const Property* typeProperty)
{
    int maxId = 0;
    if(typeProperty) {
        for(const ElementType* type : typeProperty->elementTypes()) {
            if(type)
                maxId = std::max(maxId, type->numericId());
        }
    }
    return maxId + 1;
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(MoleculeCenterOfMassModifier);
OVITO_CLASSINFO(MoleculeCenterOfMassModifier, "DisplayName", "Replace molecules with centers of mass");
OVITO_CLASSINFO(MoleculeCenterOfMassModifier, "Description",
                "Replace selected molecules by one center-of-mass particle while keeping unselected molecules atomistic.");
OVITO_CLASSINFO(MoleculeCenterOfMassModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(MoleculeCenterOfMassModifier, selectionSource);
DEFINE_PROPERTY_FIELD(MoleculeCenterOfMassModifier, selectedTypes);
DEFINE_PROPERTY_FIELD(MoleculeCenterOfMassModifier, selectionExpression);
SET_PROPERTY_FIELD_LABEL(MoleculeCenterOfMassModifier, selectionSource, "Molecule selection source");
SET_PROPERTY_FIELD_LABEL(MoleculeCenterOfMassModifier, selectedTypes, "Atom type(s)");
SET_PROPERTY_FIELD_LABEL(MoleculeCenterOfMassModifier, selectionExpression, "Selection expression");

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool MoleculeCenterOfMassModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Replaces selected molecules by center-of-mass particles.
******************************************************************************/
Future<PipelineFlowState> MoleculeCenterOfMassModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(tr("This modifier requires the particle property 'Molecule Identifier'."));

    const Property* typeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<int32_t> particleTypes(typeProperty);
    BufferReadAccess<IdentifierIntType> particleIdentifiers = particles->getProperty(Particles::IdentifierProperty);
    BufferReadAccess<SelectionIntType> inputSelection = particles->getProperty(Particles::SelectionProperty);
    BufferReadAccess<Vector3> velocities = particles->getProperty(Particles::VelocityProperty);
    BufferReadAccess<FloatType> charges = particles->getProperty(Particles::ChargeProperty);
    BufferReadAccess<ColorG> explicitColors = particles->getProperty(Particles::ColorProperty);
    BufferReadAccess<GraphicsFloatType> explicitRadii = particles->getProperty(Particles::RadiusProperty);

    ConstPropertyPtr inputMassProperty = particles->inputParticleMasses();
    BufferReadAccess<FloatType> masses(inputMassProperty);
    ConstPropertyPtr inputRadiiProperty = particles->inputParticleRadii();
    BufferReadAccess<GraphicsFloatType> effectiveRadii(inputRadiiProperty);

    const bool haveMassWeights = inputMassProperty && std::ranges::all_of(masses, [](FloatType mass) { return isPositiveFinite(mass); });
    const MoleculeSelectionSource sourceMode = selectionSource();
    if(sourceMode == CurrentParticleSelection && !inputSelection)
        throw Exception(tr("The selection source 'Current particle selection' requires an input particle selection."));

    const SimulationCell* cell = state.getObject<SimulationCell>();

    auto compute = [
            state = std::move(state),
            positions = std::move(positions),
            moleculeIds = std::move(moleculeIds),
            particleTypes = std::move(particleTypes),
            particleIdentifiers = std::move(particleIdentifiers),
            inputSelection = std::move(inputSelection),
            velocities = std::move(velocities),
            charges = std::move(charges),
            masses = std::move(masses),
            explicitColors = std::move(explicitColors),
            explicitRadii = std::move(explicitRadii),
            effectiveRadii = std::move(effectiveRadii),
            particleCount = particles->elementCount(),
            typeProperty,
            haveMassWeights,
            sourceMode,
            selectedTypes = selectedTypes(),
            selectionExpressionText = selectionExpression(),
            cell]() mutable -> PipelineFlowState
    {
        const Particles* particles = state.expectObject<Particles>();

        std::vector<uint8_t> selectorMask(particleCount, 0);
        size_t selectorMatchCount = 0;
        switch(sourceMode) {
        case AllMolecules:
            std::fill(selectorMask.begin(), selectorMask.end(), uint8_t(1));
            selectorMatchCount = particleCount;
            break;
        case CurrentParticleSelection:
            for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
                if(inputSelection[particleIndex]) {
                    selectorMask[particleIndex] = 1;
                    ++selectorMatchCount;
                }
            }
            break;
        case AtomTypes:
            selectorMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes, selectedTypes, {},
                                                   tr("atom type"), tr("Molecule center-of-mass reduction"),
                                                   &selectorMatchCount);
            break;
        case Expression:
            selectorMask = evaluateParticleSelector(state, particles, typeProperty, particleTypes, {}, selectionExpressionText,
                                                   tr("selection"), tr("Molecule center-of-mass reduction"),
                                                   &selectorMatchCount);
            break;
        }

        if(selectorMatchCount == 0) {
            state.setStatus(PipelineStatus(PipelineStatus::Success,
                                           tr("No particles matched the molecule-selection rule. The input was left unchanged.")));
            return state;
        }

        std::unordered_map<IdentifierIntType, size_t> groupLookup;
        groupLookup.reserve(particleCount);
        std::vector<MoleculeGroup> moleculeGroups;
        moleculeGroups.reserve(particleCount);

        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            const IdentifierIntType moleculeId = moleculeIds[particleIndex];
            auto [it, inserted] = groupLookup.try_emplace(moleculeId, moleculeGroups.size());
            if(inserted) {
                moleculeGroups.emplace_back();
                moleculeGroups.back().moleculeId = moleculeId;
            }

            MoleculeGroup& group = moleculeGroups[it->second];
            group.particleIndices.push_back(particleIndex);
            if(selectorMask[particleIndex])
                group.selected = true;
        }

        std::vector<uint8_t> deleteMask(particleCount, 0);
        std::vector<CenterOfMassRecord> comRecords;
        comRecords.reserve(moleculeGroups.size());
        size_t deletedAtomCount = 0;
        size_t fallbackWeightMoleculeCount = 0;
        FloatType averageComRadiusAccumulator = 0;

        for(const MoleculeGroup& group : moleculeGroups) {
            if(!group.selected)
                continue;

            deletedAtomCount += group.particleIndices.size();
            for(size_t particleIndex : group.particleIndices)
                deleteMask[particleIndex] = 1;

            CenterOfMassRecord record;
            record.moleculeId = group.moleculeId;
            record.atomCount = static_cast<int32_t>(group.particleIndices.size());

            const Point3 referencePosition = positions[group.particleIndices.front()];
            const bool useMassWeights = haveMassWeights;
            if(!useMassWeights)
                ++fallbackWeightMoleculeCount;

            FloatType weightSum = 0;
            Vector3 weightedPosition = Vector3::Zero();
            Vector3 weightedVelocity = Vector3::Zero();
            Vector3 weightedColor = Vector3::Zero();
            FloatType radiusVolumeSum = 0;
            FloatType massSum = 0;
            FloatType chargeSum = 0;

            for(size_t particleIndex : group.particleIndices) {
                const FloatType weight = useMassWeights ? masses[particleIndex] : FloatType(1);
                Vector3 displacement = positions[particleIndex] - referencePosition;
                if(cell && !cell->isDegenerate())
                    displacement = cell->wrapVector(displacement);
                const Point3 unwrappedPosition = referencePosition + displacement;

                weightedPosition += weight * (unwrappedPosition - Point3::Origin());
                weightSum += weight;

                if(velocities)
                    weightedVelocity += weight * velocities[particleIndex];
                if(explicitColors)
                    weightedColor += weight * Vector3(explicitColors[particleIndex].r(), explicitColors[particleIndex].g(), explicitColors[particleIndex].b());
                if(charges)
                    chargeSum += charges[particleIndex];
                if(useMassWeights)
                    massSum += masses[particleIndex];
                if(effectiveRadii) {
                    const FloatType radius = std::max<FloatType>(effectiveRadii[particleIndex], 0);
                    radiusVolumeSum += radius * radius * radius;
                }
            }

            const Vector3 averagePosition = weightedPosition / std::max(weightSum, FloatType(1));
            record.position = Point3(averagePosition.x(), averagePosition.y(), averagePosition.z());
            if(velocities)
                record.velocity = weightedVelocity / std::max(weightSum, FloatType(1));
            if(explicitColors)
                record.color = ColorG(weightedColor.x() / std::max(weightSum, FloatType(1)),
                                      weightedColor.y() / std::max(weightSum, FloatType(1)),
                                      weightedColor.z() / std::max(weightSum, FloatType(1)));
            record.mass = useMassWeights ? massSum : FloatType(0);
            record.charge = chargeSum;
            if(effectiveRadii)
                record.radius = static_cast<GraphicsFloatType>(std::cbrt(std::max(radiusVolumeSum, FloatType(0))));

            averageComRadiusAccumulator += record.radius;
            comRecords.push_back(record);
        }

        if(comRecords.empty()) {
            state.setStatus(PipelineStatus(PipelineStatus::Success,
                                           tr("No molecules matched the molecule-selection rule. The input was left unchanged.")));
            return state;
        }

        std::vector<size_t> keptParticleIndices;
        keptParticleIndices.reserve(particleCount - deletedAtomCount);
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex) {
            if(!deleteMask[particleIndex])
                keptParticleIndices.push_back(particleIndex);
        }

        const size_t outputParticleCount = keptParticleIndices.size() + comRecords.size();
        const FloatType averageComRadius = comRecords.empty()
            ? GraphicsFloatType(0)
            : averageComRadiusAccumulator / static_cast<FloatType>(comRecords.size());

        std::unordered_set<IdentifierIntType> usedIdentifiers;
        usedIdentifiers.reserve(outputParticleCount * 2);
        std::vector<IdentifierIntType> keptOutputIdentifiers;
        keptOutputIdentifiers.reserve(keptParticleIndices.size());

        IdentifierIntType nextIdentifier = 1;
        auto noteIdentifier = [&](IdentifierIntType identifier) {
            if(identifier > 0) {
                usedIdentifiers.insert(identifier);
                nextIdentifier = std::max<IdentifierIntType>(nextIdentifier, identifier + 1);
            }
        };

        for(size_t particleIndex : keptParticleIndices) {
            IdentifierIntType identifier = particleIdentifiers ? particleIdentifiers[particleIndex] : static_cast<IdentifierIntType>(particleIndex + 1);
            keptOutputIdentifiers.push_back(identifier);
            noteIdentifier(identifier);
        }

        auto assignFreshIdentifier = [&]() -> IdentifierIntType {
            if(nextIdentifier <= 0)
                nextIdentifier = 1;
            while(usedIdentifiers.contains(nextIdentifier) || nextIdentifier <= 0)
                ++nextIdentifier;
            const IdentifierIntType assigned = nextIdentifier;
            usedIdentifiers.insert(assigned);
            ++nextIdentifier;
            return assigned;
        };

        size_t reassignedIdentifierCount = 0;
        for(CenterOfMassRecord& record : comRecords) {
            const IdentifierIntType desiredIdentifier = record.moleculeId;
            if(desiredIdentifier > 0 && !usedIdentifiers.contains(desiredIdentifier)) {
                record.outputIdentifier = desiredIdentifier;
                noteIdentifier(desiredIdentifier);
            }
            else {
                record.outputIdentifier = assignFreshIdentifier();
                ++reassignedIdentifierCount;
            }
        }

        std::vector<KeptParticleRecord> keptRecords;
        keptRecords.reserve(keptParticleIndices.size());
        for(size_t keptIndex = 0; keptIndex < keptParticleIndices.size(); ++keptIndex) {
            const size_t particleIndex = keptParticleIndices[keptIndex];
            KeptParticleRecord record;
            record.position = positions[particleIndex];
            record.moleculeId = moleculeIds[particleIndex];
            record.identifier = keptOutputIdentifiers[keptIndex];
            if(particleTypes)
                record.typeId = particleTypes[particleIndex];
            if(velocities)
                record.velocity = velocities[particleIndex];
            if(charges)
                record.charge = charges[particleIndex];
            if(masses)
                record.mass = masses[particleIndex];
            if(inputSelection)
                record.selection = inputSelection[particleIndex];
            if(explicitColors)
                record.color = explicitColors[particleIndex];
            if(explicitRadii)
                record.radius = explicitRadii[particleIndex];
            keptRecords.push_back(record);
        }

        Particles* outputParticles = state.makeMutable(particles);
        BufferFactory<SelectionIntType> deletionSelection(particleCount);
        for(size_t particleIndex = 0; particleIndex < particleCount; ++particleIndex)
            deletionSelection[particleIndex] = deleteMask[particleIndex] ? 1 : 0;
        outputParticles->deleteElements(deletionSelection.take(), deletedAtomCount);
        outputParticles->setElementCount(outputParticleCount);

        PropertyPtr outputPositionsProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::PositionProperty);
        BufferWriteAccess<Point3, access_mode::discard_write> outputPositions(outputPositionsProperty);

        PropertyPtr outputMoleculeProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::MoleculeProperty);
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> outputMoleculeIds(outputMoleculeProperty);

        PropertyPtr outputIdentifierProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::IdentifierProperty);
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> outputIdentifiers(outputIdentifierProperty);

        PropertyPtr isCOMProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized, outputParticleCount, Property::Int32, 1,
                                                                            IsCOMPropertyName.toString());
        BufferWriteAccess<int32_t, access_mode::discard_write> isCOMFlags(isCOMProperty);

        PropertyPtr sourceAtomCountProperty = Particles::OOClass().createUserProperty(DataBuffer::Initialized, outputParticleCount, Property::Int32, 1,
                                                                                      SourceAtomCountPropertyName.toString());
        BufferWriteAccess<int32_t, access_mode::discard_write> outputSourceAtomCounts(sourceAtomCountProperty);

        PropertyPtr outputTypeProperty;
        int comTypeId = 0;
        if(typeProperty) {
            outputTypeProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::TypeProperty);
            for(const ElementType* type : typeProperty->elementTypes()) {
                if(type)
                    outputTypeProperty->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
            }
            comTypeId = makeUniqueCOMTypeId(typeProperty);
            DataOORef<ParticleType> comType = DataOORef<ParticleType>::create();
            const QString comTypeName = makeUniqueCOMTypeName(typeProperty);
            comType->initializeType([&]() {
                comType->setNumericId(comTypeId);
            }, OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty));
            comType->setName(comTypeName);
            comType->setColor({1.0f, 0.65f, 0.0f});
            if(averageComRadius > 0)
                comType->setRadius(averageComRadius);
            outputTypeProperty->addElementType(std::move(comType));
        }
        BufferWriteAccess<int32_t, access_mode::discard_write> outputTypes(outputTypeProperty);

        PropertyPtr outputVelocityProperty;
        if(velocities) {
            outputVelocityProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::VelocityProperty);
        }
        BufferWriteAccess<Vector3, access_mode::discard_write> outputVelocities(outputVelocityProperty);

        PropertyPtr outputChargeProperty;
        if(charges) {
            outputChargeProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::ChargeProperty);
        }
        BufferWriteAccess<FloatType, access_mode::discard_write> outputCharges(outputChargeProperty);

        PropertyPtr outputMassProperty;
        if(haveMassWeights) {
            outputMassProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::MassProperty);
        }
        BufferWriteAccess<FloatType, access_mode::discard_write> outputMasses(outputMassProperty);

        PropertyPtr outputSelectionProperty;
        if(inputSelection) {
            outputSelectionProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::SelectionProperty);
        }
        BufferWriteAccess<SelectionIntType, access_mode::discard_write> outputSelections(outputSelectionProperty);

        PropertyPtr outputColorProperty;
        if(explicitColors) {
            outputColorProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::ColorProperty);
        }
        BufferWriteAccess<ColorG, access_mode::discard_write> outputColors(outputColorProperty);

        PropertyPtr outputRadiusProperty;
        if(explicitRadii) {
            outputRadiusProperty = Particles::OOClass().createStandardProperty(DataBuffer::Initialized, outputParticleCount, Particles::RadiusProperty);
        }
        BufferWriteAccess<GraphicsFloatType, access_mode::discard_write> outputRadii(outputRadiusProperty);

        size_t outputIndex = 0;
        for(const KeptParticleRecord& record : keptRecords) {
            outputPositions[outputIndex] = record.position;
            outputMoleculeIds[outputIndex] = record.moleculeId;
            outputIdentifiers[outputIndex] = record.identifier;
            isCOMFlags[outputIndex] = 0;
            outputSourceAtomCounts[outputIndex] = 1;
            if(outputTypes)
                outputTypes[outputIndex] = record.typeId;
            if(outputVelocities)
                outputVelocities[outputIndex] = record.velocity;
            if(outputCharges)
                outputCharges[outputIndex] = record.charge;
            if(outputMasses)
                outputMasses[outputIndex] = record.mass;
            if(outputSelections)
                outputSelections[outputIndex] = record.selection;
            if(outputColors)
                outputColors[outputIndex] = record.color;
            if(outputRadii)
                outputRadii[outputIndex] = record.radius;
            ++outputIndex;
        }

        for(const CenterOfMassRecord& record : comRecords) {
            outputPositions[outputIndex] = record.position;
            outputMoleculeIds[outputIndex] = record.moleculeId;
            outputIdentifiers[outputIndex] = record.outputIdentifier;
            isCOMFlags[outputIndex] = 1;
            outputSourceAtomCounts[outputIndex] = record.atomCount;
            if(outputTypes)
                outputTypes[outputIndex] = comTypeId;
            if(outputVelocities)
                outputVelocities[outputIndex] = record.velocity;
            if(outputCharges)
                outputCharges[outputIndex] = record.charge;
            if(outputMasses)
                outputMasses[outputIndex] = record.mass;
            if(outputSelections)
                outputSelections[outputIndex] = 1;
            if(outputColors)
                outputColors[outputIndex] = record.color;
            if(outputRadii)
                outputRadii[outputIndex] = record.radius;
            ++outputIndex;
        }

        outputParticles->createProperty(std::move(outputPositionsProperty));
        outputParticles->createProperty(std::move(outputMoleculeProperty));
        outputParticles->createProperty(std::move(outputIdentifierProperty));
        outputParticles->createProperty(std::move(isCOMProperty));
        outputParticles->createProperty(std::move(sourceAtomCountProperty));
        if(outputTypeProperty)
            outputParticles->createProperty(std::move(outputTypeProperty));
        if(outputVelocityProperty)
            outputParticles->createProperty(std::move(outputVelocityProperty));
        if(outputChargeProperty)
            outputParticles->createProperty(std::move(outputChargeProperty));
        if(outputMassProperty)
            outputParticles->createProperty(std::move(outputMassProperty));
        if(outputSelectionProperty)
            outputParticles->createProperty(std::move(outputSelectionProperty));
        if(outputColorProperty)
            outputParticles->createProperty(std::move(outputColorProperty));
        if(outputRadiusProperty)
            outputParticles->createProperty(std::move(outputRadiusProperty));

        QStringList detailMessages;
        if(fallbackWeightMoleculeCount != 0)
            detailMessages.push_back(tr("mass data were unavailable for %1 selected molecule(s), so they were reduced using uniform weights")
                                         .arg(fallbackWeightMoleculeCount));
        if(reassignedIdentifierCount != 0)
            detailMessages.push_back(tr("%1 center-of-mass particle ID(s) were reassigned because the molecule ID would have collided with a preserved atom ID")
                                         .arg(reassignedIdentifierCount));

        QString statusMessage = tr("Replaced %1 selected molecule(s) by %1 center-of-mass particle(s), removed %2 atom(s), and kept %3 atom(s).")
                                    .arg(comRecords.size())
                                    .arg(deletedAtomCount)
                                    .arg(keptParticleIndices.size());
        if(!detailMessages.isEmpty())
            statusMessage += QLatin1Char(' ') + detailMessages.join(QStringLiteral("; ")) + QLatin1Char('.');

        state.setStatus(PipelineStatus(detailMessages.isEmpty() ? PipelineStatus::Success : PipelineStatus::Warning,
                                       statusMessage));
        return state;
    };
    return Future<PipelineFlowState>::createImmediate(compute());
}

}   // End of namespace
