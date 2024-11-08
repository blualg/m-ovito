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
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "ParaViewVTUSimulationCellImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTUSimulationCellImporter);
OVITO_CLASSINFO(ParaViewVTUSimulationCellImporter, "DisplayName", "VTU");

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTUSimulationCellImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="UnstructuredGrid">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("UnstructuredGrid")) != 0)
        return false;

    return !xml.hasError();
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void ParaViewVTUSimulationCellImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setProgressText(tr("Reading ParaView VTU UnstructuredGrid file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTU file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    size_t numberOfPoints = 0;
    int vtkHeaderType = 8; // Assume UInt64 by default

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        this_task::throwIfCanceled();

        if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QStringLiteral("UnstructuredGrid")) != 0)
                xml.raiseError(tr("VTU file is not of type UnstructuredGrid."));
            else if(xml.attributes().value("byte_order").compare(QStringLiteral("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please contact the OVITO developers to request an extension of the file parser."));
            if(xml.attributes().value("header_type").compare(QStringLiteral("UInt32")) == 0)
                vtkHeaderType = 4;
        }
        else if(xml.name().compare(QStringLiteral("UnstructuredGrid")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
            // Parse number of points.
            numberOfPoints = xml.attributes().value("NumberOfPoints").toULongLong();

            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QStringLiteral("Points")) == 0) {
            // Parse child <DataArray> element containing the point coordinates.
            if(!xml.readNextStartElement())
                break;

            // Load the VTK data array into a Nx3 buffer of floats.
            DataBufferPtr buffer = DataBufferPtr::create(numberOfPoints, DataBuffer::FloatDefault, 3);
            if(!ParaViewVTPMeshImporter::parseVTKDataArray(buffer, vtkHeaderType, xml))
                break;

            // Compute bounding box of points.
            Box3 bbox;
            bbox.addPoints(BufferReadAccess<Point3>(buffer));

            // Set up simulation cell matrix.
            AffineTransformation cellMatrix = AffineTransformation::Zero();
            cellMatrix(0, 0) = bbox.size(0);
            cellMatrix(1, 1) = bbox.size(1);
            cellMatrix(2, 2) = bbox.size(2);
            cellMatrix.translation() = bbox.minc - Point3::Origin();
            simulationCell()->setCellMatrix(cellMatrix);
            simulationCell()->setPbcFlags(false, false, false);

            xml.skipCurrentElement();
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QStringLiteral("PointData")) == 0 || xml.name().compare(QStringLiteral("CellData")) == 0 || xml.name().compare(QStringLiteral("Cells")) == 0 || xml.name().compare(QStringLiteral("DataArray")) == 0) {
            // Do nothing. Ignore element contents.
            xml.skipCurrentElement();
        }
        else {
            xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("VTU file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }

    // Call base implementation.
    StandardFrameLoader::loadFile();
}

}   // End of namespace
