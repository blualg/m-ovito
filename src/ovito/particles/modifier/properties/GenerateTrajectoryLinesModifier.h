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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/lines/LinesVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief Generates trajectory lines for particles.
 */
class OVITO_PARTICLES_EXPORT GenerateTrajectoryLinesModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class GenerateTrajectoryLinesModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(GenerateTrajectoryLinesModifier, GenerateTrajectoryLinesModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    Future<PipelineFlowState> evaluateComplexModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state, DataOORef<const Lines> trajectoryLines);

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Main function generating the trajectory lines.
    [[nodiscard]] Future<DataOORef<const Lines>> generateTrajectoryLines(ModifierEvaluationRequest request) const;

    /// Asks the modifier to replace a visual element owned by this modifier with a new compatible object, which is created
    /// on demand by the provided callback function. The callback function accepts an optional human-readable title, which will
    /// be used for the new visual element.
    virtual void replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement) override;

protected:

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// Controls which particles trajectories are created for.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, onlySelectedParticles, setOnlySelectedParticles);

    /// Controls whether the created trajectories span the entire animation interval or a sub-interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomInterval, setUseCustomInterval);

    /// The start of the custom frame interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customIntervalStart, setCustomIntervalStart);

    /// The end of the custom frame interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, customIntervalEnd, setCustomIntervalEnd);

    /// The sampling frequency for creating trajectories.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{1}, everyNthFrame, setEveryNthFrame);

    /// Controls whether trajectories are unwrapped when crossing periodic boundaries.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, unwrapTrajectories, setUnwrapTrajectories);

    /// Controls whether a particle property is sampled and transferred to the output trajectory lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, transferParticleProperties, setTransferParticleProperties);

    /// The particle property to be transferred onto the trajectory lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, particleProperty, setParticleProperty);

    /// The vis element for rendering the trajectory lines.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<LinesVis>, trajectoryVis, setTrajectoryVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

/**
 * Used by the GenerateTrajectoryLinesModifier to store the generated trajectory lines.
 */
class OVITO_PARTICLES_EXPORT GenerateTrajectoryLinesModificationNode : public ModificationNode
{
    OVITO_CLASS(GenerateTrajectoryLinesModificationNode)

public:

    /// Constructor.
    using ModificationNode::ModificationNode;

protected:

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

    /// Launches an asynchronous task to evaluate the node's modifier.
    virtual SharedFuture<PipelineFlowState> launchModifierEvaluation(ModifierEvaluationRequest&& request, SharedFuture<PipelineFlowState> inputFuture) override;

private:

    /// The asynchronous task object currently computing the trajectory lines.
    WeakSharedFuture<DataOORef<const Lines>> _trajectoryWeakFuture;

    /// The trajectory lines computed during the last successful modifier evaluation.
    DataOORef<const Lines> _trajectoryLines;

    friend class GenerateTrajectoryLinesModifier;
};

}   // End of namespace
