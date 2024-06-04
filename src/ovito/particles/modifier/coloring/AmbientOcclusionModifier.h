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
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Calculates ambient occlusion lighting for particles.
 */
class OVITO_PARTICLES_EXPORT AmbientOcclusionModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class AmbientOcclusionModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(AmbientOcclusionModifier, AmbientOcclusionModifierClass)

public:

    enum { MAX_AO_RENDER_BUFFER_RESOLUTION = 4 };

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if the user toggles just the intensity.
        if(event.field() == PROPERTY_FIELD(intensity))
            return true;
        return Modifier::shouldKeepPartialResultsAfterChange(event);
    }

private:

    /// This controls the intensity of the shading effect.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0.7}, intensity, setIntensity);

    /// Controls the quality of the lighting computation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{40}, samplingCount, setSamplingCount);

    /// Controls the resolution of the offscreen rendering buffer.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{3}, bufferResolution, setBufferResolution);
};

}   // End of namespace
