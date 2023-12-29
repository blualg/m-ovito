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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include "POSCARExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(POSCARExporter);
DEFINE_PROPERTY_FIELD(POSCARExporter, writeReducedCoordinates);
SET_PROPERTY_FIELD_LABEL(POSCARExporter, writeReducedCoordinates, "Output reduced coordinates");

/******************************************************************************
* Writes the particles of one animation frame to the current output file.
******************************************************************************/
void POSCARExporter::exportData(const PipelineFlowState& state, int frameNumber, const QString& filePath)
{
    // Get particle positions and velocities.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();
    BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<Vector3> velocityProperty = particles->getProperty(Particles::VelocityProperty);
    size_t particleCount = particles->elementCount();

    // Get simulation cell info.
    const SimulationCell* simulationCell = state.getObject<SimulationCell>();
    if(!simulationCell)
        throw Exception(tr("No simulation cell available. Cannot write POSCAR file."));

    // Write POSCAR header including the simulation cell geometry.
    textStream() << "POSCAR file written by " << Application::applicationName() << " " << Application::applicationVersionString() << "\n";
    textStream() << "1\n";
    for(size_t i = 0; i < 3; i++)
        textStream() << simulationCell->matrix()(0, i) << ' ' << simulationCell->matrix()(1, i) << ' ' << simulationCell->matrix()(2, i) << '\n';
    const Vector3& origin = simulationCell->matrix().translation();

    // Count number of particles per particle type.
    QMap<int,int> particleCounts;
    const Property* particleTypeProperty = particles->getProperty(Particles::TypeProperty);
    BufferReadAccess<int32_t> particleTypeArray(particleTypeProperty);
    if(particleTypeProperty) {
        for(int ptype : particleTypeArray)
            particleCounts[ptype]++;

        // Write line with particle type names.
        for(auto c = particleCounts.begin(); c != particleCounts.end(); ++c) {
            const ElementType* particleType = particleTypeProperty->elementType(c.key());
            if(particleType) {
                QString typeName = particleType->nameOrNumericId();
                typeName.replace(' ', '_');
                textStream() << typeName << ' ';
            }
            else textStream() << "Type" << c.key() << ' ';
        }
        textStream() << '\n';

        // Write line with particle counts per type.
        for(auto c = particleCounts.begin(); c != particleCounts.end(); ++c) {
            textStream() << c.value() << ' ';
        }
        textStream() << '\n';
    }
    else {
        // Write line with particle type name.
        textStream() << "A\n";
        // Write line with particle count.
        textStream() << particleCount << '\n';
        particleCounts[0] = particleCount;
    }

    qlonglong totalProgressCount = particleCount;
    if(velocityProperty) totalProgressCount += particleCount;
    qlonglong currentProgress = 0;
    this_task::setProgressMaximum(totalProgressCount);

    // Write atomic positions.
    textStream() << (writeReducedCoordinates() ? "Direct\n" : "Cartesian\n");
    for(auto c = particleCounts.begin(); c != particleCounts.end(); ++c) {
        int ptype = c.key();
        const Point3* p = posProperty.cbegin();
        for(size_t i = 0; i < particleCount; i++, ++p) {
            if(particleTypeArray && particleTypeArray[i] != ptype)
                continue;
            if(writeReducedCoordinates()) {
                Point3 rp = simulationCell->absoluteToReduced(*p);
                textStream() << rp.x() << ' ' << rp.y() << ' ' << rp.z() << '\n';
            }
            else {
                textStream() << (p->x() - origin.x()) << ' ' << (p->y() - origin.y()) << ' ' << (p->z() - origin.z()) << '\n';
            }

            if(!this_task::setProgressValueIntermittent(currentProgress++))
                return;
        }
    }

    // Write atomic velocities.
    if(velocityProperty) {
        textStream() << (writeReducedCoordinates() ? "Direct\n" : "Cartesian\n");
        for(auto c = particleCounts.begin(); c != particleCounts.end(); ++c) {
            int ptype = c.key();
            const Vector3* v = velocityProperty.cbegin();
            for(size_t i = 0; i < particleCount; i++, ++v) {
                if(particleTypeArray && particleTypeArray[i] != ptype)
                    continue;

                if(writeReducedCoordinates()) {
                    Vector3 rv = simulationCell->absoluteToReduced(*v);
                    textStream() << rv.x() << ' ' << rv.y() << ' ' << rv.z() << '\n';
                }
                else {
                    textStream() << v->x() << ' ' << v->y() << ' ' << v->z() << '\n';
                }

                if(!this_task::setProgressValueIntermittent(currentProgress++))
                    return;
            }
        }
    }
}

}   // End of namespace
