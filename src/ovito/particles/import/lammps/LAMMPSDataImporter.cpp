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
#include <ovito/particles/objects/BondType.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/utilities/io/NumberParsing.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include "LAMMPSDataImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(LAMMPSDataImporter);
DEFINE_PROPERTY_FIELD(LAMMPSDataImporter, atomStyle);
DEFINE_PROPERTY_FIELD(LAMMPSDataImporter, atomSubStyles);
SET_PROPERTY_FIELD_LABEL(LAMMPSDataImporter, atomStyle, "LAMMPS atom style");
SET_PROPERTY_FIELD_LABEL(LAMMPSDataImporter, atomSubStyles, "Hybrid sub-styles");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSDataImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first comment line.
    stream.readLine(1024);

    // Read some lines until we encounter the "atoms" keyword.
    for(int i = 0; i < 20; i++) {
        if(stream.eof())
            return false;
        std::string line(stream.readLine(1024));
        // Trim anything from '#' onward.
        size_t commentStart = line.find('#');
        if(commentStart != std::string::npos) line.resize(commentStart);
        // If line is blank, continue.
        if(line.find_first_not_of(" \t\n\r") == std::string::npos) continue;
        if(line.find("atoms") != std::string::npos) {
            int natoms;
            if(sscanf(line.c_str(), "%u", &natoms) != 1 || natoms < 0)
                return false;
            return true;
        }
    }

    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSDataImporter::FrameLoader::loadFile()
{
    using namespace std;
    setProgressText(tr("Reading LAMMPS data file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Read comment line
    stream.readLine();

    qlonglong natoms = 0;
    qlonglong nbonds = 0;
    qlonglong nangles = 0;
    qlonglong ndihedrals = 0;
    qlonglong nimpropers = 0;
    qlonglong nellipsoids = 0;
    int natomtypes = 0;
    int nbondtypes = 0;
    int nangletypes = 0;
    int ndihedraltypes = 0;
    int nimpropertypes = 0;
    FloatType xlo = 0, xhi = 0;
    FloatType ylo = 0, yhi = 0;
    FloatType zlo = 0, zhi = 0;
    FloatType xy = 0, xz = 0, yz = 0;

    // Read header
    while(true) {

        string line(stream.readLine());

        // Trim anything from '#' onward.
        size_t commentStart = line.find('#');
        if(commentStart != string::npos) line.resize(commentStart);

        // If line is blank, continue.
        if(line.find_first_not_of(" \t\n\r") == string::npos) continue;

        if(line.find("xlo") != string::npos && line.find("xhi") != string::npos) {
            if(sscanf(line.c_str(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &xlo, &xhi) != 2)
                throw Exception(tr("Invalid xlo/xhi values (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("ylo") != string::npos && line.find("yhi") != string::npos) {
            if(sscanf(line.c_str(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &ylo, &yhi) != 2)
                throw Exception(tr("Invalid ylo/yhi values (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("zlo") != string::npos && line.find("zhi") != string::npos) {
            if(sscanf(line.c_str(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &zlo, &zhi) != 2)
                throw Exception(tr("Invalid zlo/zhi values (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("xy") != string::npos && line.find("xz") != string::npos && line.find("yz") != string::npos) {
            if(sscanf(line.c_str(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &xy, &xz, &yz) != 3)
                throw Exception(tr("Invalid xy/xz/yz values (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("atoms") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &natoms) != 1)
                throw Exception(tr("Invalid number of atoms (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
            setProgressMaximum(natoms);
        }
        else if(line.find("atom") != string::npos && line.find("types") != string::npos) {
            if(sscanf(line.c_str(), "%u", &natomtypes) != 1)
                throw Exception(tr("Invalid number of atom types (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("bonds") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &nbonds) != 1)
                throw Exception(tr("Invalid number of bonds (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("bond") != string::npos && line.find("types") != string::npos) {
            if(sscanf(line.c_str(), "%u", &nbondtypes) != 1)
                throw Exception(tr("Invalid number of bond types (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("angles") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &nangles) != 1)
                throw Exception(tr("Invalid number of angles (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("angle") != string::npos && line.find("types") != string::npos) {
            if(sscanf(line.c_str(), "%u", &nangletypes) != 1)
                throw Exception(tr("Invalid number of angle types (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("dihedrals") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &ndihedrals) != 1)
                throw Exception(tr("Invalid number of dihedrals (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("dihedral") != string::npos && line.find("types") != string::npos) {
            if(sscanf(line.c_str(), "%u", &ndihedraltypes) != 1)
                throw Exception(tr("Invalid number of dihedral types (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("impropers") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &nimpropers) != 1)
                throw Exception(tr("Invalid number of impropers (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("improper") != string::npos && line.find("types") != string::npos) {
            if(sscanf(line.c_str(), "%u", &nimpropertypes) != 1)
                throw Exception(tr("Invalid number of improper types (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("extra") != string::npos && line.find("per") != string::npos && line.find("atom") != string::npos) {}
        else if(line.find("triangles") != string::npos) {}
        else if(line.find("ellipsoids") != string::npos) {
            if(sscanf(line.c_str(), "%llu", &nellipsoids) != 1)
                throw Exception(tr("Invalid number of ellipsoids (line %1): %2").arg(stream.lineNumber()).arg(line.c_str()));
        }
        else if(line.find("lines") != string::npos) {}
        else if(line.find("bodies") != string::npos) {}
        else if(line.find("crossterms") != string::npos) {}
        else break;
    }

    if(xhi < xlo || yhi < ylo || zhi < zlo)
        throw Exception(tr("Invalid simulation cell size in header of LAMMPS data file."));

    // Define the simulation cell geometry.
    simulationCell()->setCellMatrix(AffineTransformation(
            Vector3(xhi - xlo, 0, 0),
            Vector3(xy, yhi - ylo, 0),
            Vector3(xz, yz, zhi - zlo),
            Vector3(xlo, ylo, zlo)));

    // Skip to following line after first non-blank line.
    while(!stream.eof() && string(stream.line()).find_first_not_of(" \t\n\r") == string::npos) {
        stream.readLine();
    }

    // This flag is set to true if the atomic coordinates have been parsed.
    bool foundAtomsSection = false;
    if(natoms == 0)
        foundAtomsSection = true;

    // Set container sizes.
    setParticleCount(natoms);
    setBondCount(nbonds);
    setAngleCount(nangles);
    setDihedralCount(ndihedrals);
    setImproperCount(nimpropers);

    // Create standard particle properties.
    particles()->createProperty(DataBuffer::Initialized, ParticlesObject::PositionProperty);
    PropertyObject* typeProperty = particles()->createProperty(DataBuffer::Initialized, ParticlesObject::TypeProperty);

    // Atom type mass table.
    std::vector<FloatType> massTable(natomtypes, 0.0);
    bool hasTypeMasses = false;

    /// Lookup table that maps unique atom IDs to indices.
    std::unordered_map<qlonglong, size_t> atomIdMap;

    // Helper function for parsing a type labels. The label may be enclosed in quotes.
    // Labels may be put in either double, single, or triple (""") quotes. Triple quotes allow for the most generic type label strings,
    // but they require to have a leading and trailing blank space. The extra blanks will be ignored.
    auto parseTypeLabel = [](const char* line) {
        // Skip leading whitespace.
        while(*line && *line <= 32)
            line++;
        const char* nameBegin = line;

        // Read non-whitespace characters.
        while(*line > 32)
            line++;
        const char* nameEnd = line;

        return QLatin1StringView{nameBegin, nameEnd};
    };

    // Helper function for parsing a LAMMPS type label map.
    // The type label map consists of one line per type, containing the numeric id of the types
    // followed by the human-readable name of the type. The name can be contained in quotes.
    auto parseTypeLabelMap = [&](int ntypes, const PropertyContainerClass& containerClass, PropertyObject* typedProperty) {
        for(int i = 1; i <= ntypes; i++) {
            // Parse the type's numeric id.
            int typeId, charCount;
            const char* line = stream.readLine();
            if(sscanf(line, "%i%n", &typeId, &charCount) != 1 || typeId < 1 || typeId > ntypes)
                throw Exception(tr("Invalid type label entry in line %1: %2").arg(stream.lineNumber()).arg(stream.lineString()));
            line += charCount;

            // Register the type.
            addNumericType(containerClass, typedProperty, typeId, parseTypeLabel(line));
        }
    };

    // Read identifier strings one by one in free-form part of data file.
    QByteArray keyword = QByteArray(stream.line()).trimmed();
    for(;;) {
        // Skip blank line after keyword.
        if(stream.eof()) break;
        stream.readLine();

        if(keyword.startsWith("Atoms")) {

            // Create atom types.
            for(int i = 1; i <= natomtypes; i++)
                addNumericType(ParticlesObject::OOClass(), typeProperty, i, {});

            if(natoms != 0) {
                stream.readLine();
                detectAtomStyle(stream.line(), keyword, _atomStyleHints);
                if(_atomStyleHints.atomStyle == AtomStyle_Unknown)
                    throw Exception(tr("LAMMPS atom style of the data file could not be detected, or the number of file columns is not as expected for the selected LAMMPS atom style."));
                if(_atomStyleHints.atomStyle == AtomStyle_Hybrid && _atomStyleHints.atomSubStyles.empty())
                    throw Exception(tr("The sub-styles of LAMMPS atom style 'hybrid' could not be automatically detected. Please specify the list of sub-styles during data file import."));

                // Set up mapping of file columns to OVITO particle properties.
                // The number and order of file columns in a LAMMPS data file depends
                // on the atom style detected above.
                ParticleInputColumnMapping columnMapping = createAtomsColumnMapping(_atomStyleHints.atomStyle, _atomStyleHints.atomSubStyles, _atomStyleHints.atomDataColumnCount);
                if(_atomStyleHints.atomDataColumnCount != 0 && columnMapping.size() != _atomStyleHints.atomDataColumnCount)
                    throw Exception(tr("The LAMMPS atom style specified during data file import seems wrong. "
                        "The actual number of file columns (=%1) is not as expected for LAMMPS atom style '%2' (=%3).")
                        .arg(_atomStyleHints.atomDataColumnCount)
                        .arg(atomStyleName(_atomStyleHints.atomStyle))
                        .arg(columnMapping.size()));

                // Parse data in the Atoms section line by line:
                InputColumnReader columnParser(*this, columnMapping, particles());
                try {
                    for(size_t i = 0; i < (size_t)natoms; i++) {
                        if(!setProgressValueIntermittent(i)) return;
                        if(i != 0) stream.readLine();
                        columnParser.readElement(i, stream.line());
                    }
                }
                catch(Exception& ex) {
                    throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS data file.").arg(stream.lineNumber()));
                }
                columnParser.reset();

                // Range-check atom types.
                for(auto t : ConstDataBufferAccess<int32_t>(particles()->expectProperty(ParticlesObject::TypeProperty))) {
                    if(t < 1 || t > natomtypes)
                        throw Exception(tr("Atom type %1 is out of range in Atoms section of LAMMPS data file. Number of types is %2.").arg(t).arg(natomtypes));
                }

                // Build lookup map of atom identifiers.
                atomIdMap.reserve(natoms);
                size_t index = 0;
                for(auto id : ConstDataBufferAccess<IdentifierIntType>(particles()->expectProperty(ParticlesObject::IdentifierProperty))) {
                    atomIdMap.emplace(id, index++);
                }

                // Some LAMMPS data files contain per-particle diameter information.
                // OVITO only knows the "Radius" particle property, which is means we have to divide by 2.
                if(DataBufferAccess<GraphicsFloatType> radiusProperty = particles()->getMutableProperty(ParticlesObject::RadiusProperty)) {
                    for(auto& r : radiusProperty)
                        r *= GraphicsFloatType(0.5);
                }
            }
            foundAtomsSection = true;
        }
        else if(keyword.startsWith("Velocities")) {
            // Set up mapping of file columns to OVITO particle properties.
            // The number and order of file columns in a LAMMPS data file depends
            // on the atom style detected above.
            ParticleInputColumnMapping columnMapping = createVelocitiesColumnMapping(_atomStyleHints.atomStyle, _atomStyleHints.atomSubStyles);

            // Do not parse the atom ID in the first column.
            OVITO_ASSERT(!columnMapping.empty() && columnMapping[0].property.type() == ParticlesObject::IdentifierProperty);
            columnMapping[0].unmap();

            // Access the atomic IDs.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Velocities section in data file (error in line %1).").arg(stream.lineNumber()));

            // Parse data in the Velocities section line by line:
            InputColumnReader columnParser(*this, columnMapping, particles(), false, false);
            try {
                for(size_t i = 0; i < (size_t)natoms; i++) {
                    if(!setProgressValueIntermittent(i)) return;
                    stream.readLine();

                    // Parse the atom ID at the beginning of the line to perform remapping to
                    // the correct particle index.
                    qlonglong atomId;
                    if(sscanf(stream.line(), "%llu", &atomId) != 1)
                        throw Exception(tr("Invalid atom ID in line %1").arg(stream.lineNumber()));

                    size_t atomIndex = i;
                    if(atomId != identifierProperty[i]) {
                        auto iter = atomIdMap.find(atomId);
                        if(iter == atomIdMap.end())
                            throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                        atomIndex = iter->second;
                    }

                    columnParser.readElement(atomIndex, stream.line());
                }
            }
            catch(Exception& ex) {
                throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS data file.").arg(stream.lineNumber()));
            }
        }
        else if(keyword.startsWith("Atom Type Labels")) {
            parseTypeLabelMap(natomtypes, ParticlesObject::OOClass(), typeProperty);
        }
        else if(keyword.startsWith("Bond Type Labels")) {
            parseTypeLabelMap(nbondtypes, BondsObject::OOClass(), bonds()->createProperty(BondsObject::TypeProperty));
        }
        else if(keyword.startsWith("Angle Type Labels")) {
            parseTypeLabelMap(nangletypes, AnglesObject::OOClass(), angles()->createProperty(AnglesObject::TypeProperty));
        }
        else if(keyword.startsWith("Dihedral Type Labels")) {
            parseTypeLabelMap(ndihedraltypes, DihedralsObject::OOClass(), dihedrals()->createProperty(DihedralsObject::TypeProperty));
        }
        else if(keyword.startsWith("Improper Type Labels")) {
            parseTypeLabelMap(nimpropertypes, ImpropersObject::OOClass(), impropers()->createProperty(ImpropersObject::TypeProperty));
        }
        else if(keyword.startsWith("Masses")) {
            hasTypeMasses = true;
            // Parse atom type masses and also optional atom type names, which some data files list as comments in the Masses section.
            for(int i = 1; i <= natomtypes; i++) {
                const char* start = stream.readLine();

                // Parse mass information.
                int atomType = 0;
                FloatType mass;
                const ParticleType* type = nullptr;
                // First, try numeric type parsing.
                if(sscanf(start, "%i " FLOATTYPE_SCANF_STRING, &atomType, &mass) != 2 || atomType < 1 || atomType > natomtypes) {
                    atomType = 0;
                    // Next, try named type parsing.
                    QLatin1StringView typeName = parseTypeLabel(start);
                    type = dynamic_object_cast<ParticleType>(typeProperty->elementType(typeName));
                    if(type) {
                        // Parse mass from second token.
                        if(sscanf(typeName.cend(), FLOATTYPE_SCANF_STRING, &mass) == 1) {
                            atomType = type->numericId();
                        }
                    }
                    if(atomType == 0)
                        throw Exception(tr("Invalid mass specification (line %1): %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }
                massTable[atomType - 1] = mass;

                if(type == nullptr) {
                    // Parse atom type name, which may be appended to the line as a comment.
                    QString atomTypeName;
                    while(*start && *start != '#') start++;
                    if(*start) {
                        QStringList words = FileImporter::splitString(QString::fromLocal8Bit(start));
                        if(words.size() == 2)
                            atomTypeName = words[1];
                    }

                    // If atom type is still without a name, try to infer it from the mass. Do a reverse lookup in the built-in table of elements.
                    if(atomTypeName.isEmpty() && mass != 0) {
                        atomTypeName = ParticleType::guessTypeNameFromMass(mass);
                    }

                    type = static_object_cast<ParticleType>(addNumericType(ParticlesObject::OOClass(), typeProperty, atomType, atomTypeName));
                }
                if(mass != 0 && mass != type->mass()) {
                    ParticleType* mutableType = typeProperty->makeMutable(type);
                    mutableType->setMass(mass);
                    // Log in type name assigned by the file reader as default value for the element type.
                    // This is needed for the Python code generator to detect manual changes subsequently made by the user.
                    mutableType->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::mass)});
                }
            }
        }
        else if(keyword.startsWith("Pair Coeffs")) {
            for(int i = 0; i < natomtypes; i++) stream.readLine();
        }
        else if(keyword.startsWith("PairIJ Coeffs")) {
            for(int i = 0; i < natomtypes*(natomtypes+1)/2; i++) stream.readLine();
        }
        else if(keyword.startsWith("Bond Coeffs")) {
            for(int i = 0; i < nbondtypes; i++) stream.readLine();
        }
        else if(keyword.startsWith("Angle Coeffs") || keyword.startsWith("BondAngle Coeffs") || keyword.startsWith("BondBond Coeffs")) {
            for(int i = 0; i < nangletypes; i++) stream.readLine();
        }
        else if(keyword.startsWith("Dihedral Coeffs") || keyword.startsWith("EndBondTorsion Coeffs") || keyword.startsWith("BondBond13 Coeffs") || keyword.startsWith("MiddleBondTorsion Coeffs") || keyword.startsWith("AngleAngleTorsion Coeffs") || keyword.startsWith("AngleTorsion Coeffs")) {
            for(int i = 0; i < ndihedraltypes; i++) stream.readLine();
        }
        else if(keyword.startsWith("Improper Coeffs") || keyword.startsWith("AngleAngle Coeffs")) {
            for(int i = 0; i < nimpropertypes; i++) stream.readLine();
        }
        else if(keyword.startsWith("Bonds")) {

            // Get the atomic IDs, which have already been read.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Bonds section in data file (error in line %1).").arg(stream.lineNumber()));

            // Create bonds storage.
            DataBufferAccess<ParticleIndexPair> bondTopologyProperty = bonds()->createProperty(BondsObject::TopologyProperty);

            // Create bond type property.
            PropertyObject* typeProperty = bonds()->createProperty(BondsObject::TypeProperty);

            // Create bond types.
            for(int i = 1; i <= nbondtypes; i++)
                addNumericType(BondsObject::OOClass(), typeProperty, i, {});

            setProgressMaximum(nbonds);
            DataBufferAccess<int32_t> typePropertyAccess(typeProperty);
            auto* bondType = typePropertyAccess.begin();
            ParticleIndexPair* bond = bondTopologyProperty.begin();
            for(size_t i = 0; i < (size_t)nbonds; i++, ++bond, ++bondType) {
                if(!setProgressValueIntermittent(i)) return;
                const char* line = stream.readLine();

                qlonglong bondId;
                int charCount;
                if(sscanf(line, "%llu%n", &bondId, &charCount) != 1)
                    throw Exception(tr("Invalid bond id in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                line += charCount;

                // Parse bond type. Can be numeric or name string.
                QLatin1StringView token = parseTypeLabel(line);
                line = token.cend();

                // Try parsing numeric type id.
                bool ok = parseInt(token, *bondType);
                if(!ok) {
                    // Try lookup by type name.
                    if(const ElementType* etype = typeProperty->elementType(token))
                        *bondType = etype->numericId();
                    else
                        throw Exception(tr("Unknown bond type referenced in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(token));
                }

                // Parse atom ids.
                qlonglong atomId1, atomId2;
                if(sscanf(line, "%llu %llu", &atomId1, &atomId2) != 2)
                    throw Exception(tr("Invalid atom ids in bond specification line %1: %2").arg(stream.lineNumber()).arg(stream.lineString()));

                if(atomId1 < 0 || atomId1 >= (qlonglong)identifierProperty.size() || atomId1 != identifierProperty[atomId1]) {
                    auto iter = atomIdMap.find(atomId1);
                    if(iter == atomIdMap.end())
                        throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                    (*bond)[0] = iter->second;
                }
                else (*bond)[0] = atomId1;

                if(atomId2 < 0 || atomId2 >= (qlonglong)identifierProperty.size() || atomId2 != identifierProperty[atomId2]) {
                    auto iter = atomIdMap.find(atomId2);
                    if(iter == atomIdMap.end())
                        throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                    (*bond)[1] = iter->second;
                }
                else (*bond)[1] = atomId2;

                if(*bondType < 1 || *bondType > nbondtypes)
                    throw Exception(tr("Bond type out of range in Bonds section of LAMMPS data file at line %1.").arg(stream.lineNumber()));
            }
            typePropertyAccess.reset();
            bondTopologyProperty.reset();
            identifierProperty.reset();

            generateBondPeriodicImageProperty();
        }
        else if(keyword.startsWith("Angles")) {

            // Get the atomic IDs, which have already been read.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Angles section in data file (error in line %1).").arg(stream.lineNumber()));

            // Create angles topology storage.
            DataBufferAccess<ParticleIndexTriplet> angleTopologyProperty = angles()->createProperty(AnglesObject::TopologyProperty);

            // Create angle type property.
            PropertyObject* typeProperty = angles()->createProperty(AnglesObject::TypeProperty);

            // Create angle types.
            for(int i = 1; i <= nangletypes; i++)
                addNumericType(AnglesObject::OOClass(), typeProperty, i, {});

            setProgressMaximum(nangles);
            DataBufferAccess<int32_t> typePropertyAccess(typeProperty);
            auto* angleType = typePropertyAccess.begin();
            ParticleIndexTriplet* angle = angleTopologyProperty.begin();
            for(size_t i = 0; i < (size_t)nangles; i++, ++angle, ++angleType) {
                if(!setProgressValueIntermittent(i)) return;
                const char* line = stream.readLine();

                qlonglong angleId;
                int charCount;
                if(sscanf(line, "%llu%n", &angleId, &charCount) != 1)
                    throw Exception(tr("Invalid angle id in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                line += charCount;

                // Parse angle type. Can be numeric or name string.
                QLatin1StringView token = parseTypeLabel(line);
                line = token.cend();

                // Try parsing numeric type id.
                bool ok = parseInt(token, *angleType);
                if(!ok) {
                    // Try lookup by type name.
                    if(const ElementType* etype = typeProperty->elementType(token))
                        *angleType = etype->numericId();
                    else
                        throw Exception(tr("Unknown angle type referenced in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(token));
                }

                // Parse atom ids.
                if(sscanf(line, "%llu %llu %llu", &(*angle)[0], &(*angle)[1], &(*angle)[2]) != 3)
                    throw Exception(tr("Invalid angle specification (line %1): %2").arg(stream.lineNumber()).arg(stream.lineString()));

                for(auto& idx : *angle) {
                    if(idx < 0 || idx >= (qlonglong)identifierProperty.size() || idx != identifierProperty[idx]) {
                        auto iter = atomIdMap.find(idx);
                        if(iter == atomIdMap.end())
                            throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                        idx = iter->second;
                    }
                }

                if(*angleType < 1 || *angleType > nangletypes)
                    throw Exception(tr("Angle type out of range in Angles section of LAMMPS data file at line %1.").arg(stream.lineNumber()));
            }
        }
        else if(keyword.startsWith("Dihedrals")) {

            // Get the atomic IDs, which have already been read.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Dihedrals section in data file (error in line %1).").arg(stream.lineNumber()));

            // Create dihedrals topology storage.
            DataBufferAccess<ParticleIndexQuadruplet> dihedralTopologyProperty = dihedrals()->createProperty(DihedralsObject::TopologyProperty);

            // Create dihedral type property.
            PropertyObject* typeProperty = dihedrals()->createProperty(DihedralsObject::TypeProperty);

            // Create dihedral types.
            for(int i = 1; i <= ndihedraltypes; i++)
                addNumericType(DihedralsObject::OOClass(), typeProperty, i, {});

            setProgressMaximum(ndihedrals);
            DataBufferAccess<int32_t> typePropertyAccess(typeProperty);
            auto* dihedralType = typePropertyAccess.begin();
            ParticleIndexQuadruplet* dihedral = dihedralTopologyProperty.begin();
            for(size_t i = 0; i < (size_t)ndihedrals; i++, ++dihedral, ++dihedralType) {
                if(!setProgressValueIntermittent(i)) return;
                const char* line = stream.readLine();

                qlonglong dihedralId;
                int charCount;
                if(sscanf(line, "%llu%n", &dihedralId, &charCount) != 1)
                    throw Exception(tr("Invalid dihedral id in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                line += charCount;

                // Parse dihedral type. Can be numeric or name string.
                QLatin1StringView token = parseTypeLabel(line);
                line = token.cend();

                // Try parsing numeric type id.
                bool ok = parseInt(token, *dihedralType);
                if(!ok) {
                    // Try lookup by type name.
                    if(const ElementType* etype = typeProperty->elementType(token))
                        *dihedralType = etype->numericId();
                    else
                        throw Exception(tr("Unknown dihedral type referenced in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(token));
                }

                // Parse atom ids.
                if(sscanf(line, "%llu %llu %llu %llu", &(*dihedral)[0], &(*dihedral)[1], &(*dihedral)[2], &(*dihedral)[3]) != 4)
                    throw Exception(tr("Invalid dihedral specification (line %1): %2").arg(stream.lineNumber()).arg(stream.lineString()));

                for(auto& idx : *dihedral) {
                    if(idx < 0 || idx >= (qlonglong)identifierProperty.size() || idx != identifierProperty[idx]) {
                        auto iter = atomIdMap.find(idx);
                        if(iter == atomIdMap.end())
                            throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                        idx = iter->second;
                    }
                }

                if(*dihedralType < 1 || *dihedralType > ndihedraltypes)
                    throw Exception(tr("Dihedral type out of range in Dihedrals section of LAMMPS data file at line %1.").arg(stream.lineNumber()));
            }
        }
        else if(keyword.startsWith("Impropers")) {

            // Get the atomic IDs, which have already been read.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Impropers section in data file (error in line %1).").arg(stream.lineNumber()));

            // Create improper topology storage.
            DataBufferAccess<ParticleIndexQuadruplet> improperTopologyProperty = impropers()->createProperty(ImpropersObject::TopologyProperty);

            // Create improper type property.
            PropertyObject* typeProperty = impropers()->createProperty(ImpropersObject::TypeProperty);

            // Create improper types.
            for(int i = 1; i <= nimpropertypes; i++)
                addNumericType(ImpropersObject::OOClass(), typeProperty, i, {});

            setProgressMaximum(nimpropers);
            DataBufferAccess<int32_t> typePropertyAccess(typeProperty);
            auto* improperType = typePropertyAccess.begin();
            ParticleIndexQuadruplet* improper = improperTopologyProperty.begin();
            for(size_t i = 0; i < (size_t)nimpropers; i++, ++improper, ++improperType) {
                if(!setProgressValueIntermittent(i)) return;
                const char* line = stream.readLine();

                qlonglong improperId;
                int charCount;
                if(sscanf(line, "%llu%n", &improperId, &charCount) != 1)
                    throw Exception(tr("Invalid improper id in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                line += charCount;

                // Parse improper type. Can be numeric or name string.
                QLatin1StringView token = parseTypeLabel(line);
                line = token.cend();

                // Try parsing numeric type id.
                bool ok = parseInt(token, *improperType);
                if(!ok) {
                    // Try lookup by type name.
                    if(const ElementType* etype = typeProperty->elementType(token))
                        *improperType = etype->numericId();
                    else
                        throw Exception(tr("Unknown improper type referenced in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(token));
                }

                // Parse atom ids.
                if(sscanf(line, "%llu %llu %llu %llu", &(*improper)[0], &(*improper)[1], &(*improper)[2], &(*improper)[3]) != 4)
                    throw Exception(tr("Invalid improper specification (line %1): %2").arg(stream.lineNumber()).arg(stream.lineString()));

                for(auto& idx : *improper) {
                    if(idx < 0 || idx >= (qlonglong)identifierProperty.size() || idx != identifierProperty[idx]) {
                        auto iter = atomIdMap.find(idx);
                        if(iter == atomIdMap.end())
                            throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                        idx = iter->second;
                    }
                }

                if(*improperType < 1 || *improperType > nimpropertypes)
                    throw Exception(tr("Improper type out of range in Impropers section of LAMMPS data file at line %1.").arg(stream.lineNumber()));
            }
        }
        else if(keyword.startsWith("Ellipsoids")) {

            // Get the atomic IDs, which have already been read.
            ConstDataBufferAccess<IdentifierIntType> identifierProperty = particles()->getProperty(ParticlesObject::IdentifierProperty);
            if(!identifierProperty)
                throw Exception(tr("Atoms section must precede Ellipsoids section in data file (error in line %1).").arg(stream.lineNumber()));

            // Create properties for ellipsoidal particles.
            DataBufferAccess<Vector3G> asphericalShapeProperty = particles()->createProperty(DataBuffer::Initialized, ParticlesObject::AsphericalShapeProperty);
            DataBufferAccess<QuaternionG> orientationProperty = particles()->createProperty(DataBuffer::Initialized, ParticlesObject::OrientationProperty);

            setProgressMaximum(nellipsoids);
            for(size_t i = 0; i < (size_t)nellipsoids; i++) {
                if(!setProgressValueIntermittent(i)) return;
                const char* line = stream.readLine();

                qlonglong atomId;
                int charCount;
                if(sscanf(line, "%llu%n", &atomId, &charCount) != 1)
                    throw Exception(tr("Invalid atom id in line %1 of LAMMPS data file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                line += charCount;

                // Map atom ID to particle index.
                if(atomId < 0 || atomId >= (qlonglong)identifierProperty.size() || atomId != identifierProperty[atomId]) {
                    auto iter = atomIdMap.find(atomId);
                    if(iter == atomIdMap.end())
                        throw Exception(tr("Nonexistent atom ID encountered in line %1 of data file.").arg(stream.lineNumber()));
                    atomId = iter->second;
                }

                // Parse shapex,shapey,shapez and quatw,quati,quatj,quatk fields.
                Vector3G& shape = asphericalShapeProperty[atomId];
                QuaternionG& q = orientationProperty[atomId];
                if(sscanf(line, GRAPHICS_FLOATTYPE_SCANF_STRING " " GRAPHICS_FLOATTYPE_SCANF_STRING " " GRAPHICS_FLOATTYPE_SCANF_STRING
                        " " GRAPHICS_FLOATTYPE_SCANF_STRING " " GRAPHICS_FLOATTYPE_SCANF_STRING " " GRAPHICS_FLOATTYPE_SCANF_STRING " " GRAPHICS_FLOATTYPE_SCANF_STRING,
                        &shape.x(), &shape.y(), &shape.z(),
                        &q.w(), &q.x(), &q.y(), &q.z()) != 7)
                    throw Exception(tr("Invalid ellipsoid shape/orientation (line %1): %2").arg(stream.lineNumber()).arg(stream.lineString()));

                // Convert diameter to radius and normalize quaternion.
                shape *= GraphicsFloatType(0.5);
                q.normalizeSafely();
            }
        }
        else if(keyword.isEmpty() == false) {
            // Try to skip unknown sections.
            while(!stream.eof()) {
                const char* line = stream.readLineTrimLeft();
                if(line[0] == '\0')
                    break;
            }
        }
        else break;

        // Read up to non-blank line plus one subsequent line.
        while(!stream.eof() && string(stream.readLine()).find_first_not_of(" \t\n\r") == string::npos);

        // Read identifier strings one by one in free-form part of data file.
        keyword = QByteArray(stream.line()).trimmed();
    }

    if(!foundAtomsSection)
        throw Exception("LAMMPS data file does not contain atomic coordinates.");

    // Assign masses to particles based on their type.
    if(hasTypeMasses && !particles()->getProperty(ParticlesObject::MassProperty)) {
        DataBufferAccess<FloatType> massProperty = particles()->createProperty(ParticlesObject::MassProperty);
        boost::transform(ConstDataBufferAccess<int32_t>(typeProperty), massProperty.begin(), [&](auto atomType) {
            return massTable[atomType - 1];
        });
    }

    // Sort particles by ID if requested.
    if(_sortParticles)
        particles()->sortById();

    QString statusString = tr("Number of particles: %1").arg(natoms);
    if(nbondtypes > 0 || nbonds > 0)
        statusString += tr("\nNumber of bonds: %1").arg(nbonds);
    if(nangletypes > 0 || nangles > 0)
        statusString += tr("\nNumber of angles: %1").arg(nangles);
    if(ndihedraltypes > 0 || ndihedrals > 0)
        statusString += tr("\nNumber of dihedrals: %1").arg(ndihedrals);
    if(nimpropertypes > 0 || nimpropers > 0)
        statusString += tr("\nNumber of impropers: %1").arg(nimpropers);
    state().setStatus(statusString);

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Detects or verifies the LAMMPS atom style used by the data file.
******************************************************************************/
void LAMMPSDataImporter::detectAtomStyle(const char* firstLine, const QByteArray& keywordLine, LAMMPSAtomStyleHints& info)
{
    // Data files may contain a comment string after the 'Atoms' keyword indicating the LAMMPS atom style.
    QString atomStyleHint;
    QStringList atomSubStyleHints;
    int commentStart = keywordLine.indexOf('#');
    if(commentStart != -1) {
        QStringList tokens = FileImporter::splitString(QString::fromLatin1(keywordLine.data() + commentStart));
        if(tokens.size() >= 2) {
            atomStyleHint = tokens[1];
            atomSubStyleHints = tokens.mid(2);
        }
    }

    // Count number of columns in first data line of the Atoms section.
    QString str = QString::fromLatin1(firstLine);
    commentStart = str.indexOf(QChar('#'));
    if(commentStart > 0 && str[commentStart-1].isSpace())
        str.truncate(commentStart);
    QStringList tokens = FileImporter::splitString(str);
    info.atomDataColumnCount = tokens.size();

    if((info.atomStyle == AtomStyle_Unknown || info.atomStyle == AtomStyle_Hybrid) && !atomStyleHint.isEmpty()) {
        info.atomStyle = parseAtomStyleHint(atomStyleHint);
        if(info.atomStyle == AtomStyle_Hybrid) {
            if(!atomSubStyleHints.empty()) {
                info.atomSubStyles.clear();
                for(const QString& subStyleHint : atomSubStyleHints) {
                    info.atomSubStyles.push_back(parseAtomStyleHint(subStyleHint));
                    if(info.atomSubStyles.back() == AtomStyle_Unknown || info.atomSubStyles.back() == AtomStyle_Hybrid) {
                        info.atomSubStyles.clear();
                        qWarning() << "This atom sub-style in LAMMPS data file is not supported by OVITO:" << subStyleHint;
                        break;
                    }
                }
            }
        }
    }

    // If no style hint is given in the data file, and if the number of
    // columns is 5 (or 5+3 including image flags), assume atom style is "atomic".
    if(info.atomStyle == AtomStyle_Unknown) {
        if(info.atomDataColumnCount == 5) {
            info.atomStyle = AtomStyle_Atomic;
            return;
        }
        else if(info.atomDataColumnCount == 5 + 3) {
            if(!tokens[5].contains(QChar('.')) && !tokens[6].contains(QChar('.')) && !tokens[7].contains(QChar('.'))) {
                info.atomStyle = AtomStyle_Atomic;
                return;
            }
        }
    }
    else if(info.atomStyle == AtomStyle_Hybrid) {
        if(info.atomDataColumnCount >= 5)
            return;
    }
    else {
        // Check if the number of columns present in the data file matches the expected count for the selected atom style.
        ParticleInputColumnMapping columnMapping = createAtomsColumnMapping(info.atomStyle, {}, info.atomDataColumnCount);
        if(columnMapping.size() == info.atomDataColumnCount)
            return;
    }
    // Invalid or unexpected column count:
    info.atomStyle = AtomStyle_Unknown;
}

/******************************************************************************
* Parses a hint string for the LAMMPS atom style.
******************************************************************************/
LAMMPSDataImporter::LAMMPSAtomStyle LAMMPSDataImporter::parseAtomStyleHint(const QString& atomStyleHint)
{
    for(int i = 1; i < AtomStyle_COUNT; i++) {
        LAMMPSAtomStyle atomStyle = static_cast<LAMMPSAtomStyle>(i);
        if(atomStyleHint == atomStyleName(atomStyle))
            return atomStyle;
    }
    return AtomStyle_Unknown;
}

/******************************************************************************
* Returns the name string of the given LAMMPS atom style.
******************************************************************************/
QString LAMMPSDataImporter::atomStyleName(LAMMPSAtomStyle atomStyle)
{
    static const QString styleNames[] = {
        QStringLiteral("unknown"),
        QStringLiteral("angle"),
        QStringLiteral("atomic"),
        QStringLiteral("body"),
        QStringLiteral("bond"),
        QStringLiteral("charge"),
        QStringLiteral("dipole"),
        QStringLiteral("dpd"),
        QStringLiteral("edpd"),
        QStringLiteral("mdpd"),
        QStringLiteral("electron"),
        QStringLiteral("ellipsoid"),
        QStringLiteral("full"),
        QStringLiteral("line"),
        QStringLiteral("meso"),
        QStringLiteral("molecular"),
        QStringLiteral("peri"),
        QStringLiteral("smd"),
        QStringLiteral("sphere"),
        QStringLiteral("template"),
        QStringLiteral("tri"),
        QStringLiteral("wavepacket"),
        QStringLiteral("hybrid")
    };
    OVITO_STATIC_ASSERT(AtomStyle_COUNT == sizeof(styleNames) / sizeof(styleNames[0]));
    OVITO_ASSERT((int)atomStyle < AtomStyle_COUNT);
    return styleNames[atomStyle];
}

/******************************************************************************
* Sets up the mapping of data file columns in the 'Atoms' section to internal particle properties based on the selected LAMMPS atom style.
******************************************************************************/
ParticleInputColumnMapping LAMMPSDataImporter::createAtomsColumnMapping(LAMMPSAtomStyle atomStyle, const std::vector<LAMMPSAtomStyle>& atomSubStyles, int dataColumnCount)
{
    ParticleInputColumnMapping columnMapping;
    switch(atomStyle) {
    case AtomStyle_Angle:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Atomic:
        columnMapping.resize(5);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "x";
        columnMapping[3].columnName = "y";
        columnMapping[4].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Body:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "bodyflag";
        columnMapping[3].columnName = "mass";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        // Ignore third column (bodyflag).
        columnMapping.mapStandardColumn(3, ParticlesObject::MassProperty);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Bond:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Charge:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "q";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::ChargeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Dipole:
        columnMapping.resize(9);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "q";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping[6].columnName = "mux";
        columnMapping[7].columnName = "muy";
        columnMapping[8].columnName = "muz";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::ChargeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        columnMapping.mapStandardColumn(6, ParticlesObject::DipoleOrientationProperty, 0);
        columnMapping.mapStandardColumn(7, ParticlesObject::DipoleOrientationProperty, 1);
        columnMapping.mapStandardColumn(8, ParticlesObject::DipoleOrientationProperty, 2);
        break;
    case AtomStyle_DPD:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "theta";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("theta"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_EDPD:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "edpd_temp";
        columnMapping[3].columnName = "edpd_cv";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("edpd_temp"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(3, QStringLiteral("edpd_cv"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_MDPD:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "rho";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("rho"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Electron:
        columnMapping.resize(8);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "q";
        columnMapping[3].columnName = "spin";
        columnMapping[4].columnName = "eradius";
        columnMapping[5].columnName = "x";
        columnMapping[6].columnName = "y";
        columnMapping[7].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::ChargeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::SpinProperty);
        columnMapping.mapCustomColumn(4, QStringLiteral("eradius"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(7, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Ellipsoid:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "ellipsoidflag";
        columnMapping[3].columnName = "density";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("ellipsoidflag"), DataBuffer::Int32);
        columnMapping.mapCustomColumn(3, QStringLiteral("Density"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Full:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "q";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::ChargeProperty);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Line:
        columnMapping.resize(8);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "lineflag";
        columnMapping[4].columnName = "density";
        columnMapping[5].columnName = "x";
        columnMapping[6].columnName = "y";
        columnMapping[7].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(3, QStringLiteral("lineflag"), DataBuffer::Int32);
        columnMapping.mapCustomColumn(4, QStringLiteral("Density"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(7, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Meso:
        columnMapping.resize(8);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "rho";
        columnMapping[3].columnName = "e";
        columnMapping[4].columnName = "cv";
        columnMapping[5].columnName = "x";
        columnMapping[6].columnName = "y";
        columnMapping[7].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("rho"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(3, QStringLiteral("e"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(4, QStringLiteral("cv"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(7, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Molecular:
        columnMapping.resize(6);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "x";
        columnMapping[4].columnName = "y";
        columnMapping[5].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Peri:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "volume";
        columnMapping[3].columnName = "density";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("Volume"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(3, QStringLiteral("Density"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_SMD:
        columnMapping.resize(13);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "molecule";
        columnMapping[3].columnName = "volume";
        columnMapping[4].columnName = "mass";
        columnMapping[5].columnName = "kernel-radius";
        columnMapping[6].columnName = "contact-radius";
        columnMapping[7].columnName = "x0";
        columnMapping[8].columnName = "y0";
        columnMapping[9].columnName = "z0";
        columnMapping[10].columnName = "x";
        columnMapping[11].columnName = "y";
        columnMapping[12].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("molecule"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(3, QStringLiteral("Volume"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(4, ParticlesObject::MassProperty);
        columnMapping.mapCustomColumn(5, QStringLiteral("kernelradius"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(6, QStringLiteral("contactradius"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(7, QStringLiteral("x0"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(8, QStringLiteral("y0"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(9, QStringLiteral("z0"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(10, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(11, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(12, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Sphere:
        columnMapping.resize(7);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "diameter";
        columnMapping[3].columnName = "density";
        columnMapping[4].columnName = "x";
        columnMapping[5].columnName = "y";
        columnMapping[6].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::RadiusProperty);
        columnMapping.mapCustomColumn(3, QStringLiteral("Density"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Template:
        columnMapping.resize(8);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "template-index";
        columnMapping[3].columnName = "template-atom";
        columnMapping[4].columnName = "atom-type";
        columnMapping[5].columnName = "x";
        columnMapping[6].columnName = "y";
        columnMapping[7].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapCustomColumn(2, QStringLiteral("templateindex"), DataBuffer::Int32);
        columnMapping.mapCustomColumn(3, QStringLiteral("templateatom"), DataBuffer::Int64);
        columnMapping.mapStandardColumn(4, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(7, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Tri:
        columnMapping.resize(8);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "molecule-ID";
        columnMapping[2].columnName = "atom-type";
        columnMapping[3].columnName = "triangleflag";
        columnMapping[4].columnName = "density";
        columnMapping[5].columnName = "x";
        columnMapping[6].columnName = "y";
        columnMapping[7].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::MoleculeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::TypeProperty);
        columnMapping.mapCustomColumn(3, QStringLiteral("triangleflag"), DataBuffer::Int32);
        columnMapping.mapCustomColumn(4, QStringLiteral("Density"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(5, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(6, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(7, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Wavepacket:
        columnMapping.resize(11);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "charge";
        columnMapping[3].columnName = "spin";
        columnMapping[4].columnName = "eradius";
        columnMapping[5].columnName = "etag";
        columnMapping[6].columnName = "cs_re";
        columnMapping[7].columnName = "cs_im";
        columnMapping[8].columnName = "x";
        columnMapping[9].columnName = "y";
        columnMapping[10].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::ChargeProperty);
        columnMapping.mapStandardColumn(3, ParticlesObject::SpinProperty);
        columnMapping.mapCustomColumn(4, QStringLiteral("eradius"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(5, QStringLiteral("etag"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(6, QStringLiteral("cs_re"), DataBuffer::FloatDefault);
        columnMapping.mapCustomColumn(7, QStringLiteral("cs_im"), DataBuffer::FloatDefault);
        columnMapping.mapStandardColumn(8, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(9, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(10, ParticlesObject::PositionProperty, 2);
        break;
    case AtomStyle_Hybrid:
        columnMapping.resize(5);
        columnMapping[0].columnName = "atom-ID";
        columnMapping[1].columnName = "atom-type";
        columnMapping[2].columnName = "x";
        columnMapping[3].columnName = "y";
        columnMapping[4].columnName = "z";
        columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
        columnMapping.mapStandardColumn(1, ParticlesObject::TypeProperty);
        columnMapping.mapStandardColumn(2, ParticlesObject::PositionProperty, 0);
        columnMapping.mapStandardColumn(3, ParticlesObject::PositionProperty, 1);
        columnMapping.mapStandardColumn(4, ParticlesObject::PositionProperty, 2);
        for(LAMMPSAtomStyle substyle : atomSubStyles) {
            ParticleInputColumnMapping substyleColumns = createAtomsColumnMapping(substyle, {}, 0);
            for(const InputColumnInfo& substyleColumn : substyleColumns) {
                OVITO_ASSERT(substyleColumn.columnName.isEmpty() == false);
                if(std::none_of(columnMapping.begin(), columnMapping.end(), [&](const InputColumnInfo& column) {
                    return column.columnName == substyleColumn.columnName;
                })) {
                    columnMapping.push_back(substyleColumn);
                }
            }
        }
        break;
    case AtomStyle_Unknown:
        break;
    default:
        OVITO_ASSERT(false);
    }
    if(columnMapping.size() + 3 == dataColumnCount) {
        columnMapping.emplace_back(&ParticlesObject::OOClass(), ParticlesObject::PeriodicImageProperty, 0);
        columnMapping.emplace_back(&ParticlesObject::OOClass(), ParticlesObject::PeriodicImageProperty, 1);
        columnMapping.emplace_back(&ParticlesObject::OOClass(), ParticlesObject::PeriodicImageProperty, 2);
        columnMapping[columnMapping.size() - 3].columnName = QStringLiteral("nx");
        columnMapping[columnMapping.size() - 2].columnName = QStringLiteral("ny");
        columnMapping[columnMapping.size() - 1].columnName = QStringLiteral("nz");
    }
    return columnMapping;
}

/******************************************************************************
* Sets up the mapping of data file columns in the 'Velocities' section to internal particle properties based on the selected LAMMPS atom style.
******************************************************************************/
ParticleInputColumnMapping LAMMPSDataImporter::createVelocitiesColumnMapping(LAMMPSAtomStyle atomStyle, const std::vector<LAMMPSAtomStyle>& atomSubStyles)
{
    ParticleInputColumnMapping columnMapping;
    columnMapping.resize(4);
    columnMapping[0].columnName = "atom-ID";
    columnMapping[1].columnName = "vx";
    columnMapping[2].columnName = "vy";
    columnMapping[3].columnName = "vz";
    columnMapping.mapStandardColumn(0, ParticlesObject::IdentifierProperty);
    columnMapping.mapStandardColumn(1, ParticlesObject::VelocityProperty, 0);
    columnMapping.mapStandardColumn(2, ParticlesObject::VelocityProperty, 1);
    columnMapping.mapStandardColumn(3, ParticlesObject::VelocityProperty, 2);
    switch(atomStyle) {
    case AtomStyle_Sphere:
        columnMapping.resize(7);
        columnMapping[4].columnName = "wx";
        columnMapping[5].columnName = "wy";
        columnMapping[6].columnName = "wz";
        columnMapping.mapStandardColumn(4, ParticlesObject::AngularVelocityProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::AngularVelocityProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::AngularVelocityProperty, 2);
        break;
    case AtomStyle_Electron:
        columnMapping.resize(5);
        columnMapping[4].columnName = "ervel";
        columnMapping.mapCustomColumn(4, QStringLiteral("ervel"), DataBuffer::FloatDefault);
        break;
    case AtomStyle_Ellipsoid:
        columnMapping.resize(7);
        columnMapping[4].columnName = "lx";
        columnMapping[5].columnName = "ly";
        columnMapping[6].columnName = "lz";
        columnMapping.mapStandardColumn(4, ParticlesObject::AngularMomentumProperty, 0);
        columnMapping.mapStandardColumn(5, ParticlesObject::AngularMomentumProperty, 1);
        columnMapping.mapStandardColumn(6, ParticlesObject::AngularMomentumProperty, 2);
        break;
    case AtomStyle_Hybrid:
        for(LAMMPSAtomStyle substyle : atomSubStyles) {
            ParticleInputColumnMapping substyleColumns = createVelocitiesColumnMapping(substyle, {});
            for(const InputColumnInfo& substyleColumn : substyleColumns) {
                OVITO_ASSERT(substyleColumn.columnName.isEmpty() == false);
                if(std::none_of(columnMapping.begin(), columnMapping.end(), [&](const InputColumnInfo& column) {
                    return column.columnName == substyleColumn.columnName;
                })) {
                    columnMapping.push_back(substyleColumn);
                }
            }
        }
        break;
    default:
        break;
    }
    return columnMapping;
}

/******************************************************************************
* Inspects the header of the given file and returns the detected LAMMPS atom style.
******************************************************************************/
Future<LAMMPSDataImporter::LAMMPSAtomStyleHints> LAMMPSDataImporter::inspectFileHeader(const Frame& frame)
{
    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) -> LAMMPSAtomStyleHints {
            using namespace std;

            // Open file for reading.
            CompressedTextReader stream(fileHandle);
            // Skip comment line
            stream.readLine();
            // Parse file header.
            while(true) {
                string line(stream.readLine());

                // Trim anything from '#' onward.
                size_t commentStart = line.find('#');
                if(commentStart != string::npos) line.resize(commentStart);

                // If line is blank, continue.
                if(line.find_first_not_of(" \t\n\r") == string::npos) continue;

                if(line.find("xlo") != string::npos && line.find("xhi") != string::npos) {}
                else if(line.find("ylo") != string::npos && line.find("yhi") != string::npos) {}
                else if(line.find("zlo") != string::npos && line.find("zhi") != string::npos) {}
                else if(line.find("xy") != string::npos && line.find("xz") != string::npos && line.find("yz") != string::npos) {}
                else if(line.find("atoms") != string::npos) {
                    qlonglong natoms = 0;
                    sscanf(line.c_str(), "%llu", &natoms);
                    if(natoms <= 0) return {};
                }
                else if(line.find("atom") != string::npos && line.find("types") != string::npos) {}
                else if(line.find("bonds") != string::npos) {}
                else if(line.find("bond") != string::npos && line.find("types") != string::npos) {}
                else if(line.find("angles") != string::npos) {}
                else if(line.find("angle") != string::npos && line.find("types") != string::npos) {}
                else if(line.find("dihedrals") != string::npos) {}
                else if(line.find("dihedral") != string::npos && line.find("types") != string::npos) {}
                else if(line.find("impropers") != string::npos) {}
                else if(line.find("improper") != string::npos && line.find("types") != string::npos) {}
                else if(line.find("extra") != string::npos && line.find("per") != string::npos && line.find("atom") != string::npos) {}
                else if(line.find("triangles") != string::npos) {}
                else if(line.find("ellipsoids") != string::npos) {}
                else if(line.find("lines") != string::npos) {}
                else if(line.find("bodies") != string::npos) {}
                else break;
            }

            // Skip to following line after first non-blank line.
            while(!stream.eof() && string(stream.line()).find_first_not_of(" \t\n\r") == string::npos) {
                stream.readLine();
            }

            // Read lines one by one in free-form part of data file until we find the 'Atoms' section.
            while(!stream.eof()) {
                if(stream.lineStartsWithToken("Atoms", true)) {
                    // Try to guess the LAMMPS atom style from the 'Atoms' keyword line or the first data line.
                    LAMMPSAtomStyleHints styleHints;
                    QByteArray keyword = QByteArray(stream.line()).trimmed();
                    stream.readLine();
                    detectAtomStyle(stream.readLine(), keyword, styleHints);
                    return styleHints;
                }
                stream.readLineTrimLeft();
            }
            return {};
        });
}


}   // End of namespace
