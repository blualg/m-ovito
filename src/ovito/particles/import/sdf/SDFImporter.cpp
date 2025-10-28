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
//  Contributions:
//
//  Support for extended XYZ format has been added by James Kermode,
//  Department of Physics, King's College London.
//
///////////////////////////////////////////////////////////////////////////////

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "SDFImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SDFImporter);
OVITO_CLASSINFO(SDFImporter, "DisplayName", "SDF/MDL");

/******************************************************************************
 * Checks if the given file has format that can be read by this importer.
 ******************************************************************************/
bool SDFImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Parse the first couple of lines only.
    for(int i = 0; i < 4 && !stream.eof(); i++) {
        const char* line = stream.readLineTrimLeft(128);
        if(!line || stream.line()[0] == '\0') {
            continue;
        }

        // Read the counts line: "6 5 0 0 1 0 3 V2000" to idenfity the file format
        if(i == 3) {
            // Read the V2000 / V3000 token
            char tag[6];

            if(sscanf(line, "%*d %*d %*d %*d %*d %*d %*d %*d %*d %5s %*n", tag) == 1) {
                return strncmp(tag, "V2000", 5) == 0 || strncmp(tag, "V3000", 5) == 0;
            }
            else {
                break;
            }
        }
    }
    return false;
}

/******************************************************************************
 * Scans the data file and builds a list of source frames.
 ******************************************************************************/
void SDFImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    CompressedTextReader stream(fileHandle);

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning file %1").arg(fileHandle.toString()));
    progress.setMaximum(stream.underlyingSize());

    int frameNumber = 0;
    Frame frame(fileHandle);

    while(!stream.eof() && !this_task::isCanceled()) {
        // Note: For first frame, always use byte offset/line number 0, because otherwise a reload of frame 0 is triggered by the
        // FileSource.
        if(!frames.empty()) {
            frame.byteOffset = stream.byteOffset();
            frame.lineNumber = stream.lineNumber();
        }
        stream.recordSeekPoint();

        const char* line = stream.readLineTrimLeft();
        while(!stream.eof() && !this_task::isCanceled() && std::strncmp(line, "$$$$", 4) != 0) {
            line = stream.readLineTrimLeft();
        }

        // Create a new record for the time step.
        frame.label.setFrameOfFile(frameNumber++);
        frames.push_back(frame);
    }
}

namespace {
int32_t remapCharge(int32_t charge)
{
    constexpr static std::array<int32_t, 7> chargeRemap = {{0, 3, 2, 1, -1, -2, -3}};
    if(charge >= 0 && charge < chargeRemap.size()) {
        return chargeRemap[charge];
    }
    return std::numeric_limits<int32_t>::max();
}
}  // namespace

/******************************************************************************
 * Parses the given input file.
 * File format description adopted from Wikipedia: https://en.wikipedia.org/wiki/Chemical_table_file
 * More details at: https://www.nonlinear.com/progenesis/sdf-studio/v0.9/faq/sdf-file-format-guidance.aspx
 * and https://herongyang.com/Molecule/SDF-Format-Specification.html
 ******************************************************************************/
void SDFImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading MOL/SDF file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    // Parse 3 lines of comments
    // 1. Title
    stream.readLine();
    state().setAttribute(QStringLiteral("%1.Title").arg(OOClass().displayName()), QVariant::fromValue(stream.lineString().trimmed()), pipelineNode());
    // 2. Program / file timestamp line
    stream.readLine();
    state().setAttribute(QStringLiteral("%1.Program").arg(OOClass().displayName()), QVariant::fromValue(stream.lineString().trimmed()), pipelineNode());
    // 3. Comment line
    stream.readLine();
    state().setAttribute(QStringLiteral("%1.Comment").arg(OOClass().displayName()), QVariant::fromValue(stream.lineString().trimmed()), pipelineNode());

    // Parse number of lines
    char tag[128];
    qlonglong numParticles;
    qlonglong numBonds;
    const char* line = stream.readLine();
    if(sscanf(line, "%llu %llu %*d %*d %*d %*d %*d %*d %*d %5s%*n", &numParticles, &numBonds, tag) != 3) {
        throw Exception(tr("Invalid number of particles and bonds in line %1 of MOL/SDF file: %2")
                            .arg(stream.lineNumber())
                            .arg(stream.lineString().trimmed()));
    }
    if(strncmp(tag, "V3000", 5) == 0) {
        throw Exception(tr("Unsupported MOL/SDF version 'V3000' in line %1 of MOL/SDF file: %2")
                            .arg(stream.lineNumber())
                            .arg(stream.lineString().trimmed()));
    }

    // Store whether the "charge" column exists.
    bool chargeExists = particles()->getProperty(Particles::ChargeProperty) != nullptr;

    // Store whether the "mass difference" column exists.
    bool massDifferenceExists = particles()->getProperty(QStringLiteral("Mass difference")) != nullptr;

    //------------------------------------------------------------------------------
    // Parse Particles.
    //------------------------------------------------------------------------------

    // Prepare the file column to particle property mapping.
    ParticleInputColumnMapping particlesColumnMapping;
    particlesColumnMapping.resize(6);
    particlesColumnMapping.mapColumnToStandardProperty(0, Particles::PositionProperty, 0);
    particlesColumnMapping.mapColumnToStandardProperty(1, Particles::PositionProperty, 1);
    particlesColumnMapping.mapColumnToStandardProperty(2, Particles::PositionProperty, 2);
    particlesColumnMapping.mapColumnToStandardProperty(3, Particles::TypeProperty);
    particlesColumnMapping.mapColumnToStandardProperty(4, Particles::ChargeProperty);
    particlesColumnMapping.mapColumnToUserProperty(5, tr("Mass difference"), Property::Int32);

    setParticleCount(numParticles);

    InputColumnReader particlesColumnParser(*this, particlesColumnMapping, particles());
    progress.setMaximum(numParticles);
    try {
        for(qlonglong i = 0; i < numParticles; i++) {
            // Update progress bar and check for user cancellation.
            progress.setValueIntermittent(i);
            particlesColumnParser.readElement(i, stream.readLine());
        }
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Parsing error in line %1 of MOL/SDF file.").arg(stream.lineNumber()));
    }
    particlesColumnParser.reset();
    state().setStatus(tr("%1 particles").arg(numParticles));

    //------------------------------------------------------------------------------
    // Parse bonds.
    //------------------------------------------------------------------------------

    BondInputColumnMapping bondColumnMapping;
    bondColumnMapping.resize(7);
    bondColumnMapping.mapColumnToStandardProperty(0, Bonds::TopologyProperty, 0);
    bondColumnMapping.mapColumnToStandardProperty(1, Bonds::TopologyProperty, 1);
    bondColumnMapping.mapColumnToStandardProperty(2, Bonds::TypeProperty, 2);
    bondColumnMapping.mapColumnToUserProperty(3, tr("Bond stereo"), Property::Int32);
    bondColumnMapping.mapColumnToUserProperty(4, tr("Bond stereo"), Property::Int8);
    bondColumnMapping.mapColumnToUserProperty(6, tr("Bond configuration"), Property::Int8);

    setBondCount(numBonds);
    InputColumnReader bondsColumnParser(*this, bondColumnMapping, bonds());
    progress.setMaximum(numBonds);
    try {
        for(qlonglong i = 0; i < numBonds; i++) {
            // Update progress bar and check for user cancellation.
            progress.setValueIntermittent(i);
            bondsColumnParser.readElement(i, stream.readLine());
        }
    }
    catch(Exception& ex) {
        throw ex.prependGeneralMessage(tr("Parsing error in line %1 of MOL/SDF file.").arg(stream.lineNumber()));
    }
    bondsColumnParser.reset();
    state().combineStatus(tr("%1 bonds").arg(numBonds));

    // Remap indices from 1 based to 0 based.
    for(ParticleIndexPair& ab :
        BufferWriteAccess<ParticleIndexPair, access_mode::read_write>(bonds()->getMutableProperty(Bonds::TopologyProperty))) {
        ab[0] -= 1;
        ab[1] -= 1;
    }

    //------------------------------------------------------------------------------
    // Parse Properties.
    //------------------------------------------------------------------------------
    Property* chargeProp = particles()->getMutableProperty(Particles::ChargeProperty);
    BufferWriteAccess<FloatType, access_mode::write> chargeAcc(chargeProp);
    Property* massDifferenceProp = particles()->getMutableProperty(tr("Mass difference"));
    BufferWriteAccess<int32_t, access_mode::write> massDifferenceAcc(massDifferenceProp);

    line = stream.readLineTrimLeft();
    while(strncmp(line, "M  END", 6) != 0) {
        int itemCount;
        int consumed = 0;
        int offset = 0;
        if(sscanf(line + offset, "M  %3s %d %n", tag, &itemCount, &consumed) != 2) {
            throw Exception(tr("Invalid MOL/SDF file: Invalid M line in line %1: %2").arg(stream.lineNumber()).arg(line));
        }

        int atomIdx;
        int newValue;
        for(int i = 0; i < itemCount; i++) {
            offset += consumed;
            if(sscanf(line + offset, "%d %d %n", &atomIdx, &newValue, &consumed) != 2) {
                throw Exception(tr("Invalid MOL/SDF file: Invalid M line in line %1: %2").arg(stream.lineNumber()).arg(line));
            }

            if(strncmp(tag, "CHG", 3) == 0) {
                chargeAcc[atomIdx - 1] = (FloatType)newValue;
                chargeExists = true;
            }
            else if(strncmp(tag, "ISO", 3) == 0) {
                massDifferenceAcc[atomIdx - 1] = newValue;
                massDifferenceExists = true;
            }
            else {
                throw Exception(tr("Invalid MOL/SDF file: Unknown tag in line %1: %2").arg(stream.lineNumber()).arg(line));
            }
        }
        line = stream.readLineTrimLeft();
    };
    chargeAcc.reset();
    massDifferenceAcc.reset();

    if(!chargeExists && chargeProp->nonzeroCount() == 0) {
        particles()->removeProperty(chargeProp);
    }
    if(!massDifferenceExists && massDifferenceProp->nonzeroCount() == 0) {
        particles()->removeProperty(massDifferenceProp);
    }

    //------------------------------------------------------------------------------
    // Parse Associated data items.
    //------------------------------------------------------------------------------
    line = stream.readLineTrimLeft();
    QStringList associatedData;
    while(strncmp(line, "$$$$", 4) != 0) {
        if(sscanf(line, "> <%127s", tag) == 1) {
            line = stream.readLineTrimLeft();
            associatedData.clear();
            while(line[0] != '\0' && strncmp(line, "$$$$", 4) != 0 && strncmp(line, "> <", 3) != 0) {
                associatedData << line;
                line = stream.readLineTrimLeft();
            }
            state().setAttribute(QStringLiteral("%1.<%2").arg(OOClass().displayName()).arg(tag),
                                 QVariant::fromValue(associatedData.join(" ").trimmed()), pipelineNode());
        }
        line = stream.readLineTrimLeft();
    }

    // Generate cell (bounding box) if requested
    generateBoundingBox();

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();

    // Detect if there are more simulation frames following in the file.
    if(!stream.eof()) {
        signalAdditionalFrames();
    }
}

}  // namespace Ovito
