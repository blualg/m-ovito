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

#include <ovito/stdobj/StdObj.h>
#include "DataTableExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTableExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> DataTableExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(any_moveonly&& frameData, int frameNumber, const QString& filePath, TaskProgress& progress) override {
            // The exportable frame data.
            const PipelineFlowState state = any_cast<PipelineFlowState>(std::move(frameData));

            // Perform the following in a worker thread.
            co_await ExecutorAwaiter(ThreadPoolExecutor());

            DataObjectReference objectRef(&DataTable::OOClass(), dataObjectToExport().dataPath());
            const DataObject* containerObject = state.getLeafObject(dataObjectToExport());

            // Look up the DataTable to be exported in the pipeline state.
            // Data tables have some special export settings.
            const DataTable* table = dynamic_object_cast<const DataTable>(containerObject);
            if(table) {
                table->verifyIntegrity();
            }
            const PropertyContainer* container = dynamic_object_cast<const PropertyContainer>(containerObject);
            if(!container) {
                throw Exception(
                    tr("The pipeline output does not contain the property container to be exported (animation frame: %1; object key: %2)")
                        .arg(frameNumber)
                        .arg(objectRef.dataPath()));
            }
            container->verifyIntegrity();

            // Make sure the X property exists in the property container.
            // If not, create a temporary property for export.
            ConstPropertyPtr xstorage = table ? table->getXValues() : nullptr;
            ConstPropertyPtr ystorage = table ? table->y() : nullptr;
            if(container->properties().empty()) throw Exception(tr("Data table to be exported contains no valid data columns."));

            size_t row_count = container->elementCount();
            int xDataType = xstorage ? xstorage->dataType() : 0;

            BufferReadAccess<int8_t*> xaccessInt8(xDataType == Property::Int8 ? xstorage : nullptr);
            BufferReadAccess<int32_t*> xaccessInt32(xDataType == Property::Int32 ? xstorage : nullptr);
            BufferReadAccess<int64_t*> xaccessInt64(xDataType == Property::Int64 ? xstorage : nullptr);
            BufferReadAccess<float*> xaccessFloat32(xDataType == Property::Float32 ? xstorage : nullptr);
            BufferReadAccess<double*> xaccessFloat64(xDataType == Property::Float64 ? xstorage : nullptr);

            // Write header
            if(!container->title().isEmpty()) {
                textStream() << "# " << container->title() << " (" << (quint64)row_count << " " << container->getOOMetaClass().elementDescriptionName() << "):\n";
            }
            else if(!container->getOOClass().displayName().isEmpty()) {
                textStream() << "# " << container->getOOClass().displayName() << " (" << (quint64)row_count << " " << container->getOOMetaClass().elementDescriptionName() << "):\n";
            }
            textStream() << "#";
            auto formatColumnName = [](const QString& name) { return name.contains(QChar(' ')) ? (QChar('"') + name + QChar('"')) : name; };

            // Add special table x-column
            if(table && !xstorage) {
                textStream() << " " << formatColumnName(table->axisLabelX());
            }
            else if(table) {
                textStream() << " " << formatColumnName(xstorage->name());
            }

            // Add special table y-column
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
            for(const Property* property : container->properties()) {
                if(table && (property == table->x() || property == table->y())) continue;
                outputProperties.emplace_back(property);
                if(property->componentNames().size() == property->componentCount()) {
                    for(size_t col = 0; col < property->componentCount(); col++) {
                        textStream() << " " << formatColumnName(property->nameWithComponent(col));
                    }
                }
                else {
                    textStream() << " " << formatColumnName(property->name());
                }
            }

            textStream() << "\n";

            for(size_t row = 0; row < row_count; row++) {
                // Write the table x column.
                if(table) {
                    if(table->plotMode() == DataTable::BarChart) {
                        const ElementType* type = ystorage ? ystorage->elementType(row) : nullptr;
                        if(!type && xstorage) type = xstorage->elementType(row);
                        if(type) {
                            textStream() << formatColumnName(type->name()) << " ";
                        }
                        else
                            continue;
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
    };
    return OORef<Job>::create(this, filePath, true);
}

}  // namespace Ovito
