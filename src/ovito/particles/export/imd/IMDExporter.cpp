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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/export/FileColumnParticleExporter.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include "IMDExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(IMDExporter);

/******************************************************************************
* Writes the particles of one animation frame to the current output file.
******************************************************************************/
void IMDExporter::exportData(const PipelineFlowState& state, int frameNumber, const QString& filePath)
{
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    // Get simulation cell info.
    const SimulationCell* simulationCell = state.expectObject<SimulationCell>();

    const AffineTransformation& simCell = simulationCell->cellMatrix();
    size_t atomsCount = particles->elementCount();

    ParticlesOutputColumnMapping colMapping;
    ParticlesOutputColumnMapping filteredMapping;
    bool exportIdentifiers = false;
    const Property* posProperty = nullptr;
    const Property* typeProperty = nullptr;
    const Property* identifierProperty = nullptr;
    const Property* velocityProperty = nullptr;
    const Property* massProperty = nullptr;
    for(const ParticlePropertyReference& pref : columnMapping()) {
        if(pref.type() == Particles::PositionProperty) {
            posProperty = particles->expectProperty(Particles::PositionProperty);
        }
        else if(pref.type() == Particles::TypeProperty) {
            typeProperty = particles->expectProperty(Particles::TypeProperty);
        }
        else if(pref.type() == Particles::IdentifierProperty) {
            identifierProperty = particles->getProperty(Particles::IdentifierProperty);
            exportIdentifiers = true;
        }
        else if(pref.type() == Particles::VelocityProperty) {
            velocityProperty = particles->expectProperty(Particles::VelocityProperty);
        }
        else if(pref.type() == Particles::MassProperty) {
            massProperty = particles->expectProperty(Particles::MassProperty);
        }
        else filteredMapping.push_back(pref);
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
            colMapping.emplace_back(Particles::IdentifierProperty);
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
            const PropertyReference& pref = columnWriter.propertyRef(i);
            columnName = columnWriter.isVectorProperty(i) ? pref.nameWithComponent() : pref.name();
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

    this_task::setProgressMaximum(atomsCount);
    for(size_t i = 0; i < atomsCount; i++) {
        columnWriter.writeElement(i, textStream());

        if(!this_task::setProgressValueIntermittent(i))
            return;
    }
}

}   // End of namespace
