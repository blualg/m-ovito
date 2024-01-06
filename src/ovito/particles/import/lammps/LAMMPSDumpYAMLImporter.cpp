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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "LAMMPSDumpYAMLImporter.h"

// Qt defines the 'emit' macro. Its is in conflict with identifiers used in rapidyaml.
#ifdef emit
    #undef emit
#endif

#define RYML_SINGLE_HDR_DEFINE_NOW
#include <charconv>
#include <rapidyaml/rapidyaml-0.5.0.hpp>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSDumpYAMLImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSDumpYAMLImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Open input file.
    CompressedTextReader stream(file);

    // Read first line, which must contain "---".
    stream.readLine(8);
    if(!stream.lineStartsWithToken("---"))
        return false;

    // Read second line, which must contain "creator: LAMMPS".
    stream.readLine(16);
    if(!stream.lineStartsWith("creator: LAMMPS"))
        return false;

    return true;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void LAMMPSDumpYAMLImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
    CompressedTextReader stream(fileHandle());
    setProgressText(tr("Scanning LAMMPS dump yaml file %1").arg(fileHandle().toString()));
    setProgressMaximum(stream.underlyingSize());

    unsigned long long timestep = 0;
    size_t numElements = 0;
    Frame frame(fileHandle());

    while(!stream.eof() && !isCanceled()) {
        qint64 byteOffset = stream.byteOffset();
        int lineNumber = stream.lineNumber() + 1;

        // Parse next line, which must contain "---".
        stream.readLine();
        if(!stream.lineStartsWithToken("---"))
            break;

        while(!stream.eof()) {
            stream.readLine();
            if(stream.lineStartsWithToken("timestep:")) {
                if(sscanf(stream.line() + 9, "%llu", &timestep) != 1)
                    throw Exception(tr("LAMMPS dump yaml file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
                // Note: For first frame, always use byte offset/line number 0, because otherwise a reload of frame 0 is triggered by the FileSource.
                if(!frames.empty()) {
                    frame.byteOffset = byteOffset;
                    frame.lineNumber = lineNumber;
                }
                frame.label = QStringLiteral("Timestep %1").arg(timestep);
                frames.push_back(frame);
                stream.recordSeekPoint();
            }
            else if(stream.lineStartsWithToken("...")) {
                break;
            }
        }
    }
}

/******************************************************************************
* Helper class for parsing a YAML document.
******************************************************************************/
class YAMLParser
{
public:

    bool parseDocument(const FileHandle& fileHandle, const FileSourceImporter::Frame& frame) {
        // Open YAML file for reading.
        _stream.emplace(fileHandle, frame.byteOffset, frame.lineNumber);

        // Load file contents into memory buffer.
        // Parse first line, which must contain "---".
        _stream->readLine();
        if(!_stream->lineStartsWithToken("---"))
            throw Exception(LAMMPSDumpYAMLImporter::tr("LAMMPS dump yaml file parsing error in line %1: Expected string '---' on first document line but found: %2").arg(_stream->lineNumber()).arg(_stream->lineString()));

        _memoryBuffer.append(_stream->line());
        // Read lines until terminator "..." is found.
        while(!_stream->eof()) {
            _stream->readLine();
            _memoryBuffer.append(_stream->line());
            if(_stream->lineStartsWithToken("..."))
                break;
            if(this_task::isCanceled())
                return false;
        }

        // Set up YAML parser and error handling.
        auto on_error = [](const char* msg, size_t len, ryml::Location loc, void* user_data) {
            loc.line += *static_cast<int*>(user_data) - 1;
            Exception ex(LAMMPSDumpYAMLImporter::tr("LAMMPS dump yaml file - %1").arg(QString::fromUtf8(msg, len)));
            ex.appendDetailMessage(LAMMPSDumpYAMLImporter::tr("Location: line %1, column %2").arg(loc.line).arg(loc.col));
            throw ex;
        };
        const int* user_data = &frame.lineNumber;
        ryml::Parser parser(ryml::Callbacks(const_cast<int*>(user_data), nullptr, nullptr, on_error));

        // Parse from a mutable view of the memory buffer.
        _tree = parser.parse_in_place({}, ryml::substr(_memoryBuffer.data(), _memoryBuffer.size()));

        // Detect if another frame follows in the file.
        if(frame.byteOffset == 0 && !_stream->eof()) {
            _stream->readLine();
            if(_stream->lineStartsWithToken("---"))
                _hasAdditionalFrames = true;
        }

        _root = _tree.crootref();

        // Typically, the root node represents a stream of yaml documents.
        // We are more interested in a document's body, which is the first child of the root node.
        if(_root.is_stream() && !_root.empty())
            _root = _root.first_child();

        return true;
    }

    /// Access to the root node of the parsed YAML document.
    const ryml::ConstNodeRef& root() const { return _root; }

    /// Indicates whether the parsed YAML document contains additional trajectory frames.
    bool hasAdditionalFrames() const { return _hasAdditionalFrames; }

private:

    ryml::Tree _tree;
    ryml::ConstNodeRef _root;
    std::optional<CompressedTextReader> _stream;
    QByteArray _memoryBuffer;
    bool _hasAdditionalFrames = false;
};

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSDumpYAMLImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading LAMMPS dump yaml file %1").arg(fileHandle().toString()));

    // Parse YAML structure.
    YAMLParser parser;
    if(!parser.parseDocument(fileHandle(), frame()))
        return;

    // Detect if another frame follows in the file.
    if(parser.hasAdditionalFrames())
        signalAdditionalFrames();

    // Parse the timestep number.
    unsigned long long timestep;
    if(parser.root().get_if("timestep", &timestep))
        state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), pipelineNode());

    // Parse simulation time.
    double simulationTime;
    if(parser.root().get_if("time", &simulationTime))
        state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), pipelineNode());

    // Parse simulation unit style.
    ryml::csubstr simulationUnits;
    if(parser.root().get_if("units", &simulationUnits))
        state().setAttribute(QStringLiteral("Units"), QVariant::fromValue(QString::fromUtf8(simulationUnits.str, simulationUnits.len)), pipelineNode());

    // Parse number of atoms.
    unsigned long long natoms;
    if(!parser.root().get_if("natoms", &natoms))
        throw Exception(tr("LAMMPS dump file parsing error. Invalid or missing number of atoms."));
    setParticleCount(natoms);

    // Parse box dimensions.
    if(auto boxNode = parser.root().find_child("box"); boxNode.valid()) {
        Box3 simBox;
        int n = boxNode.num_children();
        if(n >= 3) {
            for(size_t k = 0; k < 3; k++) {
                if(!boxNode[k].is_seq() || boxNode[k].num_children() != 2)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid box dimensions."));
                boxNode[k][0] >> simBox.minc[k];
                boxNode[k][1] >> simBox.maxc[k];
            }
            FloatType tiltFactors[3];
            if(n >= 4) {
                if(!boxNode[3].is_seq() || boxNode[3].num_children() != 3)
                    throw Exception(tr("LAMMPS dump file parsing error. Invalid box tilt factors."));
                boxNode[3][0] >> tiltFactors[0];
                boxNode[3][1] >> tiltFactors[1];
                boxNode[3][2] >> tiltFactors[2];
            }
            else tiltFactors[0] = tiltFactors[1] = tiltFactors[2] = 0;

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
        }
    }

    // Parse boundary conditions.
    if(auto pbcNode = parser.root().find_child("boundary"); pbcNode.valid() && pbcNode.is_seq() && pbcNode.num_children() == 6) {
        bool pbc[3];
        for(size_t k = 0; k < 3; k++)
            pbc[k] = (pbcNode[k*2+0].val() == "p") && (pbcNode[k*2+1].val() == "p");
        simulationCell()->setPbcFlags(pbc[0], pbc[1], pbc[2]);
    }

    // Parse the column names.
    QStringList fileColumnNames;
    if(auto keywordsNode = parser.root().find_child("keywords"); keywordsNode.valid() && keywordsNode.is_seq()) {
        for(const auto& childNode : keywordsNode.children()) {
            fileColumnNames.push_back(QString::fromUtf8(childNode.val().str, childNode.val().len));
        }
    }

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

    // Look up the data section in the yaml file.
    auto dataNode = parser.root().find_child("data");
    if(!dataNode.valid() || !dataNode.is_seq())
        throw Exception(tr("LAMMPS dump yaml file parsing error. Missing 'data' section."));

    // Read the particle data.
    if(natoms != 0) {
        auto line_node = dataNode.begin();
        for(size_t i = 0; i < (size_t)natoms; i++) {
            if(!setProgressValueIntermittent(i))
                return;
            if(line_node == dataNode.end())
                throw Exception(tr("LAMMPS dump yaml file parsing error. Too few lines in 'data' section."));
            size_t col = 0;
            for(const auto& col_node : *line_node) {
                if(col == columnMapping.size())
                    break;
                const auto& token = col_node.val();
                columnParser.parseField(i, col, token.str, token.str + token.len);
                col++;
            }
            if(col < columnMapping.size())
                throw Exception(tr("LAMMPS dump yaml file parsing error. Not enough columns in 'data' section on line #%1 of 'data' section. Expected %2 columns.").arg(i+1).arg(columnMapping.size()));

            if(columnParser.readingTypeNamesFromSeparateColumns())
                columnParser.assignTypeNamesFromSeparateColumns();

            ++line_node;
        }
    }

    // Sort the particle type list since we created particles on the go and their order depends on the occurrence of types in the file.
    columnParser.sortElementTypes();
    columnParser.reset();

    // After parsing the particle data, post-processes the particle properties.
    postprocessParticleProperties(fileColumnNames, columnMapping);

    state().setStatus(tr("%1 particles at timestep %2").arg(natoms).arg(timestep));

    // Parse optional thermo record.
    if(auto thermoNode = parser.root().find_child("thermo"); thermoNode.valid()) {
        ryml::ConstNodeRef thermoKeywordsNode;
        ryml::ConstNodeRef thermoDataNode;
        for(const auto& childNode : thermoNode.children()) {
            if(childNode.is_map()) {
                if(auto keyNode = childNode.find_child("keywords"); keyNode.valid() && keyNode.is_seq()) {
                    thermoKeywordsNode = keyNode;
                }
                else if(auto dataNode = childNode.find_child("data"); dataNode.valid() && dataNode.is_seq()) {
                    thermoDataNode = dataNode;
                }
            }
        }
        if(thermoKeywordsNode.valid() && thermoKeywordsNode.is_seq() && thermoDataNode.valid() && thermoDataNode.is_seq()) {
            auto dataNode = thermoDataNode.begin();
            for(const auto& childNode : thermoKeywordsNode.children()) {
                if(dataNode == thermoDataNode.end())
                    break;
                QString key = QString::fromUtf8(childNode.val().str, childNode.val().len);
                QString value = QString::fromUtf8((*dataNode).val().str, (*dataNode).val().len);
                bool ok;
                double number = value.toDouble(&ok);
                state().setAttribute(std::move(key), ok ? QVariant::fromValue(number) : QVariant::fromValue(std::move(value)), pipelineNode());
                ++dataNode;
            }
        }
    }

    // Call base implementation to finalize the loaded data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Inspects the header of the given file and returns the number of file columns.
******************************************************************************/
Future<ParticleInputColumnMapping> LAMMPSDumpYAMLImporter::inspectFileHeader(const Frame& frame)
{
    // Retrieve file.
    return Application::instance()->fileManager().fetchUrl(frame.sourceFile)
        .then([](const FileHandle& fileHandle) {

            // Parse YAML structure.
            YAMLParser parser;
            if(!parser.parseDocument(fileHandle, FileSourceImporter::Frame()))
                return ParticleInputColumnMapping();

            // Parse the column names.
            QStringList fileColumnNames;
            if(auto keywordsNode = parser.root().find_child("keywords"); keywordsNode.valid() && keywordsNode.is_seq()) {
                for(const auto& childNode : keywordsNode.children()) {
                    fileColumnNames.push_back(QString::fromUtf8(childNode.val().str, childNode.val().len));
                }
            }

            return generateAutomaticColumnMapping(fileColumnNames);
        });
}

}   // End of namespace
