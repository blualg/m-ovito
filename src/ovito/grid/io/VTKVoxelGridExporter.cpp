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

#include <ovito/grid/Grid.h>
#include <ovito/core/app/Application.h>
#include "VTKVoxelGridExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VTKVoxelGridExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> VTKVoxelGridExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(boost::anys::unique_any frameData, int frameNumber, const QString& filePath,
                                               TaskProgress& progress) override
        {
            // The exportable frame data.
            OVITO_ASSERT(frameData.has_value());
            const auto state = boost::anys::any_cast<PipelineFlowState>(frameData);

            // Perform the following in a worker thread.
            co_await ExecutorAwaiter(ThreadPoolExecutor());

            // Look up the VoxelGrid to be exported in the pipeline state.
            DataObjectReference objectRef(&VoxelGrid::OOClass(), dataObjectToExport().dataPath());
            const VoxelGrid* voxelGrid = static_object_cast<VoxelGrid>(state.getLeafObject(objectRef));
            if(!voxelGrid) {
                throw Exception(tr("The pipeline output does not contain the voxel grid to be exported (animation frame: %1; object key: %2). Available grid keys: (%3)")
                    .arg(frameNumber).arg(objectRef.dataPath()).arg(getAvailableDataObjectList(state, VoxelGrid::OOClass())));
            }

            // Make sure the data structure to be exported is consistent.
            voxelGrid->verifyIntegrity();

            auto dims = voxelGrid->shape();
            textStream() << "# vtk DataFile Version 3.0\n";
            textStream() << "# Voxel grid data written by " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";
            textStream() << "ASCII\n";
            textStream() << "DATASET STRUCTURED_POINTS\n";
            textStream() << "DIMENSIONS " << dims[0] << " " << dims[1] << " " << dims[2] << "\n";
            if(const SimulationCell* domain = voxelGrid->domain()) {
                textStream() << "ORIGIN " << domain->cellOrigin().x() << " " << domain->cellOrigin().y() << " " << domain->cellOrigin().z() << "\n";
                textStream() << "SPACING";
                textStream() << " " << domain->cellVector1().length() / std::max(dims[0], (size_t)1);
                textStream() << " " << domain->cellVector2().length() / std::max(dims[1], (size_t)1);
                textStream() << " " << domain->cellVector3().length() / std::max(dims[2], (size_t)1);
                textStream() << "\n";
            }
            else {
                textStream() << "ORIGIN 0 0 0\n";
                textStream() << "SPACING 1 1 1\n";
            }
            textStream() << "POINT_DATA " << voxelGrid->elementCount() << "\n";

            for(const Property* prop : voxelGrid->properties()) {
                if(prop->dataType() == Property::Int8 || prop->dataType() == Property::Int32 || prop->dataType() == Property::Int64 || prop->dataType() == Property::Float32 || prop->dataType() == Property::Float64) {

                    // Write header of data field.
                    QString dataName = prop->name();
                    dataName.remove(QChar(' '));
                    if((prop->dataType() == Property::Float32 || prop->dataType() == Property::Float64) && prop->componentCount() == 3) {
                        textStream() << "\nVECTORS " << dataName << " double\n";
                    }
                    else if(prop->componentCount() <= 4) {
                        if(prop->dataType() == Property::Int32 || prop->dataType() == Property::Int8)
                            textStream() << "\nSCALARS " << dataName << " int " << prop->componentCount() << "\n";
                        else if(prop->dataType() == Property::Int64)
                            textStream() << "\nSCALARS " << dataName << " long " << prop->componentCount() << "\n";
                        else
                            textStream() << "\nSCALARS " << dataName << " double " << prop->componentCount() << "\n";
                        textStream() << "LOOKUP_TABLE default\n";
                    }
                    else continue; // VTK format supports only between 1 and 4 vector components. Skipping properties with more components during export.

                    // Write payload data.
                    size_t cmpnts = prop->componentCount();
                    OVITO_ASSERT(prop->stride() == prop->dataTypeSize() * cmpnts);
                    if(prop->dataType() == Property::Float32) {
                        BufferReadAccess<float*> data(prop);
                        for(size_t row = 0, index = 0; row < dims[1]*dims[2]; row++) {
                            this_task::throwIfCanceled();
                            for(size_t col = 0; col < dims[0]; col++, index++) {
                                for(size_t c = 0; c < cmpnts; c++)
                                    textStream() << data.get(index, c) << " ";
                            }
                            textStream() << "\n";
                        }
                    }
                    else if(prop->dataType() == Property::Float64) {
                        BufferReadAccess<double*> data(prop);
                        for(size_t row = 0, index = 0; row < dims[1]*dims[2]; row++) {
                            this_task::throwIfCanceled();
                            for(size_t col = 0; col < dims[0]; col++, index++) {
                                for(size_t c = 0; c < cmpnts; c++)
                                    textStream() << data.get(index, c) << " ";
                            }
                            textStream() << "\n";
                        }
                    }
                    else if(prop->dataType() == Property::Int8) {
                        BufferReadAccess<int8_t*> data(prop);
                        for(size_t row = 0, index = 0; row < dims[1]*dims[2]; row++) {
                            this_task::throwIfCanceled();
                            for(size_t col = 0; col < dims[0]; col++, index++) {
                                for(size_t c = 0; c < cmpnts; c++)
                                    textStream() << static_cast<qint32>(data.get(index, c)) << " ";
                            }
                            textStream() << "\n";
                        }
                    }
                    else if(prop->dataType() == Property::Int32) {
                        BufferReadAccess<int32_t*> data(prop);
                        for(size_t row = 0, index = 0; row < dims[1]*dims[2]; row++) {
                            this_task::throwIfCanceled();
                            for(size_t col = 0; col < dims[0]; col++, index++) {
                                for(size_t c = 0; c < cmpnts; c++)
                                    textStream() << static_cast<qint32>(data.get(index, c)) << " ";
                            }
                            textStream() << "\n";
                        }
                    }
                    else if(prop->dataType() == Property::Int64) {
                        BufferReadAccess<int64_t*> data(prop);
                        for(size_t row = 0, index = 0; row < dims[1]*dims[2]; row++) {
                            this_task::throwIfCanceled();
                            for(size_t col = 0; col < dims[0]; col++, index++) {
                                for(size_t c = 0; c < cmpnts; c++)
                                    textStream() << static_cast<qlonglong>(data.get(index, c)) << " ";
                            }
                            textStream() << "\n";
                        }
                    }
                    else {
                        throw Exception(tr("Grid property '%1' has a non-standard data type that cannot be exported.").arg(prop->name()));
                    }
                }
            }
        }
    };

    return OORef<Job>::create(this, filePath, true);
}

}   // End of namespace
