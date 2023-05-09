////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "LAMMPSDumpLocalImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(LAMMPSDumpLocalImporter);
DEFINE_PROPERTY_FIELD(LAMMPSDumpLocalImporter, columnMapping);
SET_PROPERTY_FIELD_LABEL(LAMMPSDumpLocalImporter, columnMapping, "File column mapping");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSDumpLocalImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first line.
    stream.readLine(15);

    // Dump files written by LAMMPS start with one of the following keywords: TIMESTEP, UNITS or TIME.
    if(!stream.lineStartsWith("ITEM: TIMESTEP") && !stream.lineStartsWith("ITEM: UNITS") && !stream.lineStartsWith("ITEM: TIME"))
        return false;

    // Continue reading until "ITEM: NUMBER OF ENTRIES" line is encountered.
    for(int i = 0; i < 20; i++) {
        if(stream.eof())
            return false;
        stream.readLine();
        if(stream.lineStartsWith("ITEM: NUMBER OF ENTRIES"))
            return true;
    }

    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void LAMMPSDumpLocalImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning LAMMPS dump local file %1").arg(fileHandle().toString()));
    setProgressMaximum(stream.underlyingSize());

    unsigned long long timestep = 0;
    size_t numElements = 0;
    Frame frame(fileHandle());

    while(!stream.eof() && !isCanceled()) {
        qint64 byteOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber();

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                frame.byteOffset = byteOffset;
                frame.lineNumber = lineNumber;
                frame.label = QString("Timestep %1").arg(timestep);
                frames.push_back(frame);
                stream.recordSeekPoint();
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                stream.readLine();
                stream.readLine();
            }
            else if(stream.lineStartsWith("ITEM: NUMBER OF ENTRIES")) {
                // Parse number of entries.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid number of entries in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                if(u > 100'000'000'000ll)
                    throw Exception(tr("LAMMPS dump local file parsing error. Number of entries in line %1 is too large. The LAMMPS dump local file reader doesn't accept files with more than 100 entries.").arg(stream.lineNumber()));
                numElements = (size_t)u;
                break;
            }
            else if(stream.lineStartsWith("ITEM: ENTRIES")) {
                for(size_t i = 0; i < numElements; i++) {
                    stream.readLine();
                    if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
                        return;
                }
                break;
            }
            else if(stream.lineStartsWith("ITEM:")) {
                // Skip lines up to next ITEM:
                while(!stream.eof()) {
                    byteOffset = stream.byteOffset();
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM:"))
                        break;
                }
            }
            else {
                throw Exception(tr("LAMMPS dump local file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSDumpLocalImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading LAMMPS dump local file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Hide particles, because this importer loads non-particle data.
    particles()->setVisElement(nullptr);

    unsigned long long timestep;
    size_t numElements = 0;

    while(!stream.eof()) {

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), dataSource());
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                FloatType simulationTime;
                if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING, &simulationTime) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid time value (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), dataSource());
                break;
            }
            else if(stream.lineStartsWith("ITEM: NUMBER OF ENTRIES")) {
                // Parse number of entries.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid number of entries in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));

                numElements = (size_t)u;
                setBondCount(numElements);
                setProgressMaximum(u);
                break;
            }
            else if(stream.lineStartsWith("ITEM: BOX BOUNDS xy xz yz")) {

                // Parse optional boundary condition flags.
                QStringList tokens = FileImporter::splitString(stream.lineString().mid(qstrlen("ITEM: BOX BOUNDS xy xz yz")));
                if(tokens.size() >= 3)
                    simulationCell()->setPbcFlags(tokens[0] == "pp", tokens[1] == "pp", tokens[2] == "pp");

                // Parse triclinic simulation box.
                FloatType tiltFactors[3];
                Box3 simBox;
                for(int k = 0; k < 3; k++) {
                    if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &simBox.minc[k], &simBox.maxc[k], &tiltFactors[k]) != 3)
                        throw Exception(tr("Invalid box size in line %1 of LAMMPS dump local file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }

                // LAMMPS only stores the outer bounding box of the simulation cell in the dump file.
                // We have to determine the size of the actual triclinic cell.
                simBox.minc.x() -= std::min(std::min(std::min(tiltFactors[0], tiltFactors[1]), tiltFactors[0]+tiltFactors[1]), (FloatType)0);
                simBox.maxc.x() -= std::max(std::max(std::max(tiltFactors[0], tiltFactors[1]), tiltFactors[0]+tiltFactors[1]), (FloatType)0);
                simBox.minc.y() -= std::min(tiltFactors[2], (FloatType)0);
                simBox.maxc.y() -= std::max(tiltFactors[2], (FloatType)0);
                simulationCell()->setCellMatrix(AffineTransformation(
                        Vector3(simBox.sizeX(), 0, 0),
                        Vector3(tiltFactors[0], simBox.sizeY(), 0),
                        Vector3(tiltFactors[1], tiltFactors[2], simBox.sizeZ()),
                        simBox.minc - Point3::Origin()));
                break;
            }
            else if(stream.lineStartsWith("ITEM: BOX BOUNDS")) {
                // Parse optional boundary condition flags.
                QStringList tokens = FileImporter::splitString(stream.lineString().mid(qstrlen("ITEM: BOX BOUNDS")));
                if(tokens.size() >= 3)
                    simulationCell()->setPbcFlags(tokens[0] == "pp", tokens[1] == "pp", tokens[2] == "pp");

                // Parse orthogonal simulation box size.
                Box3 simBox;
                for(int k = 0; k < 3; k++) {
                    if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &simBox.minc[k], &simBox.maxc[k]) != 2)
                        throw Exception(tr("Invalid box size in line %1 of LAMMPS dump local file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }

                simulationCell()->setCellMatrix(AffineTransformation(
                        Vector3(simBox.sizeX(), 0, 0),
                        Vector3(0, simBox.sizeY(), 0),
                        Vector3(0, 0, simBox.sizeZ()),
                        simBox.minc - Point3::Origin()));
                break;
            }
            else if(stream.lineStartsWith("ITEM: ENTRIES")) {

                // Read the column names list.
                QStringList tokens = FileImporter::splitString(stream.lineString());
                OVITO_ASSERT(tokens[0] == "ITEM:" && tokens[1] == "ENTRIES");
                QStringList fileColumnNames = tokens.mid(2);

                // Parse data columns.
                InputColumnReader columnParser(*this, _columnMapping, bonds());

                // If possible, use memory-mapped file access for best performance.
                const char* s_start;
                const char* s_end;
                std::tie(s_start, s_end) = stream.mmap();
                auto s = s_start;
                int lineNumber = stream.lineNumber() + 1;
                try {
                    for(size_t i = 0; i < numElements; i++, lineNumber++) {
                        if(!setProgressValueIntermittent(i)) return;
                        if(!s)
                            columnParser.readElement(i, stream.readLine());
                        else
                            s = columnParser.readElement(i, s, s_end);
                    }
                }
                catch(Exception& ex) {
                    throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS dump local file.").arg(lineNumber));
                }
                if(s) {
                    stream.munmap();
                    stream.seek(stream.byteOffset() + (s - s_start));
                }

                // Sort the element types since we created them on the go while parsing the file. Otherwise their order be dependent on the first occurrence of element types in the file.
                columnParser.sortElementTypes();
                columnParser.reset();

                // If the bond "Topology" property was loaded, we need to shift particle indices by 1, because LAMMPS
                // uses 1-based atom IDs and OVITO uses 0-based indices.
                if(PropertyAccess<ParticleIndexPair> topologyProperty = bonds()->getMutableProperty(BondsObject::TopologyProperty)) {
                    for(ParticleIndexPair& ab : topologyProperty) {
                        ab[0] -= 1;
                        ab[1] -= 1;
                    }
                }

                // Detect if there are more simulation frames following in the file (only when reading the first frame).
                if(frame().byteOffset == 0 && !stream.eof()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM: TIMESTEP") || stream.lineStartsWith("ITEM: TIME"))
                        signalAdditionalFrames();
                }

                state().setStatus(tr("%1 bonds at timestep %2").arg(numElements).arg(timestep));

                // Call base implementation to finalize the loaded data.
                ParticleImporter::FrameLoader::loadFile();

                return; // Done!

            }
            else if(stream.lineStartsWith("ITEM:")) {
                // For the sake of forward compatibility, we ignore unknown ITEM sections.
                // Skip lines until the next "ITEM:" is reached.
                while(!stream.eof() && !isCanceled()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM:"))
                        break;
                }
            }
            else {
                throw Exception(tr("LAMMPS dump local file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }

    throw Exception(tr("LAMMPS dump local file parsing error. Unexpected end of file at line %1 or \"ITEM: ENTRIES\" section is not present in dump file.").arg(stream.lineNumber()));
}

/******************************************************************************
* Inspects the header of the given file and returns the number of file columns.
******************************************************************************/
Future<BondInputColumnMapping> LAMMPSDumpLocalImporter::inspectFileHeader(const Frame& frame)
{
    activateCLocale();

    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) {

            // Start parsing the file up to the specification of the file columns.
            CompressedTextReader stream(fileHandle);

            BondInputColumnMapping detectedColumnMapping;
            while(!stream.eof()) {
                // Parse next line.
                stream.readLine();

                if(stream.lineStartsWith("ITEM: ENTRIES")) {
                    // Read the column names list.
                    QStringList tokens = FileImporter::splitString(stream.lineString());
                    OVITO_ASSERT(tokens[0] == "ITEM:" && tokens[1] == "ENTRIES");
                    QStringList fileColumnNames = tokens.mid(2);

                    if(fileColumnNames.isEmpty()) {
                        // If no file columns names are available, count at least the number of columns in the first data line.
                        stream.readLine();
                        int columnCount = FileImporter::splitString(stream.lineString()).size();
                        detectedColumnMapping.resize(columnCount);
                    }
                    else {
                        detectedColumnMapping.resize(fileColumnNames.size());
                        for(int i = 0; i < fileColumnNames.size(); i++)
                            detectedColumnMapping[i].columnName = fileColumnNames[i];
                    }
                    break;
                }
            }
            return detectedColumnMapping;
        });
}

}   // End of namespace
