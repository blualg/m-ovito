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
#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/Task.h>
#include "GeometryDescriptorModifier.h"

#include <QtMath>
#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>
#include <unordered_map>

namespace Ovito {

namespace {

struct MoleculeGroup
{
    IdentifierIntType moleculeId = 0;
    std::vector<size_t> indices;
    bool anySelected = false;
};

struct DescriptorRecord
{
    IdentifierIntType identifier = 0;
    IdentifierIntType moleculeId = 0;
    Point3 position = Point3::Origin();
    Vector3 vector = Vector3::Zero();
    FloatType magnitude = std::numeric_limits<FloatType>::quiet_NaN();
    FloatType scalar = std::numeric_limits<FloatType>::quiet_NaN();
};

struct TemplateRoleIndices
{
    int atom1 = 0;
    int atom2 = 0;
    int atom3 = 0;
    int atom4 = 0;
};

using UnsignedIdType = std::make_unsigned_t<IdentifierIntType>;

inline FloatType clampedAcos(FloatType value)
{
    return std::acos(std::clamp(value, FloatType(-1), FloatType(1)));
}

inline IdentifierIntType particleIdentifier(const BufferReadAccess<IdentifierIntType>& particleIds, size_t particleIndex)
{
    return particleIds ? particleIds[particleIndex] : static_cast<IdentifierIntType>(particleIndex + 1);
}

inline IdentifierIntType makeHashedDescriptorId(std::initializer_list<IdentifierIntType> values)
{
    auto mix = [](UnsignedIdType value) {
        value += 0x9e3779b97f4a7c15ull;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
        return value ^ (value >> 31);
    };

    UnsignedIdType hash = 0x5eeded123456789ull;
    for(IdentifierIntType value : values)
        hash ^= mix(static_cast<UnsignedIdType>(value)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);

    if(hash == 0)
        hash = 1;
    return static_cast<IdentifierIntType>(hash);
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
        Vector3 delta = positions[group.indices[atomListIndex]] - referencePosition;
        if(cell)
            delta = cell->wrapVector(delta);
        wrappedPositions.push_back(referencePosition + delta);
    }
}

std::vector<size_t> collectLocalMatches(const MoleculeGroup& group,
                                        const std::vector<uint8_t>& mask)
{
    std::vector<size_t> matches;
    matches.reserve(group.indices.size());
    for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex) {
        if(mask[group.indices[atomListIndex]])
            matches.push_back(atomListIndex);
    }
    return matches;
}

std::vector<size_t> moleculeLocalOrder(const MoleculeGroup& group,
                                       const BufferReadAccess<IdentifierIntType>& particleIds)
{
    std::vector<size_t> localOrder;
    localOrder.reserve(group.indices.size());
    for(size_t atomListIndex = 0; atomListIndex < group.indices.size(); ++atomListIndex)
        localOrder.push_back(atomListIndex);

    std::sort(localOrder.begin(), localOrder.end(), [&group, &particleIds](size_t left, size_t right) {
        return particleIdentifier(particleIds, group.indices[left]) < particleIdentifier(particleIds, group.indices[right]);
    });

    return localOrder;
}

std::vector<size_t> collectMoleculeLocalIndexMatch(const MoleculeGroup& group,
                                                   const BufferReadAccess<IdentifierIntType>& particleIds,
                                                   int oneBasedIndex)
{
    if(oneBasedIndex <= 0 || static_cast<size_t>(oneBasedIndex) > group.indices.size())
        return {};

    const std::vector<size_t> localOrder = moleculeLocalOrder(group, particleIds);
    return {localOrder[static_cast<size_t>(oneBasedIndex - 1)]};
}

int templateLocalIndexForParticleId(const MoleculeGroup& templateGroup,
                                    const BufferReadAccess<IdentifierIntType>& particleIds,
                                    IdentifierIntType requestedParticleId,
                                    const QString& roleLabel,
                                    IdentifierIntType templateMoleculeId)
{
    const std::vector<size_t> localOrder = moleculeLocalOrder(templateGroup, particleIds);
    for(size_t orderIndex = 0; orderIndex < localOrder.size(); ++orderIndex) {
        const size_t particleIndex = templateGroup.indices[localOrder[orderIndex]];
        if(particleIdentifier(particleIds, particleIndex) == requestedParticleId)
            return static_cast<int>(orderIndex + 1);
    }

    throw Exception(QObject::tr("%1 particle ID %2 was not found in template molecule ID %3.")
                        .arg(roleLabel)
                        .arg(requestedParticleId)
                        .arg(templateMoleculeId));
}

TemplateRoleIndices buildTemplateRoleIndices(const std::vector<MoleculeGroup>& molecules,
                                             const BufferReadAccess<IdentifierIntType>& particleIds,
                                             IdentifierIntType templateMoleculeId,
                                             IdentifierIntType atom1ParticleId,
                                             IdentifierIntType atom2ParticleId,
                                             IdentifierIntType atom3ParticleId,
                                             IdentifierIntType atom4ParticleId,
                                             GeometryDescriptorModifier::DescriptorMode descriptorMode)
{
    auto iter = std::find_if(molecules.begin(), molecules.end(), [templateMoleculeId](const MoleculeGroup& group) {
        return group.moleculeId == templateMoleculeId;
    });
    if(iter == molecules.end()) {
        throw Exception(QObject::tr("Template molecule ID %1 was not found in the current frame.")
                            .arg(templateMoleculeId));
    }

    TemplateRoleIndices indices;
    indices.atom1 = templateLocalIndexForParticleId(*iter, particleIds, atom1ParticleId, QObject::tr("Atom 1"), templateMoleculeId);
    indices.atom2 = templateLocalIndexForParticleId(*iter, particleIds, atom2ParticleId, QObject::tr("Atom 2"), templateMoleculeId);
    if(descriptorMode == GeometryDescriptorModifier::AngleDescriptor || descriptorMode == GeometryDescriptorModifier::DihedralDescriptor)
        indices.atom3 = templateLocalIndexForParticleId(*iter, particleIds, atom3ParticleId, QObject::tr("Atom 3"), templateMoleculeId);
    if(descriptorMode == GeometryDescriptorModifier::DihedralDescriptor)
        indices.atom4 = templateLocalIndexForParticleId(*iter, particleIds, atom4ParticleId, QObject::tr("Atom 4"), templateMoleculeId);
    return indices;
}

bool selectorsAreEquivalent(const QString& leftTypes,
                            const QString& leftExpression,
                            const QString& rightTypes,
                            const QString& rightExpression)
{
    return canonicalizeParticleSelector(leftTypes, leftExpression) == canonicalizeParticleSelector(rightTypes, rightExpression);
}

void appendVectorRecords(const MoleculeGroup& group,
                         const std::vector<Point3>& moleculePositions,
                         const std::vector<size_t>& atom1Matches,
                         const std::vector<size_t>& atom2Matches,
                         const BufferReadAccess<IdentifierIntType>& particleIds,
                         bool sameSelectors,
                         bool normalizeVectors,
                         std::vector<DescriptorRecord>& records,
                         size_t& zeroVectorCount,
                         IdentifierIntType atom1RoleId = 0,
                         IdentifierIntType atom2RoleId = 0)
{
    for(size_t atom1ListIndex : atom1Matches) {
        for(size_t atom2ListIndex : atom2Matches) {
            if(atom1ListIndex == atom2ListIndex)
                continue;
            if(sameSelectors && atom1ListIndex > atom2ListIndex)
                continue;

            const size_t atom1ParticleIndex = group.indices[atom1ListIndex];
            const size_t atom2ParticleIndex = group.indices[atom2ListIndex];
            const IdentifierIntType atom1Id = particleIdentifier(particleIds, atom1ParticleIndex);
            const IdentifierIntType atom2Id = particleIdentifier(particleIds, atom2ParticleIndex);
            const IdentifierIntType atom1DescriptorId = atom1RoleId > 0 ? atom1RoleId : atom1Id;
            const IdentifierIntType atom2DescriptorId = atom2RoleId > 0 ? atom2RoleId : atom2Id;

            Vector3 vector = moleculePositions[atom2ListIndex] - moleculePositions[atom1ListIndex];
            const FloatType magnitude = vector.length();
            if(magnitude <= FloatType(0)) {
                zeroVectorCount++;
                continue;
            }
            if(normalizeVectors)
                vector /= magnitude;

            DescriptorRecord record;
            record.identifier = makeHashedDescriptorId({1, group.moleculeId, atom1DescriptorId, atom2DescriptorId});
            record.moleculeId = group.moleculeId;
            record.position = moleculePositions[atom1ListIndex] + (moleculePositions[atom2ListIndex] - moleculePositions[atom1ListIndex]) / FloatType(2);
            record.vector = vector;
            record.magnitude = magnitude;
            records.push_back(record);
        }
    }
}

void appendAngleRecords(const MoleculeGroup& group,
                        const std::vector<Point3>& moleculePositions,
                        const std::vector<size_t>& atom1Matches,
                        const std::vector<size_t>& atom2Matches,
                        const std::vector<size_t>& atom3Matches,
                        const BufferReadAccess<IdentifierIntType>& particleIds,
                        bool sameEndpointSelectors,
                        std::vector<DescriptorRecord>& records,
                        size_t& invalidCount,
                        IdentifierIntType atom1RoleId = 0,
                        IdentifierIntType atom2RoleId = 0,
                        IdentifierIntType atom3RoleId = 0)
{
    for(size_t atom2ListIndex : atom2Matches) {
        for(size_t atom1ListIndex : atom1Matches) {
            if(atom1ListIndex == atom2ListIndex)
                continue;
            for(size_t atom3ListIndex : atom3Matches) {
                if(atom3ListIndex == atom2ListIndex || atom3ListIndex == atom1ListIndex)
                    continue;
                if(sameEndpointSelectors && atom1ListIndex > atom3ListIndex)
                    continue;

                const Vector3 v1 = moleculePositions[atom1ListIndex] - moleculePositions[atom2ListIndex];
                const Vector3 v2 = moleculePositions[atom3ListIndex] - moleculePositions[atom2ListIndex];
                const FloatType len1 = v1.length();
                const FloatType len2 = v2.length();
                if(len1 <= FloatType(0) || len2 <= FloatType(0)) {
                    invalidCount++;
                    continue;
                }

                const size_t atom1ParticleIndex = group.indices[atom1ListIndex];
                const size_t atom2ParticleIndex = group.indices[atom2ListIndex];
                const size_t atom3ParticleIndex = group.indices[atom3ListIndex];
                const IdentifierIntType atom1DescriptorId = atom1RoleId > 0 ? atom1RoleId : particleIdentifier(particleIds, atom1ParticleIndex);
                const IdentifierIntType atom2DescriptorId = atom2RoleId > 0 ? atom2RoleId : particleIdentifier(particleIds, atom2ParticleIndex);
                const IdentifierIntType atom3DescriptorId = atom3RoleId > 0 ? atom3RoleId : particleIdentifier(particleIds, atom3ParticleIndex);

                DescriptorRecord record;
                record.identifier = makeHashedDescriptorId({
                    2,
                    group.moleculeId,
                    atom1DescriptorId,
                    atom2DescriptorId,
                    atom3DescriptorId});
                record.moleculeId = group.moleculeId;
                record.position = moleculePositions[atom2ListIndex];
                record.scalar = qRadiansToDegrees(clampedAcos(v1.dot(v2) / (len1 * len2)));
                records.push_back(record);
            }
        }
    }
}

void appendDihedralRecords(const MoleculeGroup& group,
                           const std::vector<Point3>& moleculePositions,
                           const std::vector<size_t>& atom1Matches,
                           const std::vector<size_t>& atom2Matches,
                           const std::vector<size_t>& atom3Matches,
                           const std::vector<size_t>& atom4Matches,
                           const BufferReadAccess<IdentifierIntType>& particleIds,
                           std::vector<DescriptorRecord>& records,
                           size_t& invalidCount,
                           IdentifierIntType atom1RoleId = 0,
                           IdentifierIntType atom2RoleId = 0,
                           IdentifierIntType atom3RoleId = 0,
                           IdentifierIntType atom4RoleId = 0)
{
    for(size_t atom1ListIndex : atom1Matches) {
        for(size_t atom2ListIndex : atom2Matches) {
            if(atom2ListIndex == atom1ListIndex)
                continue;
            for(size_t atom3ListIndex : atom3Matches) {
                if(atom3ListIndex == atom1ListIndex || atom3ListIndex == atom2ListIndex)
                    continue;
                for(size_t atom4ListIndex : atom4Matches) {
                    if(atom4ListIndex == atom1ListIndex || atom4ListIndex == atom2ListIndex || atom4ListIndex == atom3ListIndex)
                        continue;

                    const Vector3 b1 = moleculePositions[atom2ListIndex] - moleculePositions[atom1ListIndex];
                    const Vector3 b2 = moleculePositions[atom3ListIndex] - moleculePositions[atom2ListIndex];
                    const Vector3 b3 = moleculePositions[atom4ListIndex] - moleculePositions[atom3ListIndex];
                    const Vector3 n1 = b1.cross(b2);
                    const Vector3 n2 = b2.cross(b3);
                    const FloatType n1len = n1.length();
                    const FloatType n2len = n2.length();
                    const FloatType b2len = b2.length();
                    if(n1len <= FloatType(0) || n2len <= FloatType(0) || b2len <= FloatType(0)) {
                        invalidCount++;
                        continue;
                    }

                    const Vector3 b2hat = b2 / b2len;
                    const Vector3 m1 = n1.cross(b2hat);
                    const FloatType angle = std::atan2(m1.dot(n2), n1.dot(n2));

                    DescriptorRecord record;
                    record.identifier = makeHashedDescriptorId({
                        3,
                        group.moleculeId,
                        atom1RoleId > 0 ? atom1RoleId : particleIdentifier(particleIds, group.indices[atom1ListIndex]),
                        atom2RoleId > 0 ? atom2RoleId : particleIdentifier(particleIds, group.indices[atom2ListIndex]),
                        atom3RoleId > 0 ? atom3RoleId : particleIdentifier(particleIds, group.indices[atom3ListIndex]),
                        atom4RoleId > 0 ? atom4RoleId : particleIdentifier(particleIds, group.indices[atom4ListIndex])});
                    record.moleculeId = group.moleculeId;
                    record.position = moleculePositions[atom2ListIndex] + (moleculePositions[atom3ListIndex] - moleculePositions[atom2ListIndex]) / FloatType(2);
                    record.scalar = qRadiansToDegrees(angle);
                    records.push_back(record);
                }
            }
        }
    }
}

QString descriptorLabel(GeometryDescriptorModifier::DescriptorMode mode)
{
    switch(mode) {
    case GeometryDescriptorModifier::VectorDescriptor:
        return GeometryDescriptorModifier::tr("Vector");
    case GeometryDescriptorModifier::AngleDescriptor:
        return GeometryDescriptorModifier::tr("Angle");
    case GeometryDescriptorModifier::DihedralDescriptor:
        return GeometryDescriptorModifier::tr("Dihedral");
    }
    OVITO_ASSERT(false);
    return {};
}

QString descriptorPropertyName(GeometryDescriptorModifier::DescriptorMode mode)
{
    switch(mode) {
    case GeometryDescriptorModifier::VectorDescriptor:
        return QStringLiteral("Geometry Vector");
    case GeometryDescriptorModifier::AngleDescriptor:
        return QStringLiteral("Geometry Angle");
    case GeometryDescriptorModifier::DihedralDescriptor:
        return QStringLiteral("Geometry Dihedral");
    }
    OVITO_ASSERT(false);
    return {};
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(GeometryDescriptorModifier);
OVITO_CLASSINFO(GeometryDescriptorModifier, "DisplayName", "Geometry descriptors");
OVITO_CLASSINFO(GeometryDescriptorModifier, "Description",
                "Create molecule-based vector, angle, and dihedral descriptor elements for downstream analysis.");
OVITO_CLASSINFO(GeometryDescriptorModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, descriptorMode);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atomRoleSelectionMode);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom1Types);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom1Expression);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom2Types);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom2Expression);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom3Types);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom3Expression);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom4Types);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom4Expression);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, templateMoleculeId);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom1ParticleId);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom2ParticleId);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom3ParticleId);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, atom4ParticleId);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, normalizeVectors);
DEFINE_PROPERTY_FIELD(GeometryDescriptorModifier, onlySelectedParticles);
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, descriptorMode, "Descriptor");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atomRoleSelectionMode, "Atom role selection");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom1Types, "Atom 1 type(s)");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom1Expression, "Atom 1 expression");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom2Types, "Atom 2 type(s)");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom2Expression, "Atom 2 expression");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom3Types, "Atom 3 type(s)");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom3Expression, "Atom 3 expression");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom4Types, "Atom 4 type(s)");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom4Expression, "Atom 4 expression");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, templateMoleculeId, "Template molecule ID");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom1ParticleId, "Atom 1 particle ID");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom2ParticleId, "Atom 2 particle ID");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom3ParticleId, "Atom 3 particle ID");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, atom4ParticleId, "Atom 4 particle ID");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, normalizeVectors, "Normalize vectors");
SET_PROPERTY_FIELD_LABEL(GeometryDescriptorModifier, onlySelectedParticles, "Use only selected particles");

bool GeometryDescriptorModifier::GeometryDescriptorModifierClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

Future<PipelineFlowState> GeometryDescriptorModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    BufferReadAccess<Point3> positions = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> moleculeIds = particles->getProperty(Particles::MoleculeProperty);
    if(!moleculeIds)
        throw Exception(tr("Geometry descriptors require the particle property 'Molecule Identifier'. Load molecular topology first."));

    BufferReadAccess<IdentifierIntType> particleIds = particles->getProperty(Particles::IdentifierProperty);
    const bool templateMode = atomRoleSelectionMode() == TemplateMoleculeAtomIds;
    if(templateMode && !particleIds)
        throw Exception(tr("Template molecule atom-ID mode requires the particle property 'Particle Identifier'."));

    BufferReadAccess<int32_t> particleTypes = particles->getProperty(Particles::TypeProperty);
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    if(!templateMode) {
        if(!particleTypes)
            throw Exception(tr("Geometry descriptors require the particle property 'Particle Type' unless template molecule atom-ID mode is used."));
        if(!particleTypeProperty || !particleTypeProperty->isTypedProperty())
            throw Exception(tr("Geometry descriptors require a typed 'Particle Type' property with defined element types unless template molecule atom-ID mode is used."));
    }

    BufferReadAccess<SelectionIntType> selection(onlySelectedParticles() ? particles->getProperty(Particles::SelectionProperty) : nullptr);
    if(onlySelectedParticles() && !selection)
        throw Exception(tr("The option 'Use only selected particles' requires a particle selection."));

    const SimulationCell* cell = state.getObject<SimulationCell>();

    return asyncLaunch([
            state = std::move(state),
            positions = std::move(positions),
            moleculeIds = std::move(moleculeIds),
            particleIds = std::move(particleIds),
            particleTypes = std::move(particleTypes),
            selection = std::move(selection),
            cell,
            descriptorMode = descriptorMode(),
            atom1Types = atom1Types(),
            atom1Expression = atom1Expression(),
            atom2Types = atom2Types(),
            atom2Expression = atom2Expression(),
            atom3Types = atom3Types(),
            atom3Expression = atom3Expression(),
            atom4Types = atom4Types(),
            atom4Expression = atom4Expression(),
            templateMode,
            templateMoleculeId = templateMoleculeId(),
            atom1ParticleId = atom1ParticleId(),
            atom2ParticleId = atom2ParticleId(),
            atom3ParticleId = atom3ParticleId(),
            atom4ParticleId = atom4ParticleId(),
            normalizeVectors = normalizeVectors(),
            selectedOnly = onlySelectedParticles(),
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        const Particles* particles = state.expectObject<Particles>();
        const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);

        auto evaluateSelector = [&](const QString& types, const QString& expression, const QString& role) {
            size_t matchCount = 0;
            std::vector<uint8_t> mask = evaluateParticleSelector(
                state, particles, particleTypeProperty, particleTypes,
                types, expression,
                role,
                tr("Geometry descriptors"),
                &matchCount);
            if(matchCount == 0)
                throw Exception(tr("No particles matched the %1.").arg(role));
            return mask;
        };

        std::vector<uint8_t> atom1Mask;
        std::vector<uint8_t> atom2Mask;
        std::vector<uint8_t> atom3Mask;
        std::vector<uint8_t> atom4Mask;
        if(!templateMode) {
            atom1Mask = evaluateSelector(atom1Types, atom1Expression, tr("atom 1 selector"));
            atom2Mask = evaluateSelector(atom2Types, atom2Expression, tr("atom 2 selector"));
            if(descriptorMode == AngleDescriptor || descriptorMode == DihedralDescriptor)
                atom3Mask = evaluateSelector(atom3Types, atom3Expression, tr("atom 3 selector"));
            if(descriptorMode == DihedralDescriptor)
                atom4Mask = evaluateSelector(atom4Types, atom4Expression, tr("atom 4 selector"));
        }

        const bool same12 = !templateMode && selectorsAreEquivalent(atom1Types, atom1Expression, atom2Types, atom2Expression);
        const bool same13 = templateMode
            ? atom1ParticleId == atom3ParticleId
            : selectorsAreEquivalent(atom1Types, atom1Expression, atom3Types, atom3Expression);

        const std::vector<MoleculeGroup> molecules = buildMoleculeGroups(moleculeIds, selection);
        const TemplateRoleIndices templateRoles = templateMode
            ? buildTemplateRoleIndices(molecules, particleIds, templateMoleculeId,
                                       atom1ParticleId, atom2ParticleId, atom3ParticleId, atom4ParticleId,
                                       descriptorMode)
            : TemplateRoleIndices{};
        std::vector<DescriptorRecord> records;
        records.reserve(molecules.size());

        size_t candidateMoleculeCount = 0;
        size_t skippedSelectionCount = 0;
        size_t missingTupleCount = 0;
        size_t invalidGeometryCount = 0;
        size_t zeroVectorCount = 0;
        bool usedFallbackDescriptorIds = !particleIds;
        std::vector<Point3> moleculePositions;

        for(const MoleculeGroup& group : molecules) {
            this_task::throwIfCanceled();
            if(group.indices.empty())
                continue;

            if(selectedOnly && !group.anySelected) {
                skippedSelectionCount++;
                continue;
            }

            candidateMoleculeCount++;
            buildWrappedMoleculePositions(positions, cell, group, moleculePositions);

            const std::vector<size_t> atom1Matches = templateMode
                ? collectMoleculeLocalIndexMatch(group, particleIds, templateRoles.atom1)
                : collectLocalMatches(group, atom1Mask);
            const std::vector<size_t> atom2Matches = templateMode
                ? collectMoleculeLocalIndexMatch(group, particleIds, templateRoles.atom2)
                : collectLocalMatches(group, atom2Mask);
            const size_t recordCountBefore = records.size();

            if(descriptorMode == VectorDescriptor) {
                appendVectorRecords(group, moleculePositions, atom1Matches, atom2Matches,
                                    particleIds, same12, normalizeVectors, records, zeroVectorCount,
                                    templateMode ? templateRoles.atom1 : 0,
                                    templateMode ? templateRoles.atom2 : 0);
            }
            else if(descriptorMode == AngleDescriptor) {
                const std::vector<size_t> atom3Matches = templateMode
                    ? collectMoleculeLocalIndexMatch(group, particleIds, templateRoles.atom3)
                    : collectLocalMatches(group, atom3Mask);
                appendAngleRecords(group, moleculePositions, atom1Matches, atom2Matches, atom3Matches,
                                   particleIds, same13, records, invalidGeometryCount,
                                   templateMode ? templateRoles.atom1 : 0,
                                   templateMode ? templateRoles.atom2 : 0,
                                   templateMode ? templateRoles.atom3 : 0);
            }
            else {
                const std::vector<size_t> atom3Matches = templateMode
                    ? collectMoleculeLocalIndexMatch(group, particleIds, templateRoles.atom3)
                    : collectLocalMatches(group, atom3Mask);
                const std::vector<size_t> atom4Matches = templateMode
                    ? collectMoleculeLocalIndexMatch(group, particleIds, templateRoles.atom4)
                    : collectLocalMatches(group, atom4Mask);
                appendDihedralRecords(group, moleculePositions, atom1Matches, atom2Matches, atom3Matches, atom4Matches,
                                      particleIds, records, invalidGeometryCount,
                                      templateMode ? templateRoles.atom1 : 0,
                                      templateMode ? templateRoles.atom2 : 0,
                                      templateMode ? templateRoles.atom3 : 0,
                                      templateMode ? templateRoles.atom4 : 0);
            }

            if(records.size() == recordCountBefore)
                missingTupleCount++;
        }

        std::sort(records.begin(), records.end(), [](const DescriptorRecord& left, const DescriptorRecord& right) {
            if(left.identifier != right.identifier)
                return left.identifier < right.identifier;
            return left.moleculeId < right.moleculeId;
        });

        if(records.empty()) {
            if(selectedOnly && candidateMoleculeCount == 0)
                throw Exception(tr("No selected molecules were available. Any selected atom promotes the whole molecule into this analysis."));
            throw Exception(tr("No valid geometry descriptors were generated for the current selectors."));
        }

        Particles* descriptorParticles =
            state.createObject<Particles>(DescriptorContainerIdentifier.toString(), createdByNode,
                                          ObjectInitializationFlag::DontCreateVisElement);
        descriptorParticles->setElementCount(records.size());

        BufferWriteAccess<Point3, access_mode::discard_write> descriptorPositions(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::PositionProperty));
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> descriptorIds(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::IdentifierProperty));
        BufferWriteAccess<IdentifierIntType, access_mode::discard_write> descriptorMoleculeIds(
            descriptorParticles->createProperty(DataBuffer::Initialized, Particles::MoleculeProperty));

        for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
            const DescriptorRecord& record = records[recordIndex];
            descriptorPositions[recordIndex] = record.position;
            descriptorIds[recordIndex] = record.identifier;
            descriptorMoleculeIds[recordIndex] = record.moleculeId;
        }

        DataTable* table = state.createObject<DataTable>(TableIdentifier.toString(),
                                                         createdByNode,
                                                         DataTable::Scatter,
                                                         tr("Geometry descriptor values"));
        table->setElementCount(records.size());
        table->setAxisLabelX(tr("Descriptor index"));
        table->setAxisLabelY(descriptorPropertyName(descriptorMode));
        Property* tableX = table->createProperty(DataBuffer::Initialized, QStringLiteral("Descriptor index"), Property::FloatDefault);
        BufferWriteAccess<FloatType, access_mode::discard_write> tableXAcc(tableX);
        for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex)
            tableXAcc[recordIndex] = static_cast<FloatType>(recordIndex);
        table->setX(tableX);

        const QString propertyName = descriptorPropertyName(descriptorMode);
        if(descriptorMode == VectorDescriptor) {
            BufferWriteAccess<Vector3, access_mode::discard_write> descriptorVector(
                descriptorParticles->createProperty(DataBuffer::Initialized,
                                                    propertyName,
                                                    Property::FloatDefault,
                                                    3,
                                                    QStringList{tr("X"), tr("Y"), tr("Z")}));
            BufferWriteAccess<FloatType, access_mode::discard_write> descriptorMagnitude(
                descriptorParticles->createProperty(DataBuffer::Initialized, QStringLiteral("Geometry Vector Magnitude"), Property::FloatDefault));

            Property* tableY = table->createProperty(DataBuffer::Initialized,
                                                     propertyName,
                                                     Property::FloatDefault,
                                                     3,
                                                     QStringList{tr("X"), tr("Y"), tr("Z")});
            BufferWriteAccess<FloatType*, access_mode::discard_write> tableYAcc(tableY);
            for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
                const DescriptorRecord& record = records[recordIndex];
                descriptorVector[recordIndex] = record.vector;
                descriptorMagnitude[recordIndex] = record.magnitude;
                tableYAcc.set(recordIndex, 0, record.vector.x());
                tableYAcc.set(recordIndex, 1, record.vector.y());
                tableYAcc.set(recordIndex, 2, record.vector.z());
            }
            table->setY(tableY);
        }
        else {
            BufferWriteAccess<FloatType, access_mode::discard_write> descriptorScalar(
                descriptorParticles->createProperty(DataBuffer::Initialized, propertyName, Property::FloatDefault));
            Property* tableY = table->createProperty(DataBuffer::Initialized, propertyName, Property::FloatDefault);
            BufferWriteAccess<FloatType, access_mode::discard_write> tableYAcc(tableY);
            for(size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
                descriptorScalar[recordIndex] = records[recordIndex].scalar;
                tableYAcc[recordIndex] = records[recordIndex].scalar;
            }
            table->setY(tableY);
        }

        state.setAttribute(QStringLiteral("GeometryDescriptor.mode"),
                           descriptorLabel(descriptorMode), createdByNode);
        state.setAttribute(QStringLiteral("GeometryDescriptor.count"),
                           QVariant::fromValue(static_cast<double>(records.size())), createdByNode);
        state.setAttribute(QStringLiteral("GeometryDescriptor.candidate_molecules"),
                           QVariant::fromValue(static_cast<double>(candidateMoleculeCount)), createdByNode);

        QString statusText = tr("%1 geometry descriptors: generated %2 descriptor entries from %3 molecules.")
                                 .arg(descriptorLabel(descriptorMode))
                                 .arg(records.size())
                                 .arg(candidateMoleculeCount);
        if(descriptorMode == VectorDescriptor)
            statusText += normalizeVectors ? tr(" Vector output is normalized.") : tr(" Vector output keeps displacement units.");
        else
            statusText += tr(" Scalar output is in degrees.");
        if(templateMode)
            statusText += tr(" Atom roles were selected from template molecule ID %1.").arg(templateMoleculeId);
        statusText += tr(" A descriptor particle container and a data table were created for downstream analysis.");
        if(skippedSelectionCount > 0)
            statusText += tr(" %1 molecules were skipped by the selection filter.").arg(skippedSelectionCount);
        if(missingTupleCount > 0)
            statusText += tr(" %1 molecules did not contain a valid tuple.").arg(missingTupleCount);
        if(zeroVectorCount > 0)
            statusText += tr(" %1 zero-length vectors were skipped.").arg(zeroVectorCount);
        if(invalidGeometryCount > 0)
            statusText += tr(" %1 degenerate angles/dihedrals were skipped.").arg(invalidGeometryCount);
        if(usedFallbackDescriptorIds)
            statusText += tr(" Descriptor IDs use particle ordering because 'Particle Identifier' is missing.");

        state.setStatus(PipelineStatus(statusText, static_cast<qlonglong>(records.size())));
        return std::move(state);
    });
}

}  // namespace Ovito
