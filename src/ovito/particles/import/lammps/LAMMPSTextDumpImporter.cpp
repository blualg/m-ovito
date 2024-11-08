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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "LAMMPSTextDumpImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSTextDumpImporter);
OVITO_CLASSINFO(LAMMPSTextDumpImporter, "DisplayName", "LAMMPS Dump");
DEFINE_PROPERTY_FIELD(LAMMPSTextDumpImporter, useCustomColumnMapping);
DEFINE_PROPERTY_FIELD(LAMMPSTextDumpImporter, customColumnMapping);
SET_PROPERTY_FIELD_LABEL(LAMMPSTextDumpImporter, useCustomColumnMapping, "Custom file column mapping");
SET_PROPERTY_FIELD_LABEL(LAMMPSTextDumpImporter, customColumnMapping, "File column mapping");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSTextDumpImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first line.
    stream.readLine(15);

    // Dump files written by LAMMPS start with one of the following keywords: TIMESTEP, UNITS or TIME.
    if(!stream.lineStartsWith("ITEM: TIMESTEP") && !stream.lineStartsWith("ITEM: UNITS") && !stream.lineStartsWith("ITEM: TIME"))
        return false;

    // Continue reading until "ITEM: NUMBER OF ATOMS" line is encountered.
    for(int i = 0; i < 20; i++) {
        if(stream.eof())
            return false;
        stream.readLine();
        if(stream.lineStartsWith("ITEM: NUMBER OF ATOMS"))
            return true;
    }

    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void LAMMPSTextDumpImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    CompressedTextReader stream(fileHandle);

    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Scanning LAMMPS dump file %1").arg(fileHandle.toString()));
    progress.setProgressMaximum(stream.underlyingSize());

    unsigned long long timestep = 0;
    size_t numParticles = 0;
    Frame frame(fileHandle);

    while(!stream.eof() && !this_task::isCanceled()) {
        qint64 byteOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber();

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                // Note: For first frame, always use byte offset/line number 0, because otherwise a reload of frame 0 is triggered by the FileSource.
                if(!frames.empty()) {
                    frame.byteOffset = byteOffset;
                    frame.lineNumber = lineNumber;
                }
                frame.label = QStringLiteral("Timestep %1").arg(timestep);
                frames.push_back(frame);
                stream.recordSeekPoint();
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                stream.readLine();
                stream.readLine();
            }
            else if(stream.lineStartsWith("ITEM: NUMBER OF ATOMS")) {
                // Parse number of atoms.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid number of atoms in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                if(u > 100'000'000'000ll)
                    throw Exception(tr("LAMMPS dump file parsing error. Number of atoms in line %1 is too large. The LAMMPS dump file reader doesn't accept files with more than 100 billion atoms.").arg(stream.lineNumber()));
                numParticles = (size_t)u;
                break;
            }
            else if(stream.lineStartsWith("ITEM: ATOMS")) {
                for(size_t i = 0; i < numParticles; i++) {
                    stream.readLine();
                    // Update progress bar and check for user cancellation.
                    progress.setProgressValueIntermittent(stream.underlyingByteOffset());
                }
                break;
            }
            else if(stream.lineStartsWith("ITEM:")) {
                // Skip lines up to next ITEM:
                while(!stream.eof()) {
                    byteOffset = stream.byteOffset();
                    lineNumber = stream.lineNumber();
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM:"))
                        break;
                }
            }
            else {
                throw Exception(tr("LAMMPS dump file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSTextDumpImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Reading LAMMPS dump file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    unsigned long long timestep;
    size_t numParticles = 0;

    while(!stream.eof()) {

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), pipelineNode());
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                FloatType simulationTime;
                if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING, &simulationTime) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid time value (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), pipelineNode());
                break;
            }
            else if(stream.lineStartsWith("ITEM: NUMBER OF ATOMS")) {
                // Parse number of atoms.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid number of atoms in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                if(u >= 2147483648ull)
                    throw Exception(tr("LAMMPS dump file parsing error. Number of atoms in line %1 exceeds internal limit of 2^31 atoms:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));

                numParticles = (size_t)u;
                setParticleCount(numParticles);
                progress.setProgressMaximum(u);
                break;
            }
            else if(stream.lineStartsWith("ITEM: BOX BOUNDS xy xz yz")) {
                // Parse optional boundary condition flags.
                QStringList tokens = FileImporter::splitString(stream.lineString().mid(qstrlen("ITEM: BOX BOUNDS xy xz yz")));
                if(tokens.size() >= 3) simulationCell()->setPbcFlags(tokens[0] == "pp", tokens[1] == "pp", tokens[2] == "pp");

                // Parse triclinic simulation box.
                FloatType tiltFactors[3];
                Box3 simBox;
                for(int k = 0; k < 3; k++) {
                    if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                              &simBox.minc[k], &simBox.maxc[k], &tiltFactors[k]) != 3)
                        throw Exception(
                            tr("Invalid box size in line %1 of LAMMPS dump file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }

                // LAMMPS only stores the outer bounding box of the simulation cell in the dump file.
                // We have to determine the size of the actual triclinic cell.
                simBox.minc.x() -=
                    std::min(std::min(std::min(tiltFactors[0], tiltFactors[1]), tiltFactors[0] + tiltFactors[1]), (FloatType)0);
                simBox.maxc.x() -=
                    std::max(std::max(std::max(tiltFactors[0], tiltFactors[1]), tiltFactors[0] + tiltFactors[1]), (FloatType)0);
                simBox.minc.y() -= std::min(tiltFactors[2], (FloatType)0);
                simBox.maxc.y() -= std::max(tiltFactors[2], (FloatType)0);
                simulationCell()->setCellMatrix(
                    AffineTransformation(Vector3(simBox.sizeX(), 0, 0), Vector3(tiltFactors[0], simBox.sizeY(), 0),
                                         Vector3(tiltFactors[1], tiltFactors[2], simBox.sizeZ()), simBox.minc - Point3::Origin()));
                break;
            }
            else if(stream.lineStartsWith("ITEM: BOX BOUNDS abc origin")) {
                // Parse general triclinic simulation box.
                // Format:
                // ITEM: BOX BOUNDS abc origin [boundary-strings]
                // avec[0] avec[1] avec[2] origin[0]
                // bvec[0] bvec[1] bvec[2] origin[1]
                // cvec[0] cvec[1] cvec[2] origin[2]
                QStringList tokens = FileImporter::splitString(stream.lineString().sliced(qstrlen("ITEM: BOX BOUNDS abc origin")));
                if(tokens.size() >= 3) {
                    simulationCell()->setPbcFlags(tokens[0] == "pp", tokens[1] == "pp", tokens[2] == "pp");
                }
                AffineTransformation simCell;
                for(int k = 0; k < 3; k++) {
                    if(sscanf(stream.readLine(),
                              FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                              &simCell[k][0], &simCell[k][1], &simCell[k][2], &simCell[3][k]) != 4)
                        throw Exception(tr("Invalid cell vectors in line %1 of LAMMPS dump file: %2")
                                            .arg(stream.lineNumber())
                                            .arg(stream.lineString()));
                }
                simulationCell()->setCellMatrix(simCell);
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
                        throw Exception(tr("Invalid box size in line %1 of dump file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }

                simulationCell()->setCellMatrix(AffineTransformation(
                        Vector3(simBox.sizeX(), 0, 0),
                        Vector3(0, simBox.sizeY(), 0),
                        Vector3(0, 0, simBox.sizeZ()),
                        simBox.minc - Point3::Origin()));
                break;
            }
            else if(stream.lineStartsWith("ITEM: ATOMS")) {

                // Read the column names list.
                QStringList tokens = FileImporter::splitString(stream.lineString());
                OVITO_ASSERT(tokens[0] == "ITEM:" && tokens[1] == "ATOMS");
                QStringList fileColumnNames = tokens.mid(2);

                // Set up column-to-property mapping.
                ParticleInputColumnMapping columnMapping;
                if(_useCustomColumnMapping)
                    columnMapping = _customColumnMapping;
                else
                    columnMapping = generateAutomaticColumnMapping(fileColumnNames);

                // Parse data columns.
                InputColumnReader columnParser(*this, columnMapping, particles());

                // Check if there is an 'element' file column containing the atom type names.
                int elementColumn = fileColumnNames.indexOf(QStringLiteral("element"));
                if(elementColumn != -1) {
                    int typeColumn = fileColumnNames.indexOf(QStringLiteral("type"));
                    if(typeColumn != -1 && columnMapping[typeColumn].isMapped()) {
                        columnParser.readTypeNamesFromColumn(elementColumn, typeColumn);
                    }
                }

                // If possible, use memory-mapped file access for best performance.
                const char* s_start;
                const char* s_end;
                std::tie(s_start, s_end) = stream.mmap();
                auto s = s_start;
                int lineNumber = stream.lineNumber() + 1;
                try {
                    for(size_t i = 0; i < numParticles; i++, lineNumber++) {
                        // Update progress bar and check for user cancellation.
                        progress.setProgressValueIntermittent(i);
                        if(!s)
                            columnParser.readElement(i, stream.readLine());
                        else
                            s = columnParser.readElement(i, s, s_end);
                    }
                }
                catch(Exception& ex) {
                    throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS dump file.").arg(lineNumber));
                }
                if(s) {
                    stream.munmap();
                    stream.seek(stream.byteOffset() + (s - s_start));
                }

                // Sort the particle type list since we created particles on the go and their order depends on the occurrence of types in the file.
                columnParser.sortElementTypes();
                columnParser.reset();

                // After parsing the particle data, post-processes the particle properties.
                postprocessParticleProperties(fileColumnNames, columnMapping);

                // Detect if there are more simulation frames following in the file (only when reading the first frame).
                if(frame().byteOffset == 0 && !stream.eof()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM: TIMESTEP") || stream.lineStartsWith("ITEM: TIME"))
                        signalAdditionalFrames();
                }

                state().setStatus(tr("%1 particles at timestep %2").arg(numParticles).arg(timestep));

                // Call base implementation to finalize the loaded particle data.
                ParticleImporter::FrameLoader::loadFile();

                return; // Done!
            }
            else if(stream.lineStartsWith("ITEM:")) {
                // For the sake of forward compatibility, we ignore unknown ITEM sections.
                // Skip lines until the next "ITEM:" is reached.
                while(!stream.eof() && !this_task::isCanceled()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM:"))
                        break;
                }
            }
            else {
                throw Exception(tr("LAMMPS dump file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }

    throw Exception(tr("LAMMPS dump file parsing error. Unexpected end of file at line %1 or \"ITEM: ATOMS\" section is not present in dump file.").arg(stream.lineNumber()));
}

/******************************************************************************
 * After parsing the particle data, this method post-processes the particle properties.
 *****************************************************************************/
void LAMMPSTextDumpImporter::FrameLoader::postprocessParticleProperties(const QStringList& fileColumnNames, const ParticleInputColumnMapping& columnMapping)
{
    // Determine if particle coordinates are given in reduced form and need to be rescaled to absolute form.
    bool reducedCoordinates = false;
    if(!fileColumnNames.empty()) {
        // If the dump file contains column names, then we can use them to detect
        // the type of particle coordinates. Reduced coordinates are found in columns
        // "xs, ys, zs" or "xsu, ysu, zsu".
        for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
            if(columnMapping[i].property.isStandardProperty(&Particles::OOClass(), Particles::PositionProperty)) {
                reducedCoordinates = (
                        fileColumnNames[i] == "xs" || fileColumnNames[i] == "xsu" ||
                        fileColumnNames[i] == "ys" || fileColumnNames[i] == "ysu" ||
                        fileColumnNames[i] == "zs" || fileColumnNames[i] == "zsu");
                // break; Note: Do not stop the loop here, because the 'Position' particle
                // property may be associated with several file columns, and it's the last column that
                // ends up getting imported into OVITO.
            }
        }
    }
    else {
        // If no column names are available, use the following heuristic:
        // Assume reduced coordinates if all particle coordinates are within the [-0.02,1.02] interval.
        // We allow coordinates to be slightly outside the [0,1] interval, because LAMMPS
        // wraps around particles at the periodic boundaries only occasionally.
        if(BufferReadAccess<Point3> posProperty = particles()->getProperty(Particles::PositionProperty)) {
            // Compute bounding box of particle positions.
            Box3 boundingBox;
            boundingBox.addPoints(posProperty);
            // Check if bounding box is inside the (slightly extended) unit cube.
            if(Box3(Point3(FloatType(-0.02)), Point3(FloatType(1.02))).containsBox(boundingBox))
                reducedCoordinates = true;
        }
    }

    if(reducedCoordinates) {
        if(Property* positions = particles()->getMutableProperty(Particles::PositionProperty)) {
            const AffineTransformation simCell = simulationCell()->cellMatrix();
#ifdef OVITO_USE_SYCL
            SyclBufferAccess<Point3, access_mode::read_write>{positions}.for_each([simCell](Point3& p) {
                p = simCell * p;
            });
#else
            // Convert all atom coordinates from reduced to absolute (Cartesian) format.
            for(Point3& p : BufferWriteAccess<Point3, access_mode::read_write>{positions})
                p = simCell * p;
#endif
        }
    }

    if(!fileColumnNames.empty()) {
        // If a "diameter" column was loaded and stored in the "Radius" particle property,
        // we need to divide values by two.
        for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
            if(columnMapping[i].property.isStandardProperty(&Particles::OOClass(), Particles::RadiusProperty) && fileColumnNames[i] == "diameter") {
                if(Property* radii = particles()->getMutableProperty(Particles::RadiusProperty)) {
#ifdef OVITO_USE_SYCL
                    SyclBufferAccess<GraphicsFloatType, access_mode::read_write>{radii}.for_each([](GraphicsFloatType& r) {
                        r *= GraphicsFloatType(0.5);
                    });
#else
                    for(auto& r : BufferWriteAccess<GraphicsFloatType, access_mode::read_write>{radii})
                        r *= GraphicsFloatType(0.5);
#endif
                }
                break;
            }
        }

        // Same for the "c_diameter[1..3]" columns or "shapex/shapey/shapez" columns being mapped to the "Aspherical Shape" property.
        for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
            if(columnMapping[i].property.isStandardProperty(&Particles::OOClass(), Particles::AsphericalShapeProperty) &&
                (fileColumnNames[i] == "c_diameter[1]" || fileColumnNames[i] == "c_diameter[2]" || fileColumnNames[i] == "c_diameter[3]" ||
                    fileColumnNames[i] == "shapex" || fileColumnNames[i] == "shapey" || fileColumnNames[i] == "shapez")) {
                if(Property* shapeProperty = particles()->getMutableProperty(Particles::AsphericalShapeProperty)) {
#ifdef OVITO_USE_SYCL
                    SyclBufferAccess<Vector3G, access_mode::read_write>{shapeProperty}.for_each([](Vector3G& s) {
                        s *= GraphicsFloatType(0.5);
                    });
#else
                    for(auto& s : BufferWriteAccess<Vector3G, access_mode::read_write>{shapeProperty}) {
                        s *= GraphicsFloatType(0.5);
                    }
#endif
                }
                break;
            }
        }
    }

    // Detect dimensionality of system. It's a 2D system if no file column has been mapped to the Position.Z particle property (but Position.X/Y are present).
    if(std::none_of(columnMapping.begin(), columnMapping.end(), [](const InputColumnInfo& column) {
        return column.property.isStandardProperty(&Particles::OOClass(), Particles::PositionProperty) && column.property.componentIndex(&Particles::OOClass()) == 2;
    }) && std::any_of(columnMapping.begin(), columnMapping.end(), [](const InputColumnInfo& column) {
        return column.property.isStandardProperty(&Particles::OOClass(), Particles::PositionProperty) && column.property.componentIndex(&Particles::OOClass()) != 2;
    })) {
        simulationCell()->setIs2D(true);
    }

    // Sort particles by ID.
    if(_sortParticles)
        particles()->sortById();

}

/******************************************************************************
 * Guesses the mapping of input file columns to internal particle properties.
 *****************************************************************************/
ParticleInputColumnMapping LAMMPSTextDumpImporter::generateAutomaticColumnMapping(const QStringList& columnNames)
{
    ParticleInputColumnMapping columnMapping;
    columnMapping.resize(columnNames.size());
    for(int i = 0; i < columnNames.size(); i++) {
        QString name = columnNames[i].toLower();
        columnMapping[i].columnName = columnNames[i];
        if(name == "x" || name == "xu" || name == "coordinates") columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 0);
        else if(name == "y" || name == "yu") columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 1);
        else if(name == "z" || name == "zu") columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 2);
        else if(name == "xs" || name == "xsu") { columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 0); }
        else if(name == "ys" || name == "ysu") { columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 1); }
        else if(name == "zs" || name == "zsu") { columnMapping.mapColumnToStandardProperty(i, Particles::PositionProperty, 2); }
        else if(name == "vx" || name == "velocities") columnMapping.mapColumnToStandardProperty(i, Particles::VelocityProperty, 0);
        else if(name == "vy") columnMapping.mapColumnToStandardProperty(i, Particles::VelocityProperty, 1);
        else if(name == "vz") columnMapping.mapColumnToStandardProperty(i, Particles::VelocityProperty, 2);
        else if(name == "id") columnMapping.mapColumnToStandardProperty(i, Particles::IdentifierProperty);
        else if(name == "element") columnMapping.mapColumnToStandardProperty(i, Particles::TypeProperty);
        else if(name == "type") {
            if(!columnMapping.mapColumnToStandardProperty(i, Particles::TypeProperty)) {
                // Give precedence of the 'type' column over the 'element' column.
                for(int j = 0; j < i; j++) {
                    if(columnNames[j].compare(QStringLiteral("element"), Qt::CaseInsensitive) == 0) {
                        columnMapping[j].unmap();
                        columnMapping.mapColumnToStandardProperty(i, Particles::TypeProperty);
                        break;
                    }
                }
            }
        }
        else if(name == "radius" || name == "diameter") columnMapping.mapColumnToStandardProperty(i, Particles::RadiusProperty);
        else if(name == "mol") columnMapping.mapColumnToStandardProperty(i, Particles::MoleculeProperty);
        else if(name == "q") columnMapping.mapColumnToStandardProperty(i, Particles::ChargeProperty);
        else if(name == "ix") columnMapping.mapColumnToStandardProperty(i, Particles::PeriodicImageProperty, 0);
        else if(name == "iy") columnMapping.mapColumnToStandardProperty(i, Particles::PeriodicImageProperty, 1);
        else if(name == "iz") columnMapping.mapColumnToStandardProperty(i, Particles::PeriodicImageProperty, 2);
        else if(name == "fx" || name == "forces") columnMapping.mapColumnToStandardProperty(i, Particles::ForceProperty, 0);
        else if(name == "fy") columnMapping.mapColumnToStandardProperty(i, Particles::ForceProperty, 1);
        else if(name == "fz") columnMapping.mapColumnToStandardProperty(i, Particles::ForceProperty, 2);
        else if(name == "mux") columnMapping.mapColumnToStandardProperty(i, Particles::DipoleOrientationProperty, 0);
        else if(name == "muy") columnMapping.mapColumnToStandardProperty(i, Particles::DipoleOrientationProperty, 1);
        else if(name == "muz") columnMapping.mapColumnToStandardProperty(i, Particles::DipoleOrientationProperty, 2);
        else if(name == "mu") columnMapping.mapColumnToStandardProperty(i, Particles::DipoleMagnitudeProperty);
        else if(name == "omegax") columnMapping.mapColumnToStandardProperty(i, Particles::AngularVelocityProperty, 0);
        else if(name == "omegay") columnMapping.mapColumnToStandardProperty(i, Particles::AngularVelocityProperty, 1);
        else if(name == "omegaz") columnMapping.mapColumnToStandardProperty(i, Particles::AngularVelocityProperty, 2);
        else if(name == "angmomx") columnMapping.mapColumnToStandardProperty(i, Particles::AngularMomentumProperty, 0);
        else if(name == "angmomy") columnMapping.mapColumnToStandardProperty(i, Particles::AngularMomentumProperty, 1);
        else if(name == "angmomz") columnMapping.mapColumnToStandardProperty(i, Particles::AngularMomentumProperty, 2);
        else if(name == "tqx") columnMapping.mapColumnToStandardProperty(i, Particles::TorqueProperty, 0);
        else if(name == "tqy") columnMapping.mapColumnToStandardProperty(i, Particles::TorqueProperty, 1);
        else if(name == "tqz") columnMapping.mapColumnToStandardProperty(i, Particles::TorqueProperty, 2);
        else if(name == "c_cna" || name == "pattern") columnMapping.mapColumnToStandardProperty(i, Particles::StructureTypeProperty);
        else if(name == "c_epot") columnMapping.mapColumnToStandardProperty(i, Particles::PotentialEnergyProperty);
        else if(name == "c_kpot") columnMapping.mapColumnToStandardProperty(i, Particles::KineticEnergyProperty);
        else if(name == "c_stress[1]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 0);
        else if(name == "c_stress[2]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 1);
        else if(name == "c_stress[3]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 2);
        else if(name == "c_stress[4]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 3);
        else if(name == "c_stress[5]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 4);
        else if(name == "c_stress[6]") columnMapping.mapColumnToStandardProperty(i, Particles::StressTensorProperty, 5);
        else if(name == "c_orient[1]" || name == "quati") columnMapping.mapColumnToStandardProperty(i, Particles::OrientationProperty, 0);
        else if(name == "c_orient[2]" || name == "quatj") columnMapping.mapColumnToStandardProperty(i, Particles::OrientationProperty, 1);
        else if(name == "c_orient[3]" || name == "quatk") columnMapping.mapColumnToStandardProperty(i, Particles::OrientationProperty, 2);
        else if(name == "c_orient[4]" || name == "quatw") columnMapping.mapColumnToStandardProperty(i, Particles::OrientationProperty, 3);
        else if(name == "c_shape[1]" || name == "c_diameter[1]" || name == "shapex") columnMapping.mapColumnToStandardProperty(i, Particles::AsphericalShapeProperty, 0);
        else if(name == "c_shape[2]" || name == "c_diameter[2]" || name == "shapey") columnMapping.mapColumnToStandardProperty(i, Particles::AsphericalShapeProperty, 1);
        else if(name == "c_shape[3]" || name == "c_diameter[3]" || name == "shapez") columnMapping.mapColumnToStandardProperty(i, Particles::AsphericalShapeProperty, 2);
        else {
            // Automatically map columns to standard OVITO particle properties.
            bool isStandardProperty = false;
            const static QRegularExpression invalidCharacters(QStringLiteral("[^A-Za-z\\d_]"));
            for(auto entry = Particles::OOClass().standardPropertyIds().cbegin(), end = Particles::OOClass().standardPropertyIds().cend(); entry != end; ++entry) {
                const auto componentCount = Particles::OOClass().standardPropertyComponentCount(entry->second);
                for(size_t component = 0; component < componentCount; component++) {
                    QString propertyName = entry->first;
                    propertyName.remove(invalidCharacters); // LAMMPS dump file format does not support column names containing spaces.
                    const QStringList& componentNames = Particles::OOClass().standardPropertyComponentNames(entry->second);
                    QString propertyName2;
                    if(!componentNames.empty()) {
                        OVITO_ASSERT(!componentNames[component].contains(invalidCharacters));
                        propertyName2 = propertyName + componentNames[component];
                        propertyName += QChar('.');
                        propertyName += componentNames[component];
                    }
                    if(propertyName.compare(name, Qt::CaseInsensitive) == 0 || propertyName2.compare(name, Qt::CaseInsensitive) == 0) {
                        columnMapping.mapColumnToStandardProperty(i, (Particles::Type)entry->second, component);
                        isStandardProperty = true;
                        break;
                    }
                }
                if(isStandardProperty)
                    break;
            }
            // If automatic mapping to one of the standard properties was unsuccessful, read the file column as a user-defined property.
            if(!isStandardProperty)
                columnMapping.mapColumnToUserProperty(i, Property::makePropertyNameValid(name), Property::FloatDefault);
        }
    }
    return columnMapping;
}

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void LAMMPSTextDumpImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
    ParticleImporter::propertyChanged(field);

    if((field == PROPERTY_FIELD(customColumnMapping) || field == PROPERTY_FIELD(useCustomColumnMapping)) && !isBeingLoaded()) {
        requestReload();
    }
}

/******************************************************************************
 * Saves the class' contents to the given stream.
 *****************************************************************************/
void LAMMPSTextDumpImporter::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    ParticleImporter::saveToStream(stream, excludeRecomputableData);

    stream.beginChunk(0x02);
    stream.endChunk();
}

/******************************************************************************
 * Loads the class' contents from the given stream.
 *****************************************************************************/
void LAMMPSTextDumpImporter::loadFromStream(ObjectLoadStream& stream)
{
    ParticleImporter::loadFromStream(stream);

    // For backward compatibility with OVITO 3.1:
    if(stream.expectChunkRange(0x00, 0x02) == 0x01) {
        stream >> _customColumnMapping.mutableValue();
    }
    stream.closeChunk();
}

/******************************************************************************
* Inspects the header of the given file and returns the number of file columns.
******************************************************************************/
Future<ParticleInputColumnMapping> LAMMPSTextDumpImporter::inspectFileHeader(const Frame& frame)
{
    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) {

            // Start parsing the file up to the specification of the file columns.
            CompressedTextReader stream(fileHandle);

            ParticleInputColumnMapping detectedColumnMapping;
            while(!stream.eof()) {
                // Parse next line.
                stream.readLine();

                if(stream.lineStartsWith("ITEM: ATOMS")) {
                    // Read the column names list.
                    QStringList tokens = FileImporter::splitString(stream.lineString());
                    OVITO_ASSERT(tokens[0] == "ITEM:" && tokens[1] == "ATOMS");
                    QStringList fileColumnNames = tokens.mid(2);

                    if(fileColumnNames.isEmpty()) {
                        // If no file columns names are available, count at least the number of columns in the first atom line.
                        stream.readLine();
                        int columnCount = FileImporter::splitString(stream.lineString()).size();
                        detectedColumnMapping.resize(columnCount);
                    }
                    else {
                        detectedColumnMapping = generateAutomaticColumnMapping(fileColumnNames);
                    }
                    break;
                }
            }
            return detectedColumnMapping;
        });
}

}   // End of namespace
