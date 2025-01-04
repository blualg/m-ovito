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
#include "XYZExporter.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(XYZExporter);
DEFINE_PROPERTY_FIELD(XYZExporter, subFormat);
SET_PROPERTY_FIELD_LABEL(XYZExporter, subFormat, "XYZ format style");

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> XYZExporter::createExportJob(const QString& filePath, int numberOfFrames)
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

            // The XYZ exporter, which manages the export settings.
            const XYZExporter* exporter = static_cast<const XYZExporter*>(this->exporter());

            // Get particle positions.
            const Particles* particles = state.expectObject<Particles>();

            size_t atomsCount = particles->elementCount();
            textStream() << atomsCount << '\n';

            const OutputColumnMapping& mapping = exporter->columnMapping();
            if(mapping.empty())
                throw Exception(tr("No particle properties have been selected for export to the XYZ file. Cannot write file with zero columns."));
            PropertyOutputWriter columnWriter(mapping, particles, PropertyOutputWriter::WriteNamesUnderscore);

            const SimulationCell* simulationCell = state.getObject<SimulationCell>();

            if(exporter->subFormat() == ParcasFormat) {
                textStream() << QStringLiteral("Frame %1").arg(frameNumber);
                if(simulationCell) {
                    const AffineTransformation& simCell = simulationCell->cellMatrix();
                    textStream() << " cell_orig " << simCell.translation().x() << " " << simCell.translation().y() << " " << simCell.translation().z();
                    textStream() << " cell_vec1 " << simCell.column(0).x() << " " << simCell.column(0).y() << " " << simCell.column(0).z();
                    textStream() << " cell_vec2 " << simCell.column(1).x() << " " << simCell.column(1).y() << " " << simCell.column(1).z();
                    textStream() << " cell_vec3 " << simCell.column(2).x() << " " << simCell.column(2).y() << " " << simCell.column(2).z();
                    textStream() << " pbc " << simulationCell->pbcX() << " " << simulationCell->pbcY() << " " << simulationCell->pbcZ();
                }
            }
            else if(exporter->subFormat() == ExtendedFormat) {
                if(simulationCell) {
                    const AffineTransformation& simCell = simulationCell->cellMatrix();
                    // Save cell information in extended XYZ format:
                    // see http://libatoms.github.io/QUIP/io.html#module-ase.io.extxyz for details
                    textStream() << QStringLiteral("Lattice=\"");
                    textStream() << simCell.column(0).x() << " " << simCell.column(0).y() << " " << simCell.column(0).z() << " ";
                    textStream() << simCell.column(1).x() << " " << simCell.column(1).y() << " " << simCell.column(1).z() << " ";
                    textStream() << simCell.column(2).x() << " " << simCell.column(2).y() << " " << simCell.column(2).z() << "\" ";

                    if(simCell.translation() != Vector3::Zero()) {
                        textStream() << QStringLiteral("Origin=\"");
                        textStream() << simCell.translation().x() << " " << simCell.translation().y() << " " << simCell.translation().z() << "\" ";
                    }
                }
                // Save column information in extended XYZ format:
                // see http://libatoms.github.io/QUIP/io.html#module-ase.io.extxyz for details
                textStream() << QStringLiteral("Properties=");
                QString propertiesStr;
                int i = 0;
                while(i < (int)columnWriter.columnCount()) {
                    // Convert from OVITO property type and name to extended XYZ property name
                    // Naming conventions followed are those of the QUIP code
                    QString columnName;
                    switch(columnWriter.propertyTypeId(i)) {
                    case Particles::TypeProperty: columnName = QStringLiteral("species"); break;
                    case Particles::PositionProperty: columnName = QStringLiteral("pos"); break;
                    case Particles::SelectionProperty: columnName = QStringLiteral("selection"); break;
                    case Particles::ColorProperty: columnName = QStringLiteral("color"); break;
                    case Particles::DisplacementProperty: columnName = QStringLiteral("disp"); break;
                    case Particles::DisplacementMagnitudeProperty: columnName = QStringLiteral("disp_mag"); break;
                    case Particles::PotentialEnergyProperty: columnName = QStringLiteral("local_energy"); break;
                    case Particles::KineticEnergyProperty: columnName = QStringLiteral("kinetic_energy"); break;
                    case Particles::TotalEnergyProperty: columnName = QStringLiteral("total_energy"); break;
                    case Particles::VelocityProperty: columnName = QStringLiteral("velo"); break;
                    case Particles::VelocityMagnitudeProperty: columnName = QStringLiteral("velo_mag"); break;
                    case Particles::RadiusProperty: columnName = QStringLiteral("radius"); break;
                    case Particles::ClusterProperty: columnName = QStringLiteral("cluster"); break;
                    case Particles::CoordinationProperty: columnName = QStringLiteral("n_neighb"); break;
                    case Particles::StructureTypeProperty: columnName = QStringLiteral("structure_type"); break;
                    case Particles::IdentifierProperty: columnName = QStringLiteral("id"); break;
                    case Particles::StressTensorProperty: columnName = QStringLiteral("stress"); break;
                    case Particles::StrainTensorProperty: columnName = QStringLiteral("strain"); break;
                    case Particles::DeformationGradientProperty: columnName = QStringLiteral("deform"); break;
                    case Particles::OrientationProperty: columnName = QStringLiteral("orientation"); break;
                    case Particles::ForceProperty: columnName = QStringLiteral("force"); break;
                    case Particles::MassProperty: columnName = QStringLiteral("mass"); break;
                    case Particles::ChargeProperty: columnName = QStringLiteral("charge"); break;
                    case Particles::PeriodicImageProperty: columnName = QStringLiteral("map_shift"); break;
                    case Particles::TransparencyProperty: columnName = QStringLiteral("transparency"); break;
                    case Particles::DipoleOrientationProperty: columnName = QStringLiteral("dipoles"); break;
                    case Particles::DipoleMagnitudeProperty: columnName = QStringLiteral("dipoles_mag"); break;
                    case Particles::AngularVelocityProperty: columnName = QStringLiteral("omega"); break;
                    case Particles::AngularMomentumProperty: columnName = QStringLiteral("angular_momentum"); break;
                    case Particles::TorqueProperty: columnName = QStringLiteral("torque"); break;
                    case Particles::SpinProperty: columnName = QStringLiteral("spin"); break;
                    case Particles::CentroSymmetryProperty: columnName = QStringLiteral("centro_symmetry"); break;
                    case Particles::AsphericalShapeProperty: columnName = QStringLiteral("aspherical_shape"); break;
                    case Particles::VectorColorProperty: columnName = QStringLiteral("vector_color"); break;
                    case Particles::MoleculeProperty: columnName = QStringLiteral("molecule"); break;
                    case Particles::MoleculeTypeProperty: columnName = QStringLiteral("molecule_type"); break;
                    default:
                        columnName = columnWriter.propertyRef(i).name().toString();
                        columnName.remove(QRegularExpression(QStringLiteral("[^A-Za-z\\d_]")));
                    }
                    const Property* property = columnWriter.property(i);

                    // Count the number of consecutive columns with the same property.
                    int nCols = 1;
                    while(++i < (int)columnWriter.columnCount()) {
                        if(property != columnWriter.property(i))
                            break;
                        nCols++;
                    }

                    // Convert OVITO property data type to extended XYZ type code: 'I','R','S','L'
                    int dataType = property ? property->dataType() : Property::Int32;
                    QString dataTypeStr;
                    if(dataType == Property::Float32 || dataType == Property::Float64)
                        dataTypeStr = QStringLiteral("R");
                    else if(property && property->typeId() == Particles::TypeProperty)
                        dataTypeStr = QStringLiteral("S");
                    else if(dataType == Property::Int8 || dataType == Property::Int32 || dataType == Property::Int64)
                        dataTypeStr = QStringLiteral("I");
                    else if(dataType == qMetaTypeId<bool>())
                        dataTypeStr = QStringLiteral("L");
                    else
                        throw Exception(tr("Unexpected data type '%1' for property '%2'.").arg(property ? property->dataTypeName() : "unknown").arg(property->name()));

                    if(!propertiesStr.isEmpty())
                        propertiesStr += QStringLiteral(":");
                    propertiesStr += QStringLiteral("%1:%2:%3").arg(columnName).arg(dataTypeStr).arg(nCols);
                }
                textStream() << propertiesStr;
            }
            textStream() << '\n';

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
