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
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>

namespace Ovito {

/**
 * \brief This class holds the parameters for an evaluation request of a data pipeline.
 */
class OVITO_CORE_EXPORT PipelineEvaluationRequest
{
public:

    /// Constructs a request object for evaluating the pipeline at a certain animation time.
    PipelineEvaluationRequest(AnimationTime time = AnimationTime::fromFrame(0)) :
        _time(time),
        _cachingIntervals(time) {}

    /// Constructs a request object for evaluating the pipeline at the current animation time.
    PipelineEvaluationRequest(AnimationSettings* animationSettings) : PipelineEvaluationRequest(animationSettings->currentTime()) {}

    /// Constructs a request object for evaluating the pipeline using a prescribed caching pattern.
    explicit PipelineEvaluationRequest(const TimeIntervalUnion& cachingIntervals) : _cachingIntervals(cachingIntervals) {}

    /// Returns the animation time at which the pipeline is being evaluated.
    AnimationTime time() const { return _time; }

    /// Sets a new animation time at which the pipeline should be evaluated.
    void setTime(AnimationTime time) { _time = time; }

    /// Indicates whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the pipeline stages.
    bool throwOnError() const { return _throwOnError; }

    /// Sets whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the pipeline stages.
    void setThrowOnError(bool enable = true) { _throwOnError = enable; }

    /// Returns the animation time intervals over which the pipeline should pre-cache the state.
    const TimeIntervalUnion& cachingIntervals() const { return _cachingIntervals; }

    /// Returns a non-const reference to the animation time intervals over which the pipeline should pre-cache the state.
    TimeIntervalUnion& modifiableCachingIntervals() { return _cachingIntervals; }

private:

    /// The animation time at which the pipeline is being evaluated.
    AnimationTime _time = AnimationTime::fromFrame(0);

    /// Controls whether the pipeline system should abort the evaluation by throwing an exception as soon as a first error occurs in one of the modifiers.
    bool _throwOnError = false;

    /// Indicates to the upstream pipeline stages which animation frames they should keep in the cache.
    TimeIntervalUnion _cachingIntervals;
};

/**
 * \brief This helper class manages the evaluation of a PipelineSceneNode.
 */
class OVITO_CORE_EXPORT PipelineEvaluationFuture : public SharedFuture<PipelineFlowState>
{
public:

    /// Default constructor.
    PipelineEvaluationFuture() = default;

    /// Constructs a pipeline evaluation object for a given evaluation request.
    explicit PipelineEvaluationFuture(const PipelineEvaluationRequest& request) : _request(request) {}

    /// Constructs a pipeline evaluation object and initializes it with an existing future.
    explicit PipelineEvaluationFuture(const PipelineEvaluationRequest& request, SharedFuture<PipelineFlowState>&& future, Pipeline* pipeline = nullptr) :
        SharedFuture<PipelineFlowState>(std::move(future)),
        _request(request),
        _pipeline(pipeline) {}

    /// Resets the state of the pipeline evaluation.
    void reset() {
        SharedFuture<PipelineFlowState>::reset();
        _request.reset();
        _pipeline = nullptr;
    }

    /// Returns the animation time at which the pipeline is being evaluated.
    AnimationTime time() const { OVITO_ASSERT(_request); return _request->time(); }

    /// Returns the pipeline that is being evaluated.
    Pipeline* pipeline() const { return _pipeline; }

private:

    /// Request that triggered the pipeline evaluation.
    std::optional<PipelineEvaluationRequest> _request;

    /// Pipeline currently being evaluated.
    Pipeline* _pipeline = nullptr;
};

}   // End of namespace
