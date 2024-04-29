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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "LAMMPSDumpExporter.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(LAMMPSDumpExporter);
DEFINE_PROPERTY_FIELD(LAMMPSDumpExporter, restrictedTriclinic);
SET_PROPERTY_FIELD_LABEL(LAMMPSDumpExporter, restrictedTriclinic, "Use the (legacy) restricted triclinic format");

/******************************************************************************
* Writes the particles of one animation frame to the current output file.
******************************************************************************/
bool LAMMPSDumpExporter::exportData(const PipelineFlowState& state, int frameNumber, const QString& filePath, MainThreadOperation& operation)
{
    // Get particles.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();

    // Get simulation cell info.
    const SimulationCell* simulationCell = state.getObject<SimulationCell>();
    if(!simulationCell)
        throw Exception(tr("No simulation cell available. Cannot write LAMMPS file."));

    const AffineTransformation& simCell = simulationCell->cellMatrix();
    size_t atomsCount = particles->elementCount();

    textStream() << "ITEM: TIMESTEP\n";
    textStream() << state.getAttributeValue(QStringLiteral("Timestep"), frameNumber).toInt() << '\n';
    textStream() << "ITEM: NUMBER OF ATOMS\n";
    textStream() << atomsCount << '\n';

    if(restrictedTriclinic()) {
        // Transform triclinic cell to LAMMPS canonical format.
        // Only for legacy restricted format
        FloatType xlo = simCell.translation().x();
        FloatType ylo = simCell.translation().y();
        FloatType zlo = simCell.translation().z();
        FloatType xhi = simCell.column(0).x() + xlo;
        FloatType yhi = simCell.column(1).y() + ylo;
        FloatType zhi = simCell.column(2).z() + zlo;
        FloatType xy = simCell.column(1).x();
        FloatType xz = simCell.column(2).x();
        FloatType yz = simCell.column(2).y();

        if(simCell.column(0).y() != 0 || simCell.column(0).z() != 0 || simCell.column(1).z() != 0)
            throw Exception(
                tr("Cannot save simulation cell to a LAMMPS dump file. This type of non-orthogonal "
                   "cell is not supported by LAMMPS and its file format. See the documentation of LAMMPS for details."));

        xlo += std::min((FloatType)0, std::min(xy, std::min(xz, xy + xz)));
        xhi += std::max((FloatType)0, std::max(xy, std::max(xz, xy + xz)));
        ylo += std::min((FloatType)0, yz);
        yhi += std::max((FloatType)0, yz);

        if(xy != 0 || xz != 0 || yz != 0) {
            textStream() << "ITEM: BOX BOUNDS xy xz yz";
            textStream() << (simulationCell->pbcX() ? " pp" : " ff");
            textStream() << (simulationCell->pbcY() ? " pp" : " ff");
            textStream() << (simulationCell->pbcZ() ? " pp" : " ff");
            textStream() << '\n';
            textStream() << xlo << ' ' << xhi << ' ' << xy << '\n';
            textStream() << ylo << ' ' << yhi << ' ' << xz << '\n';
            textStream() << zlo << ' ' << zhi << ' ' << yz << '\n';
        }
        else {
            textStream() << "ITEM: BOX BOUNDS";
            textStream() << (simulationCell->pbcX() ? " pp" : " ff");
            textStream() << (simulationCell->pbcY() ? " pp" : " ff");
            textStream() << (simulationCell->pbcZ() ? " pp" : " ff");
            textStream() << '\n';
            textStream() << xlo << ' ' << xhi << '\n';
            textStream() << ylo << ' ' << yhi << '\n';
            textStream() << zlo << ' ' << zhi << '\n';
        }
    }
    else {
        // new format avec, bvec, cvec, origin
        textStream() << "ITEM: BOX BOUNDS abc origin";
        textStream() << (simulationCell->pbcX() ? " pp" : " ff");
        textStream() << (simulationCell->pbcY() ? " pp" : " ff");
        textStream() << (simulationCell->pbcZ() ? " pp" : " ff");
        textStream() << '\n';
        textStream() << simulationCell->cellVector1()[0] << " " << simulationCell->cellVector1()[1] << " "
                     << simulationCell->cellVector1()[2] << simulationCell->cellOrigin()[0] << "\n";
        textStream() << simulationCell->cellVector2()[0] << " " << simulationCell->cellVector2()[1] << " "
                     << simulationCell->cellVector2()[2] << simulationCell->cellOrigin()[1] << "\n";
        textStream() << simulationCell->cellVector3()[0] << " " << simulationCell->cellVector3()[1] << " "
                     << simulationCell->cellVector3()[2] << simulationCell->cellOrigin()[2] << "\n";
    }
    textStream() << "ITEM: ATOMS";

    const ParticlesOutputColumnMapping& mapping = columnMapping();
    if(mapping.empty())
        throw Exception(tr("No particle properties have been selected for export to the LAMMPS dump file. Cannot write dump file with zero columns."));

    // Prepare effective list of output columns (e.g. expand vector properties).
    PropertyOutputWriter columnWriter(mapping, particles, PropertyOutputWriter::WriteNumericIds);

    // Write column names.
    for(size_t i = 0; i < columnWriter.columnCount(); i++) {
        const PropertyReference& pref = columnWriter.propertyRef(i);
        QString columnName;
        switch(pref.type()) {
        case Particles::PositionProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("x");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("y");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("z");
            else columnName = pref.nameWithComponent();
            break;
        case Particles::VelocityProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("vx");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("vy");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("vz");
            else columnName = pref.nameWithComponent();
            break;
        case Particles::ForceProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("fx");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("fy");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("fz");
            else columnName = pref.nameWithComponent();
            break;
        case Particles::PeriodicImageProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("ix");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("iy");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("iz");
            else columnName = pref.nameWithComponent();
            break;
        case Particles::IdentifierProperty: columnName = QStringLiteral("id"); break;
        case Particles::TypeProperty: columnName = QStringLiteral("type"); break;
        case Particles::MassProperty: columnName = QStringLiteral("mass"); break;
        case Particles::SelectionProperty: columnName = QStringLiteral("selection"); break;
        case Particles::RadiusProperty: columnName = QStringLiteral("radius"); break;
        case Particles::MoleculeProperty: columnName = QStringLiteral("mol"); break;
        case Particles::ChargeProperty: columnName = QStringLiteral("q"); break;
        case Particles::PotentialEnergyProperty: columnName = QStringLiteral("c_epot"); break;
        case Particles::KineticEnergyProperty: columnName = QStringLiteral("c_kpot"); break;
        case Particles::OrientationProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("quati");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("quatj");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("quatk");
            else if(pref.vectorComponent() == 3) columnName = QStringLiteral("quatw");
            else columnName = pref.nameWithComponent();
            break;
        case Particles::AsphericalShapeProperty:
            if(pref.vectorComponent() == 0) columnName = QStringLiteral("c_shape[1]");
            else if(pref.vectorComponent() == 1) columnName = QStringLiteral("c_shape[2]");
            else if(pref.vectorComponent() == 2) columnName = QStringLiteral("c_shape[3]");
            else columnName = pref.nameWithComponent();
            break;
        default:
            columnName = columnWriter.columnName(i);
            columnName.remove(QRegularExpression(QStringLiteral("[^A-Za-z\\d_]")));
        }
        textStream() << ' ' << columnName;
    }
    textStream() << '\n';

    operation.setProgressMaximum(atomsCount);
    for(size_t i = 0; i < atomsCount; i++) {
        columnWriter.writeElement(i, textStream());

        if(!operation.setProgressValueIntermittent(i))
            return false;
    }

    return !operation.isCanceled();
}

}   // End of namespace
