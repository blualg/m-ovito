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


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/dataset/DataSet.h>
#include "PipelineFlowState.h"
#include "PipelineStatus.h"
#include "ModifierClass.h"
#include "PipelineEvaluationRequest.h"

namespace Ovito {

/**
 * \brief Base class for algorithms that operate on a PipelineFlowState.
 *
 * \sa ModificationNode
 */
class OVITO_CORE_EXPORT Modifier : public RefTarget
{
    OVITO_CLASS_META(Modifier, ModifierClass)

public:

    /// Gets called by the system when the modifier is being inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) {}

    /// Throws an exception if the pipeline stage cannot be evaluated at this time.
    /// This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck(const PipelineEvaluationRequest& request) const {}

    /// Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
    virtual void inputCachingHints(ModifierEvaluationRequest& request) {}

    /// This method is called by the ModificationNode to let the modifier adjust the time interval
    /// of a TargetChanged event received from the upstream pipeline before it is propagated to the
    /// downstream pipeline.
    virtual void restrictInputValidityInterval(TimeInterval& iv) const {}

    /// Indicates whether the interactive viewports should be updated after the modifier has computed
    /// its results and before the entire pipeline is complete.
    virtual bool shouldRefreshViewportsAfterEvaluation() { return false; }

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() { return false; }

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) {
        // Avoid a full recomputation if the modifier gets enabled/disabled or if just its title is changed.
        if(event.field() == PROPERTY_FIELD(isEnabled))
            return true;
        return false;
    }

    /// Lets the modifier render itself in an interactive viewport.
    virtual void renderModifierVisual(ModificationNode* modNode, Pipeline* pipeline, FrameGraph& frameGraph) {}

    /// Returns the list of pipeline nodes that share this modifier.
    ///
    /// The same modifier can be applied in several data pipelines.
    /// Each application of the modifier instance is represented by an instance of the ModificationNode class.
    /// This method can be used to determine all such nodes associated with this Modifier instance.
    QVector<ModificationNode*> nodes() const;

    /// Returns one of the pipelines nodes sharing this modifier.
    ModificationNode* someNode() const;

    /// Creates a new modification node for inserting this modifier into a pipeline.
    OORef<ModificationNode> createModificationNode();

    /// Returns the UI title of this modifier.
    virtual QString objectTitle() const override {
        if(title().isEmpty())
            return RefTarget::objectTitle();
        else
            return title();
    }

    /// Changes the title of this modifier.
    void setObjectTitle(const QString& title) { setTitle(title); }

    /// Returns the current combined status of the pipeline nodes that share this modifier.
    PipelineStatus globalStatus() const;

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const { return {}; }

    /// Returns the number of animation frames this modifier provides.
    virtual int numberOfOutputFrames(ModificationNode* node) const;

    /// Given an animation time, computes the source frame to show.
    virtual int animationTimeToSourceFrame(AnimationTime time, int inputFrame) const { return inputFrame; }

    /// Given a source frame index, returns the animation time at which it is shown.
    virtual AnimationTime sourceFrameToAnimationTime(int frame, AnimationTime inputTime) const { return inputTime; }

    /// Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels(QMap<int, QString> inputLabels) const { return inputLabels; }

protected:

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const {}

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) = 0;

private:

    /// Dndicates whether the modifier is currently enabled. A disabled modifier will be skipped during pipeline evaluation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, isEnabled, setEnabled);

    /// A user-defined title of this modifier, which overrides the default title provided by the modifier class.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, title, setTitle);

    friend ModificationNode;
};

}   // End of namespace
