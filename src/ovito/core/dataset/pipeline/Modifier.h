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

    /// Constructor.
    explicit Modifier(ObjectInitializationFlags flags);

    /// \brief This method is called by the system when the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) {}

    /// \brief Throws an exception if the pipeline stage cannot be evaluated at this time. This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck() const {}

    /// \brief Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
    virtual void inputCachingHints(ModifierEvaluationRequest& request) {}

    /// \brief This method is called by the ModificationNode to let the modifier adjust the time interval
    /// of a TargetChanged event received from the upstream pipeline before it is propagated to the
    /// downstream pipeline.
    virtual void restrictInputValidityInterval(TimeInterval& iv) const {}

    /// \brief Indicates whether the interactive viewports should be updated after the modifier has computed
    ///        its results and before the entire pipeline is complete.
    virtual bool shouldRefreshViewportsAfterEvaluation() { return false; }

    /// \brief Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    ///        been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() { return false; }

    /// \brief Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) { return false; }

    /// \brief Lets the modifier render itself in an interactive viewport.
    virtual void renderModifierVisual(ModificationNode* modNode, Pipeline* pipeline, FrameGraph& frameGraph) {}

    /// \brief Returns the list of pipeline nodes that reference this modifier.
    /// \return The list of ModificationNode objects, each describing a particular use of this Modifier in a pipeline.
    ///
    /// The same modifier can be applied in several data pipelines.
    /// Each application of the modifier instance is represented by an instance of the ModificationNode class.
    /// This method can be used to determine all such nodes associated with this Modifier instance.
    QVector<ModificationNode*> nodes() const;

    /// \brief Returns one of the pipelines nodes referencing this modifier in a pipeline.
    ModificationNode* someNode() const;

    /// \brief Creates a new modification node for inserting this modifier into a pipeline.
    OORef<ModificationNode> createModificationNode();

    /// \brief Returns the title of this modifier object.
    virtual QString objectTitle() const override {
        if(title().isEmpty())
            return RefTarget::objectTitle();
        else
            return title();
    }

    /// \brief Changes the title of this modifier.
    void setObjectTitle(const QString& title) { setTitle(title); }

    /// \brief Returns the current status of the modifier's applications.
    PipelineStatus globalStatus() const;

    /// \brief Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const { return {}; }

    /// \brief Returns the number of animation frames this modifier provides.
    virtual int numberOfOutputFrames(ModificationNode* node) const;

    /// \brief Given an animation time, computes the source frame to show.
    virtual int animationTimeToSourceFrame(AnimationTime time, int inputFrame) const { return inputFrame; }

    /// \brief Given a source frame index, returns the animation time at which it is shown.
    virtual AnimationTime sourceFrameToAnimationTime(int frame, AnimationTime inputTime) const { return inputTime; }

    /// \brief Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels(QMap<int, QString> inputLabels) const { return inputLabels; }

protected:

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const {}

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) = 0;

private:

    /// Flag that indicates whether the modifier is enabled.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, isEnabled, setEnabled);

    /// The user-defined title of this modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);

    friend ModificationNode;
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::Modifier*);
