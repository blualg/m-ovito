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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/dataset/pipeline/ComplexModifierEvaluationTask.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include "GenerateTrajectoryLinesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GenerateTrajectoryLinesModifier);
OVITO_CLASSINFO(GenerateTrajectoryLinesModifier, "DisplayName", "Generate trajectory lines");
OVITO_CLASSINFO(GenerateTrajectoryLinesModifier, "Description", "Visualize trajectory lines of moving particles.");
OVITO_CLASSINFO(GenerateTrajectoryLinesModifier, "ModifierCategory", "Visualization");
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, useCustomInterval);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, customIntervalStart);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, customIntervalEnd);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, everyNthFrame);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, unwrapTrajectories);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, transferParticleProperties);
DEFINE_PROPERTY_FIELD(GenerateTrajectoryLinesModifier, particleProperty);
DEFINE_REFERENCE_FIELD(GenerateTrajectoryLinesModifier, trajectoryVis);
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, onlySelectedParticles, "Only selected particles");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, useCustomInterval, "Custom time interval");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, customIntervalStart, "Custom interval start");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, customIntervalEnd, "Custom interval end");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, everyNthFrame, "Every Nth frame");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, unwrapTrajectories, "Unwrap trajectories");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, transferParticleProperties, "Sample particle property");
SET_PROPERTY_FIELD_LABEL(GenerateTrajectoryLinesModifier, particleProperty, "Particle property");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(GenerateTrajectoryLinesModifier, everyNthFrame, IntegerParameterUnit, 1);

IMPLEMENT_CREATABLE_OVITO_CLASS(GenerateTrajectoryLinesModificationNode);
OVITO_CLASSINFO(GenerateTrajectoryLinesModificationNode, "ClassNameAlias", "GenerateTrajectoryLinesModifierApplication");  // For backward compatibility with OVITO 3.9.2
SET_MODIFICATION_NODE_TYPE(GenerateTrajectoryLinesModifier, GenerateTrajectoryLinesModificationNode);

/******************************************************************************
* Constructor.
******************************************************************************/
void GenerateTrajectoryLinesModifier::initializeObject(ObjectInitializationFlags flags)
{
    Modifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the vis element for rendering the trajectories created by the modifier.
        setTrajectoryVis(OORef<LinesVis>::create(flags));
        trajectoryVis()->setTitle(tr("Trajectory lines"));
    }
}

/******************************************************************************
* Replaces any references the modifier has to the given visual element with a new compatible object.
******************************************************************************/
void GenerateTrajectoryLinesModifier::replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement)
{
    if(trajectoryVis() == visElement)
        setTrajectoryVis(static_object_cast<LinesVis>(getReplacement(tr("Trajectory lines"))));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted into a pipeline.
******************************************************************************/
void GenerateTrajectoryLinesModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(this_task::isInteractive()) {
        auto [firstFrame, lastFrame] = this_task::ui()->datasetContainer().currentAnimationInterval();
        setCustomIntervalStart(firstFrame);
        setCustomIntervalEnd(lastFrame);
    }
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool GenerateTrajectoryLinesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void GenerateTrajectoryLinesModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
 * Modifies the input data.
 ******************************************************************************/
Future<PipelineFlowState> GenerateTrajectoryLinesModifier::evaluateComplexModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state, DataOORef<const Lines> trajectoryLines)
{
    OVITO_ASSERT(state);
    if(trajectoryLines)
        state.addObject(std::move(trajectoryLines));
    return std::move(state);
}

/******************************************************************************
 * Main function generating the trajectory lines.
 ******************************************************************************/
Future<DataOORef<const Lines>> GenerateTrajectoryLinesModifier::generateTrajectoryLines(ModifierEvaluationRequest request) const
{
    // Copy a few things into the coroutine context.
    const bool transferParticleProperties = this->transferParticleProperties();
    const bool onlySelectedParticles = this->onlySelectedParticles();
    const bool unwrapTrajectories = this->unwrapTrajectories();
    const PropertyReference particleProperty = this->particleProperty();
    OORef<LinesVis> trajectoryVis = this->trajectoryVis();
    OORef<GenerateTrajectoryLinesModificationNode> modNode = static_object_cast<GenerateTrajectoryLinesModificationNode>(request.modificationNode());
    PipelineFlowState firstState;
    std::vector<int> sampleFrames;
    std::vector<size_t> selectedIndices;
    std::set<int64_t> selectedIdentifiers;
    std::vector<Point3> pointData;
    std::vector<int32_t> timeData;
    std::vector<int64_t> idData;
    std::vector<DataBuffer::Byte> samplingPropertyData;
    std::vector<AffineTransformation> cellMatrices;
    std::vector<AffineTransformation> inverseCellMatrices;
    std::array<bool, 3> cellPbcFlags;

    // Determine time interval over which to generate trajectories and which need to be fetched from the upstream pipeline.
    int startFrame = useCustomInterval() ? customIntervalStart() : 0;
    int endFrame = useCustomInterval() ? (customIntervalEnd() + 1) : modNode->numberOfSourceFrames();
    if(endFrame <= startFrame || everyNthFrame() < 1)
        throw Exception(tr("Trajectory range contains zero frames. Cannot generate trajectory lines over this time interval."));
    auto frame_range = boost::irange(startFrame, endFrame, everyNthFrame());
    int frameCount = boost::size(frame_range);

    // Report progress in the GUI.
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Generating trajectory lines"));
    progress.setMaximum(frameCount);
    modNode->setStatus(tr("Processing %1 trajectory frames...").arg(frameCount));

    // Asynchronous loop over all input animation frames.
    for(int frame : frame_range) {
        // Evaluate upstream pipeline at current frame.
        request.setTime(modNode->sourceFrameToAnimationTime(frame));
        PipelineFlowState state = co_await FutureAwaiter(DeferredObjectExecutor(modNode), modNode->evaluateInput(request).asFuture());

        const Particles* particles = state.getObject<Particles>();
        if(!particles)
            throw Exception(tr("There are no particles in the modifier's input at frame %1.").arg(frame));
        particles->verifyIntegrity();

        // Collect the list of frames which have sampled so far.
        int timeIndex = sampleFrames.size();
        sampleFrames.push_back(frame);

        if(timeIndex == 0) {
            // Store the first frame of the trajectory for later use.
            firstState = state;

            // Determine which input particles to collect.
            if(_onlySelectedParticles) {
                if(BufferReadAccess<SelectionIntType> selectionProperty = particles->getProperty(Particles::SelectionProperty)) {
                    BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                    if(identifierProperty && identifierProperty.size() == selectionProperty.size()) {
                        const auto* s = selectionProperty.cbegin();
                        for(auto id : identifierProperty)
                            if(*s++) selectedIdentifiers.insert(id);
                    }
                    else {
                        const auto* s = selectionProperty.cbegin();
                        for(size_t index = 0; index < selectionProperty.size(); index++)
                            if(*s++) selectedIndices.push_back(index);
                    }
                }
                if(selectedIndices.empty() && selectedIdentifiers.empty())
                    throw Exception(tr("Particle selection has not been defined or is empty in first trajectory frame."));
            }
        }

        // Get the particle property to be sampled.
        RawBufferReadAccess particleSamplingProperty;
        if(transferParticleProperties) {
            if(!particleProperty)
                throw Exception(tr("Please select a particle property to be sampled."));
            particleSamplingProperty = particleProperty.findInContainer(particles);
            if(!particleSamplingProperty)
                throw Exception(tr("The particle property '%1' to be sampled and transferred to the trajectory lines does not exist (at frame %2). "
                    "Perhaps you need to restrict the sampling time interval to those times where the property is available.").arg(particleProperty.name()).arg(frame));
        }

        BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
        if(onlySelectedParticles) {
            if(!selectedIdentifiers.empty()) {
                BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                if(!identifierProperty)
                    throw Exception(tr("Input particles do not have identifiers at frame %1.").arg(frame));

                // Create a mapping from IDs to indices.
                std::unordered_map<int64_t, size_t> idmap;
                size_t index = 0;
                for(auto id : identifierProperty)
                    idmap.insert(std::make_pair(id, index++));

                for(auto id : selectedIdentifiers) {
                    if(auto entry = idmap.find(id); entry != idmap.end()) {
                        pointData.push_back(posProperty[entry->second]);
                        timeData.push_back(timeIndex);
                        idData.push_back(id);
                        if(particleSamplingProperty) {
                            const auto* dataBegin = particleSamplingProperty.cdata(entry->second, 0);
                            samplingPropertyData.insert(samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
                        }
                    }
                }
            }
            else {
                // Add coordinates of selected particles by index.
                for(auto index : selectedIndices) {
                    if(index < posProperty.size()) {
                        pointData.push_back(posProperty[index]);
                        timeData.push_back(timeIndex);
                        idData.push_back(index);
                        if(particleSamplingProperty) {
                            const auto* dataBegin = particleSamplingProperty.cdata(index, 0);
                            samplingPropertyData.insert(samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
                        }
                    }
                }
            }
        }
        else {
            // Add coordinates of all particles.
            pointData.insert(pointData.end(), posProperty.cbegin(), posProperty.cend());
            BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
            if(identifierProperty && identifierProperty.size() == posProperty.size()) {
                // Particles with IDs.
                idData.insert(idData.end(), identifierProperty.cbegin(), identifierProperty.cend());
            }
            else {
                // Particles without IDs.
                idData.resize(idData.size() + posProperty.size());
                std::iota(idData.begin() + timeData.size(), idData.end(), 0);
            }
            timeData.resize(timeData.size() + posProperty.size(), timeIndex);
            if(particleSamplingProperty)
                samplingPropertyData.insert(samplingPropertyData.end(), particleSamplingProperty.cdata(), particleSamplingProperty.cdata() + particleSamplingProperty.size() * particleSamplingProperty.stride());
        }

        // Obtain the simulation cell geometry at the current animation time.
        if(_unwrapTrajectories) {
            if(const SimulationCell* cell = state.getObject<SimulationCell>()) {
                cellMatrices.push_back(cell->matrix());
                inverseCellMatrices.push_back(cell->inverseMatrix());
                cellPbcFlags = cell->pbcFlagsCorrected();
            }
            else {
                cellMatrices.emplace_back(AffineTransformation::Zero());
                inverseCellMatrices.emplace_back(AffineTransformation::Zero());
            }
        }

        progress.incrementValue();
    }

    // After each frame of the input trajectory has been processed, build the final lines.
    // This can be performed in a separate thread.
    co_await ExecutorAwaiter(ThreadPoolExecutor());

    // Sort vertex data to obtain continuous trajectory lines.
    progress.setMaximum(0);
    progress.setText(tr("Sorting trajectory data"));
    std::vector<size_t> permutation(pointData.size());
    boost::algorithm::iota(permutation, (size_t)0);
    std::ranges::sort(permutation, [&](size_t a, size_t b) {
        if(idData[a] < idData[b]) return true;
        if(idData[a] > idData[b]) return false;
        return timeData[a] < timeData[b];
    });
    this_task::throwIfCanceled();

    // Create the lines data container object.
    DataOORef<Lines> trajectoryLines = DataOORef<Lines>::create();
    trajectoryLines->setCreatedByNode(modNode);
    trajectoryLines->setTitle(tr("Particle trajectories"));
    trajectoryLines->setIdentifier(firstState.generateUniqueIdentifier<Lines>(QStringLiteral("trajectories")));

    // Copy re-ordered trajectory points.
    trajectoryLines->setElementCount(pointData.size());
    BufferWriteAccess<Point3, access_mode::discard_read_write> trajPosProperty = trajectoryLines->createProperty(Lines::PositionProperty);
    auto piter = permutation.cbegin();
    for(Point3& p : trajPosProperty) {
        p = pointData[*piter++];
    }

    // Copy re-ordered trajectory time stamps.
    BufferWriteAccess<int32_t, access_mode::discard_write> trajTimeProperty = trajectoryLines->createProperty(Lines::SampleTimeProperty);
    piter = permutation.cbegin();
    for(int& t : trajTimeProperty) {
        t = sampleFrames[timeData[*piter++]];
    }

    // Copy re-ordered trajectory ids.
    BufferWriteAccess<int64_t, access_mode::discard_read_write> trajIdProperty = trajectoryLines->createProperty(Lines::SectionProperty);
    piter = permutation.cbegin();
    for(int64_t& id : trajIdProperty) {
        id = idData[*piter++];
    }

    // Create the trajectory line property receiving the sampled particle property values.
    if(transferParticleProperties && particleProperty && !particleProperty.isStandardProperty(&Particles::OOClass(), Particles::PositionProperty)) {
        if(const Property* inputProperty = particleProperty.findInContainer(firstState.expectObject<Particles>())) {
            OVITO_ASSERT(samplingPropertyData.size() == inputProperty->stride() * trajectoryLines->elementCount());
            if(samplingPropertyData.size() != inputProperty->stride() * trajectoryLines->elementCount())
                throw Exception(tr("Sampling buffer size mismatch. Sampled particle property '%1' seems to have a varying component count.").arg(inputProperty->name()));

            // Create a corresponding output property of the trajectory lines.
            RawBufferAccess<access_mode::discard_write> samplingProperty;
            if(inputProperty->typeId() < Property::FirstSpecificProperty && Lines::OOClass().isValidStandardPropertyId(inputProperty->typeId())) {
                // Input particle property is also a standard property for trajectory lines.
                samplingProperty = trajectoryLines->createProperty(inputProperty->typeId());
                OVITO_ASSERT(samplingProperty.dataType() == inputProperty->dataType());
                OVITO_ASSERT(samplingProperty.stride() == inputProperty->stride());
            }
            else if(Lines::OOClass().standardPropertyTypeId(inputProperty->name()) != 0) {
                // Input property name is that of a standard property for trajectory lines.
                // Must rename the property to avoid naming conflict, because user properties may not have a standard property name.
                QString newPropertyName = inputProperty->name() + QStringLiteral("_particles");
                samplingProperty = trajectoryLines->createProperty(newPropertyName, inputProperty->dataType(), inputProperty->componentCount(), inputProperty->componentNames());
            }
            else {
                // Input property is a user property for trajectory lines.
                samplingProperty = trajectoryLines->createProperty(inputProperty->name(), inputProperty->dataType(), inputProperty->componentCount(), inputProperty->componentNames());
            }

            // Copy property values from temporary sampling buffer to destination trajectory line property.
            const auto* src = samplingPropertyData.data();
            auto* dst = samplingProperty.data();
            size_t stride = samplingProperty.stride();
            piter = permutation.cbegin();
            for(size_t mapping : permutation) {
                OVITO_ASSERT(stride * (mapping + 1) <= samplingPropertyData.size());
                std::memcpy(dst, src + stride * mapping, stride);
                dst += stride;
            }
        }
    }
    this_task::throwIfCanceled();

    // Unwrap trajectory vertices at periodic boundaries of the simulation cell.
    if(unwrapTrajectories && pointData.size() >= 2 && !cellMatrices.empty() && cellMatrices.front() != AffineTransformation::Zero() && (cellPbcFlags[0] || cellPbcFlags[1] || cellPbcFlags[2])) {
        progress.setText(tr("Unwrapping trajectory lines"));
        progress.setMaximum(trajPosProperty.size() - 1);
        Point3* pos = trajPosProperty.begin();
        piter = permutation.cbegin();
        const int64_t* id = trajIdProperty.cbegin();
        for(auto pos_end = pos + trajPosProperty.size() - 1; pos != pos_end; ++pos, ++piter, ++id) {
            progress.incrementValue();
            if(id[0] == id[1]) {
                const AffineTransformation& cell1 = cellMatrices[timeData[piter[0]]];
                const AffineTransformation& cell2 = cellMatrices[timeData[piter[1]]];
                const AffineTransformation& invCell1 = inverseCellMatrices[timeData[piter[0]]];
                const AffineTransformation& invCell2 = inverseCellMatrices[timeData[piter[1]]];
                if(cell1 != AffineTransformation::Zero() && cell2 != AffineTransformation::Zero()) {
                    const Point3& p1 = pos[0];
                    Point3 p2 = pos[1];
                    for(size_t dim = 0; dim < 3; dim++) {
                        if(cellPbcFlags[dim]) {
                            FloatType reduced1 = invCell1.prodrow(p1, dim);
                            FloatType reduced2 = invCell2.prodrow(p2, dim);
                            FloatType delta = reduced2 - reduced1;
                            FloatType shift = std::floor(delta + FloatType(0.5));
                            if(shift != 0) {
                                pos[1] -= cell2.column(dim) * shift;
                            }
                        }
                    }
                }
            }
        }
    }

    // Assign the visual element, which renders the trajectory lines.
    trajectoryLines->setVisElement(std::move(trajectoryVis));

    co_return trajectoryLines;
}

/******************************************************************************
* Sends an event to all dependents of this RefTarget.
******************************************************************************/
void GenerateTrajectoryLinesModificationNode::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        // Do not discard existing results if the modifier is just turned off and on again by the user.
        const TargetChangedEvent& changeEvent = static_cast<const TargetChangedEvent&>(event);
        if(changeEvent.field() != PROPERTY_FIELD(Modifier::isEnabled) || event.sender() != modifier()) {
            // Invalidate cached trajectory lines and ongoing trajectory sampling operation when the modifier or the upstream pipeline change.
            _trajectoryLines.reset();
            _trajectoryWeakFuture.reset();
        }
    }
    ModificationNode::notifyDependentsImpl(event);
}

/******************************************************************************
 * Launches an asynchronous task to evaluate the node's modifier.
 ******************************************************************************/
SharedFuture<PipelineFlowState> GenerateTrajectoryLinesModificationNode::launchModifierEvaluation(ModifierEvaluationRequest&& request, SharedFuture<PipelineFlowState> inputFuture)
{
    // Check if the trajectory lines have already been computed or if a computation is already in progress that we can hook up to.
    SharedFuture<DataOORef<const Lines>> trajectoryFuture;
    if(_trajectoryLines) {
        // Trajectories have already been computed.
        trajectoryFuture = Future<DataOORef<const Lines>>::createImmediate(_trajectoryLines);
    }
    else if(request.interactiveMode()) {
        // In interactive mode, temporarily adopt an older result from a previous pipeline state if available.
        if(PipelineFlowState cachedState = getCachedPipelineNodeOutput(request.time(), true)) {
            if(const Lines* lines = cachedState.getObjectBy<Lines>(this, QStringLiteral("trajectories"))) {
                trajectoryFuture = Future<DataOORef<const Lines>>::createImmediate(lines);
            }
        }
        if(!trajectoryFuture)
            trajectoryFuture = Future<DataOORef<const Lines>>::createImmediateEmplace();
    }
    else {
        // Is there an ongoing operation that we can hook up to?
        trajectoryFuture = _trajectoryWeakFuture.lock();
        if(!trajectoryFuture || trajectoryFuture.isCanceled()) {
            // If not, start a new sampling operation that loops over all trajectory frames.
            try {
                trajectoryFuture = static_object_cast<GenerateTrajectoryLinesModifier>(modifier())->generateTrajectoryLines(request);
                registerActiveFuture(trajectoryFuture);
            }
            catch(Exception& ex) {
                trajectoryFuture = Future<DataOORef<const Lines>>::createFailed(std::move(ex));
            }
            // Keep a weak reference to the task.
            _trajectoryWeakFuture = trajectoryFuture;
        }
    }

    using TrajectoryTask = ComplexModifierEvaluationTask<GenerateTrajectoryLinesModifier, decltype(trajectoryFuture)>;

    return launchTask(
        std::make_shared<TrajectoryTask>(std::move(request)),
        std::move(inputFuture),
        std::move(trajectoryFuture));
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void GenerateTrajectoryLinesModifier::loadFromStreamComplete(ObjectLoadStream& stream)
{
    Modifier::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.7:
    // Convert legacy time values from ticks to frames. This requires access to the AnimationSettings object, which is stored in the scene.
    if(stream.formatVersion() <= 30008) {
        if(ModificationNode* modNode = someNode()) {
            QSet<Pipeline*> pipelines = modNode->pipelines(true);
            if(!pipelines.empty()) {
                if(SceneNode* sceneNode = (*pipelines.begin())->someSceneNode()) {
                    if(Scene* scene = sceneNode->scene()) {
                        if(scene->animationSettings()) {
                            int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
                            setCustomIntervalStart(customIntervalStart() / ticksPerFrame);
                            setCustomIntervalEnd(customIntervalEnd() / ticksPerFrame);
                        }
                    }
                }
            }
        }
    }
}

}   // End of namespace
