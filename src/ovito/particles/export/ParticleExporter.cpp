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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include "ParticleExporter.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ParticleExporter);

/******************************************************************************
* Evaluates the pipeline of an PipelineSceneNode and makes sure that the data to be
* exported contains particles and throws an exception if not.
******************************************************************************/
PipelineFlowState ParticleExporter::getParticleData(int frame) const
{
    PipelineFlowState state = getPipelineDataToBeExported(frame);
    if(!state)
        return {};

    const Particles* particles = state.getObject<Particles>();
    if(!particles)
        throw Exception(tr("The selected data collection does not contain any particles that can be exported."));
    if(!particles->getProperty(Particles::PositionProperty))
        throw Exception(tr("The particles to be exported do not have any coordinates ('Position' property is missing)."));

    // Verify data, make sure array length is consistent for all particle properties.
    particles->verifyIntegrity();

    // Verify data, make sure array length is consistent for all bond properties.
    if(particles->bonds()) {
        particles->bonds()->verifyIntegrity();
    }

    return state;
}

/******************************************************************************
 * This is called once for every output file to be written and before
 * exportFrame() is called.
 *****************************************************************************/
void ParticleExporter::openOutputFile(const QString& filePath, int numberOfFrames)
{
    OVITO_ASSERT(!_outputFile.isOpen());
    OVITO_ASSERT(!_outputStream);

    _outputFile.setFileName(filePath);
    _outputStream = std::make_unique<CompressedTextWriter>(_outputFile);
    _outputStream->setFloatPrecision(floatOutputPrecision());
}

/******************************************************************************
 * This is called once for every output file written after exportFrame()
 * has been called.
 *****************************************************************************/
void ParticleExporter::closeOutputFile(bool exportCompleted)
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
bool ParticleExporter::exportFrame(int frameNumber, const QString& filePath, MainThreadOperation& operation)
{
    // Retreive the particle data to be exported.
    const PipelineFlowState& state = getParticleData(frameNumber);
    if(operation.isCanceled() || !state)
        return false;

    // Let the subclass do the work.
    return exportData(state, frameNumber, filePath, operation);
}

}   // End of namespace
