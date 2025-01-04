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
#include <ovito/particles/export/FileColumnParticleExporter.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include "IMDExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(IMDExporter);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> IMDExporter::createExportJob(const QString& filePath, int numberOfFrames)
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

            // The IMD exporter, which manages the export settings.
            const IMDExporter* exporter = static_cast<const IMDExporter*>(this->exporter());

            const Particles* particles = state.expectObject<Particles>();

            // Get simulation cell info.
            const SimulationCell* simulationCell = state.expectObject<SimulationCell>();

            const AffineTransformation& simCell = simulationCell->cellMatrix();
            size_t atomsCount = particles->elementCount();

            OutputColumnMapping colMapping;
            OutputColumnMapping filteredMapping;
            bool exportIdentifiers = false;
            const Property* posProperty = nullptr;
            const Property* typeProperty = nullptr;
            const Property* identifierProperty = nullptr;
            const Property* velocityProperty = nullptr;
            const Property* massProperty = nullptr;
            for(const PropertyReference& pref : exporter->columnMapping()) {
                switch(Particles::OOClass().standardPropertyTypeId(pref.name())) {
                case Particles::PositionProperty:
                    posProperty = particles->expectProperty(Particles::PositionProperty);
                    break;
                case Particles::TypeProperty:
                    typeProperty = particles->expectProperty(Particles::TypeProperty);
                    break;
                case Particles::IdentifierProperty:
                    identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                    exportIdentifiers = true;
                    break;
                case Particles::VelocityProperty:
                    velocityProperty = particles->expectProperty(Particles::VelocityProperty);
                    break;
                case Particles::MassProperty:
                    massProperty = particles->expectProperty(Particles::MassProperty);
                    break;
                default:
                    filteredMapping.push_back(pref);
                }
            }

            QVector<QString> columnNames;
            textStream() << "#F A ";
            if(exportIdentifiers) {
                if(identifierProperty) {
                    textStream() << "1 ";
                    colMapping.emplace_back(identifierProperty);
                    columnNames.push_back("number");
                }
                else {
                    textStream() << "1 ";
                    colMapping.emplace_back(Particles::OOClass().standardPropertyName(Particles::IdentifierProperty));
                    columnNames.push_back("number");
                }
            }
            else textStream() << "0 ";
            if(typeProperty) {
                textStream() << "1 ";
                colMapping.emplace_back(typeProperty);
                columnNames.push_back("type");
            }
            else textStream() << "0 ";
            if(massProperty) {
                textStream() << "1 ";
                colMapping.emplace_back(massProperty);
                columnNames.push_back("mass");
            }
            else textStream() << "0 ";
            if(posProperty) {
                textStream() << "3 ";
                colMapping.emplace_back(posProperty, 0);
                colMapping.emplace_back(posProperty, 1);
                colMapping.emplace_back(posProperty, 2);
                columnNames.push_back("x");
                columnNames.push_back("y");
                columnNames.push_back("z");
            }
            else textStream() << "0 ";
            if(velocityProperty) {
                textStream() << "3 ";
                colMapping.emplace_back(velocityProperty, 0);
                colMapping.emplace_back(velocityProperty, 1);
                colMapping.emplace_back(velocityProperty, 2);
                columnNames.push_back("vx");
                columnNames.push_back("vy");
                columnNames.push_back("vz");
            }
            else textStream() << "0 ";

            for(size_t i = 0; i < filteredMapping.size(); i++) {
                colMapping.push_back(filteredMapping[i]);
            }

            PropertyOutputWriter columnWriter(colMapping, particles, PropertyOutputWriter::WriteNumericIds);

            textStream() << (columnWriter.columnCount() - columnNames.size()) << "\n";

            textStream() << "#C";
            for(size_t i = 0; i < columnWriter.columnCount(); i++) {
                QString columnName;
                if(i < (size_t)columnNames.size())
                    columnName = columnNames[i];
                else {
                    columnName = columnWriter.columnName(i);
                    columnName.remove(QRegularExpression(QStringLiteral("[^A-Za-z\\d_.]")));
                }
                textStream() << " " << columnName;
            }
            textStream() << "\n";

            textStream() << "#X " << simCell.column(0)[0] << " " << simCell.column(0)[1] << " " << simCell.column(0)[2] << "\n";
            textStream() << "#Y " << simCell.column(1)[0] << " " << simCell.column(1)[1] << " " << simCell.column(1)[2] << "\n";
            textStream() << "#Z " << simCell.column(2)[0] << " " << simCell.column(2)[1] << " " << simCell.column(2)[2] << "\n";

            textStream() << "## Generated on " << QDateTime::currentDateTime().toString() << "\n";
            textStream() << "## IMD file written by " << Application::applicationName() << "\n";
            textStream() << "#E\n";

            progress.setMaximum(atomsCount);
            for(size_t i = 0; i < atomsCount; i++) {
                columnWriter.writeElement(i, textStream());

                // Update progress bar and check for user cancellation.
                progress.setValueIntermittent(i);
            }
        }
    };

    return OORef<Job>::create(this, filePath, true);
}

}   // End of namespace
