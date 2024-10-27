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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/io/FileExporter.h>

namespace Ovito {

/**
 * \brief Abstract base class for export services that write particle datasets to an output file.
 */
class OVITO_PARTICLES_EXPORT ParticleExporter : public FileExporter
{
    OVITO_CLASS(ParticleExporter)

public:

    /// Returns the type(s) of data objects that this exporter service can export.
    virtual std::vector<DataObjectClassPtr> exportableDataObjectClass() const override {
        return { &Particles::OOClass() };
    }

    /// Evaluates the pipeline whose data is to be exported.
    virtual Future<PipelineFlowState> getPipelineDataToBeExported(int frameNumber) const override;
};

}   // End of namespace
