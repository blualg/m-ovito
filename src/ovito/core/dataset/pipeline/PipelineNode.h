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
    OVITO_CLASSINFO("ClassNameAlias", "PipelineObject");         // For backward compatibility with OVITO 3.9.2
    OVITO_CLASSINFO("ClassNameAlias", "CachingPipelineObject");  // For backward compatibility with OVITO 3.9.2

public:

    /// Constructor.
    explicit PipelineNode(ObjectInitializationFlags flags);

    /// \brief Determines the time interval over which a computed pipeline state will remain valid.
    virtual TimeInterval validityInterval(const PipelineEvaluationRequest& request) const;

    /// \brief Asks the pipeline stage to compute the results.
    virtual SharedFuture<PipelineFlowState> evaluate(const PipelineEvaluationRequest& request);

    /// \brief Asks the pipeline stage to compute the results for several animation times in a row.
    Future<std::vector<PipelineFlowState>> evaluateMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times);

    /// \brief Asks the pipeline stage to compute the preliminary results in a synchronous fashion.
    virtual PipelineFlowState evaluateSynchronous(const PipelineEvaluationRequest& request);

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

protected:

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Loads the class' contents from an input stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Asks the object for the result of the data pipeline.
    virtual Future<PipelineFlowState> evaluateInternal(const PipelineEvaluationRequest& request) = 0;

    /// Gets called by the PipelineCache whenever it returns a pipeline state from the cache.
    virtual Future<PipelineFlowState> postprocessCachedState(const PipelineEvaluationRequest& request, const PipelineFlowState& state) {
        return Future<PipelineFlowState>::createImmediateEmplace(state);
    }

    /// Lets the pipeline stage compute a preliminary result in a synchronous fashion.
    virtual PipelineFlowState evaluateInternalSynchronous(const PipelineEvaluationRequest& request) {
        return PipelineFlowState(getSourceDataCollection(), status());
    }

    /// Decides whether a preliminary viewport update is performed after this pipeline object has been
    /// evaluated but before the rest of the pipeline is complete.
    virtual bool performPreliminaryUpdateAfterEvaluation() { return true; }

private:

    /// Activates the precomputation of the pipeline results for all animation frames.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, pipelineTrajectoryCachingEnabled, setPipelineTrajectoryCachingEnabled, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

    /// Cache for the data output of this pipeline stage.
    PipelineCache _pipelineCache;

    friend class PipelineCache;
};

}   // End of namespace
