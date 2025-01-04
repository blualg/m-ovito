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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include "FHIAimsExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FHIAimsExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> FHIAimsExporter::createExportJob(const QString& filePath, int numberOfFrames)
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

            // Get particle positions and types.
            const Particles* particles = state.expectObject<Particles>();
            BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
            const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
            BufferReadAccess<int32_t> particleTypeArray(particleTypeProperty);

            textStream() << "# FHI-aims file written by " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";

            // Output simulation cell.
            Point3 origin = Point3::Origin();
            const SimulationCell* simulationCell = state.getObject<SimulationCell>();
            if(simulationCell) {
                origin = simulationCell->cellOrigin();
                if(simulationCell->pbcX() || simulationCell->pbcY() || simulationCell->pbcZ()) {
                    const AffineTransformation& cell = simulationCell->cellMatrix();
                    for(size_t i = 0; i < 3; i++)
                        textStream() << "lattice_vector " << cell(0, i) << ' ' << cell(1, i) << ' ' << cell(2, i) << '\n';
                }
            }

            // Output atoms.
            for(size_t i = 0; i < posProperty.size(); i++) {
                const Point3& p = posProperty[i];
                const ElementType* type = particleTypeArray ? particleTypeProperty->elementType(particleTypeArray[i]) : nullptr;

                textStream() << "atom " << (p.x() - origin.x()) << ' ' << (p.y() - origin.y()) << ' ' << (p.z() - origin.z());
                if(type && !type->name().isEmpty()) {
                    QString s = type->name();
                    textStream() << ' ' << s.replace(QChar(' '), QChar('_')) << '\n';
                }
                else if(particleTypeArray) {
                    textStream() << ' ' << particleTypeArray[i] << '\n';
                }
                else {
                    textStream() << " 1\n";
                }

                // Check for user cancellation.
                this_task::throwIfCanceled();
            }
        }
    };

    return OORef<Job>::create(this, filePath, true);
}

}   // End of namespace
