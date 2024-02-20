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
#include "PipelineEvaluation.h"

namespace Ovito {

/**
 * \brief Base class for algorithms that operate on a PipelineFlowState.
 *
 * \sa ModificationNode
 */
class OVITO_CORE_EXPORT Modifier : public RefTarget
{
    OVITO_CLASS_META(Modifier, ModifierClass)
    OVITO_CLASSINFO("ClassNameAlias", "AsynchronousModifier");  // For backward compatibility with OVITO 3.10.2

public:

    /// Constructor.
    explicit Modifier(ObjectInitializationFlags flags);

    /// \brief Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state);

    /// \brief Throws an exception if the pipeline stage cannot be evaluated at this time. This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck() const {}

    /// \brief Determines the time interval over which a computed pipeline state will remain valid.
    virtual TimeInterval validityInterval(const ModifierEvaluationRequest& request) const { return TimeInterval::infinite(); }

    /// \brief Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
    virtual void inputCachingHints(TimeIntervalUnion& cachingIntervals, ModificationNode* node) {}

    /// \brief This method is called by the ModificationNode to let the modifier adjust the time interval
    /// of a TargetChanged event received from the upstream pipeline before it is propagated to the
    /// downstream pipeline.
    virtual void restrictInputValidityInterval(TimeInterval& iv) const {}

    /// \brief Lets the modifier render itself in an interactive viewport.
    virtual void renderModifierVisual(const ModifierEvaluationRequest& request, Pipeline* pipeline, FrameGraph& frameGraph) {}

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

    /// \brief This method is called by the system when the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) {}

    /// \brief Indicates whether a preliminary viewport update is performed after the modifier has been
    ///        evaluated but before the entire pipeline evaluation is complete.
    virtual bool performPreliminaryUpdateAfterEvaluation() { return true; }

    /// \brief Indicates whether this modifier wants preliminary viewport updates whenever its parameters change.
    virtual bool performPreliminaryUpdateAfterChange() { return false; }

    /// \brief Returns the number of animation frames this modifier provides.
    virtual int numberOfOutputFrames(ModificationNode* node) const;

    /// \brief Given an animation time, computes the source frame to show.
    virtual int animationTimeToSourceFrame(AnimationTime time, int inputFrame) const { return inputFrame; }

    /// \brief Given a source frame index, returns the animation time at which it is shown.
    virtual AnimationTime sourceFrameToAnimationTime(int frame, AnimationTime inputTime) const { return inputTime; }

    /// \brief Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels(QMap<int, QString> inputLabels) const { return inputLabels; }

protected:

    /// \brief Modifies the input data.
    virtual Future<PipelineFlowState> evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input);

    /// Creates a computation engine that will compute the modifier's results asynchronously.
    virtual Future<ModifierEnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) { return {}; }

private:

    /// Flag that indicates whether the modifier is enabled.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, isEnabled, setEnabled);

    /// The user-defined title of this modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, title, setTitle);

    friend ModificationNode;
};

}   // End of namespace

Q_DECLARE_METATYPE(Ovito::Modifier*);

#include "ModifierEngine.h"
