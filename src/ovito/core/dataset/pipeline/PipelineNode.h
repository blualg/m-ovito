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
#include <ovito/core/dataset/pipeline/ActiveObject.h>
#include <ovito/core/dataset/pipeline/PipelineCache.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>

namespace Ovito {

/**
 * \brief Base class for steps in a data pipeline.
 */
class OVITO_CORE_EXPORT PipelineNode : public ActiveObject
{
    OVITO_CLASS(PipelineNode)

public:

    /// Constructor.
    explicit PipelineNode(ObjectInitializationFlags flags, bool enableCaching = true);

    /// Throws an exception if the pipeline stage cannot be evaluated at this time. This is called by the system to catch user mistakes that would lead to infinite recursion.
    virtual void preEvaluationCheck() const {}

    /// Is called by the pipeline system before a new evaluation begins to query the validity interval and evaluation result type of this pipeline stage.
    virtual void preevaluate(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) {
        pipelineCache().preevaluatePipeline(request, evaluationTypes, validityInterval);
    }

    /// \brief Asks the pipeline stage to compute the results.
    virtual PipelineEvaluationResult evaluate(const PipelineEvaluationRequest& request) {
        return pipelineCache().evaluatePipeline(request);
    }

    /// \brief Asks the pipeline stage to compute the results for several animation times in a row.
    Future<std::vector<PipelineFlowState>> evaluateMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times);

    /// Returns the cached output of this data pipeline stage at the given time if available.
    /// This method will never throw an exception and doesn't require a valid execution context.
    virtual PipelineFlowState getCachedPipelineNodeOutput(AnimationTime time, bool interactiveMode = true) const {
        return pipelineCache().getAt(time, interactiveMode);
    }

    /// \brief Returns a list of pipelines that contain this node.
    /// \param onlyScenePipelines If true, pipelines which are currently not part of the scene are ignored.
    QSet<Pipeline*> pipelines(bool onlyScenePipelines) const;

    /// \brief Determines whether the data pipeline branches above this pipeline node,
    ///        i.e. whether this pipeline node has multiple dependents, all using this pipeline
    ///        object as input.
    ///
    /// \param onlyScenePipelines If true, branches to pipelines which are currently not part of the scene are ignored.
    bool isPipelineBranch(bool onlyScenePipelines) const;

    /// \brief Returns the number of animation frames this pipeline node can provide.
    virtual int numberOfSourceFrames() const { return 1; }

    /// \brief Given an animation time, computes the source frame to show.
    virtual int animationTimeToSourceFrame(AnimationTime time) const;

    /// \brief Given a source frame index, returns the animation time at which it is shown.
    virtual AnimationTime sourceFrameToAnimationTime(int frame) const;

    /// \brief Returns the human-readable labels associated with the animation frames (e.g. the simulation timestep numbers).
    virtual QMap<int, QString> animationFrameLabels() const { return {}; }

    /// Returns the data collection that is managed by this object (if it is a data source).
    /// The returned data collection will be displayed under the data source in the pipeline editor.
    virtual const DataCollection* getSourceDataCollection() const { return nullptr; }

    /// Rescales the times of all animation keys from the old animation interval to the new interval.
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

    /// Returns the internal output data cache of this pipeline node.
    const PipelineCache& pipelineCache() const { return _pipelineCache; }

    /// Returns the internal output data cache of this pipeline node.
    PipelineCache& pipelineCache() { return _pipelineCache; }

    /// Decides whether a preliminary viewport update is performed after this pipeline object has been
    /// evaluated but before the rest of the pipeline is complete.
    virtual bool shouldRefreshViewportsAfterEvaluation() { return false; }

protected:

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Loads the class' contents from an input stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// This function is called by the pipeline system before a new evaluation begins to query the validity interval and evaluation result type of this pipeline stage.
    virtual void preevaluateInternal(const PipelineEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) = 0;

    /// Asks the object for the result of the data pipeline.
    virtual SharedFuture<PipelineFlowState> evaluateInternal(const PipelineEvaluationRequest& request) = 0;

    /// Gets called by the PipelineCache whenever it returns a pipeline state from the cache.
    virtual PipelineEvaluationResult postprocessCachedState(const PipelineEvaluationRequest& request, const PipelineFlowState& state) { return state; }

private:

    /// Activates the precomputation of the pipeline results for all animation frames.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, pipelineTrajectoryCachingEnabled, setPipelineTrajectoryCachingEnabled, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// Cache for the data output of this pipeline stage.
    PipelineCache _pipelineCache;

    friend class PipelineCache;
};

}   // End of namespace
