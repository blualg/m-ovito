////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "DLPOLYImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(DLPOLYImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool DLPOLYImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Ignore first comment line (record 1).
    stream.readLine(1024);

    // Parse second line (record 2).
    int levcfg;
    int imcon;
    if(stream.eof() || sscanf(stream.readLine(256), "%u %u", &levcfg, &imcon) != 2 || levcfg < 0 || levcfg > 2 || imcon < 0 || imcon > 6)
        return false;

    // Skip "timestep" record (if any).
    stream.readLine();
    if(stream.lineStartsWith("timestep"))
        stream.readLine();

    // Parse cell matrix (records 3-5, only when periodic boundary conditions are used)
    if(imcon != 0) {
        for(int i = 0; i < 3; i++) {
            char c;
            double x,y,z;
            if(sscanf(stream.line(), "%lg %lg %lg %c", &x, &y, &z, &c) != 3 || stream.eof())
                return false;
            stream.readLine();
        }
    }

    // Parse first atom record.
    double d;
    if(stream.eof() || sscanf(stream.line(), "%lg", &d) != 0) // Expect the line to start with a non-number!
        return false;
    char c;
    // Parse atomic coordinates.
    double x,y,z;
    if(sscanf(stream.readLine(), "%lg %lg %lg %c", &x, &y, &z, &c) != 3 || stream.eof())
        return false;
    // Parse atomic velocity vector.
    if(levcfg > 0) {
        if(sscanf(stream.readLine(), "%lg %lg %lg %c", &x, &y, &z, &c) != 3 || stream.eof())
            return false;
    }
    // Parse atomic force vector.
    if(levcfg > 1) {
        if(sscanf(stream.readLine(), "%lg %lg %lg %c", &x, &y, &z, &c) != 3 || stream.eof())
            return false;
    }

    return true;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void DLPOLYImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning DL_POLY file %1").arg(stream.filename()));
    setProgressMaximum(stream.underlyingSize());

    // Skip first comment line (record 1).
    stream.readLine();

    // Parse second line (record 2).
    int levcfg;
    int imcon;
    qlonglong expectedAtomCount = -1;
    int frame_count = -1;
    if(stream.eof() || sscanf(stream.readLine(), "%u %u %llu %u", &levcfg, &imcon, &expectedAtomCount, &frame_count) < 2 || levcfg < 0 || levcfg > 2 || imcon < 0 || imcon > 6)
        throw Exception(tr("Invalid record line %1 in DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

    Frame frame(fileHandle());
    QString filename = fileHandle().sourceUrl().fileName();
    frame.byteOffset = stream.byteOffset();
    frame.lineNumber = stream.lineNumber();

    // Look for "timestep" record.
    stream.readLine();
    if(stream.lineStartsWith("timestep")) {
        if(expectedAtomCount <= 0)
            throw Exception(tr("Invalid number of atoms in line %1 of DL_POLY file.").arg(stream.lineNumber()-1));
        if(frame_count <= 0)
            throw Exception(tr("Invalid frame count in line %1 of DL_POLY file.").arg(stream.lineNumber()-1));

        for(int frameIndex = 0; frameIndex < frame_count; frameIndex++) {
            if(frameIndex != 0) {
                frame.byteOffset = stream.byteOffset();
                frame.lineNumber = stream.lineNumber();
                stream.recordSeekPoint();
                stream.readLine();
            }
            int nstep;
            qlonglong megatm;
            int keytrj;
            double tstep;
            double ttime;
            if(sscanf(stream.line(), "timestep %u %llu %i %i %lg %lg", &nstep, &megatm, &keytrj, &imcon, &tstep, &ttime) != 6 || megatm != expectedAtomCount)
                throw Exception(tr("Invalid timestep record in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            frame.label = QStringLiteral("Time: %1 ps").arg(ttime);
            frames.push_back(frame);

            // Skip simulation cell.
            if(imcon != 0) {
                for(int j = 0; j < 3; j++) {
                    stream.readLine();
                }
            }

            // Skip the right number of atom lines.
            int nLinesPerAtom = 2;
            if(keytrj > 0) nLinesPerAtom++;
            if(keytrj > 1) nLinesPerAtom++;
            for(qlonglong i = 0; i < expectedAtomCount; i++) {
                for(int j = 0; j < nLinesPerAtom; j++) {
                    stream.readLine();
                }
                if((i % 1024) == 0 && !setProgressValue(stream.underlyingByteOffset()))
                    return;
            }
        }
    }
    else {
        // It's not a trajectory file. Report just a single frame.
        frames.push_back(Frame(fileHandle()));
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void DLPOLYImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading DL_POLY file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle());
    setProgressMaximum(stream.underlyingSize());

    // Read first comment line (record 1).
    stream.readLine(1024);
    QString trimmedComment = stream.lineString().trimmed();
    if(!trimmedComment.isEmpty())
        state().setAttribute(QStringLiteral("Comment"), QVariant::fromValue(trimmedComment), dataSource());

    // Parse second line (record 2).
    int levcfg;
    int imcon;
    qlonglong expectedAtomCount = -1;
    if(stream.eof() || sscanf(stream.readLine(256), "%u %u %llu", &levcfg, &imcon, &expectedAtomCount) < 2 || levcfg < 0 || levcfg > 2 || imcon < 0 || imcon > 6)
        throw Exception(tr("Invalid record line %1 in DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

    if(imcon == 0) simulationCell()->setPbcFlags(false, false, false);
    else if(imcon == 1 || imcon == 2 || imcon == 3) simulationCell()->setPbcFlags(true, true, true);
    else if(imcon == 6) simulationCell()->setPbcFlags(true, true, false);
    else throw Exception(tr("Invalid boundary condition type in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

    // Jump to byte offset again.
    if(frame().byteOffset != 0)
        stream.seek(frame().byteOffset, frame().lineNumber);

    // Parse "timestep" record (if any).
    stream.readLine();
    if(stream.lineStartsWith("timestep")) {
        int nstep;
        qlonglong megatm;
        int keytrj;
        double tstep;
        double ttime;
        if(sscanf(stream.line(), "timestep %u %llu %i %i %lg %lg", &nstep, &megatm, &keytrj, &imcon, &tstep, &ttime) != 6 || megatm != expectedAtomCount)
            throw Exception(tr("Invalid timestep record in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        state().setAttribute(QStringLiteral("IntegrationTimestep"), QVariant::fromValue(tstep), dataSource());
        state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(ttime), dataSource());
        stream.readLine();
    }

    // Parse cell matrix (records 3-5, only when periodic boundary conditions are used)
    if(imcon != 0) {
        AffineTransformation cell = AffineTransformation::Identity();
        for(size_t i = 0; i < 3; i++) {
            if(sscanf(stream.line(),
                    FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                    &cell(0,i), &cell(1,i), &cell(2,i)) != 3 || cell.column(i) == Vector3::Zero())
                throw Exception(tr("Invalid cell vector in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            stream.readLine();
        }
        cell.column(3) = cell * Vector3(-0.5, -0.5, -0.5);
        simulationCell()->setCellMatrix(cell);
    }

    // The temporary buffers for the atom records.
    std::vector<qlonglong> identifiers;
    std::vector<QString> atom_types;
    std::vector<Point3> positions;
    std::vector<Vector3> velocities;
    std::vector<Vector3> forces;
    std::vector<FloatType> masses;
    std::vector<FloatType> charges;
    std::vector<FloatType> displacementMagnitudes;

    // Parse atoms.
    do {
        // Report progress.
        if(isCanceled()) return;
        if((positions.size() % 1024) == 0)
            setProgressValueIntermittent(stream.underlyingByteOffset());

        // Parse first line of atom record.
        if(!positions.empty()) stream.readLine();
        const char* line = stream.line();
        while(*line != '\0' && *line <= ' ') ++line;
        double d;
        if(sscanf(line, "%lg", &d) != 0) // Expect the line to start with a non-number!
            throw Exception(tr("Invalid atom type specification in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

        // Parse atom type name.
        const char* line_end = line;
        while(*line_end != '\0' && *line_end > ' ') ++line_end;
        atom_types.push_back(QLatin1String(line, line_end));

        // Parse atom identifier and other info (optional).
        if(*line_end != '\0') {
            identifiers.emplace_back();
            FloatType mass, charge, displ;
            int fieldCount;
            if((fieldCount = sscanf(line_end, "%llu " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &identifiers.back(), &mass, &charge, &displ)) < 1)
                throw Exception(tr("Invalid atom identifier field in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            if(fieldCount == 4) {
                masses.push_back(mass);
                charges.push_back(charge);
                displacementMagnitudes.push_back(displ);
            }
        }

        // Parse atomic coordinates.
        char c;
        positions.emplace_back();
        Point3& pos = positions.back();
        if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " %c", &pos.x(), &pos.y(), &pos.z(), &c) != 3)
            throw Exception(tr("Invalid atom coordinate triplet in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

        if(levcfg > 0) {
            // Parse atomic velocity vector.
            velocities.emplace_back();
            Vector3& vel = velocities.back();
            if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " %c", &vel.x(), &vel.y(), &vel.z(), &c) != 3)
                throw Exception(tr("Invalid atomic velocity vector in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        }

        if(levcfg > 1) {
            // Parse atomic force vector.
            forces.emplace_back();
            Vector3& force = forces.back();
            if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " %c", &force.x(), &force.y(), &force.z(), &c) != 3)
                throw Exception(tr("Invalid atomic force vector in line %1 of DL_POLY file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        }

        if(positions.size() == expectedAtomCount)
            break;
    }
    while(!stream.eof());

    // Make sure the number of atoms specified in the header was correct.
    if(expectedAtomCount > 0 && positions.size() < (size_t)expectedAtomCount)
        throw Exception(tr("Unexpected end of DL_POLY file. Expected %1 atom records but found only %2.").arg(expectedAtomCount).arg(positions.size()));

    // Create particle properties.
    setParticleCount(positions.size());
    PropertyAccess<Point3> posProperty = particles()->createProperty(ParticlesObject::PositionProperty);
    boost::copy(positions, posProperty.begin());

    PropertyAccess<int> typeProperty = particles()->createProperty(ParticlesObject::TypeProperty);
    boost::transform(atom_types, typeProperty.begin(), [&](const QString& typeName) {
        return addNamedType(ParticlesObject::OOClass(), typeProperty.buffer(), typeName)->numericId();
    });
    // Since we created particle types on the go while reading the particles, the type ordering
    // depends on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
    // why we sort them now.
    typeProperty.buffer()->sortElementTypesByName();

    if(identifiers.size() == positions.size()) {
        PropertyAccess<qlonglong> identifierProperty = particles()->createProperty(ParticlesObject::IdentifierProperty);
        boost::copy(identifiers, identifierProperty.begin());
    }
    if(levcfg > 0) {
        PropertyAccess<Vector3> velocityProperty = particles()->createProperty(ParticlesObject::VelocityProperty);
        boost::copy(velocities, velocityProperty.begin());
    }
    if(levcfg > 1) {
        PropertyAccess<Vector3> forceProperty = particles()->createProperty(ParticlesObject::ForceProperty);
        boost::copy(forces, forceProperty.begin());
    }
    if(masses.size() == positions.size()) {
        PropertyAccess<FloatType> massProperty = particles()->createProperty(ParticlesObject::MassProperty);
        boost::copy(masses, massProperty.begin());
    }
    if(charges.size() == positions.size()) {
        PropertyAccess<FloatType> chargeProperty = particles()->createProperty(ParticlesObject::ChargeProperty);
        boost::copy(charges, chargeProperty.begin());
    }
    if(displacementMagnitudes.size() == positions.size()) {
        PropertyAccess<FloatType> displProperty = particles()->createProperty(ParticlesObject::DisplacementMagnitudeProperty);
        boost::copy(displacementMagnitudes, displProperty.begin());
    }

    // Sort particles by ID if requested.
    if(_sortParticles)
        particles()->sortById();

    state().setStatus(tr("Number of particles: %1").arg(positions.size()));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
