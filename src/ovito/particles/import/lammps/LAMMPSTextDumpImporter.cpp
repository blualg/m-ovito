////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "LAMMPSTextDumpImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(LAMMPSTextDumpImporter);
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
void LAMMPSTextDumpImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning LAMMPS dump file %1").arg(fileHandle().toString()));
    setProgressMaximum(stream.underlyingSize());

    unsigned long long timestep = 0;
    size_t numParticles = 0;
    Frame frame(fileHandle());

    while(!stream.eof() && !isCanceled()) {
        qint64 byteOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber();

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
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
    setProgressText(tr("Reading LAMMPS dump file %1").arg(fileHandle().toString()));

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
                state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), dataSource());
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                FloatType simulationTime;
                if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING, &simulationTime) != 1)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid time value (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), dataSource());
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
                        throw Exception(tr("Invalid box size in line %1 of LAMMPS dump file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
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
                        if(!setProgressValueIntermittent(i)) return;
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

                // Determine if particle coordinates are given in reduced form and need to be rescaled to absolute form.
                bool reducedCoordinates = false;
                if(!fileColumnNames.empty()) {
                    // If the dump file contains column names, then we can use them to detect
                    // the type of particle coordinates. Reduced coordinates are found in columns
                    // "xs, ys, zs" or "xsu, ysu, zsu".
                    for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
                        if(columnMapping[i].property.type() == ParticlesObject::PositionProperty) {
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
                    if(ConstPropertyAccess<Point3> posProperty = particles()->getProperty(ParticlesObject::PositionProperty)) {
                        // Compute bounding box of particle positions.
                        Box3 boundingBox;
                        boundingBox.addPoints(posProperty);
                        // Check if bounding box is inside the (slightly extended) unit cube.
                        if(Box3(Point3(FloatType(-0.02)), Point3(FloatType(1.02))).containsBox(boundingBox))
                            reducedCoordinates = true;
                    }
                }

                if(reducedCoordinates) {
                    // Convert all atom coordinates from reduced to absolute (Cartesian) format.
                    if(PropertyAccess<Point3> posProperty = particles()->getMutableProperty(ParticlesObject::PositionProperty)) {
                        const AffineTransformation simCell = simulationCell()->cellMatrix();
                        for(Point3& p : posProperty)
                            p = simCell * p;
                    }
                }

                if(!fileColumnNames.empty()) {
                    // If a "diameter" column was loaded and stored in the "Radius" particle property,
                    // we need to divide values by two.
                    for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
                        if(columnMapping[i].property.type() == ParticlesObject::RadiusProperty && fileColumnNames[i] == "diameter") {
                            if(PropertyAccess<GraphicsFloatType> radiusProperty = particles()->getMutableProperty(ParticlesObject::RadiusProperty)) {
                                for(auto& r : radiusProperty)
                                    r *= GraphicsFloatType(0.5);
                            }
                            break;
                        }
                    }

                    // Same for the "c_diameter[1..3]" columns or "shapex/shapey/shapez" columns being mapped to the "Aspherical Shape" property.
                    for(int i = 0; i < (int)columnMapping.size() && i < fileColumnNames.size(); i++) {
                        if(columnMapping[i].property.type() == ParticlesObject::AsphericalShapeProperty &&
                            (fileColumnNames[i] == "c_diameter[1]" || fileColumnNames[i] == "c_diameter[2]" || fileColumnNames[i] == "c_diameter[3]" ||
                             fileColumnNames[i] == "shapex" || fileColumnNames[i] == "shapey" || fileColumnNames[i] == "shapez")) {
                            if(PropertyAccess<Vector3G> shapeProperty = particles()->getMutableProperty(ParticlesObject::AsphericalShapeProperty)) {
                                for(auto& s : shapeProperty) {
                                    s.x() *= GraphicsFloatType(0.5);
                                    s.y() *= GraphicsFloatType(0.5);
                                    s.z() *= GraphicsFloatType(0.5);
                                }
                            }
                            break;
                        }
                    }
                }

                // Detect dimensionality of system. It's a 2D system if no file column has been mapped to the Position.Z particle property (but Position.X/Y are present).
                if(std::none_of(columnMapping.begin(), columnMapping.end(), [](const InputColumnInfo& column) {
                    return column.property.type() == ParticlesObject::PositionProperty && column.property.vectorComponent() == 2;
                }) && std::any_of(columnMapping.begin(), columnMapping.end(), [](const InputColumnInfo& column) {
                    return column.property.type() == ParticlesObject::PositionProperty && column.property.vectorComponent() != 2;
                })) {
                    simulationCell()->setIs2D(true);
                }

                // Detect if there are more simulation frames following in the file (only when reading the first frame).
                if(frame().byteOffset == 0 && !stream.eof()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM: TIMESTEP") || stream.lineStartsWith("ITEM: TIME"))
                        signalAdditionalFrames();
                }

                // Sort particles by ID.
                if(_sortParticles)
                    particles()->sortById();

                state().setStatus(tr("%1 particles at timestep %2").arg(numParticles).arg(timestep));

                // Call base implementation to finalize the loaded particle data.
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
                throw Exception(tr("LAMMPS dump file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }

    throw Exception(tr("LAMMPS dump file parsing error. Unexpected end of file at line %1 or \"ITEM: ATOMS\" section is not present in dump file.").arg(stream.lineNumber()));
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
        if(name == "x" || name == "xu" || name == "coordinates") columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 0);
        else if(name == "y" || name == "yu") columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 1);
        else if(name == "z" || name == "zu") columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 2);
        else if(name == "xs" || name == "xsu") { columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 0); }
        else if(name == "ys" || name == "ysu") { columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 1); }
        else if(name == "zs" || name == "zsu") { columnMapping.mapStandardColumn(i, ParticlesObject::PositionProperty, 2); }
        else if(name == "vx" || name == "velocities") columnMapping.mapStandardColumn(i, ParticlesObject::VelocityProperty, 0);
        else if(name == "vy") columnMapping.mapStandardColumn(i, ParticlesObject::VelocityProperty, 1);
        else if(name == "vz") columnMapping.mapStandardColumn(i, ParticlesObject::VelocityProperty, 2);
        else if(name == "id") columnMapping.mapStandardColumn(i, ParticlesObject::IdentifierProperty);
        else if(name == "element") columnMapping.mapStandardColumn(i, ParticlesObject::TypeProperty);
        else if(name == "type") {
            if(!columnMapping.mapStandardColumn(i, ParticlesObject::TypeProperty)) {
                // Give precedence of the 'type' column over the 'element' column.
                for(int j = 0; j < i; j++) {
                    if(columnNames[j].compare(QStringLiteral("element"), Qt::CaseInsensitive) == 0) {
                        columnMapping[j].unmap();
                        columnMapping.mapStandardColumn(i, ParticlesObject::TypeProperty);
                        break;
                    }
                }
            }
        }
        else if(name == "mass") columnMapping.mapStandardColumn(i, ParticlesObject::MassProperty);
        else if(name == "radius" || name == "diameter") columnMapping.mapStandardColumn(i, ParticlesObject::RadiusProperty);
        else if(name == "mol") columnMapping.mapStandardColumn(i, ParticlesObject::MoleculeProperty);
        else if(name == "q") columnMapping.mapStandardColumn(i, ParticlesObject::ChargeProperty);
        else if(name == "ix") columnMapping.mapStandardColumn(i, ParticlesObject::PeriodicImageProperty, 0);
        else if(name == "iy") columnMapping.mapStandardColumn(i, ParticlesObject::PeriodicImageProperty, 1);
        else if(name == "iz") columnMapping.mapStandardColumn(i, ParticlesObject::PeriodicImageProperty, 2);
        else if(name == "fx" || name == "forces") columnMapping.mapStandardColumn(i, ParticlesObject::ForceProperty, 0);
        else if(name == "fy") columnMapping.mapStandardColumn(i, ParticlesObject::ForceProperty, 1);
        else if(name == "fz") columnMapping.mapStandardColumn(i, ParticlesObject::ForceProperty, 2);
        else if(name == "mux") columnMapping.mapStandardColumn(i, ParticlesObject::DipoleOrientationProperty, 0);
        else if(name == "muy") columnMapping.mapStandardColumn(i, ParticlesObject::DipoleOrientationProperty, 1);
        else if(name == "muz") columnMapping.mapStandardColumn(i, ParticlesObject::DipoleOrientationProperty, 2);
        else if(name == "mu") columnMapping.mapStandardColumn(i, ParticlesObject::DipoleMagnitudeProperty);
        else if(name == "omegax") columnMapping.mapStandardColumn(i, ParticlesObject::AngularVelocityProperty, 0);
        else if(name == "omegay") columnMapping.mapStandardColumn(i, ParticlesObject::AngularVelocityProperty, 1);
        else if(name == "omegaz") columnMapping.mapStandardColumn(i, ParticlesObject::AngularVelocityProperty, 2);
        else if(name == "angmomx") columnMapping.mapStandardColumn(i, ParticlesObject::AngularMomentumProperty, 0);
        else if(name == "angmomy") columnMapping.mapStandardColumn(i, ParticlesObject::AngularMomentumProperty, 1);
        else if(name == "angmomz") columnMapping.mapStandardColumn(i, ParticlesObject::AngularMomentumProperty, 2);
        else if(name == "tqx") columnMapping.mapStandardColumn(i, ParticlesObject::TorqueProperty, 0);
        else if(name == "tqy") columnMapping.mapStandardColumn(i, ParticlesObject::TorqueProperty, 1);
        else if(name == "tqz") columnMapping.mapStandardColumn(i, ParticlesObject::TorqueProperty, 2);
        else if(name == "spin") columnMapping.mapStandardColumn(i, ParticlesObject::SpinProperty);
        else if(name == "c_cna" || name == "pattern") columnMapping.mapStandardColumn(i, ParticlesObject::StructureTypeProperty);
        else if(name == "c_epot") columnMapping.mapStandardColumn(i, ParticlesObject::PotentialEnergyProperty);
        else if(name == "c_kpot") columnMapping.mapStandardColumn(i, ParticlesObject::KineticEnergyProperty);
        else if(name == "c_stress[1]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 0);
        else if(name == "c_stress[2]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 1);
        else if(name == "c_stress[3]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 2);
        else if(name == "c_stress[4]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 3);
        else if(name == "c_stress[5]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 4);
        else if(name == "c_stress[6]") columnMapping.mapStandardColumn(i, ParticlesObject::StressTensorProperty, 5);
        else if(name == "c_orient[1]" || name == "quati") columnMapping.mapStandardColumn(i, ParticlesObject::OrientationProperty, 0);
        else if(name == "c_orient[2]" || name == "quatj") columnMapping.mapStandardColumn(i, ParticlesObject::OrientationProperty, 1);
        else if(name == "c_orient[3]" || name == "quatk") columnMapping.mapStandardColumn(i, ParticlesObject::OrientationProperty, 2);
        else if(name == "c_orient[4]" || name == "quatw") columnMapping.mapStandardColumn(i, ParticlesObject::OrientationProperty, 3);
        else if(name == "c_shape[1]" || name == "c_diameter[1]" || name == "shapex") columnMapping.mapStandardColumn(i, ParticlesObject::AsphericalShapeProperty, 0);
        else if(name == "c_shape[2]" || name == "c_diameter[2]" || name == "shapey") columnMapping.mapStandardColumn(i, ParticlesObject::AsphericalShapeProperty, 1);
        else if(name == "c_shape[3]" || name == "c_diameter[3]" || name == "shapez") columnMapping.mapStandardColumn(i, ParticlesObject::AsphericalShapeProperty, 2);
        else if(name == "selection") columnMapping.mapStandardColumn(i, ParticlesObject::SelectionProperty, 0);
        else {
            columnMapping.mapCustomColumn(i, name, PropertyObject::FloatDefault);
        }
    }
    return columnMapping;
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
    activateCLocale();

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
