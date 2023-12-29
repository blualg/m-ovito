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
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief Smoothly interpolates the particle positions by averaging multiple snapshots.
 */
class OVITO_PARTICLES_EXPORT SmoothTrajectoryModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(SmoothTrajectoryModifier, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "Smooth trajectory");
    OVITO_CLASSINFO("ClassNameAlias", "InterpolateTrajectoryModifier");
    OVITO_CLASSINFO("Description", "Time-averaged particle positions using a sliding time window.");
    OVITO_CLASSINFO("ModifierCategory", "Modification");

public:

    /// Constructor.
    explicit SmoothTrajectoryModifier(ObjectInitializationFlags flags);

    /// Determines the time interval over which a computed pipeline state will remain valid.
    virtual TimeInterval validityInterval(const ModifierEvaluationRequest& request) const override;

    /// Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
    virtual void inputCachingHints(TimeIntervalUnion& cachingIntervals, ModificationNode* node) override;

    /// Is called by the ModifierApplication to let the modifier adjust the time interval of a TargetChanged event
    /// received from the upstream pipeline before it is propagated to the downstream pipeline.
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

private:

    /// Computes the interpolated state between two input states.
    void interpolateState(PipelineFlowState& state1, const PipelineFlowState& state2, const ModifierEvaluationRequest& request, AnimationTime time1, AnimationTime time2);

    /// Computes the averaged state from several input states.
    void averageState(PipelineFlowState& state1, const std::vector<PipelineFlowState>& otherStates, const ModifierEvaluationRequest& request);

    /// Controls whether the minimum image convention is used during displacement calculation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, useMinimumImageConvention, setUseMinimumImageConvention);

    /// The number of animation frames to include in the averaging procedure.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, smoothingWindowSize, setSmoothingWindowSize);
};

/**
 * This class is no longer used since 02/2020. It's only here for backward compatibility with files written by older OVITO versions.
 * The class can safely be removed in the future.
 */
class OVITO_PARTICLES_EXPORT InterpolateTrajectoryModifierApplication : public ModificationNode
{
    OVITO_CLASS(InterpolateTrajectoryModifierApplication)

public:

    /// Constructor.
    using ModificationNode::ModificationNode;
};

}   // End of namespace
