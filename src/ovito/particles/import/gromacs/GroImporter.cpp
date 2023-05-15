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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/NumberParsing.h>
#include "GroImporter.h"

#include <3rdparty/gemmi/elem.hpp>

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(GroImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool GroImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Skip first comment line.
    stream.readLine(1024);

    // Read second line.
    const char* p = stream.readLineTrimLeft(128);
    if(*p == '\0')
        return false;

    // Parse number of atoms.
    unsigned long long numParticles;
    int charCount;
    if(sscanf(p, "%llu%n", &numParticles, &charCount) != 1 || numParticles < 1)
        return false;

    // Check trailing whitespace. There should be nothing but the number of atoms on the second line.
    bool foundNewline = false;
    for(p += charCount; *p != '\0'; ++p) {
        if(*p > ' ')
            return false;
        if(*p == '\n' || *p == '\r')
            foundNewline = true;
    }
    if(!foundNewline)
        return false;

    // Read a few atom lines to check if the columns have the right format.
    for(unsigned long long i = 0; i < 10; i++) {
        int i1, i2;
        char s1[6], s2[6];
        if(sscanf(stream.readLine(), "%5i%5s%5s%5i", &i1, s1, s2, &i2) != 4 || i1 < 1 || i2 < 1 || qstrlen(stream.line()) < 20)
            return false;

        // The following extra check is necessary to avoid mistaking an XDATCAR file, which stores the simulation cell
        // vectors in lines 3-5, for a GRO file (see test case 'POSCAR/XDATCAR.testcase'):
        if(s1[0] == '.' || s2[0] == '.')
            return false;

        // Parse atomic xyz coordinates.
        // First, determine column width by counting distance between decimal points.
        const char* token = stream.line() + 20;
        const char* c = token;
        while(*c != '\0' && *c != '.')
            c++;
        if(*c == '\0')
            return false;
        int columnWidth = 1;
        for(const char* c2 = c + 1; *c2 != '\0' && *c2 != '.'; ++c2)
            columnWidth++;
        for(size_t dim = 0; dim < 3; dim++) {
            const char* token_end = token + columnWidth;
            while(token < token_end && *token <= ' ') {
                if(*token == '\0')
                    return false;
                ++token;
            }
            FloatType f;
            if(!parseFloatType(token, token_end, f))
                return false;
            token = token_end;
        }

        // If end of atoms list is already reached, parse simulation cell definition.
        if(i == numParticles - 1) {
            AffineTransformation cell;
            if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &cell(0,0), &cell(1,1), &cell(2,2)) != 3)
                return false;
            break;
        }
    }

    return true;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void GroImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning file %1").arg(fileHandle().toString()));
    setProgressMaximum(stream.underlyingSize());

    int frameNumber = 0;
    QString filename = fileHandle().sourceUrl().fileName();
    Frame frame(fileHandle());

    while(!stream.eof() && !isCanceled()) {
        frame.byteOffset = stream.byteOffset();
        frame.lineNumber = stream.lineNumber();
        stream.recordSeekPoint();

        // Skip comment line.
        stream.readLine();

        // Parse number of atoms.
        const char* line = stream.readLineTrimLeft();

        if(line[0] == '\0') break;

        unsigned long long numParticlesLong;
        int charCount;
        if(sscanf(line, "%llu%n", &numParticlesLong, &charCount) != 1)
            throw Exception(tr("Invalid number of atoms in line %1 of Gromacs file: %2").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));

        // Check trailing whitespace. There should be nothing else but the number of atoms on the second line.
        for(const char* p = line + charCount; *p != '\0'; ++p) {
            if(*p > ' ')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected token following number of atoms:\n\n\"%2\"").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));
        }

        // Create a new record for the time step.
        frame.label = QStringLiteral("%1 (Frame %2)").arg(filename).arg(frameNumber++);
        frames.push_back(frame);

        // Skip atom lines.
        for(unsigned long long i = 0; i < numParticlesLong; i++) {
            stream.readLine();
            if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
                return;
        }

        // Skip cell geometry line.
        stream.readLine();
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void GroImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading Gromacs file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Read comment line.
    stream.readLine();
    QString commentLine = stream.lineString().trimmed();

    // Parse number of atoms.
    unsigned long long numParticles;
    int charCount;
    if(sscanf(stream.readLine(), "%llu%n", &numParticles, &charCount) != 1)
        throw Exception(tr("Invalid number of particles in line %1 of Gromacs file: %2").arg(stream.lineNumber()).arg(stream.lineString().trimmed()));

    // Check trailing whitespace. There should be nothing but the number of atoms on that line.
    for(const char* p = stream.line() + charCount; *p != '\0'; ++p) {
        if(*p > ' ')
            throw Exception(tr("Parsing error in line %1 of Gromacs file. The second line of a .gro file should contain just the number of atoms and nothing else.").arg(stream.lineNumber()));
    }
    if(numParticles > (unsigned long long)std::numeric_limits<int>::max())
        throw Exception(tr("Too many atoms in Gromacs file. This program version can read files with up to %1 atoms only.").arg(std::numeric_limits<int>::max()));
    setProgressMaximum(numParticles);
    setParticleCount(numParticles);

    // Create particle properties.
    PropertyAccess<Point3> posProperty = particles()->createProperty(DataBuffer::Initialized, ParticlesObject::PositionProperty);
    PropertyAccess<int32_t> typeProperty = particles()->createProperty(ParticlesObject::TypeProperty);
    PropertyAccess<int32_t> atomNameProperty = particles()->createProperty(QStringLiteral("Atom Name"), PropertyObject::Int32);
    PropertyAccess<int32_t> residueTypeProperty = particles()->createProperty(QStringLiteral("Residue Type"), PropertyObject::Int32);
    PropertyAccess<int64_t> residueNumberProperty = particles()->createProperty(QStringLiteral("Residue Identifier"), PropertyObject::Int64);
    PropertyAccess<int64_t> identifierProperty = particles()->createProperty(DataBuffer::Initialized, ParticlesObject::IdentifierProperty);
    PropertyAccess<Vector3> velocityProperty;

    // Give these particle properties new titles, which are displayed in the GUI under the file source.
    atomNameProperty.buffer()->setTitle(tr("Atom names"));
    residueTypeProperty.buffer()->setTitle(tr("Residue types"));

    // Parse list of atoms.
    int atomBaseNumber = 0;
    int residueBaseNumber = 0;
    for(size_t i = 0; i < numParticles; i++) {
        if(!setProgressValueIntermittent(i)) return;
        const char* token = stream.readLine();

        // Parse residue number (5 characters).
        const char* token_end = token + 5;
        while(token < token_end && *token <= ' ') {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            ++token;
        }
        int residueNumber;
        bool ok = parseInt(token, token_end, residueNumber);
        if(!ok)
            throw Exception(tr("Parsing error in line %1 of Gromacs file. Invalid residue number.").arg(stream.lineNumber()));
        if(residueNumber == 0)
            residueBaseNumber += 100000;
        residueNumber += residueBaseNumber;
        token = token_end;

        // Parse residue name (5 characters).
        char residueName[6];
        char* residueNameStart = residueName;
        token_end = token + 5;
        for(; token != token_end; token++, residueNameStart++) {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            if(*token > ' ')
                break;
        }
        char* residueNameEnd = residueNameStart;
        for(; token != token_end; token++, residueNameEnd++) {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            if(*token <= ' ')
                break;
            *residueNameEnd = *token;
        }
        token = token_end;

        // Parse atom name (5 characters).
        char atomName[6];
        char* atomNameStart = atomName;
        token_end = token + 5;
        for(; token != token_end; token++, atomNameStart++) {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            if(*token > ' ')
                break;
            *atomNameStart = *token;
        }
        char* atomNameEnd = atomNameStart;
        for(; token != token_end; token++, atomNameEnd++) {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            if(*token <= ' ')
                break;
            *atomNameEnd = *token;
        }
        *atomNameEnd = '\0';
        token = token_end;

        // Parse atom number (5 characters).
        token_end = token + 5;
        while(token < token_end && *token <= ' ') {
            if(*token == '\0')
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
            ++token;
        }
        int atomNumber = 0;
        ok = parseInt(token, token_end, atomNumber);
        if(ok && atomNumber == 0 && numParticles >= 100000)
            atomBaseNumber += 100000;
        atomNumber += atomBaseNumber;
        if(!ok || atomNumber <= 0 || atomNumber > numParticles)
            throw Exception(tr("Parsing error in line %1 of Gromacs file. Invalid atom number.").arg(stream.lineNumber()));
        token = token_end;
        size_t atomIndex = atomNumber - 1;

        // Guess chemical element from atom name.
        // The algorithm has been adopted from the OpenBabel Gromacs reader code.
        gemmi::Element element(gemmi::El::X);
        if(atomNameStart[0] == 'C') {
            if(atomNameStart[1] == 'a') element = gemmi::Element(gemmi::El::Ca);
            else if(atomNameStart[1] == 'l') element = gemmi::Element(gemmi::El::Cl);
            else if(atomNameStart[1] == 'o') element = gemmi::Element(gemmi::El::Co);
            else if(atomNameStart[1] == 'r') element = gemmi::Element(gemmi::El::Cr);
            else if(atomNameStart[1] == 'u') element = gemmi::Element(gemmi::El::Cu);
            else element = gemmi::Element(gemmi::El::C);
        }
        else if(atomNameStart[0] == 'N') {
            if(atomNameStart[1] == 'a') element = gemmi::Element(gemmi::El::Na);
            else if(atomNameStart[1] == 'b') element = gemmi::Element(gemmi::El::Nb);
            else if(atomNameStart[1] == 'e') element = gemmi::Element(gemmi::El::Ne);
            else if(atomNameStart[1] == 'i') element = gemmi::Element(gemmi::El::Ni);
            else element = gemmi::Element(gemmi::El::N);
        }
        else if(atomNameStart[0] == 'S') { // Exceptions for two-letter elements SI, FE, and BR - added in OVITO 3.7.12.
            if(atomNameStart[1] == 'I') element = gemmi::Element(gemmi::El::Si);
            else element = gemmi::Element(gemmi::El::S);
        }
        else if(atomNameStart[0] == 'F') { // Exceptions for two-letter elements elements SI, FE, and BR - added in OVITO 3.7.12.
            if(atomNameStart[1] == 'E') element = gemmi::Element(gemmi::El::Fe);
            else element = gemmi::Element(gemmi::El::F);
        }
        else if(atomNameStart[0] == 'B') { // Exceptions for two-letter elements SI, FE, and BR - added in OVITO 3.7.12.
            if(atomNameStart[1] == 'R') element = gemmi::Element(gemmi::El::Br);
            else element = gemmi::Element(gemmi::El::B);
        }
        else {
            const char singleLetterName[2] = { atomNameStart[0], '\0' };
            element = gemmi::Element(singleLetterName);
        }
        addNumericType(ParticlesObject::OOClass(), typeProperty.buffer(), element.ordinal(), QLatin1String(element.name()));

        // Store parsed value in property arrays.
        identifierProperty.set(atomIndex, atomNumber);
        typeProperty.set(atomIndex, element.ordinal());
        atomNameProperty.set(atomIndex,
            (residueNameStart != residueNameEnd) ?
            addNamedType(ParticlesObject::OOClass(), atomNameProperty.buffer(), QLatin1String(atomNameStart, atomNameEnd))->numericId()
            : 0);
        residueTypeProperty.set(atomIndex,
            (residueNameStart != residueNameEnd) ?
            addNamedType(ParticlesObject::OOClass(), residueTypeProperty.buffer(), QLatin1String(residueNameStart, residueNameEnd))->numericId()
            : 0);
        residueNumberProperty.set(atomIndex, residueNumber);

        // Parse atomic xyz coordinates.
        // First, determine column width by counting distance between decimal points.
        const char* c = token;
        while(*c != '\0' && *c != '.')
            c++;
        if(*c == '\0')
            throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
        int columnWidth = 1;
        for(const char* c2 = c + 1; *c2 != '\0' && *c2 != '.'; ++c2)
            columnWidth++;
        Point3& pos = posProperty[atomIndex];
        for(size_t dim = 0; dim < 3; dim++) {
            token_end = token + columnWidth;
            while(token < token_end && *token <= ' ') {
                if(*token == '\0')
                    throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
                ++token;
            }
            ok = parseFloatType(token, token_end, pos[dim]);
            if(!ok)
                throw Exception(tr("Parsing error in line %1 of Gromacs file. Invalid atomic coordinate (col width=%2).").arg(stream.lineNumber()).arg(columnWidth));
            token = token_end;
        }

        // Convert coordinates from nanometers to angstroms.
        pos *= FloatType(10);

        // Parse atomic velocity vectors (optional). Gromacs files use velocity units nm/ps (or km/s).
        // First, determine column width by counting distance between decimal points.
        c = token;
        while(*c != '\0' && *c != '.')
            c++;
        if(*c != '\0') {
            columnWidth = 1;
            for(const char* c2 = c + 1; *c2 != '\0' && *c2 != '.'; ++c2)
                columnWidth++;
            if(!velocityProperty)
                velocityProperty = particles()->createProperty(ParticlesObject::VelocityProperty);
            Vector3& v = velocityProperty[atomIndex];
            for(size_t dim = 0; dim < 3; dim++) {
                token_end = token + columnWidth;
                while(token < token_end && *token <= ' ') {
                    if(*token == '\0')
                        throw Exception(tr("Parsing error in line %1 of Gromacs file. Unexpected end of line.").arg(stream.lineNumber()));
                    ++token;
                }
                ok = parseFloatType(token, token_end, v[dim]);
                if(!ok)
                    throw Exception(tr("Parsing error in line %1 of Gromacs file. Invalid atomic velocity vector (col width=%2).").arg(stream.lineNumber()).arg(columnWidth));
                token = token_end;
            }
        }
    }

    // Since we created particle types on the go while reading the particles, the type ordering
    // depends on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
    // why we sort them now.
    typeProperty.buffer()->sortElementTypesById();
    atomNameProperty.buffer()->sortElementTypesByName();
    residueTypeProperty.buffer()->sortElementTypesByName();

    // Release property accessors.
    posProperty.reset();
    residueTypeProperty.reset();
    residueNumberProperty.reset();
    typeProperty.reset();
    atomNameProperty.reset();
    identifierProperty.reset();
    velocityProperty.reset();

    // Parse simulation cell definition.
    AffineTransformation cell = AffineTransformation::Identity();
    if(sscanf(stream.readLine(),
        FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " "
        FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " "
        FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING,
        &cell(0,0), &cell(1,1), &cell(2,2),
        &cell(1,0), &cell(2,0), &cell(0,1),
        &cell(2,1), &cell(0,2), &cell(1,2)
      ) < 3)
        throw Exception(tr("Parsing error in line %1 of Gromacs file. Invalid simulation cell definition: %2").arg(stream.lineNumber()).arg(stream.lineString()));
    // Convert cell size from nanometers to angstroms.
    simulationCell()->setCellMatrix(cell * FloatType(10));

    // Detect if there are more simulation frames following in the file.
    if(!stream.eof())
        signalAdditionalFrames();

    // Generate ad-hoc bonds between atoms based on their van der Waals radii.
    if(_generateBonds)
        generateBonds();
    else
        setBondCount(0);

    if(commentLine.isEmpty())
        state().setStatus(tr("%1 atoms").arg(numParticles));
    else
        state().setStatus(tr("%1 atoms\n%2").arg(numParticles).arg(commentLine));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
