////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/DeferredObjectExecutor.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/ObjectExecutor.h>
#include <ovito/core/utilities/concurrent/WhenAll.h>
#include <ovito/stdobj/table/DataTable.h>
#include "OrientationTrajectoryAnalysisHelper.h"
#include "SurvivalProbabilityModifier.h"

#include <algorithm>
#include <limits>

namespace Ovito {

namespace {

using namespace OrientationTrajectoryAnalysis;

struct CorrelationComputationResult {
    PipelineFlowState state;
    DataOORef<DataCollection> results;
    QString warningText;
    int completedRunRequestId = 0;
    int cacheGenerationId = 0;
};

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(SurvivalProbabilityModifier);
OVITO_CLASSINFO(SurvivalProbabilityModifier, "DisplayName", "Survival Probability");
OVITO_CLASSINFO(SurvivalProbabilityModifier, "Description",
                "Compute the survival probability of molecules remaining inside a reference shell over the trajectory.");
OVITO_CLASSINFO(SurvivalProbabilityModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, referenceTypes);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, referenceExpression);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, anchorTypes);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, anchorExpression);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, cutoff);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, intermittency);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, intervalStart);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, maxLag);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, referenceTypes, "Orient around atom type(s)");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, referenceExpression, "Reference expression");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, anchorTypes, "Molecule site atom type(s)");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, anchorExpression, "Molecule site expression");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, cutoff, "Distance cutoff");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, intermittency, "Intermittency");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_LABEL(SurvivalProbabilityModifier, maxLag, "Maximum lag (sampled-frame steps)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SurvivalProbabilityModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurvivalProbabilityModifier, intermittency, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurvivalProbabilityModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurvivalProbabilityModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurvivalProbabilityModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SurvivalProbabilityModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(SurvivalProbabilityModificationNode);
DEFINE_REFERENCE_FIELD(SurvivalProbabilityModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(SurvivalProbabilityModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(SurvivalProbabilityModifier, SurvivalProbabilityModificationNode);

bool SurvivalProbabilityModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void SurvivalProbabilityModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

void SurvivalProbabilityModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);
}

QVariant SurvivalProbabilityModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    return tr("SP shell");
}

std::vector<int> SurvivalProbabilityModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for survival probability analysis."));

    const int stride = std::max(1, samplingFrequency());

    int firstFrame = 0;
    int lastFrame = numFrames - 1;
    if(useCustomFrameInterval()) {
        firstFrame = std::clamp(intervalStart(), 0, numFrames - 1);
        lastFrame = std::clamp(intervalEnd(), 0, numFrames - 1);
        if(firstFrame > lastFrame)
            std::swap(firstFrame, lastFrame);
    }

    std::vector<int> result;
    result.reserve(((lastFrame - firstFrame) / stride) + 1);
    for(int frame = firstFrame; frame <= lastFrame; frame += stride)
        result.push_back(frame);

    if(result.size() < 2)
        throw Exception(tr("Survival probability analysis requires at least two sampled trajectory frames."));

    return result;
}

void SurvivalProbabilityModifier::inputCachingHints(ModifierEvaluationRequest& request)
{
    if(request.modificationNode()->numberOfSourceFrames() > 0) {
        const std::vector<int> frames = sampledFrames(request.modificationNode());
        if(!frames.empty()) {
            request.mutableCachingIntervals().add(TimeInterval(
                request.modificationNode()->sourceFrameToAnimationTime(frames.front()),
                request.modificationNode()->sourceFrameToAnimationTime(frames.back())));
        }
    }

    Modifier::inputCachingHints(request);
}

void SurvivalProbabilityModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                      PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                      TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

void SurvivalProbabilityModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

Future<PipelineFlowState> SurvivalProbabilityModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(SurvivalProbabilityModificationNode* modNode = dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Survival probability is idle. Open the Run section and click 'Run survival probability analysis' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Survival probability is queued. Click 'Run survival probability analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeCorrelationData(request, std::move(state));
}

Future<PipelineFlowState> SurvivalProbabilityModifier::computeCorrelationData(const ModifierEvaluationRequest& request,
                                                                              PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode())
        ? dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    MembershipAccumulator accumulator;
    accumulator.membershipFrames.reserve(frames.size());

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, MembershipAccumulator&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [this](const std::vector<int>&, std::vector<SharedFuture<PipelineFlowState>> batchFutures, MembershipAccumulator& accumulator) {
                const ReferenceShellMembershipRequest membershipRequest{
                    referenceTypes(),
                    referenceExpression(),
                    anchorTypes(),
                    anchorExpression(),
                    cutoff(),
                    onlySelectedParticles()
                };
                for(SharedFuture<PipelineFlowState>& future : batchFutures) {
                    this_task::throwIfCanceled();
                    appendReferenceShellMembershipSample(membershipRequest,
                                                         accumulator,
                                                         future.result(),
                                                         tr("Survival probability"));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this), [this, request, state = std::move(state), frames, cacheGenerationId](MembershipAccumulator accumulator) mutable -> Future<PipelineFlowState> {
            OORef<SurvivalProbabilityModifier> self(this);
            const int completedRunRequestId = runRequestId();

            return asyncLaunch([self = std::move(self),
                                request = ModifierEvaluationRequest(request),
                                state = std::move(state),
                                frames,
                                accumulator = std::move(accumulator),
                                completedRunRequestId,
                                cacheGenerationId]() mutable {
                CorrelationComputationResult computationResult{std::move(state)};
                if(!dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode()))
                    return computationResult;

                this_task::throwIfCanceled();
                const CorrelationCurves curves = computeSurvivalProbabilityCurves(
                    accumulator, frames, self->intermittency(), self->maxLag(), SurvivalProbabilityModifier::tr("Survival probability"));

                computationResult.results = DataOORef<DataCollection>::create();
                const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
                createLineTable(computationResult.results,
                                SurvivalProbabilityModifier::correlationTableId(),
                                SurvivalProbabilityModifier::tr("Survival probability"),
                                curves.lagFrames,
                                {curves.overall},
                                {SurvivalProbabilityModifier::tr("SP")},
                                SurvivalProbabilityModifier::tr("Lag (source frames)"),
                                SurvivalProbabilityModifier::tr("Survival probability"),
                                createdByNode);

                computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.target"), accumulator.targetLabel, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.sampled_item_count"), static_cast<double>(accumulator.itemCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.maximum_lag"), static_cast<double>(curves.lagFrames.empty() ? 0.0 : curves.lagFrames.back()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.intermittency"), static_cast<double>(self->intermittency()), createdByNode);
                if(!curves.overall.empty()) {
                    computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.zero_lag"), curves.overall.front(), createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("SurvivalProbability.final_value"), curves.overall.back(), createdByNode);
                }
                if(std::ranges::none_of(curves.overall, [](double value) { return std::isfinite(value); })) {
                    computationResult.warningText = SurvivalProbabilityModifier::tr(
                        "No valid membership samples were available for survival probability analysis.");
                }

                computationResult.completedRunRequestId = completedRunRequestId;
                computationResult.cacheGenerationId = cacheGenerationId;
                return computationResult;
            }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](CorrelationComputationResult computationResult) mutable {
                SurvivalProbabilityModificationNode* modNode =
                    dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode());
                if(!modNode || !computationResult.results)
                    return std::move(computationResult.state);

                if(modNode->cacheGenerationId() != computationResult.cacheGenerationId || runRequestId() != computationResult.completedRunRequestId)
                    return std::move(computationResult.state);

                modNode->setCachedResults(computationResult.results);
                modNode->setCachedWarningText(computationResult.warningText);
                modNode->setCompletedRunRequestId(computationResult.completedRunRequestId);
                return applyCachedResults(request, std::move(computationResult.state));
            });
        });
}

PipelineFlowState SurvivalProbabilityModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                 PipelineFlowState state) const
{
    SurvivalProbabilityModificationNode* modNode =
        dynamic_object_cast<SurvivalProbabilityModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

void SurvivalProbabilityModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

bool SurvivalProbabilityModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
