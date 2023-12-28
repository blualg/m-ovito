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

#include <ovito/stdobj/StdObj.h>
#include "DataTableExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTableExporter);

/******************************************************************************
 * This is called once for every output file to be written and before
 * exportData() is called.
 *****************************************************************************/
void DataTableExporter::openOutputFile(const QString& filePath, int numberOfFrames)
{
    OVITO_ASSERT(!_outputFile.isOpen());
    OVITO_ASSERT(!_outputStream);

    _outputFile.setFileName(filePath);
    _outputStream = std::make_unique<CompressedTextWriter>(_outputFile);
}

/******************************************************************************
 * This is called once for every output file written after exportData()
 * has been called.
 *****************************************************************************/
void DataTableExporter::closeOutputFile(bool exportCompleted)
{
    _outputStream.reset();
    if(_outputFile.isOpen())
        _outputFile.close();

    if(!exportCompleted)
        _outputFile.remove();
}

/******************************************************************************
 * Exports a single animation frame to the current output file.
 *****************************************************************************/
void DataTableExporter::exportFrame(int frameNumber, const QString& filePath)
{
    // Evaluate pipeline.
    const PipelineFlowState& state = getPipelineDataToBeExported(frameNumber);
    if(!state)
        return;

    // Look up the DataTable to be exported in the pipeline state.
    DataObjectReference objectRef(&DataTable::OOClass(), dataObjectToExport().dataPath());
    const DataTable* table = static_object_cast<DataTable>(state.getLeafObject(objectRef));
    if(!table) {
        throw Exception(tr("The pipeline output does not contain the data table to be exported (animation frame: %1; object key: %2). Available data tables: (%3)")
            .arg(frameNumber).arg(objectRef.dataPath()).arg(getAvailableDataObjectList(state, DataTable::OOClass())));
    }
    table->verifyIntegrity();

    // Make sure the X property exists in the property container.
    // If not, create a temporary property for export.

    ConstPropertyPtr xstorage = table->getXValues();
    ConstPropertyPtr ystorage = table->y();
    if(table->properties().empty())
        throw Exception(tr("Data table to be exported contains no valid data columns."));

    size_t row_count = table->elementCount();
    int xDataType = xstorage ? xstorage->dataType() : 0;

    BufferReadAccess<int8_t*> xaccessInt8(xDataType == Property::Int8 ? xstorage : nullptr);
    BufferReadAccess<int32_t*> xaccessInt32(xDataType == Property::Int32 ? xstorage : nullptr);
    BufferReadAccess<int64_t*> xaccessInt64(xDataType == Property::Int64 ? xstorage : nullptr);
    BufferReadAccess<float*> xaccessFloat32(xDataType == Property::Float32 ? xstorage : nullptr);
    BufferReadAccess<double*> xaccessFloat64(xDataType == Property::Float64 ? xstorage : nullptr);

    if(!table->title().isEmpty())
        textStream() << "# " << table->title() << " (" << (quint64)row_count << " data points):\n";
    textStream() << "# ";
    auto formatColumnName = [](const QString& name) {
        return name.contains(QChar(' ')) ? (QChar('"') + name + QChar('"')) : name;
    };
    if(!xstorage)
        textStream() << formatColumnName(table->axisLabelX());
    else
        textStream() << formatColumnName(xstorage->name());

    if(ystorage) {
        if(ystorage->componentNames().size() == ystorage->componentCount()) {
            for(size_t col = 0; col < ystorage->componentCount(); col++) {
                textStream() << " " << formatColumnName(ystorage->componentNames()[col]);
            }
        }
        else {
            textStream() << " " << formatColumnName(!table->axisLabelY().isEmpty() ? table->axisLabelY() : ystorage->name());
        }
    }

    // Collect the extra properties that should be written to the file.
    std::vector<RawBufferReadAccess> outputProperties;
    if(ystorage)
        outputProperties.emplace_back(ystorage);
    for(const Property* property : table->properties()) {
        if(property == table->x() || property == table->y())
            continue;
        outputProperties.emplace_back(property);
        if(property->componentNames().size() == property->componentCount()) {
            for(size_t col = 0; col < property->componentCount(); col++) {
                textStream() << " " << formatColumnName(QStringLiteral("%1.%2").arg(property->name()).arg(property->componentNames()[col]));
            }
        }
        else {
            textStream() << " " << formatColumnName(property->name());
        }
    }

    textStream() << "\n";

    for(size_t row = 0; row < row_count; row++) {
        // Write the X column.
        if(table->plotMode() == DataTable::BarChart) {
            const ElementType* type = ystorage ? ystorage->elementType(row) : nullptr;
            if(!type && xstorage)
                type = xstorage->elementType(row);
            if(type) {
                textStream() << formatColumnName(type->name()) << " ";
            }
            else continue;
        }
        else {
            if(xaccessInt8)
                textStream() << static_cast<qint32>(xaccessInt8.get(row, 0)) << " ";
            else if(xaccessInt32)
                textStream() << static_cast<qint32>(xaccessInt32.get(row, 0)) << " ";
            else if(xaccessInt64)
                textStream() << static_cast<qint64>(xaccessInt64.get(row, 0)) << " ";
            else if(xaccessFloat32)
                textStream() << xaccessFloat32.get(row, 0) << " ";
            else if(xaccessFloat64)
                textStream() << xaccessFloat64.get(row, 0) << " ";
            else
                textStream() << "<?> ";
        }
        // Write the data column(s).
        for(const auto& array : outputProperties) {
            for(size_t col = 0; col < array.componentCount(); col++) {
                if(array.dataType() == Property::Int8)
                    textStream() << static_cast<qint32>(*reinterpret_cast<const int8_t*>(array.cdata(row, col))) << " ";
                else if(array.dataType() == Property::Int32)
                    textStream() << static_cast<qint32>(*reinterpret_cast<const int32_t*>(array.cdata(row, col))) << " ";
                else if(array.dataType() == Property::Int64)
                    textStream() << static_cast<qint64>(*reinterpret_cast<const int64_t*>(array.cdata(row, col))) << " ";
                else if(array.dataType() == Property::Float32)
                    textStream() << *reinterpret_cast<const float*>(array.cdata(row, col)) << " ";
                else if(array.dataType() == Property::Float64)
                    textStream() << *reinterpret_cast<const double*>(array.cdata(row, col)) << " ";
                else
                    textStream() << "<?> ";
            }
        }
        textStream() << "\n";
    }
}

}   // End of namespace
