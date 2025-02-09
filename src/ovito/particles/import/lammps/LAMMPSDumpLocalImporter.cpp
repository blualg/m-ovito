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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include <ovito/core/utilities/io/FileManager.h>
#include "LAMMPSDumpLocalImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSDumpLocalImporter);
OVITO_CLASSINFO(LAMMPSDumpLocalImporter, "DisplayName", "LAMMPS Dump Local");
DEFINE_PROPERTY_FIELD(LAMMPSDumpLocalImporter, columnMapping);
SET_PROPERTY_FIELD_LABEL(LAMMPSDumpLocalImporter, columnMapping, "File column mapping");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSDumpLocalImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first line.
    stream.readLine(15);

    // Dump files written by LAMMPS start with one of the following keywords: TIMESTEP, UNITS or TIME.
    if(!stream.lineStartsWith("ITEM: TIMESTEP") && !stream.lineStartsWith("ITEM: UNITS") && !stream.lineStartsWith("ITEM: TIME"))
        return false;

    // Continue reading until "ITEM: NUMBER OF <XXX>" line is encountered.
    // Note: <XXX> may NOT be "ATOMS", because that is handled by LAMMPSTextDumpImporter.
    for(int i = 0; i < 20; i++) {
        if(stream.eof())
            return false;
        stream.readLine();
        if(stream.lineStartsWith("ITEM: NUMBER OF ")) {
            return !stream.lineStartsWithToken("ITEM: NUMBER OF ATOMS");
        }
    }

    return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void LAMMPSDumpLocalImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    using namespace std::string_literals;

    CompressedTextReader stream(fileHandle);

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning LAMMPS dump local file %1").arg(fileHandle.toString()));
    progress.setMaximum(stream.underlyingSize());

    unsigned long long timestep = 0;
    size_t numElements = 0;
    Frame frame(fileHandle);
    std::string itemsSectionName;

    while(!stream.eof() && !this_task::isCanceled()) {
        qint64 byteOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber();

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
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
            else if(stream.lineStartsWith("ITEM: NUMBER OF ")) {
                // Extract the "ITEM: <XXX>" section name (default is "ITEM: ENTRIES").
                // Note: The LAMMPS user may have customized the ITEM section name using the "dump_modify label" command, so we cannot rely on a fixed name.
                itemsSectionName = "ITEM: "s + (stream.line() + std::size("ITEM: NUMBER OF ") - 1);
                itemsSectionName.erase(itemsSectionName.find_last_not_of(" \n\r\t") + 1);

                // Parse number of entries.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid number of entries in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                if(u > 100'000'000'000ll)
                    throw Exception(tr("LAMMPS dump local file parsing error. Number of entries in line %1 is too large. The LAMMPS dump local file reader doesn't accept files with more than 100 entries.").arg(stream.lineNumber()));
                numElements = (size_t)u;
                break;
            }
            else if(!itemsSectionName.empty() && stream.lineStartsWithToken(itemsSectionName.c_str())) {
                for(size_t i = 0; i < numElements; i++) {
                    stream.readLine();
                    // Update progress bar and check for user cancellation.
                    progress.setValueIntermittent(stream.underlyingByteOffset());
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
                throw Exception(tr("LAMMPS dump local file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSDumpLocalImporter::FrameLoader::loadFile()
{
    using namespace std::string_literals;

    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading LAMMPS dump local file %1").arg(fileHandle().toString()));

    // Open file for reading.
    CompressedTextReader stream(fileHandle(), frame().byteOffset, frame().lineNumber);

    unsigned long long timestep;
    size_t numElements = 0;
    std::string itemsSectionName;

    while(!stream.eof()) {

        // Parse next line.
        stream.readLine();

        do {
            if(stream.lineStartsWith("ITEM: TIMESTEP")) {
                if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), pipelineNode());
                break;
            }
            else if(stream.lineStartsWithToken("ITEM: TIME")) {
                FloatType simulationTime;
                if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING, &simulationTime) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid time value (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), pipelineNode());
                break;
            }
            else if(stream.lineStartsWith("ITEM: NUMBER OF ")) {
                // Extract the "ITEM: <XXX>" section name (default is "ITEM: ENTRIES").
                // Note: The LAMMPS user may have customized the ITEM section name using the "dump_modify label" command, so we cannot rely on a fixed name.
                std::string itemLabel(stream.line() + std::size("ITEM: NUMBER OF ") - 1);
                itemLabel.erase(itemLabel.find_last_not_of(" \n\r\t") + 1);
                itemsSectionName = "ITEM: "s + itemLabel;

                // Parse number of entries.
                unsigned long long u;
                if(sscanf(stream.readLine(), "%llu", &u) != 1)
                    throw Exception(tr("LAMMPS dump local file parsing error. Invalid number of entries in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));

                numElements = (size_t)u;
                progress.setMaximum(u);

                // Convert itemLabel to lower case.
                std::transform(itemLabel.begin(), itemLabel.end(), itemLabel.begin(), ::tolower);

                // Determine interaction type the file contains.
                if(itemLabel == "bonds" || itemLabel == "entries")
                    _columnMapping.convertToContainerClass(&Bonds::OOClass());
                else if(itemLabel == "angles")
                    _columnMapping.convertToContainerClass(&Angles::OOClass());
                else if(itemLabel == "dihedrals")
                    _columnMapping.convertToContainerClass(&Dihedrals::OOClass());
                else if(itemLabel == "impropers")
                    _columnMapping.convertToContainerClass(&Impropers::OOClass());
                else
                    _columnMapping.convertToContainerClass(&DataTable::OOClass());

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
                        throw Exception(tr("Invalid box size in line %1 of LAMMPS dump local file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
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
                        throw Exception(tr("Invalid box size in line %1 of LAMMPS dump local file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
                }

                simulationCell()->setCellMatrix(AffineTransformation(
                        Vector3(simBox.sizeX(), 0, 0),
                        Vector3(0, simBox.sizeY(), 0),
                        Vector3(0, 0, simBox.sizeZ()),
                        simBox.minc - Point3::Origin()));
                break;
            }
            else if(!itemsSectionName.empty() && stream.lineStartsWithToken(itemsSectionName.c_str())) {

                // Get the target property container.
                PropertyContainer* container = nullptr;
                if(columnMapping().containerClass() == &Bonds::OOClass())
                    container = bonds();
                else if(columnMapping().containerClass() == &Angles::OOClass())
                    container = angles();
                else if(columnMapping().containerClass() == &Dihedrals::OOClass())
                    container = dihedrals();
                else if(columnMapping().containerClass() == &Impropers::OOClass())
                    container = impropers();
                else if(columnMapping().containerClass() == &DataTable::OOClass()) {
                    container = state().getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("imported"));
                    if(!container)
                        container = state().createObject<DataTable>(pipelineNode(), DataTable::PlotMode::None, QStringLiteral("imported"));
                }

                // Allocate the loaded number of elements.
                if(container)
                    container->setElementCount(numElements);

                // Parse data columns.
                InputColumnReader columnParser(*this, columnMapping(), container);

                // If possible, use memory-mapped file access for best performance.
                const char* s_start;
                const char* s_end;
                std::tie(s_start, s_end) = stream.mmap();
                auto s = s_start;
                int lineNumber = stream.lineNumber() + 1;
                try {
                    for(size_t i = 0; i < numElements; i++, lineNumber++) {
                        // Update progress bar and check for user cancellation.
                        progress.setValueIntermittent(i);
                        if(!s)
                            columnParser.readElement(i, stream.readLine());
                        else
                            s = columnParser.readElement(i, s, s_end);
                    }
                }
                catch(Exception& ex) {
                    throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS dump local file.").arg(lineNumber));
                }
                if(s) {
                    stream.munmap();
                    stream.seek(stream.byteOffset() + (s - s_start));
                }

                // Sort the element types since we created them on the go while parsing the file. Otherwise their order be dependent on the first occurrence of element types in the file.
                columnParser.sortElementTypes();
                columnParser.reset();

                // If the bond "Topology" property was loaded, we need to shift particle indices by 1, because LAMMPS
                // uses 1-based atom IDs and OVITO uses 0-based indices.
                if(columnMapping().containerClass() == &Bonds::OOClass()) {
                    if(BufferWriteAccess<ParticleIndexPair, access_mode::read_write> topologyProperty = bonds()->getMutableProperty(Bonds::TopologyProperty)) {
                        for(ParticleIndexPair& ab : topologyProperty) {
                            ab[0] -= 1;
                            ab[1] -= 1;
                        }
                    }
                }
                else if(columnMapping().containerClass() == &Angles::OOClass()) {
                    if(BufferWriteAccess<ParticleIndexTriplet, access_mode::read_write> topologyProperty = angles()->getMutableProperty(Angles::TopologyProperty)) {
                        for(ParticleIndexTriplet& abc : topologyProperty) {
                            abc[0] -= 1;
                            abc[1] -= 1;
                            abc[2] -= 1;
                        }
                    }
                }
                else if(columnMapping().containerClass() == &Dihedrals::OOClass()) {
                    if(BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> topologyProperty = dihedrals()->getMutableProperty(Dihedrals::TopologyProperty)) {
                        for(ParticleIndexQuadruplet& abcd : topologyProperty) {
                            abcd[0] -= 1;
                            abcd[1] -= 1;
                            abcd[2] -= 1;
                            abcd[3] -= 1;
                        }
                    }
                }
                else if(columnMapping().containerClass() == &Impropers::OOClass()) {
                    if(BufferWriteAccess<ParticleIndexQuadruplet, access_mode::read_write> topologyProperty = impropers()->getMutableProperty(Impropers::TopologyProperty)) {
                        for(ParticleIndexQuadruplet& abcd : topologyProperty) {
                            abcd[0] -= 1;
                            abcd[1] -= 1;
                            abcd[2] -= 1;
                            abcd[3] -= 1;
                        }
                    }
                }

                // Detect if there are more simulation frames following in the file (only when reading the first frame).
                if(frame().byteOffset == 0 && !stream.eof()) {
                    stream.readLine();
                    if(stream.lineStartsWith("ITEM: TIMESTEP") || stream.lineStartsWith("ITEM: TIME"))
                        signalAdditionalFrames();
                }

                state().setStatus(tr("%1 %2 at timestep %3").arg(numElements).arg(columnMapping().containerClass()->elementDescriptionName()).arg(timestep));

                // Hide particles, because this importer loads non-particle data.
                if(Particles* particles = state().getMutableObject<Particles>())
                    particles->setVisElement(nullptr);

                // Call base implementation to finalize the loaded data.
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
                throw Exception(tr("LAMMPS dump local file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
            }
        }
        while(!stream.eof());
    }

    // This point is only reached if no data section was encountered.
    if(itemsSectionName.empty())
        itemsSectionName = "ITEM: ENTRIES";
    throw Exception(tr("LAMMPS dump local file parsing error. Unexpected end of file at line %1 or \"%2\" section is not present in dump file.").arg(stream.lineNumber()).arg(QString::fromStdString(itemsSectionName)));
}

/******************************************************************************
 * Guesses the mapping of input file columns to OVITO properties.
 *****************************************************************************/
InputColumnMapping LAMMPSDumpLocalImporter::generateAutomaticColumnMapping(PropertyContainerClassPtr containerClass, const QStringList& columnNames)
{
    InputColumnMapping columnMapping(containerClass);
    columnMapping.resize(columnNames.size());
    for(int i = 0; i < columnNames.size(); i++) {
        columnMapping[i].columnName = columnNames[i];
        QString name = columnNames[i].toLower();

        // Bonds
        if(containerClass == &Bonds::OOClass() && name == "btype") columnMapping.mapColumnToStandardProperty(i, Bonds::TypeProperty);
        else if(containerClass == &Bonds::OOClass() && name == "batom1") columnMapping.mapColumnToStandardProperty(i, Bonds::ParticleIdentifiersProperty, 0);
        else if(containerClass == &Bonds::OOClass() && name == "batom2") columnMapping.mapColumnToStandardProperty(i, Bonds::ParticleIdentifiersProperty, 1);
        else if(containerClass == &Bonds::OOClass() && name == "dist") columnMapping.mapColumnToStandardProperty(i, Bonds::LengthProperty);
        // Angles
        else if(containerClass == &Angles::OOClass() && name == "atype") columnMapping.mapColumnToStandardProperty(i, Angles::TypeProperty);
        else if(containerClass == &Angles::OOClass() && name == "aatom1") columnMapping.mapColumnToStandardProperty(i, Angles::ParticleIdentifiersProperty, 0);
        else if(containerClass == &Angles::OOClass() && name == "aatom2") columnMapping.mapColumnToStandardProperty(i, Angles::ParticleIdentifiersProperty, 1);
        else if(containerClass == &Angles::OOClass() && name == "aatom3") columnMapping.mapColumnToStandardProperty(i, Angles::ParticleIdentifiersProperty, 2);
        else if(containerClass == &Angles::OOClass() && name == "theta") columnMapping.mapColumnToUserProperty(i, QStringLiteral("Theta"), Property::FloatDefault);
        // Dihedrals
        else if(containerClass == &Dihedrals::OOClass() && name == "dtype") columnMapping.mapColumnToStandardProperty(i, Dihedrals::TypeProperty);
        else if(containerClass == &Dihedrals::OOClass() && name == "datom1") columnMapping.mapColumnToStandardProperty(i, Dihedrals::ParticleIdentifiersProperty, 0);
        else if(containerClass == &Dihedrals::OOClass() && name == "datom2") columnMapping.mapColumnToStandardProperty(i, Dihedrals::ParticleIdentifiersProperty, 1);
        else if(containerClass == &Dihedrals::OOClass() && name == "datom3") columnMapping.mapColumnToStandardProperty(i, Dihedrals::ParticleIdentifiersProperty, 2);
        else if(containerClass == &Dihedrals::OOClass() && name == "datom4") columnMapping.mapColumnToStandardProperty(i, Dihedrals::ParticleIdentifiersProperty, 3);
        else if(containerClass == &Dihedrals::OOClass() && name == "phi") columnMapping.mapColumnToUserProperty(i, QStringLiteral("Phi"), Property::FloatDefault);
        // Impropers
        else if(containerClass == &Impropers::OOClass() && name == "itype") columnMapping.mapColumnToStandardProperty(i, Impropers::TypeProperty);
        else if(containerClass == &Impropers::OOClass() && name == "iatom1") columnMapping.mapColumnToStandardProperty(i, Impropers::ParticleIdentifiersProperty, 0);
        else if(containerClass == &Impropers::OOClass() && name == "iatom2") columnMapping.mapColumnToStandardProperty(i, Impropers::ParticleIdentifiersProperty, 1);
        else if(containerClass == &Impropers::OOClass() && name == "iatom3") columnMapping.mapColumnToStandardProperty(i, Impropers::ParticleIdentifiersProperty, 2);
        else if(containerClass == &Impropers::OOClass() && name == "iatom4") columnMapping.mapColumnToStandardProperty(i, Impropers::ParticleIdentifiersProperty, 3);
        else if(containerClass == &Impropers::OOClass() && name == "chi") columnMapping.mapColumnToUserProperty(i, QStringLiteral("Chi"), Property::FloatDefault);
        // Automatically map columns to standard OVITO properties.
        else {
            bool isStandardProperty = false;
            const static QRegularExpression invalidCharacters(QStringLiteral("[^A-Za-z\\d_]"));
            for(auto entry = containerClass->standardPropertyIds().cbegin(), end = containerClass->standardPropertyIds().cend(); entry != end; ++entry) {
                const auto componentCount = containerClass->standardPropertyComponentCount(entry->second);
                for(size_t component = 0; component < componentCount; component++) {
                    QString propertyName = entry->first;
                    propertyName.remove(invalidCharacters); // LAMMPS dump file format does not support column names containing spaces.
                    const QStringList& componentNames = containerClass->standardPropertyComponentNames(entry->second);
                    QString propertyName2;
                    if(!componentNames.empty()) {
                        OVITO_ASSERT(!componentNames[component].contains(invalidCharacters));
                        propertyName2 = propertyName + componentNames[component];
                        propertyName += QChar('.');
                        propertyName += componentNames[component];
                    }
                    if(propertyName.compare(name, Qt::CaseInsensitive) == 0 || propertyName2.compare(name, Qt::CaseInsensitive) == 0) {
                        columnMapping.mapColumnToStandardProperty(i, entry->second, component);
                        isStandardProperty = true;
                        break;
                    }
                }
                if(isStandardProperty)
                    break;
            }
            // If automatic mapping to one of the standard properties was unsuccessful, read the file column as a user-defined property.
            if(!isStandardProperty && containerClass == &DataTable::OOClass())
                columnMapping.mapColumnToUserProperty(i, Property::makePropertyNameValid(columnNames[i]), Property::FloatDefault);
        }
    }
    return columnMapping;
}

/******************************************************************************
* Inspects the header of the given file and returns the list of file columns.
******************************************************************************/
Future<InputColumnMapping> LAMMPSDumpLocalImporter::inspectFileHeader(const Frame& frame)
{
    using namespace std::string_literals;

    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) {

            // Start parsing the file up to the specification of the file columns.
            CompressedTextReader stream(fileHandle);
            std::string itemsSectionName;

            InputColumnMapping detectedColumnMapping;
            while(!stream.eof()) {
                // Parse next line.
                stream.readLine();

                if(stream.lineStartsWith("ITEM: NUMBER OF ")) {
                    // Extract the "ITEM: <XXX>" section name (LAMMPS default is "ITEM: ENTRIES").
                    // Note: The LAMMPS user may have customized the ITEM section name using the "dump_modify label" command,
                    // that's why we cannot rely on a fixed name here.
                    std::string itemLabel(stream.line() + std::size("ITEM: NUMBER OF ") - 1);
                    itemLabel.erase(itemLabel.find_last_not_of(" \n\r\t") + 1);
                    itemsSectionName = "ITEM: "s + itemLabel;

                    // Convert itemLabel to lower case.
                    std::transform(itemLabel.begin(), itemLabel.end(), itemLabel.begin(), ::tolower);

                    if(itemLabel == "bonds" || itemLabel == "entries")
                        detectedColumnMapping = InputColumnMapping(&Bonds::OOClass());
                    else if(itemLabel == "angles")
                        detectedColumnMapping = InputColumnMapping(&Angles::OOClass());
                    else if(itemLabel == "dihedrals")
                        detectedColumnMapping = InputColumnMapping(&Dihedrals::OOClass());
                    else if(itemLabel == "impropers")
                        detectedColumnMapping = InputColumnMapping(&Impropers::OOClass());
                    else
                        detectedColumnMapping = InputColumnMapping(&DataTable::OOClass());
                }
                else if(!itemsSectionName.empty() && stream.lineStartsWithToken(itemsSectionName.c_str())) {
                    // Read the column names list.
                    QStringList tokens = FileImporter::splitString(stream.lineString());
                    OVITO_ASSERT(tokens[0] == "ITEM:");
                    QStringList fileColumnNames = tokens.mid(2);

                    // Read also the first data line to capture file excerpt.
                    QString fileExcerpt = stream.lineString();
                    stream.readLine();
                    fileExcerpt += stream.lineString();

                    if(fileColumnNames.isEmpty()) {
                        // If no file columns names are available, count at least the number of columns in the first data line.
                        int columnCount = FileImporter::splitString(stream.lineString()).size();
                        detectedColumnMapping.resize(columnCount);
                    }
                    else {
                        detectedColumnMapping = generateAutomaticColumnMapping(detectedColumnMapping.containerClass(), fileColumnNames);
                    }

                    // Read first few lines of the data and add them to the file excerpt.
                    for(size_t i = 0; i < 3 && !stream.eof(); i++) {
                        stream.readLine();
                        fileExcerpt += stream.lineString();
                    }
                    if(!stream.eof())
                        fileExcerpt += QStringLiteral("...\n");
                    detectedColumnMapping.setFileExcerpt(std::move(fileExcerpt));
                    break;
                }
            }
            return detectedColumnMapping;
        });
}

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void LAMMPSDumpLocalImporter::propertyChanged(const PropertyFieldDescriptor* field)
{
    ParticleImporter::propertyChanged(field);

    if(field == PROPERTY_FIELD(columnMapping) && !isBeingLoaded()) {
        requestReload();
    }
}

}   // End of namespace
