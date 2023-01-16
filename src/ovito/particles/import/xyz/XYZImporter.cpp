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
//  Contributions:
//
//  Support for extended XYZ format has been added by James Kermode,
//  Department of Physics, King's College London.
//
///////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "XYZImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(XYZImporter);
DEFINE_PROPERTY_FIELD(XYZImporter, autoRescaleCoordinates);
DEFINE_PROPERTY_FIELD(XYZImporter, columnMapping);
SET_PROPERTY_FIELD_LABEL(XYZImporter, autoRescaleCoordinates, "Detect reduced coordinates");
SET_PROPERTY_FIELD_LABEL(XYZImporter, columnMapping, "File column mapping");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool XYZImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first line.
    stream.readLine(20);
    if(stream.line()[0] == '\0')
        return false;

    // Skip initial whitespace.
    const unsigned char* p = reinterpret_cast<const unsigned char*>(stream.line());
    while(isspace(*p)) {
        if(*p == '\0') return false;
        ++p;
    }
    if(!isdigit(*p)) return false;
    // Skip digits.
    while(isdigit(*p)) {
        if(*p == '\0') break;
        ++p;
    }
    // Check trailing whitespace.
    bool foundNewline = false;
    while(*p != '\0') {
        if(!isspace(*p)) return false;
        if(*p == '\n' || *p == '\r')
            foundNewline = true;
        ++p;
    }

    return foundNewline;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void XYZImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning file %1").arg(fileHandle().toString()));
    setProgressMaximum(stream.underlyingSize());

    // Regular expression for whitespace characters.
    QRegularExpression ws_re(QStringLiteral("\\s+"));

    int frameNumber = 0;
    QString filename = fileHandle().sourceUrl().fileName();
    Frame frame(fileHandle());

    while(!stream.eof() && !isCanceled()) {
        frame.byteOffset = stream.byteOffset();
        frame.lineNumber = stream.lineNumber();

        // Parse number of atoms.
        stream.readLine();

        if(stream.line()[0] == '\0') break;

        unsigned long long numParticlesLong;
        int charCount;
        if(sscanf(stream.line(), "%llu%n", &numParticlesLong, &charCount) != 1)
            throw Exception(tr("Invalid number of particles in line %1 of XYZ file: %2").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));

        // Check trailing whitespace. There should be nothing else but the number of atoms on the first line.
        for(const char* p = stream.line() + charCount; *p != '\0'; ++p) {
            if(!isspace(*p))
                throw Exception(tr("Parsing error in line %1 of XYZ file. According to the XYZ format specification, the first line of a frame section must contain just the number of particles. This is not a valid integer number:\n\n\"%2\"").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));
        }

        // Create a new record for the time step.
        frame.label = QStringLiteral("%1 (Frame %2)").arg(filename).arg(frameNumber++);
        frames.push_back(frame);

        // Skip comment line.
        const char* commentLine = stream.readLine();
        // Detect .exyz file format written by OpenBabel.
        bool isOpenBabelEXYZ = (std::strstr(commentLine, "%PBC") != nullptr);

        // Skip atom lines.
        for(unsigned long long i = 0; i < numParticlesLong; i++) {
            stream.readLine();
            if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
                return;
        }

        // Skip simulation cell section if this is a .exyz file written by OpenBabel.
        if(isOpenBabelEXYZ) {
            for(int i = 0; i < 6; i++)
                stream.readLine();
        }
    }
}

/******************************************************************************
 * Guesses the mapping of input file columns to internal particle properties.
 * Naming conventions followed are those used by QUIP code <http://www.libatoms.org>
 *****************************************************************************/
bool XYZImporter::mapVariableToProperty(ParticleInputColumnMapping& columnMapping, int column, QString name, int dataType, int vec)
{
    if(column <= columnMapping.size()) columnMapping.resize(column+1);
    columnMapping[column].columnName = name;
    QString loweredName = name.toLower();
    if(loweredName == "type" || loweredName == "element" || loweredName == "atom_types" || loweredName == "species")
        columnMapping.mapStandardColumn(column, ParticlesObject::TypeProperty);
    else if(loweredName == "pos") columnMapping.mapStandardColumn(column, ParticlesObject::PositionProperty, vec);
    else if(loweredName == "selection") columnMapping.mapStandardColumn(column, ParticlesObject::SelectionProperty, vec);
    else if(loweredName == "color") columnMapping.mapStandardColumn(column, ParticlesObject::ColorProperty, vec);
    else if(loweredName == "disp") columnMapping.mapStandardColumn(column, ParticlesObject::DisplacementProperty, vec);
    else if(loweredName == "disp_mag") columnMapping.mapStandardColumn(column, ParticlesObject::DisplacementMagnitudeProperty);
    else if(loweredName == "local_energy") columnMapping.mapStandardColumn(column, ParticlesObject::PotentialEnergyProperty);
    else if(loweredName == "kinetic_energy") columnMapping.mapStandardColumn(column, ParticlesObject::KineticEnergyProperty);
    else if(loweredName == "total_energy") columnMapping.mapStandardColumn(column, ParticlesObject::TotalEnergyProperty);
    else if(loweredName == "velo") columnMapping.mapStandardColumn(column, ParticlesObject::VelocityProperty, vec);
    else if(loweredName == "velo_mag") columnMapping.mapStandardColumn(column, ParticlesObject::VelocityMagnitudeProperty);
    else if(loweredName == "radius") columnMapping.mapStandardColumn(column, ParticlesObject::RadiusProperty);
    else if(loweredName == "cluster") columnMapping.mapStandardColumn(column, ParticlesObject::ClusterProperty);
    else if(loweredName == "n_neighb") columnMapping.mapStandardColumn(column, ParticlesObject::CoordinationProperty);
    else if(loweredName == "structure_type") columnMapping.mapStandardColumn(column, ParticlesObject::StructureTypeProperty);
    else if(loweredName == "id") columnMapping.mapStandardColumn(column, ParticlesObject::IdentifierProperty);
    else if(loweredName == "stress") columnMapping.mapStandardColumn(column, ParticlesObject::StressTensorProperty, vec);
    else if(loweredName == "strain") columnMapping.mapStandardColumn(column, ParticlesObject::StrainTensorProperty, vec);
    else if(loweredName == "deform") columnMapping.mapStandardColumn(column, ParticlesObject::DeformationGradientProperty, vec);
    else if(loweredName == "orientation") columnMapping.mapStandardColumn(column, ParticlesObject::OrientationProperty, vec);
    else if(loweredName == "force" || loweredName == "forces") columnMapping.mapStandardColumn(column, ParticlesObject::ForceProperty, vec);
    else if(loweredName == "mass") columnMapping.mapStandardColumn(column, ParticlesObject::MassProperty);
    else if(loweredName == "charge") columnMapping.mapStandardColumn(column, ParticlesObject::ChargeProperty);
    else if(loweredName == "map_shift") columnMapping.mapStandardColumn(column, ParticlesObject::PeriodicImageProperty, vec);
    else if(loweredName == "transparency") columnMapping.mapStandardColumn(column, ParticlesObject::TransparencyProperty);
    else if(loweredName == "dipoles") columnMapping.mapStandardColumn(column, ParticlesObject::DipoleOrientationProperty, vec);
    else if(loweredName == "dipoles_mag") columnMapping.mapStandardColumn(column, ParticlesObject::DipoleMagnitudeProperty);
    else if(loweredName == "omega") columnMapping.mapStandardColumn(column, ParticlesObject::AngularVelocityProperty, vec);
    else if(loweredName == "angular_momentum") columnMapping.mapStandardColumn(column, ParticlesObject::AngularMomentumProperty, vec);
    else if(loweredName == "torque") columnMapping.mapStandardColumn(column, ParticlesObject::TorqueProperty, vec);
    else if(loweredName == "spin") columnMapping.mapStandardColumn(column, ParticlesObject::SpinProperty, vec);
    else if(loweredName == "centro_symmetry") columnMapping.mapStandardColumn(column, ParticlesObject::CentroSymmetryProperty);
    else if(loweredName == "aspherical_shape") columnMapping.mapStandardColumn(column, ParticlesObject::AsphericalShapeProperty, vec);
    else if(loweredName == "vector_color") columnMapping.mapStandardColumn(column, ParticlesObject::VectorColorProperty, vec);
    else if(loweredName == "molecule") columnMapping.mapStandardColumn(column, ParticlesObject::MoleculeProperty);
    else if(loweredName == "molecule_type") columnMapping.mapStandardColumn(column, ParticlesObject::MoleculeTypeProperty);
    else {
        // Only int or float custom properties are supported
        if(dataType == PropertyObject::Float || dataType == PropertyObject::Int || dataType == PropertyObject::Int64)
            columnMapping.mapCustomColumn(column, name, dataType, vec);
        else
            return false;
    }
    return true;
}

/******************************************************************************
 * Helper function that converts a string repr. of a bool ('T' or 'F') to an int
 *****************************************************************************/
inline bool parseBool(const char* s, int& d)
{
    if(s[1] != '\0') return false;
    if(s[0] == 'T' || s[0] == '1') {
        d = 1;
        return true;
    }
    else if(s[0] == 'F' || s[0] == '0') {
        d = 0;
        return true;
    }
    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void XYZImporter::FrameLoader::loadFile()
{
    // Open file for reading.
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Reading XYZ file %1").arg(fileHandle().toString()));

    // Jump to byte offset.
    if(frame().byteOffset != 0)
        stream.seek(frame().byteOffset, frame().lineNumber);

    // Parse number of atoms.
    unsigned long long numParticlesLong;
    int charCount;
    if(sscanf(stream.readLine(), "%llu%n", &numParticlesLong, &charCount) != 1)
        throw Exception(tr("Invalid number of particles in line %1 of XYZ file: %2").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));

    // Check trailing whitespace. There should be nothing else but the number of atoms on the first line.
    for(const char* p = stream.line() + charCount; *p != '\0'; ++p) {
        if(!isspace(*p))
            throw Exception(tr("Parsing error in line %1 of XYZ file. According to the XYZ format specification, the first line should contain the number of particles. This is not a valid integer number of particles:\n\n\"%2\"").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));
    }
    if(numParticlesLong > (unsigned long long)std::numeric_limits<int>::max())
        throw Exception(tr("Too many particles in XYZ file. This program version can read XYZ files with up to %1 particles only.").arg(std::numeric_limits<int>::max()));

    setParticleCount(numParticlesLong);
    setProgressMaximum(numParticlesLong);

    // Extract some useful information from the comment line.
    const char* commentLine_cstr = stream.readLine();
    bool hasSimulationCell = false;
    int movieMode = -1;

    // Detect .exyz file format written by OpenBabel.
    bool isOpenBabelEXYZ = (std::strstr(commentLine_cstr, "%PBC") != nullptr);
    
    simulationCell()->setPbcFlags(false, false, false);
    Vector3 cellOrigin = Vector3::Zero();
    Vector3 cellVector1 = Vector3::Zero();
    Vector3 cellVector2 = Vector3::Zero();
    Vector3 cellVector3 = Vector3::Zero();
    QString remainder;
    int index;

    // Try to parse the simulation cell geometry from the comment line.
    QString commentLine = stream.lineString();
    if((index = commentLine.indexOf("Lxyz=")) >= 0)
        remainder = commentLine.mid(index + 5).trimmed();
    else if((index = commentLine.indexOf("boxsize")) >= 0)
        remainder = commentLine.mid(index + 7).trimmed();
    if(!remainder.isEmpty()) {
        QStringList list = FileImporter::splitString(remainder);
        if(list.size() >= 3) {
            bool ok1, ok2, ok3;
            FloatType sx = (FloatType)list[0].toDouble(&ok1);
            FloatType sy = (FloatType)list[1].toDouble(&ok2);
            FloatType sz = (FloatType)list[2].toDouble(&ok3);
            if(ok1 && ok2 && ok3) {
                simulationCell()->setCellMatrix(AffineTransformation(Vector3(sx, 0, 0), Vector3(0, sy, 0), Vector3(0, 0, sz), Vector3(-sx / 2, -sy / 2, -sz / 2)));
                hasSimulationCell = true;
            }
        }
    }

    if((index = commentLine.indexOf(QStringLiteral("Lattice=\""), 0, Qt::CaseInsensitive)) >= 0) {
        // Extended XYZ format: Lattice="R11 R21 R31 R12 R22 R32 R13 R23 R33"
        // See https://web.archive.org/web/20190811094343/https://libatoms.github.io/QUIP/io.html#extendedxyz for details.
        // Or see https://atomsk.univ-lille.fr/doc/en/format_xyz.html.

        QString latticeStr = commentLine.mid(index + 9);
        latticeStr.truncate(latticeStr.indexOf("\""));
        QStringList list = FileImporter::splitString(latticeStr);
        if(list.size() >= 9) {
            for(int k = 0; k < 3; k++)
                cellVector1[k] = (FloatType)list[k].toDouble();
            for(int k = 3; k < 6; k++)
                cellVector2[k - 3] = (FloatType)list[k].toDouble();
            for(int k = 6; k < 9; k++)
                cellVector3[k - 6] = (FloatType)list[k].toDouble();
        }

        int origin_index1 = commentLine.toLower().indexOf(QStringLiteral("cell_origin=\""));
        int origin_index2 = commentLine.toLower().indexOf(QStringLiteral("origin=\""));
        if(origin_index1 >= 0 || origin_index2 >= 0) {
            QString cellOriginStr = commentLine.mid((origin_index1 >= 0) ? (origin_index1+13) : (origin_index2+8));
            cellOriginStr.truncate(cellOriginStr.indexOf(QStringLiteral("\"")));
            QStringList list = FileImporter::splitString(cellOriginStr);
            for(int k = 0; k < list.size() && k < 3; k++)
                cellOrigin[k] = (FloatType)list[k].toDouble();
        }

        // Parse other key/value pairs from the extended XYZ comment line.
        int key_start = 0;
        for(;;) {
            while(key_start < commentLine.size() && commentLine[key_start].isSpace())
                key_start++;
            if(key_start >= commentLine.size())
                break;
            int key_end = key_start + 1;
            while(key_end < commentLine.size() && commentLine[key_end] != QChar('='))
                key_end++;
            if(key_end >= commentLine.size() - 1)
                break;

            int value_start = key_end + 1;
            bool isQuoted = false;
            if(commentLine[value_start] == QChar('\"')) {
                value_start++;
                isQuoted = true;
            }
            int value_end = value_start;
            while(value_end < commentLine.size() && ((isQuoted && commentLine[value_end] != QChar('\"')) || (!isQuoted && !commentLine[value_end].isSpace())))
                value_end++;
            if(value_end > value_start) {
                QString key = commentLine.mid(key_start, key_end - key_start);
                QString keyLower = key.toLower();
                QString value = commentLine.mid(value_start, value_end - value_start);
                if(keyLower != QStringLiteral("lattice") && keyLower != QStringLiteral("properties") && keyLower != QStringLiteral("cell_origin") && keyLower != QStringLiteral("origin")) {
                    bool ok;
                    int intValue = value.toInt(&ok);
                    if(ok)
                        state().setAttribute(key, QVariant::fromValue(intValue), dataSource());
                    else {
                        double doubleValue = value.toDouble(&ok);
                        if(ok)
                            state().setAttribute(key, QVariant::fromValue(doubleValue), dataSource());
                        else
                            state().setAttribute(key, QVariant::fromValue(value), dataSource());
                    }
                }
            }
            key_start = value_end + 1;
            if(isQuoted) key_start++;
        }
    }
    else {

        // Make comment line string available to Python scripts.
        QString trimmedComment = commentLine.trimmed();
        if(!trimmedComment.isEmpty())
            state().setAttribute(QStringLiteral("Comment"), QVariant::fromValue(trimmedComment), dataSource());

        // XYZ file written by Parcas MD code contain simulation cell info in comment line.

        if((index = commentLine.indexOf("cell_orig ")) >= 0) {
            QStringList list = FileImporter::splitString(commentLine.mid(index + 10));
            for(int k = 0; k < list.size() && k < 3; k++)
                cellOrigin[k] = (FloatType)list[k].toDouble();
        }
        if((index = commentLine.indexOf("cell_vec1 ")) >= 0) {
            QStringList list = FileImporter::splitString(commentLine.mid(index + 10));
            for(int k = 0; k < list.size() && k < 3; k++)
                cellVector1[k] = (FloatType)list[k].toDouble();
        }
        if((index = commentLine.indexOf("cell_vec2 ")) >= 0) {
            QStringList list = FileImporter::splitString(commentLine.mid(index + 10));
            for(int k = 0; k < list.size() && k < 3; k++)
                cellVector2[k] = (FloatType)list[k].toDouble();
        }
        if((index = commentLine.indexOf("cell_vec3 ")) >= 0) {
            QStringList list = FileImporter::splitString(commentLine.mid(index + 10));
            for(int k = 0; k < list.size() && k < 3; k++)
                cellVector3[k] = (FloatType)list[k].toDouble();
        }
    }

    if(cellVector1 != Vector3::Zero() && cellVector2 != Vector3::Zero() && cellVector3 != Vector3::Zero()) {
        simulationCell()->setCellMatrix(AffineTransformation(cellVector1, cellVector2, cellVector3, cellOrigin));
        hasSimulationCell = true;
    }

    if((index = commentLine.indexOf("pbc ")) >= 0) {
        QStringList list = FileImporter::splitString(commentLine.mid(index + 4));
        simulationCell()->setPbcFlags((bool)list[0].toInt(), (bool)list[1].toInt(), (bool)list[2].toInt());
    }
    else if((index = commentLine.indexOf("pbc=\"")) >= 0) {
        // Look for Extended XYZ PBC keyword
        QString pbcStr = commentLine.mid(index + 5);
        pbcStr.truncate(pbcStr.indexOf("\""));
        QStringList list = FileImporter::splitString(pbcStr);
        int pbcFlags[3] = {0, 0, 0};
        for(int i=0; i < list.size() && i < 3; i++) {
            QByteArray ba = list[i].toLatin1();
            parseBool(ba.data(), pbcFlags[i]);
        }
        simulationCell()->setPbcFlags(pbcFlags[0], pbcFlags[1], pbcFlags[2]);
    }
    else if(hasSimulationCell) {
        simulationCell()->setPbcFlags(true, true, true);
    }

    // If this is an extended XYZ file, extract the column mapping from the comment line.
    if(_columnMapping.empty())
        _columnMapping = parseExtendedXYZColumnSpecification(commentLine);

    // In script mode, assume standard set of XYZ columns unless the user has specified otherwise or
    // the file constains column metadata.
    if(_columnMapping.empty()) {
        _columnMapping.resize(4);
        _columnMapping.mapStandardColumn(0, ParticlesObject::TypeProperty);
        _columnMapping.mapStandardColumn(1, ParticlesObject::PositionProperty, 0);
        _columnMapping.mapStandardColumn(2, ParticlesObject::PositionProperty, 1);
        _columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 2);
    }

    // Parse data columns.
    InputColumnReader columnParser(*this, _columnMapping, particles());
    try {
        for(size_t i = 0; i < numParticlesLong; i++) {
            if(!setProgressValueIntermittent(i)) return;
            stream.readLine();
            columnParser.readElement(i, stream.line());
        }
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Parsing error in line %1 of XYZ file.").arg(stream.lineNumber()));
    }

    // Since we created particle types on the go while reading the particles, the assigned particle type IDs
    // depend on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
    // why we sort them now according to their names.
    columnParser.sortElementTypes();
    columnParser.reset();

    // If this is a .exyz file written by OpenBabel, parse the simulation cell section that follows the atom lines.
    if(isOpenBabelEXYZ) {
        AffineTransformation cell = AffineTransformation::Zero();
        // Skip empty line.
        stream.readLine();
        // Parse 1st cell vector.
        if(sscanf(stream.readLine(), "Vector1 " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &cell(0,0), &cell(1,0), &cell(2,0)) < 3)
            throw Exception(tr("Parsing error in line %1 of EXYZ file. Invalid simulation cell vector: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        // Parse 2nd cell vector.
        if(sscanf(stream.readLine(), "Vector2 " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &cell(0,1), &cell(1,1), &cell(2,1)) < 3)
            throw Exception(tr("Parsing error in line %1 of EXYZ file. Invalid simulation cell vector: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        // Parse 3rd cell vector.
        if(sscanf(stream.readLine(), "Vector3 " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &cell(0,2), &cell(1,2), &cell(2,2)) < 3)
            throw Exception(tr("Parsing error in line %1 of EXYZ file. Invalid simulation cell vector: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        // Parse cell origin.
        if(sscanf(stream.readLine(), "Offset " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &cell(0,3), &cell(1,3), &cell(2,3)) < 3)
            throw Exception(tr("Parsing error in line %1 of EXYZ file. Invalid simulation cell origin: %2").arg(stream.lineNumber()).arg(stream.lineString()));
        simulationCell()->setCellMatrix(cell);
        simulationCell()->setPbcFlags(true, true, true);
        hasSimulationCell = true;
    }

    PropertyAccess<Point3> posProperty = particles()->getMutableProperty(ParticlesObject::PositionProperty);
    if(posProperty && numParticlesLong != 0) {
        Box3 boundingBox;
        boundingBox.addPoints(posProperty);

        if(!hasSimulationCell) {
            // If the input file does not contain simulation cell info,
            // use bounding box of particles as simulation cell.
            simulationCell()->setCellMatrix(AffineTransformation(
                    Vector3(boundingBox.sizeX(), 0, 0),
                    Vector3(0, boundingBox.sizeY(), 0),
                    Vector3(0, 0, boundingBox.sizeZ()),
                    boundingBox.minc - Point3::Origin()));
        }
        else if(_autoRescaleCoordinates) {
            // Determine if coordinates are given in reduced format and need to be rescaled to absolute format.
            // Assume reduced format if all coordinates are within the [0,1] or [-0.5,+0.5] range (plus some small epsilon).
            if(Box3(Point3(FloatType(-0.01)), Point3(FloatType(1.01))).containsBox(boundingBox)) {
                // Convert all atom coordinates from reduced to absolute (Cartesian) format.
                const AffineTransformation simCell = simulationCell()->cellMatrix();
                for(Point3& p : posProperty)
                    p = simCell * p;
            }
            else if(Box3(Point3(FloatType(-0.51)), Point3(FloatType(0.51))).containsBox(boundingBox)) {
                // Convert all atom coordinates from reduced to absolute (Cartesian) format.
                const AffineTransformation simCell = simulationCell()->cellMatrix();
                for(Point3& p : posProperty)
                    p = simCell * (p + Vector3(FloatType(0.5)));
            }
        }
    }

    // Detect if there are more simulation frames following in the file.
    if(!stream.eof())
        signalAdditionalFrames();

    // Sort particles by ID.
    if(_sortParticles)
        particles()->sortById();

    if(commentLine.isEmpty())
        state().setStatus(tr("%1 particles").arg(numParticlesLong));
    else
        state().setStatus(tr("%1 particles\n%2").arg(numParticlesLong).arg(commentLine));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Interprets the comment line of an extended XYZ file.
******************************************************************************/
ParticleInputColumnMapping XYZImporter::parseExtendedXYZColumnSpecification(const QString& commentLine)
{
    ParticleInputColumnMapping mapping;

    // Auto-generate column mapping when Extended XYZ Properties key is present.
    // Format is described at http://jrkermode.co.uk/quippy/io.html#extendedxyz
    // Example: Properties=species:S:1:pos:R:3 for atomic species (1 column, string property)
    // and atomic positions (3 columns, real property)
    int index = commentLine.indexOf(QStringLiteral("properties="), 0, Qt::CaseInsensitive);
    if(index >= 0) {

        // Regular expression for whitespace characters.
        QRegularExpression ws_re(QStringLiteral("\\s+"));

        QString propertiesStr = commentLine.mid(index + 11);
        propertiesStr = propertiesStr.left(propertiesStr.indexOf(ws_re));
        QStringList fields = propertiesStr.split(":");

        int col = 0;
        for(int i = 0; i < fields.size() / 3; i++) {
            QString propName = (fields[3 * i + 0]);
            QString propTypeStr = (fields[3 * i + 1]).left(1);
            QByteArray propTypeBA = propTypeStr.toUtf8();
            char propType = propTypeBA.data()[0];
            int nCols = (int)fields[3 * i + 2].toInt();
            switch(propType) {
            case 'I':
                for(int k = 0; k < nCols; k++) {
                    mapVariableToProperty(mapping, col, propName, PropertyObject::Int, k);
                    col++;
                }
                break;
            case 'R':
                for(int k = 0; k < nCols; k++) {
                    mapVariableToProperty(mapping, col, propName, PropertyObject::Float, k);
                    col++;
                }
                break;
            case 'L':
                for(int k = 0; k < nCols; k++) {
                    mapVariableToProperty(mapping, col, propName, PropertyObject::Int, k);
                    col++;
                }
                break;
            case 'S':
                for(int k = 0; k < nCols; k++) {
                    if(!mapVariableToProperty(mapping, col, propName, qMetaTypeId<char>(), k) && k == 0)
                        qDebug() << "Warning: Skipping field" << propName << "of XYZ file because it has an unsupported data type (string).";
                    col++;
                }
                break;
            }
        }
    }

    return mapping;
}

/******************************************************************************
* Inspects the header of the given file and returns the number of file columns.
******************************************************************************/
Future<ParticleInputColumnMapping> XYZImporter::inspectFileHeader(const Frame& frame)
{
    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) {

            // Parse the file header to determine the number of data columns.
            activateCLocale();

            // Open file for reading.
            CompressedTextReader stream(fileHandle);

            // Skip first line (number of atoms).
            stream.readLine();
            QString fileExcerpt = stream.lineString();

            // Parse the comment line.
            stream.readLine();
            QString commentLine = stream.lineString();
            fileExcerpt += commentLine;

            // If this is an extended XYZ file, extract the column mapping from the comment line.
            ParticleInputColumnMapping detectedColumnMapping = parseExtendedXYZColumnSpecification(commentLine);

            // Read first few lines of the atom data and add them to the file excerpt.
            for(size_t i = 0; i < 5 && !stream.eof(); i++) {
                stream.readLine();
                fileExcerpt += stream.lineString();
                if(i == 0)
                    detectedColumnMapping.resize(FileImporter::splitString(stream.lineString()).size());
            }
            if(!stream.eof()) 
                fileExcerpt += QStringLiteral("...\n");

            detectedColumnMapping.setFileExcerpt(fileExcerpt);

            // If there is no preset column mapping, and if the XYZ file has exactly 4 columns, assume
            // it is a standard XYZ file containing the chemical type and the x,y,z positions.
            if(detectedColumnMapping.size() == 4) {
                if(std::none_of(detectedColumnMapping.begin(), detectedColumnMapping.end(), [](const InputColumnInfo& col) { return col.isMapped(); })) {
                    detectedColumnMapping.mapStandardColumn(0, ParticlesObject::TypeProperty);
                    detectedColumnMapping.mapStandardColumn(1, ParticlesObject::PositionProperty, 0);
                    detectedColumnMapping.mapStandardColumn(2, ParticlesObject::PositionProperty, 1);
                    detectedColumnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 2);
                }
            }
            return detectedColumnMapping;
        });
}

/******************************************************************************
 * Saves the class' contents to the given stream.
 *****************************************************************************/
void XYZImporter::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    ParticleImporter::saveToStream(stream, excludeRecomputableData);

    stream.beginChunk(0x02);
    stream.endChunk();
}

/******************************************************************************
 * Loads the class' contents from the given stream.
 *****************************************************************************/
void XYZImporter::loadFromStream(ObjectLoadStream& stream)
{
    ParticleImporter::loadFromStream(stream);

    // For backward compatibility with OVITO 3.1:
    if(stream.expectChunkRange(0x00, 0x02) == 0x01) {
        stream >> _columnMapping.mutableValue();
    }
    stream.closeChunk();
}

}   // End of namespace
