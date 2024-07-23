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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PipelineNode);
OVITO_CLASSINFO(PipelineNode, "ClassNameAlias", "PipelineObject");         // For backward compatibility with OVITO 3.9.2
OVITO_CLASSINFO(PipelineNode, "ClassNameAlias", "CachingPipelineObject");  // For backward compatibility with OVITO 3.9.2
DEFINE_PROPERTY_FIELD(PipelineNode, pipelineTrajectoryCachingEnabled);
SET_PROPERTY_FIELD_LABEL(PipelineNode, pipelineTrajectoryCachingEnabled, "Precompute all trajectory frames");

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void PipelineNode::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(pipelineTrajectoryCachingEnabled)) {
        pipelineCache().setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());

        // Send target changed event to trigger a new pipeline evaluation, which is
        // needed to start the precomputation process.
        if(pipelineTrajectoryCachingEnabled())
            notifyTargetChanged(PROPERTY_FIELD(pipelineTrajectoryCachingEnabled));
    }

    ActiveObject::propertyChanged(field);
}

/******************************************************************************
* Returns a list of pipelines that contain this node.
******************************************************************************/
QSet<Pipeline*> PipelineNode::pipelines(bool onlyScenePipelines) const
{
    QSet<Pipeline*> pipelinesList;
    visitDependents([&](RefMaker* dependent) {
        if(PipelineNode* node = dynamic_object_cast<PipelineNode>(dependent)) {
            pipelinesList.unite(node->pipelines(onlyScenePipelines));
        }
        else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->head() == this) {
                if(!onlyScenePipelines || pipeline->isInScene())
                    pipelinesList.insert(pipeline);
            }
        }
    });
    return pipelinesList;
}

/******************************************************************************
* Determines whether the data pipeline branches above this pipeline node,
* i.e. whether this pipeline node has multiple dependents, all using this pipeline
* node as input.
******************************************************************************/
bool PipelineNode::isPipelineBranch(bool onlyScenePipelines) const
{
    int pipelineCount = 0;
    visitDependents([&](RefMaker* dependent) {
        if(ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent)) {
            if(node->input() == this && !node->pipelines(onlyScenePipelines).empty())
                pipelineCount++;
        }
        else if(Pipeline* pipeline = dynamic_object_cast<Pipeline>(dependent)) {
            if(pipeline->head() == this) {
                if(!onlyScenePipelines || pipeline->isInScene())
                    pipelineCount++;
            }
        }
    });
    return pipelineCount > 1;
}

/******************************************************************************
* Given an animation time, computes the source frame to show.
******************************************************************************/
int PipelineNode::animationTimeToSourceFrame(AnimationTime time) const
{
    OVITO_ASSERT(time != AnimationTime::negativeInfinity());
    OVITO_ASSERT(time != AnimationTime::positiveInfinity());
    return time.frame();
}

/******************************************************************************
* Given a source frame index, returns the animation time at which it is shown.
******************************************************************************/
AnimationTime PipelineNode::sourceFrameToAnimationTime(int frame) const
{
    return AnimationTime::fromFrame(frame);
}

/******************************************************************************
* Loads the class' contents from an input stream.
******************************************************************************/
void PipelineNode::loadFromStream(ObjectLoadStream& stream)
{
    ActiveObject::loadFromStream(stream);

    // Transfer the caching flag loaded from the state file to the internal cache instance.
    pipelineCache().setPrecomputeAllFrames(pipelineTrajectoryCachingEnabled());
}

/******************************************************************************
* Rescales the times of all animation keys from the old animation interval to the new interval.
******************************************************************************/
void PipelineNode::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
    ActiveObject::rescaleTime(oldAnimationInterval, newAnimationInterval);
    pipelineCache().invalidate();
}

/******************************************************************************
* Asks the pipeline stage to compute the results for several animation times.
******************************************************************************/
Future<std::vector<PipelineFlowState>> PipelineNode::evaluateMultiple(const PipelineEvaluationRequest& request, std::vector<AnimationTime> times)
{
    // This function should only be used to request final pipeline results, not preliminary results.
    OVITO_ASSERT(request.interactiveMode() == false);

    // Perform the evaluation for all requested animation frames.
    return for_each_sequential(
        std::move(times),
        ObjectExecutor(this, true), // require deferred execution
        // Iteration start function:
        [request = PipelineEvaluationRequest(request), this](AnimationTime time, std::vector<PipelineFlowState>&) mutable {
            request.setTime(time);
            return this->evaluate(request).asFuture();
        },
        // Iteration complete function:
        [](AnimationTime time, PipelineFlowState&& result, std::vector<PipelineFlowState>& outputList) {
            outputList.push_back(std::move(result));
        },
        std::vector<PipelineFlowState>{});
}

}   // End of namespace
