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
#include "PDBImporter.h"

#include <3rdparty/gemmi/pdb.hpp>
#include <3rdparty/gemmi/remarks.hpp>

namespace gemmi {
template<>
inline size_t copy_line_from_stream<Ovito::CompressedTextReader&>(char* line, int size, Ovito::CompressedTextReader& stream)
{
    using namespace gemmi::pdb_impl;

    // Return no line if end of file has been reached.
    if(stream.eof())
        return 0;

    // Read a single line form the input stream.
    const char* src_line = stream.readLine();

    // Stop reading the file when ENDMDL marker is reached. We don't want Gemmi to read all frames of a trajectory file.
    if(is_record_type(src_line, "ENDMDL")) {
        return 0;
    }

    // Copy line contents to output buffer.
    size_t len = qstrlen(src_line);
    qstrncpy(line, src_line, size);

    if(is_record_type(src_line, "ATOM") || is_record_type(src_line, "HETATM")) {
        // Some PDB files have an ATOM or HETATM line that is shorter than what Gemmi's parser expects.
        // Pad such lines by appending  additional whitespace.
        if(len < 66 && len >= 54 && size > 66) {
            while(len < 66)
                line[len++] = ' ';
            line[len] = '\0';
        }

        // Gemmi expects atom names to start at column index 12. Some files have one extra space at this positions and the
        // name actually begins at position 13. Make the parser happy by moving the text by one positon to the left.
        // For example, turn " Au " into "Au  ", but preserve " CA " or " HE ".
        if(len >= 16 && size >= 16 && line[12] == ' ' && line[13] >= 'A' && line[13] <= 'Z' && line[14] >= 'a' && line[14] <= 'z' && line[15] == ' ') {
            line[12] = line[13];
            line[13] = line[14];
            line[14] = ' ';
            line[15] = ' ';
        }
        // Some files have 2 extra spaces at this positions and the name actually begins at position 14. Make the parser happy by moving the text by two characters to the left.
        // For example, turn "  O " into "O   ":
        else if(len >= 16 && size >= 16 && line[12] == ' ' && line[13] == ' ' && line[14] >= 'A' && line[14] <= 'Z') {
            line[12] = line[14];
            line[13] = line[15];
            line[14] = ' ';
            line[15] = ' ';
        }
        // Some files have a digit prepended to the element name. Remove it so that Gemmi can recognize the element correctly.
        // For example, turn "1HH1" into " HH1":
        else if(len >= 16 && size >= 16 && line[12] >= '1' && line[12] <= '9' && line[13] >= 'A' && line[13] <= 'Z') {
            line[12] = ' ';
        }
    }

    // Return line length (up to maximum) to caller.
    return std::min(len, (size_t)(size - 1));
}
}

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(PDBImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool PDBImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read up to 60 lines from the beginning of the file.
    for(int i = 0; i < 60 && !stream.eof(); i++) {
        stream.readLine(122);
        if(qstrlen(stream.line()) > 120 && !stream.lineStartsWithToken("TITLE"))
            return false;
        if(qstrlen(stream.line()) >= 7 && stream.line()[6] != ' ' && std::find(stream.line(), stream.line()+6, ' ') != stream.line()+6)
            return false;
        if(stream.lineStartsWithToken("HEADER") || stream.lineStartsWithToken("ATOM") || stream.lineStartsWith("HETATM"))
            return true;
    }

    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void PDBImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning PDB file %1").arg(stream.filename()));
    setProgressMaximum(stream.underlyingSize());

    Frame frame(fileHandle());
    frame.byteOffset = stream.byteOffset();
    frame.lineNumber = stream.lineNumber();
    bool endOnPreviousLine = false;
    while(!stream.eof()) {

        if(isCanceled())
            return;

        stream.readLine();

        if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
            return;

        if(stream.lineStartsWithToken("ENDMDL")) {
            frames.push_back(frame);
            frame.byteOffset = stream.byteOffset();
            frame.lineNumber = stream.lineNumber();
        }
        else if(stream.lineStartsWithToken("REMARK    Step")) {
            // Recognize and parse CP2K timestep information, which has the following format:
            //   Step <NUMBER>, time = <TIME>, E = <ENERGY>
            static const QRegularExpression cp2k_re(QStringLiteral(R"(REMARK\s+Step\s+(\d+))"));
            QRegularExpressionMatch match = cp2k_re.match(stream.lineString());
            if(match.hasMatch())
                frame.label = QString("Timestep %1").arg(match.captured(1));
        }
        else if(stream.lineStartsWithToken("END")) {
            if(frames.empty())
                frames.push_back(frame);
            endOnPreviousLine = true;
            frame.byteOffset = stream.byteOffset();
            frame.lineNumber = stream.lineNumber();
            stream.recordSeekPoint();
        }
        else if(endOnPreviousLine) {
            frames.push_back(frame);
            endOnPreviousLine = false;
        }
    }

    if(frames.empty()) {
        // It's not a trajectory file. Report just a single frame.
        frames.push_back(Frame(fileHandle()));
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void PDBImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading PDB file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    try {
        // Parse the PDB file's contents.
        gemmi::Structure structure = gemmi::pdb_impl::read_pdb_from_stream(stream, qPrintable(frame().sourceFile.path()), gemmi::PdbReadOptions());
        if(isCanceled()) return;

        // Import PDB metadata fields as global attributes.
        for(const auto& m : structure.info) {
            state().setAttribute(QString::fromStdString(m.first), QVariant::fromValue(QString::fromStdString(m.second)), dataSource());
        }

        // Import PDB remark lines as global attributes.
        int remarkIndex = 0;
        for(const std::string& remark : structure.raw_remarks) {
            if(gemmi::remark_number(remark) == 0) {
                QString remarkString = QString::fromStdString(remark);
                if(remarkString.startsWith("REMARK"))
                    remarkString.remove(0, 6);
                remarkString = std::move(remarkString).trimmed();
                // Recognize CP2K trajectory records.
                if(remarkString.startsWith("Step ")) {
                    // Parse CP2K timestep information, which has the following format:
                    //   Step <NUMBER>, time = <TIME>, E = <ENERGY>
                    static const QRegularExpression cp2k_re(QStringLiteral(R"(Step\s+(\d+)\s*,\s*time\s*=\s*([-+]?[0-9]*\.?[0-9]+)\s*,\s*E\s*=\s*([-+]?[0-9]*\.?[0-9]+))"));
                    QRegularExpressionMatch match = cp2k_re.match(remarkString);
                    if(match.hasMatch()) {
                        bool ok1, ok2, ok3;
                        qlonglong timestep = match.captured(1).toLongLong(&ok1);
                        FloatType time = match.captured(2).toDouble(&ok2);
                        FloatType energy = match.captured(3).toDouble(&ok3);
                        if(ok1)
                            state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), dataSource());
                        if(ok2)
                            state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(time), dataSource());
                        if(ok3)
                            state().setAttribute(QStringLiteral("Energy"), QVariant::fromValue(energy), dataSource());
                        continue;
                    }
                }
                state().setAttribute(QStringLiteral("pdb.remark.%1").arg(++remarkIndex), QVariant::fromValue(remarkString.trimmed()), dataSource());
            }
        }

        structure.merge_chain_parts();
        if(isCanceled()) return;

        if(structure.models.empty())
            throw Exception(tr("PDB parsing error: No structural models."));
        const gemmi::Model& model = structure.models.back();

        // Count total number of atoms.
        size_t natoms = 0;
        for(const gemmi::Chain& chain : model.chains) {
            for(const gemmi::Residue& residue : chain.residues) {
                natoms += residue.atoms.size();
            }
        }

        // Allocate property arrays for atoms.
        setParticleCount(natoms);
        BufferAccess<Point3> posAccess = particles()->createProperty(ParticlesObject::PositionProperty);
        PropertyObject* typeProperty = particles()->createProperty(ParticlesObject::TypeProperty);
        PropertyObject* atomNameProperty = particles()->createProperty(QStringLiteral("Atom Name"), DataBuffer::Int32);
        PropertyObject* residueTypeProperty = particles()->createProperty(QStringLiteral("Residue Type"), DataBuffer::Int32);

        // Give these particle properties new titles, which are displayed in the GUI under the file source.
        atomNameProperty->setTitle(tr("Atom names"));
        residueTypeProperty->setTitle(tr("Residue types"));

        BufferAccess<int32_t> typeAccess(typeProperty);;
        BufferAccess<int32_t> atomNameAccess(atomNameProperty);
        BufferAccess<int32_t> residueTypeAccess(residueTypeProperty);
        auto* posIter = posAccess.begin();
        auto* typeIter = typeAccess.begin();
        auto* atomNameIter = atomNameAccess.begin();
        auto* residueTypeIter = residueTypeAccess.begin();

        // Transfer atomic data from Gemmi to OVITO data structures.
        bool hasOccupancy = false;
        for(const gemmi::Chain& chain : model.chains) {
            for(const gemmi::Residue& residue : chain.residues) {
                if(isCanceled()) return;
                int residueTypeId = (residue.name.empty() == false) ? addNamedType(ParticlesObject::OOClass(), residueTypeProperty, QLatin1String(residue.name.c_str(), residue.name.size()))->numericId() : 0;
                for(const gemmi::Atom& atom : residue.atoms) {
                    // Atomic position.
                    *posIter++ = Point3(atom.pos.x, atom.pos.y, atom.pos.z);

                    // Chemical type.
                    *typeIter++ = atom.element.ordinal();
                    addNumericType(ParticlesObject::OOClass(), typeProperty, atom.element.ordinal(), QString::fromStdString(atom.element.name()));

                    // Atom name.
                    *atomNameIter++ = addNamedType(ParticlesObject::OOClass(), atomNameProperty, QLatin1String(atom.name.c_str(), atom.name.size()))->numericId();

                    // Residue type.
                    *residueTypeIter++ = residueTypeId;

                    // Check for presence of occupancy values.
                    if(atom.occ != 0 && atom.occ != 1) hasOccupancy = true;
                }
            }
        }
        if(isCanceled())
            return;
        typeAccess.reset();
        atomNameAccess.reset();
        residueTypeAccess.reset();

        // Parse the optional site occupancy information.
        if(hasOccupancy) {
            BufferAccess<FloatType> occupancyProperty = particles()->createProperty(QStringLiteral("Occupancy"), DataBuffer::FloatDefault);
            FloatType* occupancyIter = occupancyProperty.begin();
            for(const gemmi::Chain& chain : model.chains) {
                for(const gemmi::Residue& residue : chain.residues) {
                    for(const gemmi::Atom& atom : residue.atoms) {
                        *occupancyIter++ = atom.occ;
                    }
                }
            }
            OVITO_ASSERT(occupancyIter == occupancyProperty.end());
        }

        // Since we created particle types on the go while reading the particles, the assigned particle type IDs
        // depend on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
        // why we sort them now.
        typeProperty->sortElementTypesById();
        atomNameProperty->sortElementTypesByName();
        residueTypeProperty->sortElementTypesByName();

        // Parse unit cell if available.
        //
        // Gemmi provides the UnitCell::is_crystal() method to determin whether (periodic) cell information
        // is available. But it turned out to be too strict. Atomsk writes PDB files containing unity
        // SCALEn records, which make Gemmi intrept these files as non-periodic. We replace the check
        // with a simpler criterion.
        // See https://www.ovito.org/forum/topic/polyhedral-visualazation/#postid-4244.
        if(structure.cell.a != 1.0 || structure.cell.b != 1.0 || structure.cell.c != 1.0 || structure.cell.alpha != 90.0 || structure.cell.beta != 90.0 || structure.cell.gamma != 90.0) {

            // Some PDB files use wrong column widths in the CRYST1 record line. These leads to invalid cell values when parsed by gemmi.
            if(std::isnan(structure.cell.alpha) || std::isnan(structure.cell.beta) || std::isnan(structure.cell.gamma) ||
                structure.cell.alpha < 0 || structure.cell.alpha > 180 || structure.cell.beta < 0 || structure.cell.beta > 180 || structure.cell.gamma < 0 || structure.cell.gamma > 180 ||
                std::isnan(structure.cell.a) || std::isnan(structure.cell.b) || std::isnan(structure.cell.c))
                    throw Exception(tr("PDB file parsing error: CRYST1 record is invalid or has wrong format. Cannot parse a valid simulation cell."));

            // Process periodic unit cell definition.
            AffineTransformation cell = AffineTransformation::Identity();
            if(structure.cell.alpha == 90 && structure.cell.beta == 90 && structure.cell.gamma == 90) {
                cell(0,0) = structure.cell.a;
                cell(1,1) = structure.cell.b;
                cell(2,2) = structure.cell.c;
            }
            else if(structure.cell.alpha == 90 && structure.cell.beta == 90) {
                FloatType gamma = qDegreesToRadians(structure.cell.gamma);
                cell(0,0) = structure.cell.a;
                cell(0,1) = structure.cell.b * std::cos(gamma);
                cell(1,1) = structure.cell.b * std::sin(gamma);
                cell(2,2) = structure.cell.c;
            }
            else {
                FloatType alpha = qDegreesToRadians(structure.cell.alpha);
                FloatType beta = qDegreesToRadians(structure.cell.beta);
                FloatType gamma = qDegreesToRadians(structure.cell.gamma);
                FloatType v = structure.cell.a * structure.cell.b * structure.cell.c * sqrt(1.0 - std::cos(alpha)*std::cos(alpha) - std::cos(beta)*std::cos(beta) - std::cos(gamma)*std::cos(gamma) + 2.0 * std::cos(alpha) * std::cos(beta) * std::cos(gamma));
                cell(0,0) = structure.cell.a;
                cell(0,1) = structure.cell.b * std::cos(gamma);
                cell(1,1) = structure.cell.b * std::sin(gamma);
                cell(0,2) = structure.cell.c * std::cos(beta);
                cell(1,2) = structure.cell.c * (std::cos(alpha) - std::cos(beta)*std::cos(gamma)) / std::sin(gamma);
                cell(2,2) = v / (structure.cell.a * structure.cell.b * std::sin(gamma));
            }
            simulationCell()->setCellMatrix(cell);
        }
        else if(posAccess.size() != 0) {
            // Use bounding box of atomic coordinates as non-periodic simulation cell.
            Box3 boundingBox;
            boundingBox.addPoints(posAccess);
            simulationCell()->setPbcFlags(false, false, false);
            simulationCell()->setCellMatrix(AffineTransformation(
                    Vector3(boundingBox.sizeX(), 0, 0),
                    Vector3(0, boundingBox.sizeY(), 0),
                    Vector3(0, 0, boundingBox.sizeZ()),
                    boundingBox.minc - Point3::Origin()));
        }
        state().setStatus(tr("Number of atoms: %1").arg(natoms));
    }
    catch(const Exception&) {
        throw;
    }
    catch(const std::exception& e) {
        throw Exception(tr("PDB file error: %1").arg(e.what()));
    }

    // Detect if there are more simulation frames following in the file (only when reading the first frame).
    if(frame().byteOffset == 0 && !stream.eof()) {
        stream.readLine();
        if(!stream.eof()) {
            signalAdditionalFrames();
        }
    }

    // Generate ad-hoc bonds between atoms based on their van der Waals radii.
    if(_generateBonds)
        generateBonds();
    else
        setBondCount(0);

    // If the loaded particles are centered on the coordinate origin but the periodic simulation box corner is positioned at (0,0,0),
    // then shift the cell to center it on (0,0,0) too, leaving the particle coordinates as is.
    // correctOffcenterCell();

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
