////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/particles/objects/BondType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/mesh/util/CapPolygonTessellator.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include "GSDImporter.h"
#include "GSDFile.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GSDImporter);
OVITO_CLASSINFO(GSDImporter, "DisplayName", "GSD");
DEFINE_PROPERTY_FIELD(GSDImporter, roundingResolution);
SET_PROPERTY_FIELD_LABEL(GSDImporter, roundingResolution, "Shape rounding resolution");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(GSDImporter, roundingResolution, IntegerParameterUnit, 1, 6);

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void GSDImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
    ParticleImporter::propertyChanged(field);

    if(field == PROPERTY_FIELD(roundingResolution) && !isBeingLoaded()) {
        // Clear shape cache and reload GSD file when the rounding resolution is changed.
        _cacheSynchronization.lockForWrite();
        _particleShapeCache.clear();
        _cacheSynchronization.unlock();
        requestReload();
    }
}

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool GSDImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    QString filename = QDir::toNativeSeparators(file.localFilePath());
    if(!filename.isEmpty() && !filename.startsWith(QChar(':'))) {
        gsd_handle handle;
#ifndef Q_OS_WIN
        if(::gsd_open(&handle, QFile::encodeName(filename).constData(), GSD_OPEN_READONLY) == gsd_error::GSD_SUCCESS) {
#else
        if(::gsd_open(&handle, filename.toStdWString().c_str(), GSD_OPEN_READONLY) == gsd_error::GSD_SUCCESS) {
#endif
            ::gsd_close(&handle);
            return true;
        }
    }

    return false;
}

/******************************************************************************
* Stores the particle shape geometry generated from a JSON string in the internal cache.
******************************************************************************/
void GSDImporter::storeParticleShapeInCache(const QByteArray& jsonString, const DataOORef<const TriangleMesh>& mesh)
{
    QWriteLocker locker(&_cacheSynchronization);
    _particleShapeCache.insert(jsonString, mesh);
}

/******************************************************************************
* Looks up a particle shape geometry in the internal cache that was previously
* generated from a JSON string.
******************************************************************************/
DataOORef<const TriangleMesh> GSDImporter::lookupParticleShapeInCache(const QByteArray& jsonString) const
{
    QReadLocker locker(&_cacheSynchronization);
    if(auto iter = _particleShapeCache.find(jsonString); iter != _particleShapeCache.end())
        return iter.value();
    else
        return {};
}

/******************************************************************************
* Scans the input file for simulation timesteps.
******************************************************************************/
void GSDImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Scanning file %1").arg(fileHandle.toString()));

    // First close text stream, we don't need it here.
    QString filename = QDir::toNativeSeparators(fileHandle.localFilePath());
    if(filename.isEmpty())
        throw Exception(tr("The GSD file reader supports reading only from physical files. Cannot read data from an in-memory buffer."));

    // Open GSD file for reading.
#ifndef Q_OS_WIN
    GSDFile gsd(QFile::encodeName(filename).constData());
#else
    GSDFile gsd(filename.toStdWString().c_str());
#endif
    uint64_t nFrames = gsd.numerOfFrames();

    Frame frame(fileHandle);
    for(uint64_t i = 0; i < nFrames; i++) {
        uint64_t simulationStep = gsd.readOptionalScalar<uint64_t>("configuration/step", i, std::numeric_limits<uint64_t>::max());
        if(simulationStep != std::numeric_limits<uint64_t>::max())
            frame.label = QStringLiteral("Timestep %1").arg(simulationStep);
        else
            frame.label = QStringLiteral("Frame %1").arg(i);
        frame.byteOffset = i;
        frames.push_back(frame);
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void GSDImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Reading GSD file %1").arg(fileHandle().toString()));

    // Open GSD file for reading.
    QString filename = QDir::toNativeSeparators(fileHandle().localFilePath());
    if(filename.isEmpty())
        throw Exception(tr("The GSD file reader supports reading only from physical files. Cannot read data from an in-memory buffer."));
#ifndef Q_OS_WIN
    GSDFile gsd(QFile::encodeName(filename).constData());
#else
    GSDFile gsd(filename.toStdWString().c_str());
#endif

    // Check schema name.
    if(qstrcmp(gsd.schemaName(), "hoomd") != 0)
        throw Exception(tr("Failed to open GSD file for reading. File schema must be 'hoomd', but found '%1'.").arg(gsd.schemaName()));

    // The animation frame to read from the GSD file.
    uint64_t frameNumber = frame().byteOffset;

    // Parse simulation step.
    uint64_t simulationStep = gsd.readOptionalScalar<uint64_t>("configuration/step", frameNumber, 0);
    state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(simulationStep), pipelineNode());

    // Parse number of dimensions.
    uint8_t ndimensions = gsd.readOptionalScalar<uint8_t>("configuration/dimensions", frameNumber, 3);

    // Parse simulation box.
    std::array<float,6> boxValues = {1,1,1,0,0,0};
    gsd.readOptional1DArray("configuration/box", frameNumber, boxValues);
    AffineTransformation simCell = AffineTransformation::Identity();
    simCell(0,0) = boxValues[0];
    simCell(1,1) = boxValues[1];
    simCell(2,2) = boxValues[2];
    simCell(0,1) = boxValues[3] * boxValues[1];
    simCell(0,2) = boxValues[4] * boxValues[2];
    simCell(1,2) = boxValues[5] * boxValues[2];
    simCell.translation() = simCell.linear() * Vector3(FloatType(-0.5));
    if(ndimensions == 2)
        simulationCell()->setIs2D(true);
    simulationCell()->setCellMatrix(simCell);
    simulationCell()->setPbcFlags(true, true, true);

    // Parse number of particles.
    uint32_t numParticles = gsd.readOptionalScalar<uint32_t>("particles/N", frameNumber, 0);
    setParticleCount(numParticles);

    // Parse list of particle type names.
    QByteArrayList particleTypeNames = gsd.readStringTable("particles/types", frameNumber);
    if(particleTypeNames.empty())
        particleTypeNames.push_back(QByteArrayLiteral("A"));

    {
        // Read particle positions.
        BufferWriteAccess<FloatType*, access_mode::discard_write> posProperty = particles()->createProperty(Particles::PositionProperty);
        if(gsd.hasChunk("particles/position", frameNumber))
            gsd.readFloatArray("particles/position", frameNumber, posProperty.begin(), numParticles, posProperty.componentCount());
        else
            posProperty.take()->fill<Point3>(Point3::Origin());
        this_task::throwIfCanceled();
    }

    {
        // Create particle types.
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        for(int i = 0; i < particleTypeNames.size(); i++)
            addNumericType(Particles::OOClass(), typeProperty, i, QString::fromUtf8(particleTypeNames[i]));

        // Read particle types.
        if(gsd.hasChunk("particles/typeid", frameNumber))
            gsd.readIntArray("particles/typeid", frameNumber, BufferWriteAccess<int32_t, access_mode::discard_write>(typeProperty).begin(), numParticles);
        else
            typeProperty->fill<int32_t>(0);
        this_task::throwIfCanceled();
    }

    // Parse particle shape information.
    QByteArrayList particleTypeShapes = gsd.readStringTable("particles/type_shapes", frameNumber);
    if(particleTypeShapes.size() == particleTypeNames.size()) {
        for(int i = 0; i < particleTypeShapes.size(); i++) {
            this_task::throwIfCanceled();
            parseParticleShape(i, particleTypeShapes[i]);
        }
    }

    // Default property values specified by the HOOMD GSD schema (see https://gsd.readthedocs.io/en/stable/schema-hoomd.html#data-chunks):
    const FloatType defaultMass = 1.0;
    const FloatType defaultCharge = 0.0;
    const Vector3 defaultVelocity = Vector3::Zero();
    const Vector3I defaultImage = Vector3I::Zero();

    readOptionalProperty(gsd, "particles/mass", frameNumber, Particles::MassProperty, particles(), &defaultMass, sizeof(defaultMass));
    readOptionalProperty(gsd, "particles/charge", frameNumber, Particles::ChargeProperty, particles(), &defaultCharge, sizeof(defaultCharge));
    readOptionalProperty(gsd, "particles/velocity", frameNumber, Particles::VelocityProperty, particles(), &defaultVelocity, sizeof(defaultVelocity));
    readOptionalProperty(gsd, "particles/image", frameNumber, Particles::PeriodicImageProperty, particles(), &defaultImage, sizeof(defaultImage));

    const GraphicsFloatType defaultDiameter = 1;
    if(BufferWriteAccess<GraphicsFloatType, access_mode::discard_read_write> radiusProperty = readOptionalProperty(gsd, "particles/diameter", frameNumber, Particles::RadiusProperty, particles(), &defaultDiameter, sizeof(defaultDiameter))) {
        // Convert particle diameters to radii.
        for(auto& r : radiusProperty)
            r /= 2;
    }

    const QuaternionG identityQuaternion = QuaternionG(1,0,0,0);
    if(BufferWriteAccess<QuaternionG, access_mode::discard_read_write> orientationProperty = readOptionalProperty(gsd, "particles/orientation", frameNumber, Particles::OrientationProperty, particles(), &identityQuaternion, sizeof(identityQuaternion))) {
        // Convert quaternion representation from GSD format to OVITO's internal format.
        // Left-shift all quaternion components by one: (W,X,Y,Z) -> (X,Y,Z,W).
        for(auto& q : orientationProperty)
            std::rotate(q.begin(), q.begin() + 1, q.end());
    }

    // Read in "particles/angmom" chunk as user-defined property named "angmom". It's not clear how to map the HOOMD quaternion to OVITO's "Angular Momentum" vector property.
    // But it should be possible, see https://hoomd-blue.readthedocs.io/en/v2.9.5/aniso.html#quaternions-for-angular-momentum
    const Quaternion nullQuaternion = Quaternion(0,0,0,0);
    readOptionalProperty(gsd, "particles/angmom", frameNumber, Particles::UserProperty, particles(), &nullQuaternion, sizeof(nullQuaternion));

    // Read "particles/body" chunk.
    const int32_t defaultBody = -1;
    readOptionalProperty(gsd, "particles/body", frameNumber, Particles::UserProperty, particles(), &defaultBody, sizeof(defaultBody));

    // Read "particles/moment_inertia" chunk.
    const Vector3 defaultMomentInertia(0,0,0);
    readOptionalProperty(gsd, "particles/moment_inertia", frameNumber, Particles::UserProperty, particles(), &defaultMomentInertia, sizeof(defaultMomentInertia));

    this_task::throwIfCanceled();

    // Read any user-defined particle properties.
    const char* chunkName = gsd.findMatchingChunkName("log/particles/", nullptr);
    while(chunkName) {
        this_task::throwIfCanceled();
        readOptionalProperty(gsd, chunkName, frameNumber, Particles::UserProperty, particles(), nullptr, 0);
        chunkName = gsd.findMatchingChunkName("log/particles/", chunkName);
    }

    // Read any user-defined log chunks and add them to the global attributes dictionary.
    chunkName = gsd.findMatchingChunkName("log/", nullptr);
    while(chunkName) {
        QString key = QString::fromUtf8(chunkName + 4); // Cut off "log/" prefix
        state().setAttribute(key, gsd.readVariant(chunkName, frameNumber), pipelineNode());
        chunkName = gsd.findMatchingChunkName("log/", chunkName);
    }

    // Parse bonds.
    uint32_t numBonds = gsd.readOptionalScalar<uint32_t>("bonds/N", frameNumber, 0);
    setBondCount(numBonds);
    if(numBonds != 0) {
        // Read bonds list.
        std::vector<uint32_t> bondList(numBonds * 2);
        gsd.readIntArray("bonds/group", frameNumber, bondList.data(), numBonds, 2);
        this_task::throwIfCanceled();

        // Convert to OVITO format.
        BufferWriteAccess<ParticleIndexPair, access_mode::discard_write> bondTopologyProperty = bonds()->createProperty(Bonds::TopologyProperty);
        auto bondTopoPtr = bondList.cbegin();
        for(ParticleIndexPair& bond : bondTopologyProperty) {
            if(*bondTopoPtr >= (uint32_t)numParticles)
                throw Exception(tr("Nonexistent atom tag in bond list in GSD file."));
            bond[0] = *bondTopoPtr++;
            if(*bondTopoPtr >= (uint32_t)numParticles)
                throw Exception(tr("Nonexistent atom tag in bond list in GSD file."));
            bond[1] = *bondTopoPtr++;
        }
        bondTopologyProperty.reset();
        generateBondPeriodicImageProperty();
        this_task::throwIfCanceled();

        // Read types.
        if(gsd.hasChunk("bonds/types", frameNumber)) {

            // Parse list of bond type names.
            QByteArrayList bondTypeNames = gsd.readStringTable("bonds/types", frameNumber);
            if(bondTypeNames.empty())
                bondTypeNames.push_back(QByteArrayLiteral("A"));

            // Create bond types.
            Property* bondTypeProperty = bonds()->createProperty(Bonds::TypeProperty);
            for(int i = 0; i < bondTypeNames.size(); i++)
                addNumericType(Bonds::OOClass(), bondTypeProperty, i, QString::fromUtf8(bondTypeNames[i]));

            // Read bond types.
            if(gsd.hasChunk("bonds/typeid", frameNumber)) {
                gsd.readIntArray("bonds/typeid", frameNumber, BufferWriteAccess<int32_t, access_mode::discard_write>(bondTypeProperty).begin(), numBonds);
            }
            else {
                bondTypeProperty->fill<int32_t>(0);
            }
            this_task::throwIfCanceled();
        }

        // Read any user-defined properties.
        const char* chunkName = gsd.findMatchingChunkName("log/bonds/", nullptr);
        while(chunkName) {
            this_task::throwIfCanceled();
            readOptionalProperty(gsd, chunkName, frameNumber, Bonds::UserProperty, bonds(), nullptr, 0);
            chunkName = gsd.findMatchingChunkName("log/bonds/", chunkName);
        }
    }

    // Parse angles.
    uint32_t numAngles = gsd.readOptionalScalar<uint32_t>("angles/N", frameNumber, 0);
    setAngleCount(numAngles);
    if(numAngles != 0) {
        // Read angles list.
        std::vector<uint32_t> groupList(numAngles * 3);
        gsd.readIntArray("angles/group", frameNumber, groupList.data(), numAngles, 3);
        this_task::throwIfCanceled();

        // Convert to OVITO format.
        BufferWriteAccess<ParticleIndexTriplet, access_mode::discard_write> topologyProperty = angles()->createProperty(Angles::TopologyProperty);
        auto topoPtr = groupList.cbegin();
        for(ParticleIndexTriplet& angle : topologyProperty) {
            for(auto& idx : angle) {
                if(*topoPtr >= (uint32_t)numParticles)
                    throw Exception(tr("Nonexistent atom tag in angles list in GSD file."));
                idx = *topoPtr++;
            }
        }
        topologyProperty.reset();
        this_task::throwIfCanceled();

        // Read types.
        if(gsd.hasChunk("angles/types", frameNumber)) {

            // Parse list of type names.
            QByteArrayList typeNames = gsd.readStringTable("angles/types", frameNumber);
            if(typeNames.empty())
                typeNames.push_back(QByteArrayLiteral("A"));

            // Create element types.
            Property* typeProperty = angles()->createProperty(Angles::TypeProperty);
            for(int i = 0; i < typeNames.size(); i++)
                addNumericType(Angles::OOClass(), typeProperty, i, QString::fromUtf8(typeNames[i]));

            // Read element types.
            if(gsd.hasChunk("angles/typeid", frameNumber)) {
                gsd.readIntArray("angles/typeid", frameNumber, BufferWriteAccess<int32_t, access_mode::discard_write>(typeProperty).begin(), numAngles);
            }
            else {
                typeProperty->fill<int32_t>(0);
            }
            this_task::throwIfCanceled();
        }

        // Read any user-defined properties.
        const char* chunkName = gsd.findMatchingChunkName("log/angles/", nullptr);
        while(chunkName) {
            this_task::throwIfCanceled();
            readOptionalProperty(gsd, chunkName, frameNumber, Angles::UserProperty, angles(), nullptr, 0);
            chunkName = gsd.findMatchingChunkName("log/angles/", chunkName);
        }
    }

    // Parse dihedrals.
    uint32_t numDihedrals = gsd.readOptionalScalar<uint32_t>("dihedrals/N", frameNumber, 0);
    setDihedralCount(numDihedrals);
    if(numDihedrals != 0) {
        // Read dihedrals list.
        std::vector<uint32_t> groupList(numDihedrals * 4);
        gsd.readIntArray("dihedrals/group", frameNumber, groupList.data(), numDihedrals, 4);
        this_task::throwIfCanceled();

        // Convert to OVITO format.
        BufferWriteAccess<ParticleIndexQuadruplet, access_mode::discard_write> topologyProperty = dihedrals()->createProperty(Dihedrals::TopologyProperty);
        auto topoPtr = groupList.cbegin();
        for(ParticleIndexQuadruplet& dihedral : topologyProperty) {
            for(int64_t& idx : dihedral) {
                if(*topoPtr >= (uint32_t)numParticles)
                    throw Exception(tr("Nonexistent atom tag in dihedrals list in GSD file."));
                idx = *topoPtr++;
            }
        }
        topologyProperty.reset();
        this_task::throwIfCanceled();

        // Read types.
        if(gsd.hasChunk("dihedrals/types", frameNumber)) {

            // Parse list of type names.
            QByteArrayList typeNames = gsd.readStringTable("dihedrals/types", frameNumber);
            if(typeNames.empty())
                typeNames.push_back(QByteArrayLiteral("A"));

            // Create element types.
            Property* typeProperty = dihedrals()->createProperty(Dihedrals::TypeProperty);
            for(int i = 0; i < typeNames.size(); i++)
                addNumericType(Dihedrals::OOClass(), typeProperty, i, QString::fromUtf8(typeNames[i]));

            // Read element types.
            if(gsd.hasChunk("dihedrals/typeid", frameNumber)) {
                gsd.readIntArray("dihedrals/typeid", frameNumber, BufferWriteAccess<int32_t, access_mode::discard_write>(typeProperty).begin(), numDihedrals);
            }
            else {
                typeProperty->fill<int32_t>(0);
            }
            this_task::throwIfCanceled();
        }

        // Read any user-defined properties.
        const char* chunkName = gsd.findMatchingChunkName("log/dihedrals/", nullptr);
        while(chunkName) {
            this_task::throwIfCanceled();
            readOptionalProperty(gsd, chunkName, frameNumber, Dihedrals::UserProperty, dihedrals(), nullptr, 0);
            chunkName = gsd.findMatchingChunkName("log/dihedrals/", chunkName);
        }
    }

    // Parse impropers.
    uint32_t numImpropers = gsd.readOptionalScalar<uint32_t>("impropers/N", frameNumber, 0);
    setImproperCount(numImpropers);
    if(numImpropers != 0) {
        // Read impropers list.
        std::vector<uint32_t> groupList(numImpropers * 4);
        gsd.readIntArray("impropers/group", frameNumber, groupList.data(), numImpropers, 4);
        this_task::throwIfCanceled();

        // Convert to OVITO format.
        BufferWriteAccess<ParticleIndexQuadruplet, access_mode::discard_write> topologyProperty = impropers()->createProperty(Impropers::TopologyProperty);
        auto topoPtr = groupList.cbegin();
        for(ParticleIndexQuadruplet& improper : topologyProperty) {
            for(int64_t& idx : improper) {
                if(*topoPtr >= (uint32_t)numParticles)
                    throw Exception(tr("Nonexistent atom tag in impropers list in GSD file."));
                idx = *topoPtr++;
            }
        }
        topologyProperty.reset();
        this_task::throwIfCanceled();

        // Read types.
        if(gsd.hasChunk("impropers/types", frameNumber)) {

            // Parse list of type names.
            QByteArrayList typeNames = gsd.readStringTable("impropers/types", frameNumber);
            if(typeNames.empty())
                typeNames.push_back(QByteArrayLiteral("A"));

            // Create element types.
            Property* typeProperty = impropers()->createProperty(Impropers::TypeProperty);
            for(int i = 0; i < typeNames.size(); i++)
                addNumericType(Impropers::OOClass(), typeProperty, i, QString::fromUtf8(typeNames[i]));

            // Read element types.
            if(gsd.hasChunk("impropers/typeid", frameNumber)) {
                gsd.readIntArray("impropers/typeid", frameNumber, BufferWriteAccess<int32_t, access_mode::discard_write>(typeProperty).begin(), numImpropers);
            }
            else {
                typeProperty->fill<int32_t>(0);
            }
            this_task::throwIfCanceled();
        }

        // Read any user-defined properties.
        const char* chunkName = gsd.findMatchingChunkName("log/impropers/", nullptr);
        while(chunkName) {
            this_task::throwIfCanceled();
            readOptionalProperty(gsd, chunkName, frameNumber, Impropers::UserProperty, impropers(), nullptr, 0);
            chunkName = gsd.findMatchingChunkName("log/impropers/", chunkName);
        }
    }

    QString statusString = tr("Number of particles: %1").arg(numParticles);
    if(numBonds != 0)
        statusString += tr("\nNumber of bonds: %1").arg(numBonds);
    if(numAngles != 0)
        statusString += tr("\nNumber of angles: %1").arg(numAngles);
    if(numDihedrals != 0)
        statusString += tr("\nNumber of dihedrals: %1").arg(numDihedrals);
    if(numImpropers != 0)
        statusString += tr("\nNumber of impropers: %1").arg(numImpropers);
    state().setStatus(statusString);

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Reads the values of a particle or bond property from the GSD file.
******************************************************************************/
Property* GSDImporter::FrameLoader::readOptionalProperty(GSDFile& gsd, const char* chunkName, uint64_t frameNumber, int propertyType, PropertyContainer* container, const void* defaultValue, size_t defaultValueSize)
{
    Property* prop = nullptr;
    if(gsd.hasChunk(chunkName, frameNumber)) {
        if(propertyType != Property::GenericUserProperty) {
            prop = container->createProperty(propertyType);
        }
        else {
            QString propertyName = QString::fromUtf8(chunkName);
            int slashPos = propertyName.lastIndexOf(QChar('/'));
            if(slashPos != -1) propertyName.remove(0, slashPos + 1);
            std::pair<int, size_t> dataTypeAndComponents = gsd.getChunkDataTypeAndComponentCount(chunkName);
            prop = container->createProperty(Property::makePropertyNameValid(propertyName), dataTypeAndComponents.first, dataTypeAndComponents.second);
        }
        if(prop->dataType() == Property::Float32)
            gsd.readFloatArray(chunkName, frameNumber, BufferWriteAccess<float*, access_mode::discard_write>(prop).begin(), container->elementCount(), prop->componentCount());
        else if(prop->dataType() == Property::Float64)
            gsd.readFloatArray(chunkName, frameNumber, BufferWriteAccess<double*, access_mode::discard_write>(prop).begin(), container->elementCount(), prop->componentCount());
        else if(prop->dataType() == Property::Int8)
            gsd.readIntArray(chunkName, frameNumber, BufferWriteAccess<int8_t*, access_mode::discard_write>(prop).begin(), container->elementCount(), prop->componentCount());
        else if(prop->dataType() == Property::Int32)
            gsd.readIntArray(chunkName, frameNumber, BufferWriteAccess<int32_t*, access_mode::discard_write>(prop).begin(), container->elementCount(), prop->componentCount());
        else if(prop->dataType() == Property::Int64)
            gsd.readIntArray(chunkName, frameNumber, BufferWriteAccess<int64_t*, access_mode::discard_write>(prop).begin(), container->elementCount(), prop->componentCount());
        else
            throw Exception(tr("Property '%1' cannot be read from GSD file, because its data type is not supported by OVITO.").arg(prop->name()));
    }
    else if(defaultValue != nullptr) {
        const char* found_name = gsd.findMatchingChunkName(chunkName, nullptr);
        if(found_name && qstrcmp(found_name, chunkName) == 0) {
            // If the GSD file contains the requested chunk in some other trajectory frame(s), just not in the current frame, then
            // fill the property array with the default value for that chunk as specified by the HOOMD standard.
            if(propertyType != Property::GenericUserProperty) {
                prop = container->createProperty(DataBuffer::Uninitialized, propertyType);
            }
            else {
                QString propertyName = QString::fromUtf8(chunkName);
                int slashPos = propertyName.lastIndexOf(QChar('/'));
                if(slashPos != -1)
                    propertyName.remove(0, slashPos + 1);
                std::pair<int, size_t> dataTypeAndComponents = gsd.getChunkDataTypeAndComponentCount(chunkName);
                prop = container->createProperty(DataBuffer::Uninitialized, Property::makePropertyNameValid(propertyName), dataTypeAndComponents.first, dataTypeAndComponents.second);
            }
            if(prop->stride() == defaultValueSize) {
                RawBufferAccess<access_mode::discard_write> access(prop);
                auto* dest = access.data();
                for(size_t i = 0; i < prop->size(); i++, dest += defaultValueSize) {
                    std::memcpy(dest, defaultValue, defaultValueSize);
                }
            }
            else {
                prop->fillZero();
            }
        }
    }
    return prop;
}

/******************************************************************************
* Assigns a mesh-based shape to a particle type.
******************************************************************************/
void GSDImporter::FrameLoader::setParticleTypeShape(int typeId, DataOORef<const TriangleMesh> shapeMesh)
{
    const Property* existingTypeProperty = particles()->expectProperty(Particles::TypeProperty);
    const ParticleType* existingType = static_object_cast<ParticleType>(existingTypeProperty->elementType(typeId));
    OVITO_ASSERT(existingType);

    // Check whether the shape mesh is already assigned to the existing particle type.
    if(!existingType || existingType->shapeMesh() == shapeMesh)
        return;

    // Assign the shape to the particle type.
    Property* typeProperty = particles()->makeMutable(existingTypeProperty);
    ParticleType* mutableType = typeProperty->makeMutable(existingType);
    mutableType->setShapeMesh(std::move(shapeMesh));
    mutableType->setShape(ParticlesVis::ParticleShape::Mesh);
    mutableType->setRadius(1.0);
    mutableType->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::radius), SHADOW_PROPERTY_FIELD(ParticleType::shape)});
}

/******************************************************************************
* Parse a JSON string containing a particle shape definition.
******************************************************************************/
void GSDImporter::FrameLoader::parseParticleShape(int typeId, const QByteArray& shapeSpecString)
{
    // Check if an existing geometry is already stored in the cache for the JSON string.
    DataOORef<const TriangleMesh> cacheShapeMesh = _importer->lookupParticleShapeInCache(shapeSpecString);
    if(cacheShapeMesh) {
        // Assign shape to particle type.
        setParticleTypeShape(typeId, std::move(cacheShapeMesh));
        return; // No need to parse the JSON string again.
    }

    // Parse the JSON string.
    QJsonParseError parsingError;
    QJsonDocument shapeSpec = QJsonDocument::fromJson(shapeSpecString, &parsingError);
    if(shapeSpec.isNull())
        throw Exception(tr("Invalid particle shape specification string in GSD file: %1").arg(parsingError.errorString()));

    // Empty JSON documents are ignored (assuming spherical particle shape with default radius).
    if(!shapeSpec.isObject() || shapeSpec.object().isEmpty())
        return;

    // Parse the "type" field.
    QString shapeType = shapeSpec.object().value("type").toString();
    if(shapeType.isNull() || shapeType.isEmpty())
        throw Exception(tr("Missing 'type' field in particle shape specification in GSD file."));

    if(shapeType == "Sphere") {
        parseSphereShape(typeId, shapeSpec.object());
    }
    else if(shapeType == "Ellipsoid") {
        parseEllipsoidShape(typeId, shapeSpec.object());
    }
    else if(shapeType == "Polygon") {
        parsePolygonShape(typeId, shapeSpec.object(), shapeSpecString);
    }
    else if(shapeType == "ConvexPolyhedron") {
        parseConvexPolyhedronShape(typeId, shapeSpec.object(), shapeSpecString);
    }
    else if(shapeType == "Mesh") {
        parseMeshShape(typeId, shapeSpec.object(), shapeSpecString);
    }
    else if(shapeType == "SphereUnion") {
        parseSphereUnionShape(typeId, shapeSpec.object(), shapeSpecString);
    }
    else {
        qWarning() << "GSD file reader: The following particle shape type is not supported by this version of OVITO:" << shapeType;
    }
}

/******************************************************************************
* Parsing routine for 'Sphere' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parseSphereShape(int typeId, QJsonObject definition)
{
    const double diameter = definition.value("diameter").toDouble();
    if(diameter <= 0)
        throw Exception(tr("Missing or invalid 'diameter' field in 'Sphere' particle shape definition in GSD file."));

    const FloatType radius = diameter / 2;

    // Set the radius value to the existing particle type.
    const Property* existingTypeProperty = particles()->expectProperty(Particles::TypeProperty);
    if(const ParticleType* existingType = static_object_cast<ParticleType>(existingTypeProperty->elementType(typeId))) {
        if(existingType->radius() != radius) {
            Property* typeProperty = particles()->makeMutable(existingTypeProperty);
            ParticleType* mutableType = typeProperty->makeMutable(existingType);
            mutableType->setRadius(radius);
            mutableType->setRadiusIsPrescribed(true);
            mutableType->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::radius)});
        }
    }
}

/******************************************************************************
* Parsing routine for 'Ellipsoid' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parseEllipsoidShape(int typeId, QJsonObject definition)
{
    Vector3G abc;
    abc.x() = definition.value("a").toDouble();
    abc.y() = definition.value("b").toDouble();
    abc.z() = definition.value("c").toDouble();
    if(abc.x() <= 0.0f)
        throw Exception(tr("Missing or invalid 'a' field in 'Ellipsoid' particle shape definition in GSD file. Value must be positive."));

    if(abc.y() == 0.0f)
        abc.y() = abc.x();
    else if(abc.y() < 0.0f)
        throw Exception(tr("Invalid 'b' field in 'Ellipsoid' particle shape definition in GSD file. Value must not be negative."));

    if(abc.z() == 0.0f)
        abc.z() = abc.y();
    else if(abc.z() < 0.0f)
        throw Exception(tr("Invalid 'c' field in 'Ellipsoid' particle shape definition in GSD file. Value must not be negative."));

    // Create the 'Aspherical Shape' particle property if it doesn't exist yet.
    BufferWriteAccess<Vector3G, access_mode::read_write> ashapeProperty = particles()->createProperty(DataBuffer::Initialized, Particles::AsphericalShapeProperty);

    // Assign the [a,b,c] values to those particles which are of the given type.
    BufferReadAccess<int32_t> typeProperty = particles()->expectProperty(Particles::TypeProperty);
    for(size_t i = 0; i < typeProperty.size(); i++) {
        if(typeProperty[i] == typeId)
            ashapeProperty[i] = abc;
    }
}

/******************************************************************************
* Parsing routine for 'Polygon' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parsePolygonShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString)
{
    // Parse the list of vertices.
    const QJsonValue vertexArrayVal = definition.value("vertices");
    if(!vertexArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'vertex' array in 'Polygon' particle shape definition in GSD file."));
    std::vector<Point2> vertices;
    for(QJsonValue val : vertexArrayVal.toArray()) {
        const QJsonArray coordinateArray = val.toArray();
        if(coordinateArray.size() != 2)
            throw Exception(tr("Invalid vertex value in 'vertex' array of 'Polygon' particle shape definition in GSD file."));
        Point2 vertex = Point2::Origin();
        for(int c = 0; c < 2; c++)
            vertex[c] = coordinateArray[c].toDouble();
        vertices.push_back(vertex);
    }
    if(vertices.size() < 3)
        throw Exception(tr("Invalid 'Polygon' particle shape definition in GSD file: Number of vertices must be at least 3."));

    // Parse rounding radius.
    FloatType roundingRadius = definition.value("rounding_radius").toDouble();
    if(roundingRadius > 0) {
        // Constructed the rounded polygon.
        std::vector<Point2> roundedVertices;
        int res = (1 << (_roundingResolution - 1));
        roundedVertices.reserve(vertices.size()*(res+1));
        auto v1 = vertices.cend() - 1;
        auto v2 = vertices.cbegin();
        auto v3 = vertices.cbegin() + 1;
        Vector2 u1 = (*v1) - (*v2);
        u1.normalizeSafely();
        do {
            Vector2 u2 = (*v2) - (*v3);
            u2.normalizeSafely();
            FloatType theta1 = std::atan2(u1.x(), -u1.y());
            FloatType theta2 = std::atan2(u2.x(), -u2.y());
            FloatType delta_theta = std::fmod(theta2 - theta1, FLOATTYPE_PI*2);
            if(delta_theta < 0) delta_theta += FLOATTYPE_PI*2;
            delta_theta /= res;
            for(int i = 0; i < res+1; i++, theta1 += delta_theta) {
                Vector2 delta(cos(theta1) * roundingRadius, sin(theta1) * roundingRadius);
                roundedVertices.push_back((*v2) + delta);
            }
            v1 = v2;
            v2 = v3;
            ++v3;
            if(v3 == vertices.cend()) v3 = vertices.cbegin();
            u1 = u2;
        }
        while(v2 != vertices.cbegin());
        vertices.swap(roundedVertices);
    }

    // Create triangulation of (convex or concave) polygon.
    DataOORef<TriangleMesh> triMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    triMesh->setIdentifier(QStringLiteral("generated"));    // Indicate to the ParticleType by assigning this ID that the shape mesh has been generated by the file importer (and was not assigned by the user).
    CapPolygonTessellator tessellator(*triMesh, 2, CapPolygonTessellator::FrontFace, true);
    tessellator.beginPolygon();
    tessellator.beginContour();
    for(const Point2& p : vertices)
        tessellator.vertex(p);
    tessellator.endContour();
    tessellator.endPolygon();
    triMesh->flipFaces();
    triMesh->determineEdgeVisibility();

    // Store shape geometry in internal cache to avoid parsing the JSON string again for other animation frames.
    _importer->storeParticleShapeInCache(shapeSpecString, triMesh);

    // Assign shape to particle type.
    setParticleTypeShape(typeId, std::move(triMesh));
}

/******************************************************************************
* Recursive helper function that tessellates a corner face.
******************************************************************************/
static void tessellateCornerFacet(SurfaceMesh::face_index seedFace, int recursiveDepth, FloatType roundingRadius, SurfaceMeshBuilder& mesh, SurfaceMeshBuilder::VertexGrower& vertexGrower, SurfaceMeshBuilder::FaceGrower& faceGrower, std::vector<Vector3>& vertexNormals, const Point3& center)
{
    if(recursiveDepth <= 1)
        return;

    // List of edges that should be split during the next iteration.
    std::set<SurfaceMesh::edge_index> edgeList;

    // List of faces that should be subdivided during the next iteration.
    std::vector<SurfaceMesh::face_index> faceList, faceList2;

    // Initialize lists.
    faceList.push_back(seedFace);
    SurfaceMesh::edge_index e = mesh.firstFaceEdge(seedFace);
    do {
        edgeList.insert(e);
        e = mesh.nextFaceEdge(e);
    }
    while(e != mesh.firstFaceEdge(seedFace));

    // Perform iterations of the recursive refinement procedure.
    for(int iteration = 1; iteration < recursiveDepth; iteration++) {

        // Create new vertices at the midpoints of the existing edges.
        for(SurfaceMesh::edge_index edge : edgeList) {
            Point3 midpoint = vertexGrower.vertexPosition(mesh.vertex1(edge));
            midpoint += vertexGrower.vertexPosition(mesh.vertex2(edge)) - Point3::Origin();
            Vector3 normal = (midpoint * FloatType(0.5)) - center;
            normal.normalizeSafely();
            mesh.splitEdge(edge, center + normal * roundingRadius, vertexGrower);
            vertexNormals.push_back(normal);
        }
        edgeList.clear();

        // Subdivide the faces.
        for(SurfaceMesh::face_index face : faceList) {
            int order = mesh.topology()->countFaceEdges(face) / 2;
            SurfaceMesh::edge_index e = mesh.firstFaceEdge(face);
            for(int i = 0; i < order; i++) {
                SurfaceMesh::edge_index edge2 = mesh.nextFaceEdge(mesh.nextFaceEdge(e));
                e = mesh.splitFace(e, edge2, faceGrower);
                // Put edges and the sub-face itself into the list so that
                // they get refined during the next iteration of the algorithm.
                SurfaceMesh::edge_index oe = mesh.oppositeEdge(e);
                for(int j = 0; j < 3; j++) {
                    edgeList.insert((oe < mesh.oppositeEdge(oe)) ? oe : mesh.oppositeEdge(oe));
                    oe = mesh.nextFaceEdge(oe);
                }
                faceList2.push_back(mesh.adjacentFace(oe));
            }
            faceList2.push_back(face);
        }
        faceList.clear();
        faceList.swap(faceList2);
    }
}

/******************************************************************************
* Parsing routine for 'ConvexPolyhedron' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parseConvexPolyhedronShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString)
{
    // Parse the list of vertices.
    std::vector<Point3> vertices;
    const QJsonValue vertexArrayVal = definition.value("vertices");
    if(!vertexArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'vertex' array in 'ConvexPolyhedron' particle shape definition in GSD file."));
    for(QJsonValue val : vertexArrayVal.toArray()) {
        const QJsonArray coordinateArray = val.toArray();
        if(coordinateArray.size() != 3)
            throw Exception(tr("Invalid vertex value in 'vertex' array of 'ConvexPolyhedron' particle shape definition in GSD file."));
        Point3 vertex;
        for(int c = 0; c < 3; c++)
            vertex[c] = coordinateArray[c].toDouble();
        vertices.push_back(vertex);
    }
    if(vertices.size() < 4)
        throw Exception(tr("Invalid 'ConvexPolyhedron' particle shape definition in GSD file: Number of vertices must be at least 4."));

    // Construct the convex hull of the vertices.
    // This yields a half-edge surface mesh data structure.
    DataOORef<SurfaceMesh> meshObj = DataOORef<SurfaceMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    {
        SurfaceMeshBuilder meshBuilder(meshObj);
        meshBuilder.constructConvexHull(std::move(vertices));
        meshBuilder.joinCoplanarFaces();
    }

    // Parse rounding radius.
    FloatType roundingRadius = definition.value("rounding_radius").toDouble();
    std::vector<Vector3> vertexNormals;
    if(roundingRadius > 0) {
        DataOORef<SurfaceMesh> roundedMeshObj = DataOORef<SurfaceMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
        {
            SurfaceMeshReadAccess mesh(meshObj);
            SurfaceMeshBuilder roundedMesh(roundedMeshObj);
            SurfaceMeshBuilder::VertexGrower roundedMeshVertexGrower(roundedMesh);
            SurfaceMeshBuilder::FaceGrower roundedMeshFaceGrower(roundedMesh);
            BufferReadAccess<Point3> vertexPositions(mesh.expectVertexProperty(SurfaceMeshVertices::PositionProperty));

            // Maps edges of the old mesh to edges of the new mesh.
            std::vector<SurfaceMesh::edge_index> edgeMapping(mesh.edgeCount());

            // Copy the faces of the existing mesh over to the new mesh data structure.
            SurfaceMesh::size_type originalFaceCount = mesh.faceCount();
            for(SurfaceMesh::face_index face = 0; face <originalFaceCount; face++) {

                // Compute the offset by which the face needs to be extruded outward.
                Vector3 faceNormal = mesh.computeFaceNormal(face, vertexPositions);
                Vector3 offset = faceNormal * roundingRadius;

                // Duplicate the vertices and shift them along the extrusion vector.
                SurfaceMesh::size_type faceVertexCount = 0;
                SurfaceMesh::edge_index e = mesh.firstFaceEdge(face);
                do {
                    roundedMeshVertexGrower.createVertex(vertexPositions[mesh.vertex1(e)] + offset);
                    vertexNormals.push_back(faceNormal);
                    faceVertexCount++;
                    e = mesh.nextFaceEdge(e);
                }
                while(e != mesh.firstFaceEdge(face));

                // Connect the duplicated vertices by a new face.
                SurfaceMesh::face_index new_f = roundedMeshFaceGrower.createFace(
                    roundedMesh.topology()->end_vertices() - faceVertexCount,
                    roundedMesh.topology()->end_vertices());

                // Register the newly created edges.
                SurfaceMesh::edge_index new_e = roundedMesh.firstFaceEdge(new_f);
                do {
                    edgeMapping[e] = new_e;
                    e = mesh.nextFaceEdge(e);
                    new_e = roundedMesh.nextFaceEdge(new_e);
                }
                while(e != mesh.firstFaceEdge(face));
            }

            // Insert new faces in between two faces that share an edge.
            for(SurfaceMesh::edge_index e = 0; e < mesh.edgeCount(); e++) {
                // Skip every other half-edge.
                if(e > mesh.oppositeEdge(e)) continue;

                SurfaceMesh::edge_index edge = edgeMapping[e];
                SurfaceMesh::edge_index opposite_edge = edgeMapping[mesh.oppositeEdge(e)];

                SurfaceMesh::face_index new_f = roundedMeshFaceGrower.createFace({
                    roundedMesh.vertex2(edge),
                    roundedMesh.vertex1(edge),
                    roundedMesh.vertex2(opposite_edge),
                    roundedMesh.vertex1(opposite_edge) });

                roundedMesh.linkOppositeEdges(edge, roundedMesh.firstFaceEdge(new_f));
                roundedMesh.linkOppositeEdges(opposite_edge, roundedMesh.nextFaceEdge(roundedMesh.nextFaceEdge(roundedMesh.firstFaceEdge(new_f))));
            }

            // Fill in the holes at the vertices of the old mesh.
            for(SurfaceMesh::edge_index original_edge = 0; original_edge < edgeMapping.size(); original_edge++) {
                SurfaceMesh::edge_index new_edge = roundedMesh.oppositeEdge(edgeMapping[original_edge]);
                SurfaceMesh::edge_index border_edges[2] = {
                    roundedMesh.nextFaceEdge(new_edge),
                    roundedMesh.prevFaceEdge(new_edge)
                };
                SurfaceMesh::vertex_index corner_vertices[2] = {
                    mesh.vertex1(original_edge),
                    mesh.vertex2(original_edge)
                };
                for(int i = 0; i < 2; i++) {
                    SurfaceMesh::edge_index e = border_edges[i];
                    if(roundedMesh.hasOppositeEdge(e))
                        continue;
                    SurfaceMesh::face_index new_f = roundedMeshFaceGrower.createFace();
                    SurfaceMesh::edge_index edge = e;
                    do {
                        roundedMesh.createOppositeEdge(edge, new_f);
                        edge = roundedMesh.prevFaceEdge(roundedMesh.oppositeEdge(roundedMesh.prevFaceEdge(roundedMesh.oppositeEdge(roundedMesh.prevFaceEdge(edge)))));
                    }
                    while(edge != e);

                    // Tessellate the inserted corner element.
                    tessellateCornerFacet(new_f, _roundingResolution, roundingRadius, roundedMesh, roundedMeshVertexGrower, roundedMeshFaceGrower, vertexNormals, vertexPositions[corner_vertices[i]]);
                }
            }

            // Tessellate the inserted edge elements.
            for(SurfaceMesh::edge_index e = 0; e < mesh.edgeCount(); e++) {
                // Skip every other half-edge.
                if(e > mesh.oppositeEdge(e)) continue;

                SurfaceMesh::edge_index startEdge = roundedMesh.oppositeEdge(edgeMapping[e]);
                SurfaceMesh::edge_index edge1 = roundedMesh.prevFaceEdge(roundedMesh.prevFaceEdge(startEdge));
                SurfaceMesh::edge_index edge2 = roundedMesh.nextFaceEdge(startEdge);

                for(int i = 1; i < (1<<(_roundingResolution-1)); i++) {
                    edge2 = roundedMesh.splitFace(edge1, edge2, roundedMeshFaceGrower);
                    edge1 = roundedMesh.prevFaceEdge(edge1);
                    edge2 = roundedMesh.nextFaceEdge(edge2);
                }
            }
            OVITO_ASSERT(roundedMesh.topology()->isClosed());
        }

        // Adopt the newly constructed mesh as particle shape.
        meshObj.swap(roundedMeshObj);
    }

    // Convert half-edge mesh into a conventional triangle mesh for visualization.
    DataOORef<TriangleMesh> triMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    triMesh->setIdentifier(QStringLiteral("generated"));    // Indicate to the ParticleType by assigning this ID that the shape mesh has been generated by the file importer (and was not assigned by the user).
    SurfaceMeshReadAccess(meshObj).convertToTriMesh(*triMesh, false);
    if(triMesh->faceCount() == 0) {
        qWarning() << "GSD file reader: Convex hull construction did not produce a valid triangle mesh for particle type" << typeId;
        return;
    }

    // Assign precomputed vertex normals to triangle mesh for smooth shading of rounded edges.
    OVITO_ASSERT(vertexNormals.empty() || vertexNormals.size() == triMesh->vertexCount());
    if(!vertexNormals.empty()) {
        triMesh->setHasNormals(true);
        auto normal = triMesh->normals().begin();
        for(const TriMeshFace& face : triMesh->faces()) {
            for(int v = 0; v < 3; v++)
                *normal++ = vertexNormals[face.vertex(v)].toDataType<GraphicsFloatType>();
        }
    }

    // Store shape geometry in internal cache to avoid parsing the JSON string again for other animation frames.
    _importer->storeParticleShapeInCache(shapeSpecString, triMesh);

    // Assign shape to particle type.
    setParticleTypeShape(typeId, std::move(triMesh));
}

/******************************************************************************
* Parsing routine for 'Mesh' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parseMeshShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString)
{
    // Parse the list of vertices.
    DataOORef<TriangleMesh> triMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    triMesh->setIdentifier(QStringLiteral("generated"));    // Indicate to the ParticleType by assigning this ID that the shape mesh has been generated by the file importer (and was not assigned by the user).
    const QJsonValue vertexArrayVal = definition.value("vertices");
    if(!vertexArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'vertex' array in 'Mesh' particle shape definition in GSD file."));
    for(QJsonValue val : vertexArrayVal.toArray()) {
        const QJsonArray coordinateArray = val.toArray();
        if(coordinateArray.size() != 3)
            throw Exception(tr("Invalid vertex value in 'vertex' array of 'Mesh' particle shape definition in GSD file."));
        Point3 vertex;
        for(int c = 0; c < 3; c++)
            vertex[c] = coordinateArray[c].toDouble();
        triMesh->addVertex(vertex);
    }
    if(triMesh->vertexCount() < 3)
        throw Exception(tr("Invalid 'Mesh' particle shape definition in GSD file: Number of vertices must be at least 3."));

    // Parse the face list.
    const QJsonValue faceArrayVal = definition.value("indices");
    if(!faceArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'indices' array in 'Mesh' particle shape definition in GSD file."));
    for(QJsonValue val : faceArrayVal.toArray()) {
        const QJsonArray indicesArray = val.toArray();
        if(indicesArray.size() < 3)
            throw Exception(tr("Invalid face definition in 'indices' array of 'Mesh' particle shape definition in GSD file."));
        int nVertices = 0;
        int vindices[3];

        // Parse face vertex list and triangle the face in case it has more than 3 vertices.
        for(const QJsonValue val2 : indicesArray) {
            vindices[std::min(nVertices,2)] = val2.toInt();
            if(!val2.isDouble() || vindices[std::min(nVertices,2)] < 0 || vindices[std::min(nVertices,2)] >= triMesh->vertexCount())
                throw Exception(tr("Invalid face definition in 'indices' array of 'Mesh' particle shape definition in GSD file. Vertex index is out of range."));
            nVertices++;
            if(nVertices >= 3) {
                triMesh->addFace().setVertices(vindices[0], vindices[1], vindices[2]);
                vindices[1] = vindices[2];
            }
        }
    }
    if(triMesh->faceCount() < 1)
        throw Exception(tr("Invalid 'Mesh' particle shape definition in GSD file: Face list is empty."));

    // Render only sharp edges of the mesh in wireframe mode.
    triMesh->determineEdgeVisibility();

    // Store shape geometry in internal cache to avoid parsing the JSON string again for other animation frames.
    _importer->storeParticleShapeInCache(shapeSpecString, triMesh);

    // Assign shape to particle type.
    setParticleTypeShape(typeId, std::move(triMesh));
}

/******************************************************************************
* Parsing routine for 'SphereUnion' particle shape definitions.
******************************************************************************/
void GSDImporter::FrameLoader::parseSphereUnionShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString)
{
    // Parse the list of sphere centers.
    std::vector<Point3> centers;
    const QJsonValue centersArrayVal = definition.value("centers");
    if(!centersArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'centers' array in 'SphereUnion' particle shape definition in GSD file."));
    for(QJsonValue val : centersArrayVal.toArray()) {
        const QJsonArray coordinateArray = val.toArray();
        if(coordinateArray.size() != 3)
            throw Exception(tr("Invalid vertex value in 'centers' array of 'SphereUnion' particle shape definition in GSD file."));
        Point3 center;
        for(int c = 0; c < 3; c++)
            center[c] = coordinateArray[c].toDouble();
        centers.push_back(center);
    }
    if(centers.size() < 1)
        throw Exception(tr("Invalid 'SphereUnion' particle shape definition in GSD file: Number of spheres must be at least 1."));

    // Parse the list of sphere diameters.
    std::vector<FloatType> diameters;
    const QJsonValue diametersArrayVal = definition.value("diameters");
    if(!diametersArrayVal.isArray())
        throw Exception(tr("Missing or invalid 'diameters' array in 'SphereUnion' particle shape definition in GSD file."));
    for(QJsonValue val : diametersArrayVal.toArray()) {
        diameters.push_back(val.toDouble());
        if(diameters.back() <= 0)
            throw Exception(tr("Invalid diameters value in 'diameters' array of 'SphereUnion' particle shape definition in GSD file."));
    }
    if(diameters.size() != centers.size())
        throw Exception(tr("Invalid 'SphereUnion' particle shape definition in GSD file: Length of diameters array must match length of centers array."));

    // Build template for a triangulated (ico)sphere:
    DataOORef<TriangleMesh> sphereTemplate = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    sphereTemplate->createIcosphere(_roundingResolution - 1);
    const int unitSphereVertexCount = sphereTemplate->vertexCount();
    const int unitSphereFaceCount = sphereTemplate->faceCount();

    // Generate the triangle mesh for the union of spheres by duplicating the unit sphere template.
    DataOORef<TriangleMesh> triMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
    triMesh->setIdentifier(QStringLiteral("generated"));    // Indicate to the ParticleType by assigning this ID that the shape mesh has been generated by the file importer (and was not assigned by the user).
    triMesh->setVertexCount(unitSphereVertexCount * centers.size());
    triMesh->setFaceCount(unitSphereFaceCount * centers.size());
    triMesh->setHasNormals(true);
    auto vertex = triMesh->vertices().begin();
    auto face = triMesh->faces().begin();
    auto normal = triMesh->normals().begin();
    for(size_t sphereIndex = 0; sphereIndex < centers.size(); sphereIndex++) {
        const Point3 center = centers[sphereIndex];
        const FloatType diameter = 0.5 * diameters[sphereIndex];
        const int baseVertex = sphereIndex * unitSphereVertexCount;
        for(const Point3& p : sphereTemplate->vertices())
            *vertex++ = Point3(p.x() * diameter + center.x(), p.y() * diameter + center.y(), p.z() * diameter + center.z());
        for(const TriMeshFace& inFace : sphereTemplate->faces()) {
            for(int v = 0; v < 3; v++) {
                face->setVertex(v, inFace.vertex(v) + baseVertex);
                const Point3& vpos = sphereTemplate->vertex(inFace.vertex(v));
                *normal++ = Vector3G(vpos.x(), vpos.y(), vpos.z());
            }
            ++face;
        }
    }

    // Store shape geometry in internal cache to avoid parsing the JSON string again for other animation frames.
    _importer->storeParticleShapeInCache(shapeSpecString, triMesh);

    // Assign shape to particle type.
    setParticleTypeShape(typeId, std::move(triMesh));
}

}   // End of namespace
