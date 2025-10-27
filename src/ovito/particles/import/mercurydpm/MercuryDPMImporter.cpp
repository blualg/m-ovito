////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "MercuryDPMImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MercuryDPMImporter);
OVITO_CLASSINFO(MercuryDPMImporter, "DisplayName", "MercuryDPM");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool MercuryDPMImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Parse header line.
    const char* line = stream.readLine(1024);
    qlonglong N;
    double time, xmin, ymin, zmin, xmax, ymax, zmax;
    int nread;
    if(stream.eof() || sscanf(line, "%lld %lg %lg %lg %lg %lg %lg %lg%n", &N, &time, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax, &nread) != 8)
        return false;
    if(N <= 0 || time < 0.0 || xmin > xmax || ymin > ymax || zmin > zmax)
        return false;
    // Nothing should follow on the same line.
    if(!CompressedTextReader::isOnlyWhitespace(line + nread))
        return false;

    // Parse first and second line of particle data.
    for(size_t i = 0; i < 2 && i < N && !stream.eof(); i++) {
        line = stream.readLine(1024);
        double rx, ry, rz, vx, vy, vz, rad, alpha, beta, gamma, omex, omey, omez, info;
        if(sscanf(line, "%lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg%n", &rx, &ry, &rz, &vx, &vy, &vz, &rad, &alpha, &beta, &gamma, &omex, &omey, &omez, &info, &nread) != 14)
            return false;
        if(rad < 0.0)
            return false;
        // Nothing should follow on the same line.
        if(!CompressedTextReader::isOnlyWhitespace(line + nread))
            return false;
    }

    return true;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void MercuryDPMImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    CompressedTextReader stream(fileHandle);

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning MercuryDPM data file %1").arg(stream.filename()));
    progress.setMaximum(stream.underlyingSize());

    int frameNumber = 0;
    Frame frame(fileHandle);

    while(!stream.eof() && !this_task::isCanceled()) {
        // Note: For first frame, always use byte offset/line number 0, because otherwise a reload of frame 0 is triggered by the FileSource.
        if(!frames.empty()) {
            frame.byteOffset = stream.byteOffset();
            frame.lineNumber = stream.lineNumber();
        }
        stream.recordSeekPoint();

        // Parse header line.
        const char* line = stream.readLine(1024);
        qlonglong N;
        double time, xmin, ymin, zmin, xmax, ymax, zmax;
        int nread;
        if(stream.eof() || sscanf(line, "%lld %lg %lg %lg %lg %lg %lg %lg%n", &N, &time, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax, &nread) != 8)
            throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        if(N <= 0 || time < 0.0 || xmin > xmax || ymin > ymax || zmin > zmax)
            throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        // Nothing should follow on the same line.
        if(!CompressedTextReader::isOnlyWhitespace(line + nread))
            throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));

        // Create a new record for the time step.
        frame.label.setToTime(time);
        frames.push_back(frame);

        // Skip particle data.
        for(qlonglong i = 0; i < N; i++) {
            stream.readLine();
            // Update progress bar and check for user cancellation.
            progress.setValueIntermittent(stream.underlyingByteOffset());
        }
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void MercuryDPMImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading MercuryDPM data file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Parse header line.
    const char* line = stream.readLine(1024);
    qlonglong N;
    double time, xmin, ymin, zmin, xmax, ymax, zmax;
    int nread;
    if(stream.eof() || sscanf(line, "%lld %lg %lg %lg %lg %lg %lg %lg%n", &N, &time, &xmin, &ymin, &zmin, &xmax, &ymax, &zmax, &nread) != 8)
        throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
    if(N <= 0 || time < 0.0 || xmin > xmax || ymin > ymax || zmin > zmax)
        throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
    // Nothing should follow on the same line.
    if(!CompressedTextReader::isOnlyWhitespace(line + nread))
        throw Exception(tr("Invalid header line %1 in MercuryDPM data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
    state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(time), pipelineNode());

    // Prepare the file column to particle property mapping.
    ParticleInputColumnMapping columnMapping;
    columnMapping.resize(14);
    columnMapping.mapColumnToStandardProperty(0, Particles::PositionProperty, 0);
    columnMapping.mapColumnToStandardProperty(1, Particles::PositionProperty, 1);
    columnMapping.mapColumnToStandardProperty(2, Particles::PositionProperty, 2);
    columnMapping.mapColumnToStandardProperty(3, Particles::VelocityProperty, 0);
    columnMapping.mapColumnToStandardProperty(4, Particles::VelocityProperty, 1);
    columnMapping.mapColumnToStandardProperty(5, Particles::VelocityProperty, 2);
    columnMapping.mapColumnToStandardProperty(6, Particles::RadiusProperty);
    columnMapping.mapColumnToUserProperty(7, QStringLiteral("Angular Position"), Property::FloatDefault, 0);
    columnMapping.mapColumnToUserProperty(8, QStringLiteral("Angular Position"), Property::FloatDefault, 1);
    columnMapping.mapColumnToUserProperty(9, QStringLiteral("Angular Position"), Property::FloatDefault, 2);
    columnMapping.mapColumnToStandardProperty(10, Particles::AngularVelocityProperty, 0);
    columnMapping.mapColumnToStandardProperty(11, Particles::AngularVelocityProperty, 1);
    columnMapping.mapColumnToStandardProperty(12, Particles::AngularVelocityProperty, 2);
    columnMapping.mapColumnToUserProperty(13, QStringLiteral("info"), Property::Float64); // Note: Using double precision to possibly store 32-bit integer type IDs without loss of precision.

    // Parse data columns.
    setParticleCount(N);
    progress.setMaximum(N);
    InputColumnReader columnParser(*this, columnMapping, particles());
    try {
        for(qlonglong i = 0; i < N; i++) {
            // Update progress bar and check for user cancellation.
            progress.setValueIntermittent(i);
            columnParser.readElement(i, stream.readLine());
        }
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Parsing error in line %1 of MercuryDPM data file.").arg(stream.lineNumber()));
    }
    columnParser.reset();
    state().setStatus(tr("%1 particles").arg(N));

    // Axis-aligned simulation box.
    simulationCell()->setCellMatrix(AffineTransformation(
            Vector3(xmax - xmin, 0, 0),
            Vector3(0, ymax - ymin, 0),
            Vector3(0, 0, zmax - zmin),
            Vector3(xmin, ymin, zmin)));
    simulationCell()->setPbcFlags(false, false, false);

    // Check if the "info" column contains valid particle type IDs (all positive integer values).
    // If so, we can use it to assign the particle types.
    BufferReadAccess<double> infoPropertyAccess = particles()->getProperty(QStringLiteral("info"));
    bool isAllInteger = std::ranges::all_of(infoPropertyAccess, [](double value) {
        return std::floor(value) == value && value >= 1;
    });
    if(isAllInteger) {
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> typePropertyAccess(typeProperty);
        std::transform(infoPropertyAccess.begin(), infoPropertyAccess.end(), typePropertyAccess.begin(), [&](double value) {
            int typeId = static_cast<int>(value);
            addNumericType(Particles::OOClass(), typeProperty, typeId, {});
            return typeId;
        });
        typeProperty->sortElementTypesById();
    }

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
