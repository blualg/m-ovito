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
#include "MoleculeOrientationalRelaxationModifier.h"
#include "OrientationTrajectoryAnalysisHelper.h"

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

VectorSubsetMode toVectorSubsetMode(MoleculeOrientationalRelaxationModifier::SelectionMode mode)
{
    switch(mode) {
    case MoleculeOrientationalRelaxationModifier::AllElements:
        return VectorSubsetMode::AllElements;
    case MoleculeOrientationalRelaxationModifier::SelectedAtTimeOrigin:
        return VectorSubsetMode::SelectedAtTimeOrigin;
    case MoleculeOrientationalRelaxationModifier::SelectedAtBothTimes:
        return VectorSubsetMode::SelectedAtBothTimes;
    }
    OVITO_ASSERT(false);
    return VectorSubsetMode::AllElements;
}

DescriptorMode toDescriptorMode(MoleculeOrientationalRelaxationModifier::DescriptorMode mode)
{
    switch(mode) {
    case MoleculeOrientationalRelaxationModifier::DipoleVector:
        return DescriptorMode::DipoleVector;
    case MoleculeOrientationalRelaxationModifier::AtomTypeCentroidVector:
        return DescriptorMode::AtomTypeCentroidVector;
    case MoleculeOrientationalRelaxationModifier::MatchingPairVector:
        return DescriptorMode::MatchingPairVector;
    }
    OVITO_ASSERT(false);
    return DescriptorMode::DipoleVector;
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(MoleculeOrientationalRelaxationModifier);
OVITO_CLASSINFO(MoleculeOrientationalRelaxationModifier, "DisplayName", "Molecule Orientational Relaxation");
OVITO_CLASSINFO(MoleculeOrientationalRelaxationModifier, "Description",
                "Compute a Legendre orientational relaxation function for molecule- or pair-based orientation vectors around reference atoms.");
OVITO_CLASSINFO(MoleculeOrientationalRelaxationModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, descriptorMode);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, fromTypeId);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, fromExpression);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, toTypeId);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, toExpression);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, referenceTypes);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, referenceExpression);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, anchorTypes);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, anchorExpression);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, cutoff);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, legendreOrder);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, selectionMode);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, useCustomFrameInterval);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, intervalStart);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, intervalEnd);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, samplingFrequency);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, maxLag);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier, runRequestId);
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, descriptorMode, "Descriptor");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, fromTypeId, "Direction start atom type");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, fromExpression, "Direction start expression");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, toTypeId, "Direction end atom type");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, toExpression, "Direction end expression");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, referenceTypes, "Orient around atom type(s)");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, referenceExpression, "Reference expression");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, anchorTypes, "Molecule site atom type(s)");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, anchorExpression, "Molecule site expression");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, cutoff, "Distance cutoff");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, legendreOrder, "Legendre order");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, selectionMode, "Vector subset");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, useCustomFrameInterval, "Restrict analysis interval");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, intervalStart, "Start frame");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, intervalEnd, "End frame");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, samplingFrequency, "Sample every Nth frame");
SET_PROPERTY_FIELD_LABEL(MoleculeOrientationalRelaxationModifier, maxLag, "Maximum lag (sampled-frame steps)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(MoleculeOrientationalRelaxationModifier, cutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(MoleculeOrientationalRelaxationModifier, legendreOrder, IntegerParameterUnit, 1, 12);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(MoleculeOrientationalRelaxationModifier, intervalStart, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(MoleculeOrientationalRelaxationModifier, intervalEnd, IntegerParameterUnit, 0, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(MoleculeOrientationalRelaxationModifier, samplingFrequency, IntegerParameterUnit, 1, std::numeric_limits<int>::max());
SET_PROPERTY_FIELD_UNITS_AND_RANGE(MoleculeOrientationalRelaxationModifier, maxLag, IntegerParameterUnit, 0, std::numeric_limits<int>::max());

IMPLEMENT_CREATABLE_OVITO_CLASS(MoleculeOrientationalRelaxationModificationNode);
DEFINE_REFERENCE_FIELD(MoleculeOrientationalRelaxationModificationNode, cachedResults);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModificationNode, cachedWarningText);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModificationNode, completedRunRequestId);
DEFINE_PROPERTY_FIELD(MoleculeOrientationalRelaxationModificationNode, cacheGenerationId);
SET_MODIFICATION_NODE_TYPE(MoleculeOrientationalRelaxationModifier, MoleculeOrientationalRelaxationModificationNode);

bool MoleculeOrientationalRelaxationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

void MoleculeOrientationalRelaxationModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);
}

void MoleculeOrientationalRelaxationModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);
}

QVariant MoleculeOrientationalRelaxationModifier::getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const
{
    return tr("P%1 %2").arg(legendreOrder()).arg(descriptorModeLabel(toDescriptorMode(descriptorMode())));
}

std::vector<int> MoleculeOrientationalRelaxationModifier::sampledFrames(const ModificationNode* modNode) const
{
    OVITO_ASSERT(modNode);

    const int numFrames = modNode->numberOfSourceFrames();
    if(numFrames <= 0)
        throw Exception(tr("The input trajectory does not provide any source frames for orientational relaxation analysis."));

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
        throw Exception(tr("Molecule orientational relaxation requires at least two sampled trajectory frames."));

    return result;
}

void MoleculeOrientationalRelaxationModifier::inputCachingHints(ModifierEvaluationRequest& request)
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

void MoleculeOrientationalRelaxationModifier::preevaluateModifier(const ModifierEvaluationRequest& request,
                                                                 PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                                                 TimeInterval& validityInterval) const
{
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

void MoleculeOrientationalRelaxationModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);
    iv.setEmpty();
}

Future<PipelineFlowState> MoleculeOrientationalRelaxationModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(MoleculeOrientationalRelaxationModificationNode* modNode =
           dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode())) {
        if(modNode->hasCachedResults() && runRequestId() <= modNode->completedRunRequestId())
            return applyCachedResults(request, std::move(state));

        if(runRequestId() <= modNode->completedRunRequestId()) {
            state.setStatus(PipelineStatus(tr(
                "Molecule orientational relaxation is idle. Open the Run section and click 'Run orientational relaxation analysis' to compute the selected observable.")));
            return std::move(state);
        }
    }

    if(request.interactiveMode()) {
        state.setStatus(PipelineStatus(tr(
            "Molecule orientational relaxation is queued. Click 'Run orientational relaxation analysis' to launch the full trajectory evaluation.")));
        return std::move(state);
    }

    return computeCorrelationData(request, std::move(state));
}

Future<PipelineFlowState> MoleculeOrientationalRelaxationModifier::computeCorrelationData(const ModifierEvaluationRequest& request,
                                                                                          PipelineFlowState&& state)
{
    const std::vector<int> frames = sampledFrames(request.modificationNode());
    const std::vector<std::vector<int>> frameBatches = buildFrameBatches(frames, 32);
    const int cacheGenerationId = dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode())
        ? dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode())->cacheGenerationId()
        : 0;

    VectorAccumulator accumulator;
    accumulator.frames.reserve(frames.size());
    if(selectionMode() != AllElements)
        accumulator.selectionFrames.reserve(frames.size());

    return for_each_sequential(
            frameBatches,
            DeferredObjectExecutor(this),
            [request = ModifierEvaluationRequest(request)](const std::vector<int>& frameBatch, VectorAccumulator&) mutable {
                std::vector<SharedFuture<PipelineFlowState>> batchFutures;
                batchFutures.reserve(frameBatch.size());
                for(int frame : frameBatch) {
                    ModifierEvaluationRequest frameRequest(request);
                    frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                    batchFutures.push_back(request.modificationNode()->evaluateInput(frameRequest).asFuture());
                }
                return when_all_futures(std::move(batchFutures));
            },
            [this](const std::vector<int>&, std::vector<SharedFuture<PipelineFlowState>> batchFutures, VectorAccumulator& accumulator) {
                const ReferenceShellDescriptorRequest descriptorRequest{
                    toDescriptorMode(descriptorMode()),
                    fromTypeId(),
                    fromExpression(),
                    toTypeId(),
                    toExpression(),
                    referenceTypes(),
                    referenceExpression(),
                    anchorTypes(),
                    anchorExpression(),
                    cutoff(),
                    onlySelectedParticles()
                };
                for(SharedFuture<PipelineFlowState>& future : batchFutures) {
                    this_task::throwIfCanceled();
                    appendDescriptorVectorSample(descriptorRequest,
                                                 accumulator,
                                                 future.result(),
                                                 tr("Molecule orientational relaxation"));
                }
            },
            std::move(accumulator))
        .then(DeferredObjectExecutor(this), [this, request, state = std::move(state), frames, cacheGenerationId](VectorAccumulator accumulator) mutable -> Future<PipelineFlowState> {
            OORef<MoleculeOrientationalRelaxationModifier> self(this);
            const int completedRunRequestId = runRequestId();

            return asyncLaunch([self = std::move(self),
                                request = ModifierEvaluationRequest(request),
                                state = std::move(state),
                                frames,
                                accumulator = std::move(accumulator),
                                completedRunRequestId,
                                cacheGenerationId]() mutable {
                CorrelationComputationResult computationResult{std::move(state)};
                if(!dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode()))
                    return computationResult;

                this_task::throwIfCanceled();
                const CorrelationCurves curves = computeVectorReorientationCurves(
                    accumulator, frames, self->legendreOrder(), toVectorSubsetMode(self->selectionMode()), self->maxLag(),
                    MoleculeOrientationalRelaxationModifier::tr("Molecule orientational relaxation"));

                computationResult.results = DataOORef<DataCollection>::create();
                const OOWeakRef<const PipelineNode> createdByNode = request.modificationNodeWeak();
                createLineTable(computationResult.results,
                                MoleculeOrientationalRelaxationModifier::correlationTableId(),
                                MoleculeOrientationalRelaxationModifier::tr("Molecule orientational relaxation"),
                                curves.lagFrames,
                                {curves.overall},
                                {MoleculeOrientationalRelaxationModifier::tr("P%1").arg(self->legendreOrder())},
                                MoleculeOrientationalRelaxationModifier::tr("Lag (source frames)"),
                                MoleculeOrientationalRelaxationModifier::tr("Orientational correlation"),
                                createdByNode);

                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.target"), accumulator.targetLabel, createdByNode);
                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.sampled_frame_count"), static_cast<double>(frames.size()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.sampled_item_count"), static_cast<double>(accumulator.itemCount), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.maximum_lag"), static_cast<double>(curves.lagFrames.empty() ? 0.0 : curves.lagFrames.back()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.legendre_order"), static_cast<double>(self->legendreOrder()), createdByNode);
                computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.selection_mode"), vectorSubsetModeLabel(toVectorSubsetMode(self->selectionMode())), createdByNode);
                if(!curves.overall.empty()) {
                    computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.zero_lag"), curves.overall.front(), createdByNode);
                    computationResult.results->setAttribute(QStringLiteral("MoleculeOrientationalRelaxation.final_value"), curves.overall.back(), createdByNode);
                }
                if(std::ranges::none_of(curves.overall, [](double value) { return std::isfinite(value); })) {
                    computationResult.warningText = MoleculeOrientationalRelaxationModifier::tr(
                        "No valid vector samples satisfied the current selection and normalization criteria.");
                }

                computationResult.completedRunRequestId = completedRunRequestId;
                computationResult.cacheGenerationId = cacheGenerationId;
                return computationResult;
            }).then(ObjectExecutor(this), [this, request = ModifierEvaluationRequest(request)](CorrelationComputationResult computationResult) mutable {
                MoleculeOrientationalRelaxationModificationNode* modNode =
                    dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode());
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

PipelineFlowState MoleculeOrientationalRelaxationModifier::applyCachedResults(const ModifierEvaluationRequest& request,
                                                                              PipelineFlowState state) const
{
    MoleculeOrientationalRelaxationModificationNode* modNode =
        dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(request.modificationNode());
    if(!modNode || !modNode->cachedResults())
        return state;

    state.mutableData()->adoptAttributesFrom(*modNode->cachedResults(), request.modificationNodeWeak());
    for(const DataOORef<const DataObject>& objectRef : modNode->cachedResults()->objects())
        state.addObjectWithUniqueId(objectRef.get());

    if(!modNode->cachedWarningText().isEmpty())
        state.combineStatus(PipelineStatus::Warning, modNode->cachedWarningText());

    return state;
}

void MoleculeOrientationalRelaxationModificationNode::invalidateCachedResults()
{
    setCachedResults(nullptr);
    setCachedWarningText(QString{});
    setCacheGenerationId(cacheGenerationId() + 1);
}

bool MoleculeOrientationalRelaxationModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input() || source == modifier())
            invalidateCachedResults();
    }
    return ModificationNode::referenceEvent(source, event);
}

}  // namespace Ovito
