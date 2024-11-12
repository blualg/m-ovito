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

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/objects/VoxelGridVis.h>
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include "ParaViewVTIGridImporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTIGridImporter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTIGridImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="ImageData">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("ImageData")) != 0)
        return false;

    // Continue reading until the expected <ImageData> element is reached.
    while(xml.readNextStartElement()) {
        if(xml.name().compare(QStringLiteral("ImageData")) == 0) {
            return !xml.hasError();
        }
    }

    return false;
}

/******************************************************************************
* Parses the input file.
******************************************************************************/
void ParaViewVTIGridImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading ParaView VTI ImageData file %1").arg(fileHandle().toString()));

    // Create the destination voxel grid.
    QString gridIdentifier = loadRequest().dataBlockPrefix;
    if(gridIdentifier.isEmpty())
        gridIdentifier = QStringLiteral("vtk-grid");
    VoxelGrid* gridObj = state().getMutableLeafObject<VoxelGrid>(VoxelGrid::OOClass(), gridIdentifier);
    if(!gridObj) {
        gridObj = state().createObject<VoxelGrid>(pipelineNode());
        gridObj->setIdentifier(gridIdentifier);
        VoxelGridVis* vis = gridObj->visElement<VoxelGridVis>();
        if(!gridIdentifier.isEmpty()) {
            gridObj->setTitle(QStringLiteral("%1: %2").arg(gridObj->objectTitle()).arg(gridIdentifier));
            gridObj->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(PropertyContainer::title)});
            if(vis) {
                vis->setTitle(QStringLiteral("%1: %2").arg(vis->objectTitle()).arg(gridIdentifier));
                vis->freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ActiveObject::title)});
            }
        }
    }

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTI file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    std::vector<PropertyPtr> cellDataArrays;
    int vtkHeaderType = 8; // Assume UInt64 by default
    std::optional<VoxelGrid::GridType> gridType;

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        this_task::throwIfCanceled();

        if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QStringLiteral("ImageData")) != 0)
                xml.raiseError(tr("VTI file is not of type ImageData."));
            else if(xml.attributes().value("byte_order").compare(QStringLiteral("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please ask the OVITO developers to extend the capabilities of the file parser."));
            else if(!xml.attributes().value("compressor").isEmpty())
                xml.raiseError(tr("Current implementation does not support compressed data arrays. Please ask the OVITO developers to extend the capabilities of the file parser."));
            if(xml.attributes().value("header_type").compare(QStringLiteral("UInt32")) == 0)
                vtkHeaderType = 4;
        }
        else if(xml.name().compare(QStringLiteral("ImageData")) == 0) {
            // Parse grid dimensions.
            auto tokens = splitString(xml.attributes().value("WholeExtent"));
            if(tokens.size() != 6) {
                xml.raiseError(tr("Expected 'WholeExtent' attribute (value list of length 6)."));
                break;
            }
            VoxelGrid::GridDimensions shape;
            shape[0] = tokens[1].toULongLong() - tokens[0].toULongLong();
            shape[1] = tokens[3].toULongLong() - tokens[2].toULongLong();
            shape[2] = tokens[5].toULongLong() - tokens[4].toULongLong();
            constexpr size_t maxGridSize = 100000;
            if(shape[0] == 0 || shape[0] > maxGridSize || shape[1] == 0 || shape[1] > maxGridSize || shape[2] == 0 || shape[2] > maxGridSize) {
                xml.raiseError(tr("'WholeExtent' attribute: Invalid grid dimensions."));
                break;
            }
            gridObj->setShape(shape);
            gridObj->setElementCount(shape[0] * shape[1] * shape[2]);

            // Parse simulation cell geometry.
            if(xml.attributes().hasAttribute("Spacing")) {
                // Parse grid spacings.
                auto tokens = splitString(xml.attributes().value("Spacing"));
                if(tokens.size() != 3) {
                    xml.raiseError(tr("Expected 'Spacing' attribute (value list of length 3)."));
                    break;
                }
                // Cell vectors are given by grid spacing size times the number of grid cells.
                AffineTransformation cellMatrix = AffineTransformation::Zero();
                cellMatrix(0, 0) = tokens[0].toDouble() * shape[0];
                cellMatrix(1, 1) = tokens[1].toDouble() * shape[1];
                cellMatrix(2, 2) = tokens[2].toDouble() * shape[2];
                // Parse origin coordinates of grid.
                if(xml.attributes().hasAttribute("Origin")) {
                    tokens = splitString(xml.attributes().value("Origin"));
                    if(tokens.size() != 3) {
                        xml.raiseError(tr("Invalid 'Origin' attribute (expected value list of length 3)."));
                        break;
                    }
                    cellMatrix(0, 3) = tokens[0].toDouble();
                    cellMatrix(1, 3) = tokens[1].toDouble();
                    cellMatrix(2, 3) = tokens[2].toDouble();
                }
                simulationCell()->setCellMatrix(cellMatrix);
                simulationCell()->setPbcFlags(false, false, false);
                gridObj->setDomain(simulationCell());
            }

            // Continue with parsing the child elements.
        }
        else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
            // Parse piece extents.
            // The current file parser implementation can only handle files with a single <Piece> element
            // spanning the entire grid extents.
            if(xml.attributes().hasAttribute("Extent")) {
                auto tokens = splitString(xml.attributes().value("Extent"));
                if(tokens.size() != 6) {
                    xml.raiseError(tr("Expected 'Extent' attribute (value list of length 6)."));
                    break;
                }
                VoxelGrid::GridDimensions shape;
                shape[0] = tokens[1].toULongLong() - tokens[0].toULongLong();
                shape[1] = tokens[3].toULongLong() - tokens[2].toULongLong();
                shape[2] = tokens[5].toULongLong() - tokens[4].toULongLong();
                if(shape != gridObj->shape()) {
                    xml.raiseError(tr("VTI file reader can only handle single-piece datasets. 'Extent' attribute must exactly match 'WholeExtent' of image data."));
                    break;
                }
            }

            // Continue with parsing child elements.
        }
        else if(xml.name().compare(QStringLiteral("CellData")) == 0 || xml.name().compare(QStringLiteral("PointData")) == 0) {
            VoxelGrid::GridType parsingType = (xml.name().compare(QStringLiteral("CellData")) == 0) ? VoxelGrid::GridType::CellData : VoxelGrid::GridType::PointData;
            // Parse <DataArray> child elements.
            while(xml.readNextStartElement() && !this_task::isCanceled()) {
                if(xml.name().compare(QStringLiteral("DataArray")) == 0) {
                    if(!gridType.has_value()) {
                        gridType = parsingType;
                        gridObj->setGridType(parsingType);

                        if(parsingType == VoxelGrid::GridType::PointData) {
                            VoxelGrid::GridDimensions shape = gridObj->shape();
                            shape[0]++;
                            shape[1]++;
                            shape[2]++;
                            gridObj->setShape(shape);
                            gridObj->setElementCount(shape[0] * shape[1] * shape[2]);
                        }
                    }
                    else if(gridType.value() != parsingType) {
                        xml.raiseError(tr("OVITO can only load VTK ImageData files that contain either point data or cell data, but not both."));
                        break;
                    }

                    int vectorComponent = -1;
                    if(Property* property = createGridPropertyForDataArray(gridObj, xml)) {
                        if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, vtkHeaderType, xml, vectorComponent))
                            break;
                    }
                    if(xml.tokenType() != QXmlStreamReader::EndElement)
                        xml.skipCurrentElement();
                }
                else {
                    xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
                    break;
                }
            }
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QStringLiteral("PointData")) == 0) {
            // Ignore contents of the <PointData> element.
            xml.skipCurrentElement();
        }
        else {
            xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("VTI file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }

    // Report grid dimensions to the user.
    state().setStatus(tr("Grid dimensions: %1 x %2 x %3")
        .arg(gridObj->shape()[0])
        .arg(gridObj->shape()[1])
        .arg(gridObj->shape()[2]));

    // Call base implementation.
    StandardFrameLoader::loadFile();
}

/******************************************************************************
* Creates the right kind of OVITO property object that will receive the data
* read from a <DataArray> element.
******************************************************************************/
Property* ParaViewVTIGridImporter::FrameLoader::createGridPropertyForDataArray(VoxelGrid* gridObj, QXmlStreamReader& xml)
{
    int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());
    auto name = xml.attributes().value("Name");

    return gridObj->createProperty(Property::makePropertyNameValid(name.toString()), Property::FloatDefault, numComponents);
}

}   // End of namespace
