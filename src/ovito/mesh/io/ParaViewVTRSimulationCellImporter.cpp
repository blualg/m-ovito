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

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/simcell/SimulationCellVis.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "ParaViewVTRSimulationCellImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTRSimulationCellImporter);
OVITO_CLASSINFO(ParaViewVTRSimulationCellImporter, "DisplayName", "VTR");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTRSimulationCellImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="RectilinearGrid">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("RectilinearGrid")) != 0)
        return false;

    return !xml.hasError();
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void ParaViewVTRSimulationCellImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Reading ParaView VTR RectilinearGrid file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTR file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // The simulation cell matrix.
    AffineTransformation cellMatrix = AffineTransformation::Zero();

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        this_task::throwIfCanceled();

        if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QStringLiteral("RectilinearGrid")) != 0)
                xml.raiseError(tr("VTK file is not of type RectilinearGrid."));
            else if(xml.attributes().value("byte_order").compare(QStringLiteral("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please contact the OVITO developers to request an extension of the file parser."));
        }
        else if(xml.name().compare(QStringLiteral("RectilinearGrid")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QStringLiteral("Coordinates")) == 0) {
            // Parse three <DataArray> elements.
            for(size_t dim = 0; dim < 3; dim++) {
                if(!xml.readNextStartElement())
                    break;
                if(xml.name().compare(QStringLiteral("DataArray")) == 0) {
                    FloatType rangeMin = xml.attributes().value("RangeMin").toDouble();
                    FloatType rangeMax = xml.attributes().value("RangeMax").toDouble();
                    cellMatrix(dim, dim) = rangeMax - rangeMin;
                    cellMatrix(dim, 3) = rangeMin;
                    xml.skipCurrentElement();
                }
                else {
                    xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
                }
            }
            break;
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QStringLiteral("PointData")) == 0 || xml.name().compare(QStringLiteral("CellData")) == 0 || xml.name().compare(QStringLiteral("DataArray")) == 0) {
            // Do nothing. Ignore element contents.
            xml.skipCurrentElement();
        }
        else {
            xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("VTR file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }

    simulationCell()->setCellMatrix(cellMatrix);
    simulationCell()->setPbcFlags(false, false, false);

    // Call base implementation.
    StandardFrameLoader::loadFile();
}

}   // End of namespace
