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

#include <ovito/grid/Grid.h>
#include <ovito/grid/objects/VoxelGrid.h>
#include <ovito/grid/objects/VoxelGridVis.h>
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include "ParaViewVTSGridImporter.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ParaViewVTSGridImporter);
IMPLEMENT_OVITO_CLASS(GridParaViewVTMFileFilter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTSGridImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="StructuredGrid">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QStringLiteral("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QStringLiteral("StructuredGrid")) != 0)
        return false;

    // Continue reading until the expected <StructuredGrid> element is reached.
    while(xml.readNextStartElement()) {
        if(xml.name().compare(QStringLiteral("StructuredGrid")) == 0) {
            return !xml.hasError();
        }
    }

    return false;
}

/******************************************************************************
* Parses the input file.
******************************************************************************/
void ParaViewVTSGridImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading ParaView VTS StructuredGrid file %1").arg(fileHandle().toString()));

    // Create the VoxelGrid object.
    QString gridIdentifier = loadRequest().dataBlockPrefix;
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
        throw Exception(tr("Failed to open VTS file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    std::vector<PropertyPtr> cellDataArrays;
    Box_3<qlonglong> wholeExtent;
    Box_3<qlonglong> pieceExtent;

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        if(isCanceled())
            return;

        if(xml.name().compare(QStringLiteral("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QStringLiteral("StructuredGrid")) != 0)
                xml.raiseError(tr("VTS file is not of type StructuredGrid."));
            else if(xml.attributes().value("byte_order").compare(QStringLiteral("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please ask the OVITO developers to extend the capabilities of the file parser."));
            else if(!xml.attributes().value("compressor").isEmpty())
                xml.raiseError(tr("Current implementation does not support compressed data arrays. Please ask the OVITO developers to extend the capabilities of the file parser."));
        }
        else if(xml.name().compare(QStringLiteral("StructuredGrid")) == 0) {
            // Parse grid dimensions.
            auto tokens = splitString(xml.attributes().value("WholeExtent"));
            if(tokens.size() != 6) {
                xml.raiseError(tr("Expected 'WholeExtent' attribute (list of length 6)."));
                break;
            }
            wholeExtent.minc.x() = tokens[0].toULongLong();
            wholeExtent.minc.y() = tokens[2].toULongLong();
            wholeExtent.minc.z() = tokens[4].toULongLong();
            wholeExtent.maxc.x() = tokens[1].toULongLong();
            wholeExtent.maxc.y() = tokens[3].toULongLong();
            wholeExtent.maxc.z() = tokens[5].toULongLong();
            VoxelGrid::GridDimensions shape;
            shape[0] = wholeExtent.size(0);
            shape[1] = wholeExtent.size(1);
            shape[2] = wholeExtent.size(2);
            constexpr size_t maxGridSize = std::numeric_limits<int>::max();
            if(shape[0] == 0 || shape[0] > maxGridSize || shape[1] == 0 || shape[1] > maxGridSize || shape[2] == 0 || shape[2] > maxGridSize) {
                xml.raiseError(tr("'WholeExtent' attribute: Invalid grid dimensions."));
                break;
            }
            gridObj->setShape(shape);
            gridObj->setElementCount(shape[0] * shape[1] * shape[2]);
            // Continue with parsing the child elements.
        }
        else if(xml.name().compare(QStringLiteral("Piece")) == 0) {
            // Parse piece extents.
            auto tokens = splitString(xml.attributes().value("Extent"));
            if(tokens.size() != 6) {
                xml.raiseError(tr("Expected 'Extent' attribute (list of length 6)."));
                break;
            }
            pieceExtent.minc.x() = tokens[0].toULongLong();
            pieceExtent.minc.y() = tokens[2].toULongLong();
            pieceExtent.minc.z() = tokens[4].toULongLong();
            pieceExtent.maxc.x() = tokens[1].toULongLong();
            pieceExtent.maxc.y() = tokens[3].toULongLong();
            pieceExtent.maxc.z() = tokens[5].toULongLong();
            for(size_t dim = 0; dim < 3; dim++) {
                if(pieceExtent.minc[dim] < wholeExtent.minc[dim] || pieceExtent.maxc[dim] > wholeExtent.maxc[dim]) {
                    xml.raiseError(tr("Piece extents exceed extents of whole structured grid."));
                    break;
                }
            }

            if(pieceExtent.minc != wholeExtent.minc || pieceExtent.maxc != wholeExtent.maxc) {
                xml.raiseError(tr("VTS file reader can only handle single-piece datasets. 'Extent' attribute must exactly match 'WholeExtent' of structured grid."));
                break;
            }

            // Continue with parsing child elements.
        }
        else if(xml.name().compare(QStringLiteral("CellData")) == 0) {
            // Parse <DataArray> child elements.
            while(xml.readNextStartElement() && !isCanceled()) {
                if(xml.name().compare(QStringLiteral("DataArray")) == 0) {

                    // Use the 'type' attribute to decide which data type to use for the OVITO property array.
                    QString dataTypeName = xml.attributes().value("type").toString();
                    int dataType = DataBuffer::FloatDefault;
                    if(dataTypeName == "Float32") {
                        dataType = DataBuffer::Float32;
                    }
                    else if(dataTypeName == "Float64") {
                        dataType = DataBuffer::Float64;
                    }
                    else if(dataTypeName == "Int32" || dataTypeName == "UInt32") {
                        dataType = DataBuffer::Int32;
                    }
                    else if(dataTypeName == "Int64" || dataTypeName == "UInt64") {
                        dataType = DataBuffer::Int64;
                    }

                    // Parse number of array components.
                    int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());

                    // Parse name of grid property.
                    auto name = xml.attributes().value("Name");

                    // Create voxel grid property that receives the values.
                    Property* property = gridObj->createProperty(Property::makePropertyNameValid(name.toString()), dataType, numComponents);

                    // Parse values from XML file.
                    if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, xml))
                        break;

                    if(xml.tokenType() != QXmlStreamReader::EndElement)
                        xml.skipCurrentElement();
                }
                else {
                    xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
                }
            }
        }
        else if(xml.name().compare(QStringLiteral("Points")) == 0) {
            // Parse child <DataArray> element containing the point coordinates.
            if(!xml.readNextStartElement())
                break;

            // Load the VTK point coordinates into a Nx3 buffer of floats.
            size_t numberOfPoints = (pieceExtent.size(0) + 1) * (pieceExtent.size(1) + 1) * (pieceExtent.size(2) + 1);
            DataBufferPtr buffer = DataBufferPtr::create(numberOfPoints, DataBuffer::FloatDefault, 3);
            if(!ParaViewVTPMeshImporter::parseVTKDataArray(buffer, xml))
                break;

            // Derive domain geometry from spacing between grid points.
            BufferReadAccess<Point3> points(buffer);
            AffineTransformation cellMatrix = AffineTransformation::Zero();
            cellMatrix.column(0) = (points[1] - points[0]) * (FloatType)wholeExtent.size(0);
            cellMatrix.column(1) = (points[pieceExtent.size(0) + 1] - points[0]) * (FloatType)wholeExtent.size(1);
            cellMatrix.column(2) = (points[(pieceExtent.size(0) + 1) * (pieceExtent.size(1) + 1)] - points[0]) * (FloatType)wholeExtent.size(2);
            cellMatrix.translation() = points[0] - Point3::Origin();
            simulationCell()->setCellMatrix(cellMatrix);
            simulationCell()->setPbcFlags(false, false, false);
            gridObj->setDomain(simulationCell());

            xml.skipCurrentElement();
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
        throw Exception(tr("VTS file parsing error on line %1, column %2: %3")
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
* Is called once before the datasets referenced in a multi-block VTM file will be loaded.
******************************************************************************/
void GridParaViewVTMFileFilter::preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets, FileSourceImporter::LoadOperationRequest& request, const ParaViewVTMImporter& vtmImporter)
{
    // Clear existing voxel grid objects by resizing them to zero elements.
    // This is mainly done to hide the grids in those animation frames in which the VTM file contains no corresponding data blocks.
    for(const DataObject* grid : request.state.getObjects(VoxelGrid::OOClass())) {
        VoxelGrid* mutableGrid = static_object_cast<VoxelGrid>(request.state.mutableData()->makeMutable(grid));
        mutableGrid->setElementCount(0);
        mutableGrid->setShape({0,0,0});
    }
}

}   // End of namespace
