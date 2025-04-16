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
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "QuantumEspressoImporter.h"

#include <boost/algorithm/string.hpp>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(QuantumEspressoImporter);
OVITO_CLASSINFO(QuantumEspressoImporter, "DisplayName", "QE");

/******************************************************************************
* Determines if a character is a normal letter.
******************************************************************************/
static bool isalpha_ascii(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool QuantumEspressoImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Maximum number of lines we are going to read from the input file before giving up.
    int numLinesToRead = 20;

    while(!stream.eof() && numLinesToRead > 0) {
        numLinesToRead--;
        const char* line = stream.readLineTrimLeft(256);
        // Skip parameter blocks.
        if(line[0] == '&' && isalpha_ascii(line[1])) {
            while(!stream.eof()) {
                const char* line = stream.readLineTrimLeft();
                if(line[0] == '/') {
                    numLinesToRead = 20;
                    break;
                }
            }
            continue;
        }
        else if(stream.lineStartsWithToken("ATOMIC_SPECIES")) {
            return true;
        }
        else if(line[0] != '\0') {
            return false;
        }
    }

    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void QuantumEspressoImporter::FrameLoader::loadFile()
{
    // Open file for reading.
    CompressedTextReader stream(fileHandle());

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading Quantum Espresso file %1").arg(fileHandle().toString()));

    // For converting Bohr radii to Angstrom units:
    constexpr FloatType bohr2angstrom = 0.529177;

    // Parsed parameters:
    FloatType alat = 1;
    int natoms = 0;
    int ntypes = 0;
    int ibrav = 0;
    std::vector<QString> type_names;
    std::vector<FloatType> type_masses;
    bool hasCellVectors = false;
    bool convertToAbsoluteCoordinates = false;
    BufferWriteAccess<Point3, access_mode::discard_read_write> posAccess;

    while(!stream.eof() && !this_task::isCanceled()) {
        const char* line = stream.readLineTrimLeft();

        // Skip comment lines, which start with a '!' or a '#'.
        if(line[0] == '!' || line[0] == '#') {
            continue;
        }

        // Read parameter blocks, which start with a '&'.
        if(line[0] == '&' && isalpha_ascii(line[1])) {
            while(!stream.eof() && !this_task::isCanceled()) {
                line = stream.readLineTrimLeft();
                if(line[0] == '/') {
                    break;
                }
#if 1
                static const QRegularExpression ibravRe(R"(ibrav\s*=\s*(\d+))");
                QRegularExpressionMatch match = ibravRe.match(line);
                if(match.hasMatch()) {
                    bool ok;
                    ibrav = match.captured(1).toInt(&ok);
                    OVITO_ASSERT(ok);
                }

                static const QRegularExpression celldmRe(R"(celldm\(1\)\s*=\s*([+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)))");
                match = celldmRe.match(line);
                if(match.hasMatch()) {
                    bool ok;
                    alat = match.captured(1).toDouble(&ok);
                    alat *= bohr2angstrom;
                    OVITO_ASSERT(ok);
                }

                static const QRegularExpression aRe(R"(A\s*=\s*(\d+))");
                match = aRe.match(line);
                if(match.hasMatch()) {
                    bool ok;
                    alat = match.captured(1).toDouble(&ok);
                    OVITO_ASSERT(ok);
                }

                static const QRegularExpression natRe(R"(nat\s*=\s*(\d+))");
                match = natRe.match(line);
                if(match.hasMatch()) {
                    bool ok;
                    natoms = match.captured(1).toInt(&ok);
                    OVITO_ASSERT(ok);
                }

                static const QRegularExpression ntypRe(R"(ntyp\s*=\s*(\d+))");
                match = ntypRe.match(line);
                if(match.hasMatch()) {
                    bool ok;
                    ntypes = match.captured(1).toInt(&ok);
                    OVITO_ASSERT(ok);
                }
#else
                constexpr static std::array<const char*, 5> keywords = {"ibrav", "A", "celldm(1)", "nat", "ntyp"};
                const std::string_view line_sv(line);
                for(size_t i = 0; i < keywords.size(); ++i) {
                    const char* key = keywords[i];
                    size_t keyPos = line_sv.find(key);
                    if(keyPos != std::string::npos) {
                        // Find the '=' character after the keyword.
                        size_t eqPos = line_sv.find('=', keyPos);
                        if(eqPos != std::string::npos) {
                            // Start position for the value: after the equals sign.
                            size_t valueStart = eqPos + 1;
                            // Skip any whitespace.
                            while(valueStart < line_sv.size() && std::isspace(line[valueStart])) {
                                ++valueStart;
                            }

                            // Assume the value ends at a comma or at the end of the line.
                            size_t valueEnd = line_sv.find_first_of(",\n", valueStart);
                            if(valueEnd == std::string::npos) {
                                valueEnd = line_sv.size();
                            }

                            std::string_view line_sv_sub = line_sv.substr(valueStart, valueEnd - valueStart);

                            switch(i) {
                                case 0:
                                    if(std::from_chars(line_sv_sub.begin(), line_sv_sub.end(), ibrav).ec != std::errc{}) {
                                        throw Exception(tr("Invalid 'ibrav' value in line %1 of QE file: %2")
                                                            .arg(stream.lineNumber())
                                                            .arg(stream.lineString()));
                                    }
                                    break;
                                case 1:
                                    // from_chars doesnt support doubles yet -> update later
                                    if(sscanf(line_sv_sub.begin(), FLOATTYPE_SCANF_STRING, &alat) != 1) {
                                        throw Exception(tr("Invalid 'A' value in line %1 of QE file: %2")
                                                            .arg(stream.lineNumber())
                                                            .arg(stream.lineString()));
                                    }
                                    break;
                                case 2:
                                    // from_chars doesnt support doubles yet -> update later
                                    if(sscanf(line_sv_sub.begin(), FLOATTYPE_SCANF_STRING, &alat) != 1) {
                                        throw Exception(tr("Invalid 'celldm(1)' value in line %1 of QE file: %2")
                                                            .arg(stream.lineNumber())
                                                            .arg(stream.lineString()));
                                    }
                                    alat *= bohr2angstrom;
                                    break;
                                case 3:
                                    if(std::from_chars(line_sv_sub.begin(), line_sv_sub.end(), natoms).ec != std::errc{}) {
                                        throw Exception(tr("Invalid 'nat' value in line %1 of QE file: %2")
                                                            .arg(stream.lineNumber())
                                                            .arg(stream.lineString()));
                                    }
                                    break;
                                case 4:
                                    if(std::from_chars(line_sv_sub.begin(), line_sv_sub.end(), ntypes).ec != std::errc{}) {
                                        throw Exception(tr("Invalid 'ntyp' value in line %1 of QE file: %2")
                                                            .arg(stream.lineNumber())
                                                            .arg(stream.lineString()));
                                    }
                                    break;
                                default: OVITO_ASSERT(false); break;
                            }
                        }
                    }
                }
#endif
            }
            continue;
        }

        if(stream.lineStartsWithToken("ATOMIC_SPECIES")) {
            type_names.resize(ntypes);
            type_masses.resize(ntypes);
            for(int i = 0; i < ntypes; i++) {
                const char* line = stream.readLineTrimLeft();

                // Parse atom type name.
                const char* token_end = line;
                while(*token_end > ' ') ++token_end;
                type_names[i] = QLatin1String(line, token_end);

                // Parse atomic mass.
                if(sscanf(token_end, FLOATTYPE_SCANF_STRING, &type_masses[i]) != 1)
                    throw Exception(tr("Invalid atom type definition in line %1 of QE file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            }
        }
        else if(stream.lineStartsWithToken("ATOMIC_POSITIONS")) {
            // Parse the unit specification.
            const char* units_start = stream.line() + 16;
            while(*units_start > 0 && (*units_start <= ' ' || *units_start == '(' || *units_start == '{')) ++units_start;
            const char* units_end = units_start;
            while(*units_end > ' ' && *units_end != ')' && *units_end != '}') ++units_end;
            std::string units(units_start, units_end);
            FloatType scaling = 1;
            if(units == "alat" || units.empty()) {
                scaling = alat;
            }
            else if(units == "angstrom") {
                // No scaling.
            }
            else if(units == "crystal") {
                // Conversion from reduced to absolute coordinates will be done later.
                convertToAbsoluteCoordinates = true;
            }
            else if(units == "bohr") {
                // Convert from Bohr radii to Angstroms:
                scaling = bohr2angstrom;
            }
            else {
                throw Exception(tr("Unit type used in line %1 of QE file is not supported: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            }

            // Create particle properties.
            setParticleCount(natoms);
            posAccess = particles()->createProperty(Particles::PositionProperty);
            Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
            Property* massProperty = particles()->createProperty(DataBuffer::Initialized, Particles::MassProperty);

            // Add the registered atom types.
            for(int i = 0; i < ntypes; i++) {
                const ElementType* type = addNamedType(Particles::OOClass(), typeProperty, type_names[i]);
                static_object_cast<ParticleType>(typeProperty->makeMutable(type))->setMass(type_masses[i]);
            }

            BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);
            BufferWriteAccess<FloatType, access_mode::discard_write> massAccess(massProperty);

            // Parse atom definitions.
            for(int i = 0; i < natoms; i++) {
                const char* line = stream.readLineTrimLeft();

                // Parse atom type name.
                const char* token_end = line;
                while(*token_end > ' ') ++token_end;
                int typeId = addNamedType(Particles::OOClass(), typeProperty, QLatin1String(line, token_end))->numericId();
                typeAccess[i] = typeId;
                if(typeId >= 1 && typeId <= type_masses.size())
                    massAccess[i] = type_masses[typeId-1];

                // Parse atomic coordinates.
                Point3 pos;
                if(sscanf(token_end, FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &pos.x(), &pos.y(), &pos.z()) != 3)
                    throw Exception(tr("Invalid atomic coordinates in line %1 of QE file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                posAccess[i] = pos * scaling;
            }
        }
        else if(stream.lineStartsWithToken("CELL_PARAMETERS")) {
            // Parse the unit specification.
            const char* units_start = stream.line() + 16;
            while(*units_start > 0 && (*units_start <= ' ' || *units_start == '(' || *units_start == '{')) ++units_start;
            const char* units_end = units_start;
            while(*units_end > ' ' && *units_end != ')' && *units_end != '}') ++units_end;
            std::string units(units_start, units_end);
            FloatType scaling = 1;
            if(units == "alat" || units.empty()) {
                scaling = alat;
            }
            else if(units == "angstrom") {
                // No scaling.
            }
            else if(units == "bohr") {
                // Convert from Bohr radii to Angstroms:
                scaling = bohr2angstrom;
            }
            else {
                throw Exception(tr("Unit type used in line %1 of QE file is not supported: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            }
            // Read cell matrix.
            AffineTransformation cell = AffineTransformation::Identity();
            for(size_t i = 0; i < 3; i++) {
                std::string line = stream.readLine();
                // Convert Fortran number format to C format:
                for(char& c : line)
                    if(c == 'd' || c == 'D') c = 'e';
                if(sscanf(line.c_str(),
                        FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
                        &cell(0,i), &cell(1,i), &cell(2,i)) != 3 || cell.column(i) == Vector3::Zero())
                    throw Exception(tr("Invalid cell vector in line %1 of QE file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            }
            simulationCell()->setCellMatrix(cell * scaling);
            hasCellVectors = true;
        }
    }
    this_task::throwIfCanceled();

    // Make sure some atoms have been defined in the file.
    if(natoms <= 0 || ntypes <= 0)
        throw Exception(tr("Invalid Quantum Espresso file. No atoms defined."));

    if(!hasCellVectors) {
        Matrix3 cell;
        switch(ibrav) {
            case 0: throw Exception(tr("Invalid 'ibrav' value in QE file: ibrav==0 requires a CELL_PARAMETERS card."));
            case 1: // SC:
                cell = Matrix3(Vector3(alat, 0, 0), Vector3(0, alat, 0), Vector3(0, 0, alat));
                break;
            case 2: // FCC:
                cell = Matrix3(Vector3(-alat/2, 0, alat/2), Vector3(0, alat/2, alat/2), Vector3(-alat/2, alat/2, 0));
                break;
            case 3: // BCC:
                cell = Matrix3(Vector3(alat/2, alat/2, alat/2), Vector3(-alat/2, alat/2, alat/2), Vector3(-alat/2, -alat/2, alat/2));
                break;
            case -3: // BCC, more symmetric axis:
                cell = Matrix3(Vector3(-alat/2, alat/2, alat/2), Vector3(alat/2, -alat/2, alat/2), Vector3(alat/2, alat/2, -alat/2));
                break;
            default: throw Exception(tr("Unsupported 'ibrav' value in QE file: %1").arg(ibrav));
        }
        simulationCell()->setCellMatrix(AffineTransformation(cell));
    }

    if(convertToAbsoluteCoordinates) {
        // Convert all atom coordinates from reduced to absolute (Cartesian) format.
        const AffineTransformation simCell = simulationCell()->cellMatrix();
        for(Point3& p : posAccess)
            p = simCell * p;
    }

    state().setStatus(tr("Number of particles: %1").arg(natoms));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
