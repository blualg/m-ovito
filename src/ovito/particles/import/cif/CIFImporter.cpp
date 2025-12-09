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
#include "CIFImporter.h"

#include <gemmi/cif.hpp>
#include <gemmi/smcif.hpp>  // for reading small molecules

namespace Ovito {

namespace cif = gemmi::cif;

IMPLEMENT_CREATABLE_OVITO_CLASS(CIFImporter);
OVITO_CLASSINFO(CIFImporter, "DisplayName", "CIF");

/******************************************************************************
 * Checks if the given file has format that can be read by this importer.
 ******************************************************************************/
bool CIFImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
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
        else if(stream.lineStartsWith("data_", true)) {
            // Make sure the "data_XXX" block header appears.
            if(foundBlockHeader) return false;
            foundBlockHeader = true;
        }
        else if(stream.lineStartsWith("_", true)) {
            // Make sure at least one "_XXX" item appears.
            foundItem = true;
            break;
        }
    }

    // Make sure it is a CIF file.
    if(!foundBlockHeader || !foundItem) return false;

    // Continue reading the entire file until at least one "_atom_site_XXX" entry is found.
    // These entries are specific to the CIF format and do not occur in mmCIF files (macromolecular files).
    for(;;) {
        if(stream.lineStartsWith("_atom_site_", true) && !stream.lineContains(".")) return true;
        if(stream.eof()) return false;
        stream.readLine();
    }

    return false;
}

/******************************************************************************
 * Parses the given input file.
 ******************************************************************************/
void CIFImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading CIF file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Map the whole file into memory for parsing.
    const char* buffer_start;
    const char* buffer_end;
    QByteArray fileContents;
    std::tie(buffer_start, buffer_end) = stream.mmap();
    if(!buffer_start) {
        // Could not map CIF file into memory. Read it into a in-memory buffer instead.
        fileContents = stream.readAll();
        buffer_start = fileContents.constData();
        buffer_end = buffer_start + fileContents.size();
    }

    try {
        // Parse the CIF file's contents.
        cif::Document doc = cif::read_memory(buffer_start, buffer_end - buffer_start, qPrintable(frame().sourceFile.path()));

        // Unmap the input file from memory.
        if(fileContents.isEmpty()) stream.munmap();
        this_task::throwIfCanceled();

        // Parse the CIF data into an atomic structure representation.
        const cif::Block& block = doc.sole_block();
        gemmi::SmallStructure structure = gemmi::make_small_structure_from_block(block);
        this_task::throwIfCanceled();

        // Parse list of atomic sites.
        std::vector<gemmi::SmallStructure::Site> sites = structure.get_all_unit_cell_sites();
        setParticleCount(sites.size());
        BufferWriteAccess<Point3, access_mode::discard_write> posAcc = particles()->createProperty(Particles::PositionProperty);
        Property* typeProperty = particles()->createProperty(Particles::TypeProperty);
        BufferWriteAccess<int32_t, access_mode::discard_write> typeAcc(typeProperty);
        Property* labelProperty = particles()->createProperty(QStringLiteral("Label"), Property::Int32);
        BufferWriteAccess<int32_t, access_mode::discard_write> labelAcc(labelProperty);
        bool hadExistingCharges = (particles()->getProperty(Particles::ChargeProperty) != nullptr);
        Property* chargeProperty = particles()->createProperty(Particles::ChargeProperty);
        BufferWriteAccess<FloatType, access_mode::discard_write> chargeAcc(chargeProperty);
        bool hasNonzeroCharges = false;
        std::vector<int> partialSiteIndices;
        int siteIndex = 0;
        for(const gemmi::SmallStructure::Site& site : sites) {
            gemmi::Position pos = structure.cell.orthogonalize(site.fract.wrap_to_unit());
            posAcc[siteIndex] = Point3(static_cast<FloatType>(pos.x), static_cast<FloatType>(pos.y), static_cast<FloatType>(pos.z));
            typeAcc[siteIndex] =
                addNumericType(Particles::OOClass(), typeProperty, site.element.ordinal(), site.element.name())->numericId();
            chargeAcc[siteIndex] = static_cast<FloatType>(site.charge);
            if(site.charge != 0) hasNonzeroCharges = true;
            labelAcc[siteIndex] = addNamedType(Particles::OOClass(), labelProperty, site.label)->numericId();
            ;
            if(site.occ != 1) partialSiteIndices.push_back(siteIndex);
            ++siteIndex;
            partialSiteIndices.push_back(siteIndex);
            ++siteIndex;
        }
        this_task::throwIfCanceled();

        posAcc.reset();
        typeAcc.reset();
        chargeAcc.reset();
        labelAcc.reset();

        if(!hasNonzeroCharges && !hadExistingCharges) {
            // Remove charge property if all charges are zero and no previous charge property existed.
            particles()->removeProperty(chargeProperty);
        }

        // Since we've created particle types on the go while reading the particles, the type ordering
        // depends on the storage order of particles in the file. We rather want a well-defined particle type ordering, that's
        // why we sort them now.
        typeProperty->sortElementTypesByName();

        // Parse the optional site occupancy information.
        if(!partialSiteIndices.empty() && !typeProperty->elementTypes().empty()) {
            // Identify sites with partial occupancy that share the same coordinates.
            std::sort(partialSiteIndices.begin(), partialSiteIndices.end(), [&](int a, int b) {
                const gemmi::SmallStructure::Site& siteA = sites[a];
                const gemmi::SmallStructure::Site& siteB = sites[b];
                if(siteA.fract.x != siteB.fract.x) return siteA.fract.x < siteB.fract.x;
                if(siteA.fract.y != siteB.fract.y) return siteA.fract.y < siteB.fract.y;
                return siteA.fract.z < siteB.fract.z;
            });

            // Create "Occupancy" property with one component per element type.
            QStringList componentsNames;
            for(const ElementType* type : typeProperty->elementTypes()) {
                componentsNames.append(type->name());
            }
#if 0
            BufferWriteAccess<FloatType*, access_mode::discard_write> occupancyProperty = particles()->createProperty(
                DataBuffer::BufferInitialization::Uninitialized,
                QStringLiteral("Occupancy"),
                Property::FloatDefault,
                componentsNames.size(),
                std::move(componentsNames));

            //FloatType* occupancyIter = occupancyProperty.begin();
            //for(const gemmi::SmallStructure::Site& site : sites) {
            //    *occupancyIter++ = site.occ;
            //}
#endif
        }
        else {
            if(const Property* occProp = particles()->getProperty(QStringLiteral("Occupancy"))) particles()->removeProperty(occProp);
        }

        // Parse unit cell.
        if(structure.cell.is_crystal()) {
            // Process periodic unit cell definition.
            AffineTransformation cell = AffineTransformation::Identity();
            if(structure.cell.alpha == 90 && structure.cell.beta == 90 && structure.cell.gamma == 90) {
                cell(0, 0) = structure.cell.a;
                cell(1, 1) = structure.cell.b;
                cell(2, 2) = structure.cell.c;
            }
            else if(structure.cell.alpha == 90 && structure.cell.beta == 90) {
                FloatType gamma = qDegreesToRadians(structure.cell.gamma);
                cell(0, 0) = structure.cell.a;
                cell(0, 1) = structure.cell.b * std::cos(gamma);
                cell(1, 1) = structure.cell.b * std::sin(gamma);
                cell(2, 2) = structure.cell.c;
            }
            else {
                FloatType alpha = qDegreesToRadians(structure.cell.alpha);
                FloatType beta = qDegreesToRadians(structure.cell.beta);
                FloatType gamma = qDegreesToRadians(structure.cell.gamma);
                FloatType v = structure.cell.a * structure.cell.b * structure.cell.c *
                              sqrt(1.0 - std::cos(alpha) * std::cos(alpha) - std::cos(beta) * std::cos(beta) -
                                   std::cos(gamma) * std::cos(gamma) + 2.0 * std::cos(alpha) * std::cos(beta) * std::cos(gamma));
                cell(0, 0) = structure.cell.a;
                cell(0, 1) = structure.cell.b * std::cos(gamma);
                cell(1, 1) = structure.cell.b * std::sin(gamma);
                cell(0, 2) = structure.cell.c * std::cos(beta);
                cell(1, 2) = structure.cell.c * (std::cos(alpha) - std::cos(beta) * std::cos(gamma)) / std::sin(gamma);
                cell(2, 2) = v / (structure.cell.a * structure.cell.b * std::sin(gamma));
            }
            simulationCell()->setCellMatrix(cell);
        }
        else {
            // Use bounding box of atomic coordinates as non-periodic simulation cell.
            generateBoundingBox();
        }

        state().setStatus(tr("Number of atoms: %1").arg(particles()->elementCount()));
    }
    catch(const Exception&) {
        throw;
    }
    catch(const std::exception& e) {
        throw Exception(tr("CIF file reader: %1").arg(e.what()));
    }

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}  // namespace Ovito
