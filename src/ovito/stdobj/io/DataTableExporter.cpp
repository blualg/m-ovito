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

namespace Ovito::StdObj {

IMPLEMENT_OVITO_CLASS(DataTableExporter);

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
bool DataTableExporter::exportFrame(int frameNumber, const QString& filePath, MainThreadOperation& operation)
{
    // Evaluate pipeline.
    const PipelineFlowState& state = getPipelineDataToBeExported(frameNumber);
    if(!state)
        return false;

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

    operation.setProgressText(tr("Writing file %1").arg(filePath));

    ConstPropertyPtr xstorage = table->getXValues();
    ConstPropertyPtr ystorage = table->y();
    const PropertyObject* xprop = table->x();
    const PropertyObject* yprop = table->y();
    if(!ystorage || !yprop)
        throw Exception(tr("Data table to be exported contains no valid data columns."));

    size_t row_count = table->elementCount();
    int xDataType = xstorage ? xstorage->dataType() : 0;
    int yDataType = ystorage->dataType();

    ConstBufferAccess<int8_t, true> xaccessInt8(xDataType == PropertyObject::Int8 ? xstorage : nullptr);
    ConstBufferAccess<int32_t, true> xaccessInt32(xDataType == PropertyObject::Int32 ? xstorage : nullptr);
    ConstBufferAccess<int64_t, true> xaccessInt64(xDataType == PropertyObject::Int64 ? xstorage : nullptr);
    ConstBufferAccess<float, true> xaccessFloat32(xDataType == PropertyObject::Float32 ? xstorage : nullptr);
    ConstBufferAccess<double, true> xaccessFloat64(xDataType == PropertyObject::Float64 ? xstorage : nullptr);

    if(!table->title().isEmpty())
        textStream() << "# " << table->title() << " (" << (quint64)row_count << " data points):\n";
    textStream() << "# ";
    auto formatColumnName = [](const QString& name) {
        return name.contains(QChar(' ')) ? (QChar('"') + name + QChar('"')) : name;
    };
    textStream() << formatColumnName((!xprop || !table->axisLabelX().isEmpty()) ? table->axisLabelX() : xprop->name());

    if(ystorage->componentNames().size() == ystorage->componentCount()) {
        for(size_t col = 0; col < ystorage->componentCount(); col++) {
            textStream() << " " << formatColumnName(ystorage->componentNames()[col]);
        }
    }
    else {
        textStream() << " " << formatColumnName(!table->axisLabelY().isEmpty() ? table->axisLabelY() : ystorage->name());
    }

    // Collect the extra properties that should be written to the file.
    std::vector<ConstBufferAccess<void,true>> outputProperties;
    outputProperties.emplace_back(ystorage);
    for(const PropertyObject* propObj : table->properties()) {
        if(propObj == table->x()) continue;
        if(propObj == table->y()) continue;
        outputProperties.emplace_back(propObj);
        if(propObj->componentNames().size() == propObj->componentCount()) {
            for(size_t col = 0; col < propObj->componentCount(); col++) {
                textStream() << " " << formatColumnName(QStringLiteral("%1.%2").arg(propObj->name()).arg(propObj->componentNames()[col]));
            }
        }
        else {
            textStream() << " " << formatColumnName(propObj->name());
        }
    }

    textStream() << "\n";

    for(size_t row = 0; row < row_count; row++) {
        // Write the X column.
        if(table->plotMode() == DataTable::BarChart) {
            const ElementType* type = yprop->elementType(row);
            if(!type && xprop) type = xprop->elementType(row);
            if(type) {
                textStream() << formatColumnName(type->name()) << " ";
            }
            else continue;
        }
        else {
            if(xaccessInt8)
                textStream() << xaccessInt8.get(row, 0) << " ";
            else if(xaccessInt32)
                textStream() << xaccessInt32.get(row, 0) << " ";
            else if(xaccessInt64)
                textStream() << xaccessInt64.get(row, 0) << " ";
            else if(xaccessFloat32)
                textStream() << xaccessFloat32.get(row, 0) << " ";
            else if(xaccessFloat64)
                textStream() << xaccessFloat64.get(row, 0) << " ";
            else
                textStream() << "<?> ";
        }
        // Write the Y column(s).
        for(const ConstBufferAccess<void,true>& array : outputProperties) {
            for(size_t col = 0; col < array.componentCount(); col++) {
                if(array.dataType() == PropertyObject::Int8)
                    textStream() << *reinterpret_cast<const int8_t*>(array.cdata(row, col)) << " ";
                else if(array.dataType() == PropertyObject::Int32)
                    textStream() << *reinterpret_cast<const int32_t*>(array.cdata(row, col)) << " ";
                else if(array.dataType() == PropertyObject::Int64)
                    textStream() << *reinterpret_cast<const int64_t*>(array.cdata(row, col)) << " ";
                else if(array.dataType() == PropertyObject::Float32)
                    textStream() << *reinterpret_cast<const float*>(array.cdata(row, col)) << " ";
                else if(array.dataType() == PropertyObject::Float64)
                    textStream() << *reinterpret_cast<const double*>(array.cdata(row, col)) << " ";
                else
                    textStream() << "<?> ";
            }
        }
        textStream() << "\n";
    }

    return !operation.isCanceled();
}

}   // End of namespace
