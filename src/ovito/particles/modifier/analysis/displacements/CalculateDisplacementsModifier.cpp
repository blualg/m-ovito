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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include "CalculateDisplacementsModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CalculateDisplacementsModifier);
DEFINE_REFERENCE_FIELD(CalculateDisplacementsModifier, vectorVis);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
CalculateDisplacementsModifier::CalculateDisplacementsModifier(ObjectInitializationFlags flags) : ReferenceConfigurationModifier(flags)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create vis element for vectors.
        setVectorVis(OORef<VectorVis>::create(flags));
        vectorVis()->setObjectTitle(tr("Displacements"));

        // Don't show vectors by default, because too many vectors can make the
        // program freeze. User has to enable the display manually.
        vectorVis()->setEnabled(false);

        // Configure vector display such that arrows point from the reference particle positions
        // to the current particle positions.
        vectorVis()->setReverseArrowDirection(false);
        vectorVis()->setArrowPosition(VectorVis::Head);

        // In GUI mode, visualize the displacement magnitude by default.
        if(ExecutionContext::isInteractive())
            vectorVis()->colorMapping()->setSourceProperty(ParticlePropertyReference(Particles::DisplacementMagnitudeProperty));
    }
}

/******************************************************************************
* Adopts existing computation results for an interactive pipeline evaluation.
******************************************************************************/
Future<PipelineFlowState> CalculateDisplacementsModifier::reuseCachedState(const ModifierEvaluationRequest& request, Particles* particles, PipelineFlowState&& output, const PipelineFlowState& cachedState)
{
    // Adopt the displacement property from the cached state.
    if(DataOORef<const Particles> cachedParticles = cachedState.getObject<Particles>()) {
        const Property* cachedDisplacements = cachedParticles->getProperty(Particles::DisplacementProperty);
        const Property* cachedDisplacementMags = cachedParticles->getProperty(Particles::DisplacementMagnitudeProperty);
        return AsynchronousTask<PipelineFlowState>::runAsync([output = std::move(output), particles, cachedDisplacements, cachedDisplacementMags, cachedParticles = std::move(cachedParticles)]() mutable {
            particles->tryToAdoptProperties(cachedParticles, {cachedDisplacements, cachedDisplacementMags}, {particles});
            return std::move(output);
        });
    }
    return std::move(output);
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the modifier's results.
******************************************************************************/
std::unique_ptr<ReferenceConfigurationModifier::Engine> CalculateDisplacementsModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input, const PipelineFlowState& referenceState)
{
    // Get the current particle positions.
    const Particles* particles = input.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);

    // Get the reference particle position.
    const Particles* refParticles = referenceState.getObject<Particles>();
    if(!refParticles)
        throw Exception(tr("Reference configuration does not contain particles."));
    refParticles->verifyIntegrity();
    const Property* refPosProperty = refParticles->expectProperty(Particles::PositionProperty);

    // Get the simulation cells.
    const SimulationCell* inputCell = input.expectObject<SimulationCell>();
    const SimulationCell* refCell = referenceState.getObject<SimulationCell>();
    if(!refCell)
        throw Exception(tr("Reference configuration does not contain simulation cell info."));

    // Get particle identifiers.
    const Property* identifierProperty = particles->getProperty(Particles::IdentifierProperty);
    const Property* refIdentifierProperty = refParticles->getProperty(Particles::IdentifierProperty);

    // Create the output particle properties.
    PropertyPtr displacements = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::DisplacementProperty);
    PropertyPtr displacementMagnitudes = Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, particles->elementCount(), Particles::DisplacementMagnitudeProperty);
    displacements->setVisElement(vectorVis());

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    return std::make_unique<DisplacementEngine>(
            std::move(displacements), std::move(displacementMagnitudes),
            posProperty, inputCell,
            refPosProperty, refCell,
            identifierProperty, refIdentifierProperty,
            affineMapping(), useMinimumImageConvention());
}

/******************************************************************************
* Performs the actual computation of the modifier's results.
******************************************************************************/
void CalculateDisplacementsModifier::DisplacementEngine::perform(PipelineFlowState& state)
{
    // First determine the mapping from particles of the reference config to particles
    // of the current config.
    buildParticleMapping(true, false);

    BufferWriteAccess<Vector3, access_mode::discard_write> displacementsAcc(displacements());
    BufferWriteAccess<FloatType, access_mode::discard_write> displacementMagnitudesAcc(displacementMagnitudes());
    BufferReadAccess<Point3> positionsAcc(positions());
    BufferReadAccess<Point3> refPositionsArray(refPositions());

    const auto refCellPbcFlags = refCell()->pbcFlagsCorrected();
    const auto refCellMatrix = refCell()->matrix();

    // Compute displacement vectors.
    if(affineMapping() != NO_MAPPING) {
        const AffineTransformation reduced_to_absolute = (affineMapping() == TO_REFERENCE_CELL) ? refCellMatrix : cell()->matrix();
        parallelFor(displacements()->size(), 1024, [&](size_t i) {
            const Point3& p = positionsAcc[i];
            auto index = currentToRefIndexMap()[i];
            Point3 reduced_current_pos = cell()->inverseMatrix() * p;
            Point3 reduced_reference_pos = refCell()->inverseMatrix() * refPositionsArray[index];
            Vector3 delta = reduced_current_pos - reduced_reference_pos;
            if(useMinimumImageConvention()) {
                for(size_t k = 0; k < 3; k++) {
                    if(refCellPbcFlags[k])
                        delta[k] -= std::floor(delta[k] + FloatType(0.5));
                }
            }
            Vector3 u = reduced_to_absolute * delta;
            displacementsAcc[i] = u;
            displacementMagnitudesAcc[i] = u.length();
        });
    }
    else {
        parallelFor(displacements()->size(), 1024, [&](size_t i) {
            const Point3& p = positionsAcc[i];
            auto index = currentToRefIndexMap()[i];
            Vector3 u = p - refPositionsArray[index];
            if(useMinimumImageConvention()) {
                for(size_t k = 0; k < 3; k++) {
                    if(refCellPbcFlags[k]) {
                        while((u + refCellMatrix.column(k)).squaredLength() < u.squaredLength())
                            u += refCellMatrix.column(k);

                        while((u - refCellMatrix.column(k)).squaredLength() < u.squaredLength())
                            u -= refCellMatrix.column(k);
                    }
                }
            }
            displacementsAcc[i] = u;
            displacementMagnitudesAcc[i] = u.length();
        });
    }
    displacementsAcc.reset();
    displacementMagnitudesAcc.reset();

    Particles* particles = state.expectMutableObject<Particles>();
    particles->createProperty(displacements());
    particles->createProperty(displacementMagnitudes());
}

}   // End of namespace
