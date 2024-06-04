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
#include <ovito/stdobj/vectors/VectorVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/modifier/analysis/ReferenceConfigurationModifier.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Calculates the per-particle displacement vectors based on a reference configuration.
 */
class OVITO_PARTICLES_EXPORT CalculateDisplacementsModifier : public ReferenceConfigurationModifier
{
    OVITO_CLASS(CalculateDisplacementsModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual std::unique_ptr<Engine> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input, const PipelineFlowState& referenceState) override;

    /// Adopts existing computation results for an interactive pipeline evaluation.
    virtual Future<PipelineFlowState> reuseCachedState(const ModifierEvaluationRequest& request, Particles* particles, PipelineFlowState&& output, const PipelineFlowState& cachedState) override;

private:

    /// Computes the modifier's results.
    class DisplacementEngine : public Engine
    {
    public:

        /// Constructor.
        DisplacementEngine(
                PropertyPtr displacements,
                PropertyPtr displacementMagnitudes,
                ConstPropertyPtr positions,
                const SimulationCell* simCell,
                ConstPropertyPtr refPositions,
                const SimulationCell* simCellRef,
                ConstPropertyPtr identifiers,
                ConstPropertyPtr refIdentifiers,
                AffineMappingType affineMapping,
                bool useMinimumImageConvention) :
            Engine(positions, simCell, std::move(refPositions), simCellRef,
                std::move(identifiers), std::move(refIdentifiers), affineMapping, useMinimumImageConvention),
            _displacements(std::move(displacements)),
            _displacementMagnitudes(std::move(displacementMagnitudes)) {}

        /// Performs the actual computation of the modifier's results.
        virtual void perform(PipelineFlowState& state) override;

        /// Returns the property storage that contains the computed displacement vectors.
        const PropertyPtr& displacements() const { return _displacements; }

        /// Returns the property storage that contains the computed displacement vector magnitudes.
        const PropertyPtr& displacementMagnitudes() const { return _displacementMagnitudes; }

    private:

        const PropertyPtr _displacements;
        const PropertyPtr _displacementMagnitudes;
    };

    /// The vis element for rendering the displacement vectors.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<VectorVis>, vectorVis, setVectorVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
