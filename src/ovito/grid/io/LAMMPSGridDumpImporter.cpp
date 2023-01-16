////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/utilities/io/CompressedTextReader.h>
#include "LAMMPSGridDumpImporter.h"

namespace Ovito::Grid {

IMPLEMENT_OVITO_CLASS(LAMMPSGridDumpImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool LAMMPSGridDumpImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
	// Open input file.
	CompressedTextReader stream(file);

	// Read first line.
	stream.readLine(15);

	// Dump files written by LAMMPS start with one of the following keywords: TIMESTEP, UNITS or TIME.  
	if(!stream.lineStartsWith("ITEM: TIMESTEP") && !stream.lineStartsWith("ITEM: UNITS") && !stream.lineStartsWith("ITEM: TIME"))
		return false;

	// Continue reading until "ITEM: GRID SIZE" line is encountered.
	for(int i = 0; i < 20; i++) {
		if(stream.eof())
			return false;
		stream.readLine();
		if(stream.lineStartsWith("ITEM: GRID SIZE"))
			return true;
	}

	return false;
}

/******************************************************************************
* Scans the data file and builds a list of source frames.
******************************************************************************/
void LAMMPSGridDumpImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames)
{
	CompressedTextReader stream(fileHandle());
	setProgressText(tr("Scanning LAMMPS grid dump file %1").arg(fileHandle().toString()));
	setProgressMaximum(stream.underlyingSize());

	unsigned long long timestep = 0;
	size_t numVoxels = 0;
	Frame frame(fileHandle());

	while(!stream.eof() && !isCanceled()) {
		qint64 byteOffset = stream.byteOffset();
		int lineNumber = stream.lineNumber();

		// Parse next line.
		stream.readLine();

		do {
			if(stream.lineStartsWith("ITEM: TIMESTEP")) {
				if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				frame.byteOffset = byteOffset;
				frame.lineNumber = lineNumber;
				frame.label = QString("Timestep %1").arg(timestep);
				frames.push_back(frame);
				break;
			}
			else if(stream.lineStartsWithToken("ITEM: TIME")) {
				stream.readLine();
				stream.readLine();
			}
			else if(stream.lineStartsWith("ITEM: GRID SIZE")) {
				// Parse grid size.
				unsigned long long nx, ny, nz;
				if(sscanf(stream.readLine(), "%llu %llu %llu", &nx, &ny, &nz) != 3)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid grid size in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				if(nx > 100'000'000ll || ny > 100'000'000ll || nz > 100'000'000ll)
					throw Exception(tr("LAMMPS grid dump file parsing error. Number of grid cells in line %1 is too large. The file reader doesn't accept files with more than 100 million cells in each direction.").arg(stream.lineNumber()));
				numVoxels = (size_t)nx * (size_t)ny * (size_t)nz;
				break;
			}
			else if(stream.lineStartsWith("ITEM: GRID CELLS")) {
				for(size_t i = 0; i < numVoxels; i++) {
					stream.readLine();
					if(!setProgressValueIntermittent(stream.underlyingByteOffset()))
						return;
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
				throw Exception(tr("LAMMPS grid dump file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
			}
		}
		while(!stream.eof());
	}
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void LAMMPSGridDumpImporter::FrameLoader::loadFile()
{
	// Open file for reading.
	CompressedTextReader stream(fileHandle());
	setProgressText(tr("Reading LAMMPS grid dump file %1").arg(fileHandle().toString()));

	// Jump to byte offset.
	if(frame().byteOffset != 0)
		stream.seek(frame().byteOffset, frame().lineNumber);

	unsigned long long timestep;
	size_t numVoxels = 0;
	VoxelGrid::GridDimensions gridDims = {0, 0, 0};

	while(!stream.eof()) {

		// Parse next line.
		stream.readLine();

		do {
			if(stream.lineStartsWith("ITEM: TIMESTEP")) {
				if(sscanf(stream.readLine(), "%llu", &timestep) != 1)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid timestep number (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), dataSource());
				break;
			}
			else if(stream.lineStartsWithToken("ITEM: TIME")) {
				FloatType simulationTime;
				if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING, &simulationTime) != 1)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid time value (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				state().setAttribute(QStringLiteral("Time"), QVariant::fromValue(simulationTime), dataSource());
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
						throw Exception(tr("Invalid box size in line %1 of LAMMPS dump file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
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
			else if(stream.lineStartsWith("ITEM: BOX BOUNDS")) {
				// Parse optional boundary condition flags.
				QStringList tokens = FileImporter::splitString(stream.lineString().mid(qstrlen("ITEM: BOX BOUNDS")));
				if(tokens.size() >= 3)
					simulationCell()->setPbcFlags(tokens[0] == "pp", tokens[1] == "pp", tokens[2] == "pp");

				// Parse orthogonal simulation box size.
				Box3 simBox;
				for(int k = 0; k < 3; k++) {
					if(sscanf(stream.readLine(), FLOATTYPE_SCANF_STRING " " FLOATTYPE_SCANF_STRING, &simBox.minc[k], &simBox.maxc[k]) != 2)
						throw Exception(tr("Invalid box size in line %1 of dump file: %2").arg(stream.lineNumber()).arg(stream.lineString()));
				}

				simulationCell()->setCellMatrix(AffineTransformation(
						Vector3(simBox.sizeX(), 0, 0),
						Vector3(0, simBox.sizeY(), 0),
						Vector3(0, 0, simBox.sizeZ()),
						simBox.minc - Point3::Origin()));
				break;
			}
			else if(stream.lineStartsWithToken("ITEM: DIMENSION")) {
				int dimensionality;
				if(sscanf(stream.readLine(), "%i", &dimensionality) != 1)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid dimensionality (line %1):\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				if(dimensionality == 2)
					simulationCell()->setIs2D(true);
				else if(dimensionality == 3)
					simulationCell()->setIs2D(false);
				break;
			}
			else if(stream.lineStartsWith("ITEM: GRID SIZE")) {
				// Parse grid size.
				unsigned long long nx, ny, nz;
				if(sscanf(stream.readLine(), "%llu %llu %llu", &nx, &ny, &nz) != 3)
					throw Exception(tr("LAMMPS grid dump file parsing error. Invalid grid size in line %1:\n%2").arg(stream.lineNumber()).arg(stream.lineString()));
				numVoxels = (size_t)nx * (size_t)ny * (size_t)nz;
				gridDims = {nx, ny, nz};
				setProgressMaximum(numVoxels);
				break;
			}
			else if(stream.lineStartsWith("ITEM: GRID CELLS")) {

				// The unique identifier of the import voxel grid. 
				// This default identifier may be replaced below by a grid name found in the file. 
				QString gridIdentifier = QStringLiteral("imported");

				// Read the column names list.
				QStringList tokens = FileImporter::splitString(stream.lineString());
				OVITO_ASSERT(tokens[0] == "ITEM:" && tokens[1] == "GRID" && tokens[2] == "CELLS");
				QStringList fileColumnNames = tokens.mid(3);

				// Set up column-to-property mapping.
				VoxelInputColumnMapping columnMapping;
				columnMapping.resize(fileColumnNames.size());
				for(int i = 0; i < fileColumnNames.size(); i++) {
					QString propertyName = fileColumnNames[i];
					int vectorComponent = 0;
					int dataType = PropertyObject::Float;

					// Parse LAMMPScolumn name, which should have the form <fix/compute name>:<grid name>:<data field>.
					QStringList tokens = fileColumnNames[i].split(QChar(':'));
					if(tokens.size() == 3 && !tokens[0].isEmpty() && !tokens[2].isEmpty()) {
						// Use LAMMPS fix/compute name as property name.
						propertyName = tokens[0];

						// Extract vector component from 3rd field.
						if(tokens[2] == QStringLiteral("count")) {
							propertyName += QStringLiteral("_count");
							dataType = PropertyObject::Int64;
						}
						else if(tokens[2].startsWith(QStringLiteral("data[")) && tokens[2].endsWith(QChar(']'))) {
							unsigned int index = tokens[2].mid(5, tokens[2].size() - 6).toUInt();
							if(index > 0)
								vectorComponent = index - 1;
							else
								propertyName += QChar('_') + tokens[2];
						}
						else propertyName += QChar('_') + tokens[2];

						// Adopt LAMMPS grid name as OVITO VoxelGrid identifier.
						if(!tokens[1].isEmpty() && !tokens[1].contains(QChar('/')))
							gridIdentifier = tokens[1];
					}

					columnMapping.mapCustomColumn(i, propertyName, dataType, vectorComponent);
					columnMapping[i].columnName = fileColumnNames[i];
				}

				// Create the destination voxel grid.
				VoxelGrid* voxelGrid = state().getMutableLeafObject<VoxelGrid>(VoxelGrid::OOClass(), gridIdentifier);
				if(!voxelGrid) {
					voxelGrid = state().createObject<VoxelGrid>(dataSource());
					voxelGrid->setIdentifier(gridIdentifier);
				}
				voxelGrid->setShape(gridDims);
				voxelGrid->setElementCount(numVoxels);
				voxelGrid->setDomain(simulationCell());

				// Parse data columns.
				InputColumnReader columnParser(*this, columnMapping, voxelGrid);

				// If possible, use memory-mapped file access for best performance.
				const char* s_start;
				const char* s_end;
				std::tie(s_start, s_end) = stream.mmap();
				auto s = s_start;
				int lineNumber = stream.lineNumber() + 1;
				try {
					for(size_t i = 0; i < numVoxels; i++, lineNumber++) {
						if(!setProgressValueIntermittent(i)) return;
						if(!s)
							columnParser.readElement(i, stream.readLine());
						else
							s = columnParser.readElement(i, s, s_end);
					}
				}
				catch(Exception& ex) {
					throw ex.prependGeneralMessage(tr("Parsing error in line %1 of LAMMPS grid dump file.").arg(lineNumber));
				}
				if(s) {
					stream.munmap();
					stream.seek(stream.byteOffset() + (s - s_start));
				}
				columnParser.reset();

				// Detect if there are more simulation frames following in the file.
				if(!stream.eof()) {
					stream.readLine();
					if(stream.lineStartsWith("ITEM: TIMESTEP") || stream.lineStartsWith("ITEM: TIME"))
						signalAdditionalFrames();
				}

				state().setStatus(tr("%1 x %2 x %3 grid at timestep %4").arg(gridDims[0]).arg(gridDims[1]).arg(gridDims[2]).arg(timestep));

				// Call base implementation to finalize the loaded data.
				StandardFrameLoader::loadFile();

				return; // Done!
			}
			else if(stream.lineStartsWith("ITEM:")) {
				// For the sake of forward compatibility, we ignore unknown ITEM sections.
				// Skip lines until the next "ITEM:" is reached.
				while(!stream.eof() && !isCanceled()) {
					stream.readLine();
					if(stream.lineStartsWith("ITEM:"))
						break;
				}
			}
			else {
				throw Exception(tr("LAMMPS grid dump file parsing error. Line %1 of file %2 is invalid.").arg(stream.lineNumber()).arg(stream.filename()));
			}
		}
		while(!stream.eof());
	}

	throw Exception(tr("LAMMPS grid dump file parsing error. Unexpected end of file at line %1 or \"ITEM: GRID CELLS\" section is not present in dump file.").arg(stream.lineNumber()));
}

}	// End of namespace
