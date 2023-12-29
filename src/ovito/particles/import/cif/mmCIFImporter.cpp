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
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "mmCIFImporter.h"

#include <3rdparty/gemmi/cif.hpp>
#include <3rdparty/gemmi/mmread.hpp>

namespace Ovito {

namespace cif = gemmi::cif;

IMPLEMENT_CREATABLE_OVITO_CLASS(mmCIFImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool mmCIFImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // First, determine if it is a CIF file.
    // Read the first N lines of the file which are not comments.
    int maxLines = 12;
    bool foundBlockHeader = false;
    bool foundItem = false;
    for(int i = 0; i < maxLines && !stream.eof(); i++) {
        // Note: Maximum line length of CIF files is 2048 characters.
        stream.readLine(2048);

        if(stream.lineStartsWith("#", true)) {
            maxLines++;
            continue;
        }
        else if(stream.lineStartsWith("data_", false)) {
            // Make sure the "data_XXX" block header appears.
            if(foundBlockHeader) return false;
            foundBlockHeader = true;
        }
        else if(stream.lineStartsWith("_", false)) {
            // Make sure at least one "_XXX" item appears.
            foundItem = true;
            break;
        }
    }

    // Make sure it is a CIF file.
    if(!foundBlockHeader || !foundItem)
        return false;

    // Continue reading the entire file until at least one "_atom_site.XXX" entry is found.
    // These entries are specific to the mmCIF format and do not occur in CIF files (small molecule files).
    for(;;) {
        if(stream.lineStartsWith("_atom_site.", false))
            return true;
        if(stream.eof())
            return false;
        stream.readLine();
    }

    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void mmCIFImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading mmCIF file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Map the whole file into memory for parsing.
    const char* buffer_start;
    const char* buffer_end;
    QByteArray fileContents;
    std::tie(buffer_start, buffer_end) = stream.mmap();
    if(!buffer_start) {
        // Could not map mmCIF file into memory. Read it into a in-memory buffer instead.
        fileContents = stream.readAll();
        buffer_start = fileContents.constData();
        buffer_end = buffer_start + fileContents.size();
    }

    try {
        // Parse the mmCIF file's contents.
        cif::Document doc = cif::read_memory(buffer_start, buffer_end - buffer_start, qPrintable(frame().sourceFile.path()));

        // Unmap the input file from memory.
        if(fileContents.isEmpty())
            stream.munmap();
        if(isCanceled()) return;

        // Parse the mmCIF data into an molecular structure representation.
        gemmi::Structure structure = gemmi::make_structure(doc);
        structure.merge_chain_parts();
        if(isCanceled()) return;

        // Import metadata fields as global attributes.
        for(const auto& m : structure.info)
            state().setAttribute(QString::fromStdString(m.first), QVariant::fromValue(QString::fromStdString(m.second)), pipelineNode());

        const gemmi::Model& model = structure.first_model();

        // Count total number of atoms.
        size_t natoms = 0;
        for(const gemmi::Chain& chain : model.chains) {
            for(const gemmi::Residue& residue : chain.residues) {
                natoms += residue.atoms.size();
            }
        }

        // Allocate property arrays for atoms.
        setParticleCount(natoms);
        BufferWriteAccess<Point3, access_mode::discard_write> posProperty = particles()->createProperty(Particles::PositionProperty);
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        Property* atomNameProperty = particles()->createProperty(QStringLiteral("Atom Name"), Property::Int32);
        Property* residueTypeProperty = particles()->createProperty(QStringLiteral("Residue Type"), Property::Int32);

        // Give these particle properties new titles, which are displayed in the GUI under the file source.
        atomNameProperty->setTitle(tr("Atom names"));
        residueTypeProperty->setTitle(tr("Residue types"));

        auto* posIter = posProperty.begin();
        BufferWriteAccess<int32_t, access_mode::discard_write> typeAccess(typeProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> atomNameAccess(atomNameProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> residueTypeAccess(residueTypeProperty);
        auto* typeIter = typeAccess.begin();
        auto* atomNameIter = atomNameAccess.begin();
        auto* residueTypeIter = residueTypeAccess.begin();

        // Transfer atomic data to OVITO data structures.
        bool hasOccupancy = false;
        for(const gemmi::Chain& chain : model.chains) {
            for(const gemmi::Residue& residue : chain.residues) {
                if(isCanceled()) return;
                int residueTypeId = (residue.name.empty() == false) ? addNamedType(Particles::OOClass(), residueTypeProperty, QLatin1String(residue.name.c_str(), residue.name.size()))->numericId() : 0;
                for(const gemmi::Atom& atom : residue.atoms) {
                    // Atomic position.
                    *posIter++ = Point3(atom.pos.x, atom.pos.y, atom.pos.z);

                    // Chemical type.
                    *typeIter++ = atom.element.ordinal();
                    addNumericType(Particles::OOClass(), typeProperty, atom.element.ordinal(), QString::fromStdString(atom.element.name()));

                    // Atom name.
                    *atomNameIter++ = addNamedType(Particles::OOClass(), atomNameProperty, QLatin1String(atom.name.c_str(), atom.name.size()))->numericId();

                    // Residue type.
                    *residueTypeIter++ = residueTypeId;

                    // Check for presence of occupancy values.
                    if(atom.occ != 1) hasOccupancy = true;
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
            BufferWriteAccess<FloatType, access_mode::discard_write> occupancyProperty = particles()->createProperty(QStringLiteral("Occupancy"), Property::FloatDefault);
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
        // depend on the storage order of particles in the file We rather want a well-defined particle type ordering, that's
        // why we sort them now.
        typeProperty->sortElementTypesById();
        atomNameProperty->sortElementTypesByName();
        residueTypeProperty->sortElementTypesByName();

        // Parse unit cell.
        if(structure.cell.is_crystal()) {
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
        else if(posProperty.size() != 0) {
            // Use bounding box of atomic coordinates as non-periodic simulation cell.
            Box3 boundingBox;
            boundingBox.addPoints(posProperty);
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
        throw Exception(tr("mmCIF file reader error: %1").arg(e.what()));
    }

    // Generate ad-hoc bonds between atoms based on their van der Waals radii.
    if(_generateBonds)
        generateBonds();
    else
        setBondCount(0);

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
