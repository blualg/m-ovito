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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "GenerateTrajectoryLinesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GenerateTrajectoryLinesModifier);
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
DEFINE_REFERENCE_FIELD(GenerateTrajectoryLinesModificationNode, trajectoryData);
SET_MODIFICATION_NODE_TYPE(GenerateTrajectoryLinesModifier, GenerateTrajectoryLinesModificationNode);

/******************************************************************************
* Constructor.
******************************************************************************/
GenerateTrajectoryLinesModifier::GenerateTrajectoryLinesModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _onlySelectedParticles(true),
    _useCustomInterval(false),
    _customIntervalStart(0),
    _customIntervalEnd(0),
    _everyNthFrame(1),
    _unwrapTrajectories(true),
    _transferParticleProperties(false)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Create the vis element for rendering the trajectories created by the modifier.
        setTrajectoryVis(OORef<LinesVis>::create(flags));
        trajectoryVis()->setTitle(tr("Trajectory lines"));
    }
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted into a pipeline.
******************************************************************************/
void GenerateTrajectoryLinesModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    if(ExecutionContext::isInteractive()) {
        auto [firstFrame, lastFrame] = ExecutionContext::current().ui().datasetContainer().currentAnimationInterval();
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
* Modifies the input data synchronously.
******************************************************************************/
void GenerateTrajectoryLinesModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    // Inject the precomputed trajectory lines, which are stored in the modifier application, into the pipeline.
    if(GenerateTrajectoryLinesModificationNode* myModNode = dynamic_object_cast<GenerateTrajectoryLinesModificationNode>(request.modificationNode())) {
        if(myModNode->trajectoryData()) {
            state.addObject(myModNode->trajectoryData());
        }
    }
}

/******************************************************************************
* Updates the stored trajectories from the source particle object.
******************************************************************************/
bool GenerateTrajectoryLinesModifier::generateTrajectories(AnimationTime currentTime)
{
    for(ModificationNode* modNode : nodes()) {
        GenerateTrajectoryLinesModificationNode* myModNode = dynamic_object_cast<GenerateTrajectoryLinesModificationNode>(modNode);
        if(!myModNode)
            continue;

        // Get input particles.
        SharedFuture<PipelineFlowState> stateFuture = myModNode->evaluateInput(PipelineEvaluationRequest(currentTime));
        if(!stateFuture.waitForFinished())
            return false;

        const PipelineFlowState& state = stateFuture.result();
        const Particles* particles = state.getObject<Particles>();
        if(!particles)
            throw Exception(tr("Cannot generate trajectory lines. The pipeline data contains no particles."));
        particles->verifyIntegrity();

        // Determine set of input particles in the current frame.
        std::vector<size_t> selectedIndices;
        std::set<int64_t> selectedIdentifiers;
        if(onlySelectedParticles()) {
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
                throw Exception(tr("Cannot generate trajectory lines for selected particles. Particle selection has not been defined or selection set is empty."));
        }

        // Determine time interval over which trajectories should be generated.
        int startFrame, endFrame;
        if(useCustomInterval()) {
            startFrame = customIntervalStart();
            endFrame = customIntervalEnd();
        }
        else {
            startFrame = 0;
            endFrame = myModNode->numberOfSourceFrames() - 1;
        }
        if(startFrame >= endFrame)
            throw Exception(tr("The current simulation sequence consists only of a single frame. Thus, no trajectory lines were created."));

        // Generate list of animation times at which particle positions should be sampled.
        std::vector<int> sampleFrames;
        for(int frame = startFrame; frame <= endFrame; frame += everyNthFrame()) {
            sampleFrames.push_back(frame);
        }
        this_task::setProgressMaximum(sampleFrames.size());

        // Collect particle positions to generate trajectory line vertices.
        std::vector<Point3> pointData;
        std::vector<int32_t> timeData;
        std::vector<int64_t> idData;
        std::vector<std::byte> samplingPropertyData;
        std::vector<DataOORef<const SimulationCell>> cells;
        int timeIndex = 0;
        for(int frame : sampleFrames) {
            this_task::setProgressText(tr("Generating trajectory lines (frame %1 of %2)").arg(timeIndex+1).arg(sampleFrames.size()));

            SharedFuture<PipelineFlowState> stateFuture = myModNode->evaluateInput(PipelineEvaluationRequest(myModNode->sourceFrameToAnimationTime(frame)));
            if(!stateFuture.waitForFinished())
                return false;

            const PipelineFlowState& state = stateFuture.result();
            const Particles* particles = state.getObject<Particles>();
            if(!particles)
                throw Exception(tr("Input data contains no particles at frame %1.").arg(frame));
            particles->verifyIntegrity();
            BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);

            // Get the particle property to be sampled.
            RawBufferReadAccess particleSamplingProperty;
            if(transferParticleProperties()) {
                if(particleProperty().isNull())
                    throw Exception(tr("Please select a particle property to be sampled."));
                particleSamplingProperty = particleProperty().findInContainer(particles);
                if(!particleSamplingProperty)
                    throw Exception(tr("The particle property '%1' to be sampled and transferred to the trajectory lines does not exist (at frame %2). "
                        "Perhaps you need to restrict the sampling time interval to those times where the property is available.").arg(particleProperty().name()).arg(frame));
            }

            if(onlySelectedParticles()) {
                if(!selectedIdentifiers.empty()) {
                    BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                    if(!identifierProperty || identifierProperty.size() != posProperty.size())
                        throw Exception(tr("Input particles do not possess identifiers at frame %1.").arg(frame));

                    // Create a mapping from IDs to indices.
                    std::map<int64_t,size_t> idmap;
                    size_t index = 0;
                    for(auto id : identifierProperty)
                        idmap.insert(std::make_pair(id, index++));

                    for(auto id : selectedIdentifiers) {
                        if(auto entry = idmap.find(id); entry != idmap.end()) {
                            pointData.push_back(posProperty[entry->second]);
                            timeData.push_back(timeIndex);
                            idData.push_back(id);
                            if(particleSamplingProperty) {
                                const std::byte* dataBegin = particleSamplingProperty.cdata(entry->second, 0);
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
                                const std::byte* dataBegin = particleSamplingProperty.cdata(index, 0);
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

            // Onbtain the simulation cell geometry at the current animation time.
            if(unwrapTrajectories()) {
                if(const SimulationCell* simCellObj = state.getObject<SimulationCell>()) {
                    cells.push_back(simCellObj);
                }
                else {
                    cells.push_back({});
                }
            }

            if(!this_task::incrementProgressValue(1))
                return false;
            timeIndex++;
        }

        // Sort vertex data to obtain continuous trajectory lines.
        this_task::setProgressMaximum(0);
        this_task::setProgressText(tr("Sorting trajectory data"));
        std::vector<size_t> permutation(pointData.size());
        std::iota(permutation.begin(), permutation.end(), (size_t)0);
        std::sort(permutation.begin(), permutation.end(), [&idData, &timeData](size_t a, size_t b) {
            if(idData[a] < idData[b]) return true;
            if(idData[a] > idData[b]) return false;
            return timeData[a] < timeData[b];
        });
        if(this_task::isCanceled())
            return false;

        // Do not create undo records while computing the trajectories.
        DataOORef<Lines> trajectoryLines = DataOORef<Lines>::create();
        trajectoryLines->setTitle(tr("Particle trajectories"));
        trajectoryLines->setIdentifier(state.generateUniqueIdentifier<Lines>(QStringLiteral("trajectories")));
        {
            UndoSuspender noUndo;

            // Copy re-ordered trajectory points.
            trajectoryLines->setElementCount(pointData.size());
            BufferWriteAccess<Point3, access_mode::discard_read_write> trajPosProperty =
                trajectoryLines->createProperty(Lines::PositionProperty);
            auto piter = permutation.cbegin();
            for(Point3& p : trajPosProperty) {
                p = pointData[*piter++];
            }

            // Copy re-ordered trajectory time stamps.
            BufferWriteAccess<int32_t, access_mode::discard_write> trajTimeProperty =
                trajectoryLines->createProperty(Lines::SampleTimeProperty);
            piter = permutation.cbegin();
            for(int& t : trajTimeProperty) {
                t = sampleFrames[timeData[*piter++]];
            }

            // Copy re-ordered trajectory ids.
            BufferWriteAccess<int64_t, access_mode::discard_read_write> trajIdProperty =
                trajectoryLines->createProperty(Lines::SectionProperty);
            piter = permutation.cbegin();
            for(int64_t& id : trajIdProperty) {
                id = idData[*piter++];
            }

            // Create the trajectory line property receiving the sampled particle property values.
            if(transferParticleProperties() && particleProperty() && particleProperty().type() != Particles::PositionProperty) {
                if(const Property* inputProperty = particleProperty().findInContainer(particles)) {
                    OVITO_ASSERT(samplingPropertyData.size() == inputProperty->stride() * trajectoryLines->elementCount());
                    if(samplingPropertyData.size() != inputProperty->stride() * trajectoryLines->elementCount())
                        throw Exception(tr("Sampling buffer size mismatch. Sampled particle property '%1' seems to have a varying component count.").arg(inputProperty->name()));

                    // Create a corresponding output property of the trajectory lines.
                    RawBufferAccess<access_mode::discard_write> samplingProperty;
                    if(inputProperty->type() < Property::FirstSpecificProperty &&
                       Lines::OOClass().isValidStandardPropertyId(inputProperty->type())) {
                        // Input particle property is also a standard property for trajectory lines.
                        samplingProperty = trajectoryLines->createProperty(inputProperty->type());
                        OVITO_ASSERT(samplingProperty.dataType() == inputProperty->dataType());
                        OVITO_ASSERT(samplingProperty.stride() == inputProperty->stride());
                    }
                    else if(Lines::OOClass().standardPropertyTypeId(inputProperty->name()) != 0) {
                        // Input property name is that of a standard property for trajectory lines.
                        // Must rename the property to avoid naming conflict, because user properties may not have a standard property name.
                        QString newPropertyName = inputProperty->name() + tr("_particles");
                        samplingProperty = trajectoryLines->createProperty(newPropertyName, inputProperty->dataType(), inputProperty->componentCount(), inputProperty->componentNames());
                    }
                    else {
                        // Input property is a user property for trajectory lines.
                        samplingProperty = trajectoryLines->createProperty(inputProperty->name(), inputProperty->dataType(), inputProperty->componentCount(), inputProperty->componentNames());
                    }

                    // Copy property values from temporary sampling buffer to destination trajectory line property.
                    const std::byte* src = samplingPropertyData.data();
                    std::byte* dst = samplingProperty.data();
                    size_t stride = samplingProperty.stride();
                    piter = permutation.cbegin();
                    for(size_t mapping : permutation) {
                        OVITO_ASSERT(stride * (mapping + 1) <= samplingPropertyData.size());
                        std::memcpy(dst, src + stride * mapping, stride);
                        dst += stride;
                    }
                }
            }

            if(this_task::isCanceled())
                return false;

            // Unwrap trajectory vertices at periodic boundaries of the simulation cell.
            if(unwrapTrajectories() && pointData.size() >= 2 && !cells.empty() && cells.front() && cells.front()->hasPbcCorrected()) {
                this_task::setProgressText(tr("Unwrapping trajectory lines"));
                this_task::setProgressMaximum(trajPosProperty.size() - 1);
                Point3* pos = trajPosProperty.begin();
                piter = permutation.cbegin();
                const int64_t* id = trajIdProperty.cbegin();
                for(auto pos_end = pos + trajPosProperty.size() - 1; pos != pos_end; ++pos, ++piter, ++id) {
                    if(!this_task::incrementProgressValue())
                        return false;
                    if(id[0] == id[1]) {
                        const SimulationCell* cell1 = cells[timeData[piter[0]]];
                        const SimulationCell* cell2 = cells[timeData[piter[1]]];
                        if(cell1 && cell2) {
                            const Point3& p1 = pos[0];
                            Point3 p2 = pos[1];
                            for(size_t dim = 0; dim < 3; dim++) {
                                if(cell1->hasPbcCorrected(dim)) {
                                    FloatType reduced1 = cell1->inverseMatrix().prodrow(p1, dim);
                                    FloatType reduced2 = cell2->inverseMatrix().prodrow(p2, dim);
                                    FloatType delta = reduced2 - reduced1;
                                    FloatType shift = std::floor(delta + FloatType(0.5));
                                    if(shift != 0) {
                                        pos[1] -= cell2->matrix().column(dim) * shift;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            trajectoryLines->setVisElement(trajectoryVis());

            // Enable undo recording again from here on, because the trajectory line generation should be an undoable operation.
        }

        // Store generated trajectory lines in the ModificationNode.
        myModNode->setTrajectoryData(std::move(trajectoryLines));
    }

    return true;
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
                if(Scene* scene = (*pipelines.begin())->scene()) {
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

}   // End of namespace
