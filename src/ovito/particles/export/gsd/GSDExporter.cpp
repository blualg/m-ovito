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
#include <ovito/particles/objects/Angles.h>
#include <ovito/particles/objects/Dihedrals.h>
#include <ovito/particles/objects/Impropers.h>
#include <ovito/particles/import/gsd/GSDFile.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "GSDExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GSDExporter);
DEFINE_PROPERTY_FIELD(GSDExporter, dataType);
SET_PROPERTY_FIELD_LABEL(GSDExporter, dataType, "Output data type");

namespace {
template<std::floating_point T>
void exportFrameDataT(const PipelineFlowState& state, const int frameNumber, GSDFile* gsdFile)
{
    // Get particles.
    const Particles* particles = state.expectObject<Particles>();

    // Get simulation cell info.
    const SimulationCell* cell = state.expectObject<SimulationCell>();
    const AffineTransformation& simCell = cell->matrix();

    // Output simulation step.
    uint64_t timestep = state.getAttributeValue(QStringLiteral("Timestep"), frameNumber).toLongLong();
    gsdFile->writeChunk<uint64_t>("configuration/step", 1, 1, &timestep);

    // Output dimensionality of the particle system.
    if(cell->is2D()) {
        uint8_t dimensionality = 2;
        gsdFile->writeChunk<uint8_t>("configuration/dimensions", 1, 1, &dimensionality);
    }

    // Transform triclinic simulation cells to HOOMD canonical format.
    AffineTransformation hoomdCell;
    hoomdCell(0, 0) = simCell.column(0).length();
    hoomdCell(1, 0) = hoomdCell(2, 0) = 0;
    hoomdCell(0, 1) = simCell.column(1).dot(simCell.column(0)) / hoomdCell(0, 0);
    hoomdCell(1, 1) = sqrt(simCell.column(1).squaredLength() - hoomdCell(0, 1) * hoomdCell(0, 1));
    hoomdCell(2, 1) = 0;
    hoomdCell(0, 2) = simCell.column(2).dot(simCell.column(0)) / hoomdCell(0, 0);
    hoomdCell(1, 2) = (simCell.column(1).dot(simCell.column(2)) - hoomdCell(0, 1) * hoomdCell(0, 2)) / hoomdCell(1, 1);
    hoomdCell(2, 2) = sqrt(simCell.column(2).squaredLength() - hoomdCell(0, 2) * hoomdCell(0, 2) - hoomdCell(1, 2) * hoomdCell(1, 2));
    hoomdCell.translation() = hoomdCell.linear() * Vector3(FloatType(-0.5));
    AffineTransformation transformation = hoomdCell * simCell.inverse();

    // Output simulation cell geometry.
    const std::array<T, 6> box = {(T)hoomdCell(0, 0),
                                  (T)hoomdCell.column(1).length(),
                                  (T)hoomdCell.column(2).length(),
                                  (T)(hoomdCell(0, 1) / hoomdCell.column(1).length()),   // xy
                                  (T)(hoomdCell(0, 2) / hoomdCell.column(2).length()),   // xz
                                  (T)(hoomdCell(1, 2) / hoomdCell.column(2).length())};  // yz
    gsdFile->writeChunk<T>("configuration/box", 6, 1, box.data());

    // Output number of particles.
    if(particles->elementCount() > (size_t)std::numeric_limits<uint32_t>::max())
        throw Exception(QObject::tr("Number of particles exceeds maximum number supported by the GSD/HOOMD format."));
    uint32_t particleCount = particles->elementCount();
    gsdFile->writeChunk<uint32_t>("particles/N", 1, 1, &particleCount);
    this_task::throwIfCanceled();

    // Determine particle ordering.
    std::vector<size_t> ordering(particles->elementCount());
    boost::algorithm::iota(ordering, (size_t)0);
    if(BufferReadAccess<int64_t> idProperty = particles->getProperty(Particles::IdentifierProperty)) {
        std::ranges::sort(ordering, [&](size_t a, size_t b) { return idProperty[a] < idProperty[b]; });
    }
    this_task::throwIfCanceled();

    // Output particle coordinates.
    BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
    // Apply coordinate transformation matrix, wrapping a periodic box boundaries and data type conversion:
    std::vector<Point_3<T>> posBuffer(posProperty.size());
    std::vector<Vector_3<int32_t>> imageBuffer(posProperty.size());
    for(size_t i = 0; i < ordering.size(); i++) {
        const Point3& p = posProperty[ordering[i]];
        for(size_t dim = 0; dim < 3; dim++) {
            const FloatType s = std::floor(cell->inverseMatrix().prodrow(p, dim));
            posBuffer[i][dim] = static_cast<T>(transformation.prodrow(p - s * cell->matrix().column(dim), dim));
            imageBuffer[i][dim] = static_cast<int32_t>(s);
        }
    }
    gsdFile->writeChunk<T>("particles/position", posBuffer.size(), 3, posBuffer.data());
    this_task::throwIfCanceled();
    gsdFile->writeChunk<int32_t>("particles/image", imageBuffer.size(), 3, imageBuffer.data());
    this_task::throwIfCanceled();

    // Output particle types.
    if(const Property* typeProperty = particles->getProperty(Particles::TypeProperty)) {
        // GSD/HOOMD requires particle types to form a contiguous range starting at base index 0.
        std::map<int, int> idMapping;
        ConstPropertyPtr typeIds;
        std::tie(idMapping, typeIds) = typeProperty->generateContiguousTypeIdMapping(0);

        // Build list of type names.
        std::vector<QByteArray> typeNames(idMapping.size());
        int maxStringLength = 0;
        for(size_t i = 0; i < typeNames.size(); i++) {
            OVITO_ASSERT(idMapping.contains(i));
            if(const ElementType* ptype = typeProperty->elementType(idMapping[i])) typeNames[i] = ptype->name().toUtf8();
            if(typeNames[i].size() == 0 && i < 26) typeNames[i] = QByteArray(1, 'A' + (char)i);
            maxStringLength = qMax(maxStringLength, typeNames[i].size());
        }
        maxStringLength++;  // Include terminating null character.
        std::vector<int8_t> typeNameBuffer(maxStringLength * typeNames.size(), 0);
        for(size_t i = 0; i < typeNames.size(); i++) {
            std::copy(typeNames[i].cbegin(), typeNames[i].cend(), typeNameBuffer.begin() + (i * maxStringLength));
        }
        gsdFile->writeChunk<int8_t>("particles/types", typeNames.size(), maxStringLength, typeNameBuffer.data());

        // Build typeid array.
        BufferReadAccess<int32_t> typeIdsArray(typeIds);
        std::vector<uint32_t> typeIdBuffer(typeIdsArray.size());
        std::ranges::transform(ordering, typeIdBuffer.begin(), [&](size_t i) { return typeIdsArray[i]; });
        gsdFile->writeChunk<uint32_t>("particles/typeid", typeIdBuffer.size(), 1, typeIdBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle masses.
    if(BufferReadAccess<FloatType> massProperty = particles->getProperty(Particles::MassProperty)) {
        // Apply particle index mapping and data type conversion:
        std::vector<T> massBuffer(massProperty.size());
        std::ranges::transform(ordering, massBuffer.begin(), [&](size_t i) { return static_cast<T>(massProperty[i]); });
        gsdFile->writeChunk<T>("particles/mass", massBuffer.size(), 1, massBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle charges.
    if(BufferReadAccess<FloatType> chargeProperty = particles->getProperty(Particles::ChargeProperty)) {
        // Apply particle index mapping and data type conversion:
        std::vector<T> chargeBuffer(chargeProperty.size());
        std::ranges::transform(ordering, chargeBuffer.begin(), [&](size_t i) { return static_cast<T>(chargeProperty[i]); });
        gsdFile->writeChunk<T>("particles/charge", chargeBuffer.size(), 1, chargeBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle diameters.
    if(BufferReadAccess<GraphicsFloatType> radiusProperty = particles->getProperty(Particles::RadiusProperty)) {
        // Apply particle index mapping, data type conversion and
        // multiplying with a factor of 2 to convert from radii to diameters:
        std::vector<float> diameterBuffer(radiusProperty.size());
        std::ranges::transform(ordering, diameterBuffer.begin(), [&](size_t i) { return 2 * radiusProperty[i]; });
        gsdFile->writeChunk<float>("particles/diameter", diameterBuffer.size(), 1, diameterBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle orientations.
    if(BufferReadAccess<QuaternionG> orientationProperty = particles->getProperty(Particles::OrientationProperty)) {
        // Apply particle index mapping and data type conversion.
        // Also right-shift the quaternion components, because GSD uses a different representation.
        // (X,Y,Z,W) -> (W,X,Y,Z).
        std::vector<std::array<T, 4>> orientationBuffer(orientationProperty.size());
        std::ranges::transform(ordering, orientationBuffer.begin(), [&](size_t i) {
            const QuaternionG& q = orientationProperty[i];
            return std::array<T, 4>{{static_cast<T>(q.w()), static_cast<T>(q.x()), static_cast<T>(q.y()), static_cast<T>(q.z())}};
        });
        gsdFile->writeChunk<T>("particles/orientation", orientationBuffer.size(), 4, orientationBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle velocities.
    if(BufferReadAccess<Vector3> velocityProperty = particles->getProperty(Particles::VelocityProperty)) {
        // Apply particle index mapping and data type conversion:
        // Also apply affine transform of simulation cell to velocity vectors.
        std::vector<Vector_3<T>> velocityBuffer(velocityProperty.size());
        std::ranges::transform(
            ordering, velocityBuffer.begin(), [&](size_t i) { return (transformation * velocityProperty[i]).toDataType<T>(); });
        gsdFile->writeChunk<T>("particles/velocity", velocityBuffer.size(), 3, velocityBuffer.data());
        this_task::throwIfCanceled();
    }

    // Output particle angular momenta. Note: The GSDImporter currently stores these values in the user-defined particle property "angmom".
    if(const Property* angularMomentumProperty = particles->getProperty(QStringLiteral("angmom"))) {
        if(angularMomentumProperty->componentCount() == 4 &&
           (angularMomentumProperty->dataType() == Property::Float64 || angularMomentumProperty->dataType() == Property::Float32)) {
            // Output buffer
            std::vector<QuaternionT<T>> angMomBuffer(angularMomentumProperty->size());
            if(angularMomentumProperty->dataType() == Property::Float64) {
                BufferReadAccess<QuaternionT<double>> angularMomentumPropertyAccess(angularMomentumProperty);
                // Apply particle index mapping and data type conversion:
                std::ranges::transform(
                    ordering, angMomBuffer.begin(), [&](size_t i) { return angularMomentumPropertyAccess[i].toDataType<T>(); });
            }
            else if(angularMomentumProperty->dataType() == Property::Float32) {
                BufferReadAccess<QuaternionT<float>> angularMomentumPropertyAccess(angularMomentumProperty);
                // Apply particle index mapping and data type conversion:
                std::ranges::transform(
                    ordering, angMomBuffer.begin(), [&](size_t i) { return angularMomentumPropertyAccess[i].toDataType<T>(); });
            }
            else {
                OVITO_ASSERT(false);
                throw Exception(QObject::tr("GSD/HOOMD file export error: Unsupported data type for 'angmom'."));
            }
            gsdFile->writeChunk<T>("particles/angmom", angMomBuffer.size(), 4, angMomBuffer.data());
            this_task::throwIfCanceled();
        }
    }

    // Output particle body property. Note: The GSDImporter currently stores the values in the user-defined particle property "body".
    if(const Property* bodyProperty = particles->getProperty(QStringLiteral("body"))) {
        if(bodyProperty->dataType() == Property::Int32 && bodyProperty->componentCount() == 1) {
            BufferReadAccess<int32_t> bodyPropertyAccess(bodyProperty);
            // Apply particle index mapping:
            std::vector<int> bodyBuffer(bodyProperty->size());
            std::ranges::transform(ordering, bodyBuffer.begin(), [&](size_t i) { return bodyPropertyAccess[i]; });
            gsdFile->writeChunk<int>("particles/body", bodyBuffer.size(), 1, bodyBuffer.data());
            this_task::throwIfCanceled();
        }
    }

    std::vector<size_t> reverseOrdering;

    // Export bonds (optional).
    if(const Bonds* bonds = particles->bonds()) {
        BufferReadAccess<ParticleIndexPair> bondTopologyProperty = bonds->expectProperty(Bonds::TopologyProperty);

        // Output number of bonds.
        if(bonds->elementCount() > (size_t)std::numeric_limits<uint32_t>::max())
            throw Exception(QObject::tr("Number of bonds exceeds maximum number supported by the GSD/HOOMD format."));
        uint32_t bondsCount = bonds->elementCount();
        gsdFile->writeChunk<uint32_t>("bonds/N", 1, 1, &bondsCount);
        this_task::throwIfCanceled();

        // Build reverse mapping of particle indices.
        if(reverseOrdering.empty()) {
            reverseOrdering.resize(ordering.size());
            for(size_t i = 0; i < ordering.size(); i++) reverseOrdering[ordering[i]] = i;
        }

        // Output topology array.
        std::vector<std::array<uint32_t, 2>> bondsBuffer(bondTopologyProperty.size());
        for(size_t i = 0; i < bondTopologyProperty.size(); i++) {
            size_t a = bondTopologyProperty[i][0];
            size_t b = bondTopologyProperty[i][1];
            if(a >= reverseOrdering.size() || b >= reverseOrdering.size())
                throw Exception(QObject::tr("GSD/HOOMD file export error: Particle indices in bond topology array are out of range."));
            bondsBuffer[i][0] = reverseOrdering[a];
            bondsBuffer[i][1] = reverseOrdering[b];
        }
        gsdFile->writeChunk<uint32_t>("bonds/group", bondsBuffer.size(), 2, bondsBuffer.data());
        this_task::throwIfCanceled();

        // Output bond types.
        if(const Property* typeProperty = bonds->getProperty(Bonds::TypeProperty)) {
            // GSD/HOOMD requires bond types to form a contiguous range starting at base index 0.
            std::map<int, int> idMapping;
            ConstPropertyPtr typeIds;
            std::tie(idMapping, typeIds) = typeProperty->generateContiguousTypeIdMapping(0);

            // Build list of type names.
            std::vector<QByteArray> typeNames(idMapping.size());
            int maxStringLength = 0;
            for(size_t i = 0; i < typeNames.size(); i++) {
                OVITO_ASSERT(idMapping.contains(i));
                if(const ElementType* ptype = typeProperty->elementType(idMapping[i])) typeNames[i] = ptype->name().toUtf8();
                if(typeNames[i].size() == 0 && i < 26) typeNames[i] = QByteArray(1, 'A' + (char)i);
                maxStringLength = qMax(maxStringLength, typeNames[i].size());
            }
            maxStringLength++;  // Include terminating null character.
            std::vector<int8_t> typeNameBuffer(maxStringLength * typeNames.size(), 0);
            for(size_t i = 0; i < typeNames.size(); i++) {
                std::copy(typeNames[i].cbegin(), typeNames[i].cend(), typeNameBuffer.begin() + (i * maxStringLength));
            }
            gsdFile->writeChunk<int8_t>("bonds/types", typeNames.size(), maxStringLength, typeNameBuffer.data());

            // Output typeid array.
            gsdFile->writeChunk<uint32_t>("bonds/typeid", typeIds->size(), 1, BufferReadAccess<int32_t>(typeIds).cbegin());
            this_task::throwIfCanceled();
        }
    }

    // Export angles (optional).
    if(const Angles* angles = particles->angles()) {
        BufferReadAccess<ParticleIndexTriplet> topologyProperty = angles->expectProperty(Angles::TopologyProperty);

        // Output number of angles.
        if(angles->elementCount() > (size_t)std::numeric_limits<uint32_t>::max())
            throw Exception(QObject::tr("Number of angles exceeds maximum number supported by the GSD/HOOMD format."));
        uint32_t anglesCount = angles->elementCount();
        gsdFile->writeChunk<uint32_t>("angles/N", 1, 1, &anglesCount);
        this_task::throwIfCanceled();

        // Build reverse mapping of particle indices.
        if(reverseOrdering.empty()) {
            reverseOrdering.resize(ordering.size());
            for(size_t i = 0; i < ordering.size(); i++) reverseOrdering[ordering[i]] = i;
        }

        // Output topology array.
        std::vector<std::array<uint32_t, 3>> anglesBuffer(topologyProperty.size());
        for(size_t i = 0; i < topologyProperty.size(); i++) {
            size_t a = topologyProperty[i][0];
            size_t b = topologyProperty[i][1];
            size_t c = topologyProperty[i][2];
            if(a >= reverseOrdering.size() || b >= reverseOrdering.size() || c >= reverseOrdering.size())
                throw Exception(QObject::tr("GSD/HOOMD file export error: Particle indices in angle topology array are out of range."));
            anglesBuffer[i][0] = reverseOrdering[a];
            anglesBuffer[i][1] = reverseOrdering[b];
            anglesBuffer[i][2] = reverseOrdering[c];
        }
        gsdFile->writeChunk<uint32_t>("angles/group", anglesBuffer.size(), 3, anglesBuffer.data());
        this_task::throwIfCanceled();

        // Output angle types.
        if(const Property* typeProperty = angles->getProperty(Angles::TypeProperty)) {
            // GSD/HOOMD requires angle types to form a contiguous range starting at base index 0.
            std::map<int, int> idMapping;
            ConstPropertyPtr typeIds;
            std::tie(idMapping, typeIds) = typeProperty->generateContiguousTypeIdMapping(0);

            // Build list of type names.
            std::vector<QByteArray> typeNames(idMapping.size());
            int maxStringLength = 0;
            for(size_t i = 0; i < typeNames.size(); i++) {
                OVITO_ASSERT(idMapping.contains(i));
                if(const ElementType* ptype = typeProperty->elementType(idMapping[i])) typeNames[i] = ptype->name().toUtf8();
                if(typeNames[i].size() == 0 && i < 26) typeNames[i] = QByteArray(1, 'A' + (char)i);
                maxStringLength = qMax(maxStringLength, typeNames[i].size());
            }
            maxStringLength++;  // Include terminating null character.
            std::vector<int8_t> typeNameBuffer(maxStringLength * typeNames.size(), 0);
            for(size_t i = 0; i < typeNames.size(); i++) {
                std::copy(typeNames[i].cbegin(), typeNames[i].cend(), typeNameBuffer.begin() + (i * maxStringLength));
            }
            gsdFile->writeChunk<int8_t>("angles/types", typeNames.size(), maxStringLength, typeNameBuffer.data());

            // Output typeid array.
            gsdFile->writeChunk<uint32_t>("angles/typeid", typeIds->size(), 1, BufferReadAccess<int32_t>(typeIds).cbegin());
            this_task::throwIfCanceled();
        }
    }

    // Export dihedrals (optional).
    if(const Dihedrals* dihedrals = particles->dihedrals()) {
        BufferReadAccess<ParticleIndexQuadruplet> topologyProperty = dihedrals->expectProperty(Dihedrals::TopologyProperty);

        // Output number of dihedrals.
        if(dihedrals->elementCount() > (size_t)std::numeric_limits<uint32_t>::max())
            throw Exception(QObject::tr("Number of dihedrals exceeds maximum number supported by the GSD/HOOMD format."));
        uint32_t dihedralsCount = dihedrals->elementCount();
        gsdFile->writeChunk<uint32_t>("dihedrals/N", 1, 1, &dihedralsCount);
        this_task::throwIfCanceled();

        // Build reverse mapping of particle indices.
        if(reverseOrdering.empty()) {
            reverseOrdering.resize(ordering.size());
            for(size_t i = 0; i < ordering.size(); i++) reverseOrdering[ordering[i]] = i;
        }

        // Output topology array.
        std::vector<std::array<uint32_t, 4>> dihedralsBuffer(topologyProperty.size());
        for(size_t i = 0; i < topologyProperty.size(); i++) {
            size_t a = topologyProperty[i][0];
            size_t b = topologyProperty[i][1];
            size_t c = topologyProperty[i][2];
            size_t d = topologyProperty[i][3];
            if(a >= reverseOrdering.size() || b >= reverseOrdering.size() || c >= reverseOrdering.size() || d >= reverseOrdering.size())
                throw Exception(QObject::tr("GSD/HOOMD file export error: Particle indices in dihedral topology array are out of range."));
            dihedralsBuffer[i][0] = reverseOrdering[a];
            dihedralsBuffer[i][1] = reverseOrdering[b];
            dihedralsBuffer[i][2] = reverseOrdering[c];
            dihedralsBuffer[i][3] = reverseOrdering[d];
        }
        gsdFile->writeChunk<uint32_t>("dihedrals/group", dihedralsBuffer.size(), 4, dihedralsBuffer.data());
        this_task::throwIfCanceled();

        // Output dihedral types.
        if(const Property* typeProperty = dihedrals->getProperty(Dihedrals::TypeProperty)) {
            // GSD/HOOMD requires dihedral types to form a contiguous range starting at base index 0.
            std::map<int, int> idMapping;
            ConstPropertyPtr typeIds;
            std::tie(idMapping, typeIds) = typeProperty->generateContiguousTypeIdMapping(0);

            // Build list of type names.
            std::vector<QByteArray> typeNames(idMapping.size());
            int maxStringLength = 0;
            for(size_t i = 0; i < typeNames.size(); i++) {
                OVITO_ASSERT(idMapping.contains(i));
                if(const ElementType* ptype = typeProperty->elementType(idMapping[i])) typeNames[i] = ptype->name().toUtf8();
                if(typeNames[i].size() == 0 && i < 26) typeNames[i] = QByteArray(1, 'A' + (char)i);
                maxStringLength = qMax(maxStringLength, typeNames[i].size());
            }
            maxStringLength++;  // Include terminating null character.
            std::vector<int8_t> typeNameBuffer(maxStringLength * typeNames.size(), 0);
            for(size_t i = 0; i < typeNames.size(); i++) {
                std::copy(typeNames[i].cbegin(), typeNames[i].cend(), typeNameBuffer.begin() + (i * maxStringLength));
            }
            gsdFile->writeChunk<int8_t>("dihedrals/types", typeNames.size(), maxStringLength, typeNameBuffer.data());

            // Output typeid array.
            gsdFile->writeChunk<uint32_t>("dihedrals/typeid", typeIds->size(), 1, BufferReadAccess<int32_t>(typeIds).cbegin());
            this_task::throwIfCanceled();
        }
    }

    // Export impropers (optional).
    if(const Impropers* impropers = particles->impropers()) {
        BufferReadAccess<ParticleIndexQuadruplet> topologyProperty = impropers->expectProperty(Impropers::TopologyProperty);

        // Output number of impropers.
        if(impropers->elementCount() > (size_t)std::numeric_limits<uint32_t>::max())
            throw Exception(QObject::tr("Number of impropers exceeds maximum number supported by the GSD/HOOMD format."));
        uint32_t impropersCount = impropers->elementCount();
        gsdFile->writeChunk<uint32_t>("impropers/N", 1, 1, &impropersCount);
        this_task::throwIfCanceled();

        // Build reverse mapping of particle indices.
        if(reverseOrdering.empty()) {
            reverseOrdering.resize(ordering.size());
            for(size_t i = 0; i < ordering.size(); i++) reverseOrdering[ordering[i]] = i;
        }

        // Output topology array.
        std::vector<std::array<uint32_t, 4>> impropersBuffer(topologyProperty.size());
        for(size_t i = 0; i < topologyProperty.size(); i++) {
            size_t a = topologyProperty[i][0];
            size_t b = topologyProperty[i][1];
            size_t c = topologyProperty[i][2];
            size_t d = topologyProperty[i][3];
            if(a >= reverseOrdering.size() || b >= reverseOrdering.size() || c >= reverseOrdering.size() || d >= reverseOrdering.size())
                throw Exception(QObject::tr("GSD/HOOMD file export error: Particle indices in improper topology array are out of range."));
            impropersBuffer[i][0] = reverseOrdering[a];
            impropersBuffer[i][1] = reverseOrdering[b];
            impropersBuffer[i][2] = reverseOrdering[c];
            impropersBuffer[i][3] = reverseOrdering[d];
        }
        gsdFile->writeChunk<uint32_t>("impropers/group", impropersBuffer.size(), 4, impropersBuffer.data());
        this_task::throwIfCanceled();

        // Output improper types.
        if(const Property* typeProperty = impropers->getProperty(Impropers::TypeProperty)) {
            // GSD/HOOMD requires improper types to form a contiguous range starting at base index 0.
            std::map<int, int> idMapping;
            ConstPropertyPtr typeIds;
            std::tie(idMapping, typeIds) = typeProperty->generateContiguousTypeIdMapping(0);

            // Build list of type names.
            std::vector<QByteArray> typeNames(idMapping.size());
            int maxStringLength = 0;
            for(size_t i = 0; i < typeNames.size(); i++) {
                OVITO_ASSERT(idMapping.contains(i));
                if(const ElementType* ptype = typeProperty->elementType(idMapping[i])) typeNames[i] = ptype->name().toUtf8();
                if(typeNames[i].size() == 0 && i < 26) typeNames[i] = QByteArray(1, 'A' + (char)i);
                maxStringLength = qMax(maxStringLength, typeNames[i].size());
            }
            maxStringLength++;  // Include terminating null character.
            std::vector<int8_t> typeNameBuffer(maxStringLength * typeNames.size(), 0);
            for(size_t i = 0; i < typeNames.size(); i++) {
                std::copy(typeNames[i].cbegin(), typeNames[i].cend(), typeNameBuffer.begin() + (i * maxStringLength));
            }
            gsdFile->writeChunk<int8_t>("impropers/types", typeNames.size(), maxStringLength, typeNameBuffer.data());

            // Output typeid array.
            gsdFile->writeChunk<uint32_t>("impropers/typeid", typeIds->size(), 1, BufferReadAccess<int32_t>(typeIds).cbegin());
            this_task::throwIfCanceled();
        }
    }

    // Close the current frame that has been written to the GSD file.
    gsdFile->endFrame();
}
}  // namespace

/******************************************************************************
 * Creates a worker performing the actual data export.
 *****************************************************************************/
OORef<FileExportJob> GSDExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    private:
        /// The GSD file object.
        std::unique_ptr<GSDFile> _gsdFile;

    public:
        /// Constructor.
        void initializeObject(const FileExporter* exporter, const QString& filePath)
        {
            FileExportJob::initializeObject(exporter, filePath, false);

            // Open the input file for writing.
            _gsdFile = GSDFile::create(QDir::toNativeSeparators(filePath).toUtf8().constData(), "ovito", "hoomd", 1, 4);
        }

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(boost::anys::unique_any frameData,
                                               int frameNumber,
                                               const QString& filePath,
                                               TaskProgress& progress) override
        {
            // The exportable frame data.
            OVITO_ASSERT(frameData.has_value());
            const auto state = boost::anys::any_cast<PipelineFlowState>(frameData);

            const GSDExporter::DataType exportDataType = static_cast<const GSDExporter*>(this->exporter())->dataType();

            // Perform the following in a worker thread.
            co_await ExecutorAwaiter(ThreadPoolExecutor());

            switch(exportDataType) {
                case GSDExporter::DataType::Float32: exportFrameDataT<float>(state, frameNumber, _gsdFile.get()); break;
                case GSDExporter::DataType::Float64: exportFrameDataT<double>(state, frameNumber, _gsdFile.get()); break;
                default: OVITO_ASSERT(false); throw Exception(QObject::tr("GSD/HOOMD file export error: Unsupported data type."));
            }
        }
    };

    return OORef<Job>::create(this, filePath);
}

}   // End of namespace
