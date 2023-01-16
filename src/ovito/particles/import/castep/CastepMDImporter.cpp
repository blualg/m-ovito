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
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "CastepMDImporter.h"

#include <boost/algorithm/string.hpp>

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(CastepMDImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool CastepMDImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Look for string 'BEGIN header' to occur on first line.
    if(!boost::algorithm::istarts_with(stream.readLineTrimLeft(32), "BEGIN header"))
        return false;

    // Look for string 'END header' to occur within the first 50 lines of the file.
    for(int i = 0; i < 50 && !stream.eof(); i++) {
        if(boost::algorithm::istarts_with(stream.readLineTrimLeft(1024), "END header"))
            return true;
    }

    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void CastepMDImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning CASTEP file %1").arg(stream.filename()));
    setProgressMaximum(stream.underlyingSize());

    // Look for string 'BEGIN header' to occur on first line.
    if(!boost::algorithm::istarts_with(stream.readLineTrimLeft(32), "BEGIN header"))
        throw Exception(tr("Invalid CASTEP md/geom file header"));

    // Fast forward to line 'END header'.
    for(;;) {
        if(stream.eof())
            throw Exception(tr("Invalid CASTEP md/geom file. Unexpected end of file."));
        if(boost::algorithm::istarts_with(stream.readLineTrimLeft(), "END header"))
            break;
        if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
            return;
    }

    Frame frame(fileHandle());
    QString filename = fileHandle().sourceUrl().fileName();
    int frameNumber = 0;

    while(!stream.eof()) {
        frame.byteOffset = stream.byteOffset();
        frame.lineNumber = stream.lineNumber();
        stream.readLine();
        if(stream.lineEndsWith("<-- h")) {
            frame.label = tr("%1 (Frame %2)").arg(filename).arg(frameNumber++);
            frames.push_back(frame);
            // Skip the two other lines of the cell matrix
            stream.readLine();
            stream.readLine();
        }

        if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
            return;
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void CastepMDImporter::FrameLoader::loadFile()
{
    // Open file for reading.
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Reading CASTEP file %1").arg(fileHandle().toString()));

    // Jump to byte offset.
    if(frame().byteOffset != 0)
        stream.seek(frame().byteOffset, frame().lineNumber);

    std::vector<Point3> coords;
    std::vector<QString> types;
    std::vector<Vector3> velocities;
    std::vector<Vector3> forces;

    AffineTransformation cell = AffineTransformation::Identity();
    int numCellVectors = 0;

    while(!stream.eof()) {
        const char* line = stream.readLineTrimLeft();

        if(stream.lineEndsWith("<-- h")) {
            if(numCellVectors == 3) break;
            if(sscanf(line, FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                    &cell(0,numCellVectors), &cell(1,numCellVectors), &cell(2,numCellVectors)) != 3)
                throw Exception(tr("Invalid simulation cell in CASTEP file at line %1").arg(stream.lineNumber()));
            // Convert units from Bohr to Angstrom.
            cell.column(numCellVectors) *= 0.529177210903;
            numCellVectors++;
        }
        else if(stream.lineEndsWith("<-- R")) {
            Point3 pos;
            if(sscanf(line, "%*s %*u " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                    &pos.x(), &pos.y(), &pos.z()) != 3)
                throw Exception(tr("Invalid coordinates in CASTEP file at line %1").arg(stream.lineNumber()));
            // Convert units from Bohr to Angstrom.
            pos *= 0.529177210903;
            coords.push_back(pos);
            const char* typeNameEnd = line;
            while(*typeNameEnd > ' ') typeNameEnd++;
            types.push_back(QLatin1String(line, typeNameEnd));
        }
        else if(stream.lineEndsWith("<-- V")) {
            Vector3 v;
            if(sscanf(line, "%*s %*u " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                    &v.x(), &v.y(), &v.z()) != 3)
                throw Exception(tr("Invalid velocity in CASTEP file at line %1").arg(stream.lineNumber()));
            velocities.push_back(v);
        }
        else if(stream.lineEndsWith("<-- F")) {
            Vector3 f;
            if(sscanf(line, "%*s %*u " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                    &f.x(), &f.y(), &f.z()) != 3)
                throw Exception(tr("Invalid force in CASTEP file at line %1").arg(stream.lineNumber()));
            forces.push_back(f);
        }

        if(isCanceled())
            return;
    }
    simulationCell()->setCellMatrix(cell);

    // Create the particle properties.
    setParticleCount(coords.size());
    PropertyAccess<Point3> posProperty = particles()->createProperty(ParticlesObject::PositionProperty);
    boost::copy(coords, posProperty.begin());

    PropertyAccess<int> typeProperty = particles()->createProperty(ParticlesObject::TypeProperty);
    boost::transform(types, typeProperty.begin(), [&](const QString& typeName) {
        return addNamedType(ParticlesObject::OOClass(), typeProperty.buffer(), typeName)->numericId();
    });

    // Since we created particle types on the go while reading the particles, the particle type ordering
    // depends on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
    // why we sort them now.
    typeProperty.buffer()->sortElementTypesByName();

    if(velocities.size() == coords.size()) {
        PropertyAccess<Vector3> velocityProperty = particles()->createProperty(ParticlesObject::VelocityProperty);
        boost::copy(velocities, velocityProperty.begin());
    }
    if(forces.size() == coords.size()) {
        PropertyAccess<Vector3> forceProperty = particles()->createProperty(ParticlesObject::ForceProperty);
        boost::copy(forces, forceProperty.begin());
    }

    state().setStatus(tr("%1 atoms").arg(coords.size()));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
