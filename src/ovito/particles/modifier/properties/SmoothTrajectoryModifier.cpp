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

#include <ovito/particles/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/app/Application.h>
#include "SmoothTrajectoryModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SmoothTrajectoryModifier);
DEFINE_PROPERTY_FIELD(SmoothTrajectoryModifier, useMinimumImageConvention);
DEFINE_PROPERTY_FIELD(SmoothTrajectoryModifier, smoothingWindowSize);
SET_PROPERTY_FIELD_LABEL(SmoothTrajectoryModifier, useMinimumImageConvention, "Use minimum image convention");
SET_PROPERTY_FIELD_LABEL(SmoothTrajectoryModifier, smoothingWindowSize, "Smoothing window size");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SmoothTrajectoryModifier, smoothingWindowSize, IntegerParameterUnit, 1, 200);

// This class can be removed in a future version of OVITO:
IMPLEMENT_CREATABLE_OVITO_CLASS(InterpolateTrajectoryModifierApplication);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
SmoothTrajectoryModifier::SmoothTrajectoryModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _useMinimumImageConvention(true),
    _smoothingWindowSize(1)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool SmoothTrajectoryModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval SmoothTrajectoryModifier::validityInterval(const ModifierEvaluationRequest& request) const
{
    TimeInterval iv = Modifier::validityInterval(request);
    // Interpolation results will only be valid for the duration of the current frame.
    iv.intersect(request.time());
    return iv;
}

/******************************************************************************
* Asks the modifier for the set of animation time intervals that should be
* cached by the upstream pipeline.
******************************************************************************/
void SmoothTrajectoryModifier::inputCachingHints(TimeIntervalUnion& cachingIntervals, ModificationNode* node)
{
    Modifier::inputCachingHints(cachingIntervals, node);

    TimeIntervalUnion originalIntervals = cachingIntervals;
    for(const TimeInterval& iv : originalIntervals) {
        // Round interval start down to the previous animation frame.
        // Round interval end up to the next animation frame.
        int startFrame = node->animationTimeToSourceFrame(iv.start());
        int endFrame = node->animationTimeToSourceFrame(iv.end());
        if(node->sourceFrameToAnimationTime(endFrame) < iv.end())
            endFrame++;
        startFrame -= (smoothingWindowSize() - 1) / 2;
        endFrame += smoothingWindowSize() / 2;
        AnimationTime newStartTime = node->sourceFrameToAnimationTime(startFrame);
        AnimationTime newEndTime = node->sourceFrameToAnimationTime(endFrame);
        OVITO_ASSERT(newStartTime <= iv.start());
        OVITO_ASSERT(newEndTime >= iv.end());
        cachingIntervals.add(TimeInterval(newStartTime, newEndTime));
    }
}

/******************************************************************************
* Is called by the ModifierApplication to let the modifier adjust the
* time interval of a TargetChanged event received from the upstream pipeline
* before it is propagated to the downstream pipeline.
******************************************************************************/
void SmoothTrajectoryModifier::restrictInputValidityInterval(TimeInterval& iv) const
{
    Modifier::restrictInputValidityInterval(iv);

    // If the upstream pipeline changes, all computed output frames of the modifier become invalid.
    iv.setEmpty();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> SmoothTrajectoryModifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Determine the current frame, preferably from the attribute stored with the pipeline flow state.
    // If the source frame attribute is not present, fall back to inferring it from the current animation time.
    int currentFrame = input.data() ? input.data()->sourceFrame() : -1;
    if(currentFrame < 0)
        currentFrame = request.modificationNode()->animationTimeToSourceFrame(request.time());
    AnimationTime time1 = request.modificationNode()->sourceFrameToAnimationTime(currentFrame);

    // If we are exactly on a source frame, there is no need to interpolate between frames.
    if(time1 == request.time() && smoothingWindowSize() <= 1) {
        // The validity of the resulting state is restricted to the current animation time.
        PipelineFlowState output = input;
        output.intersectStateValidity(time1);
        return output;
    }

    if(smoothingWindowSize() == 1) {
        // Perform interpolation between two consecutive frames.
        int nextFrame = currentFrame + 1;
        AnimationTime time2 = request.modificationNode()->sourceFrameToAnimationTime(nextFrame);

        // Obtain the subsequent input frame by evaluating the upstream pipeline.
        PipelineEvaluationRequest frameRequest = request;
        frameRequest.setTime(time2);

        // Wait for the second frame to become available.
        return request.modificationNode()->evaluateInput(frameRequest)
            .then(*this, [this, request, state = input, time1, time2](const PipelineFlowState& nextState) mutable {
                // Compute interpolated state.
                interpolateState(state, nextState, request, time1, time2);
                return std::move(state);
            });
    }
    else {
        // Perform averaging of several frames. Determine frame interval first.
        int startFrame = currentFrame - (smoothingWindowSize() - 1) / 2;
        int endFrame = currentFrame + smoothingWindowSize() / 2;

        // Prepare the upstream pipeline request.
        PipelineEvaluationRequest frameRequest = request;
        frameRequest.setTime(request.modificationNode()->sourceFrameToAnimationTime(startFrame));

        // List of animation times at which to evaluate the upstream pipeline.
        std::vector<AnimationTime> otherTimes;
        otherTimes.reserve(endFrame - startFrame);
        for(int frame = startFrame; frame <= endFrame; frame++) {
            if(frame != currentFrame)
                otherTimes.push_back(request.modificationNode()->sourceFrameToAnimationTime(frame));
        }

        // Obtain the range of input frames from the upstream pipeline.
        return request.modificationNode()->evaluateInputMultiple(frameRequest, std::move(otherTimes))
            .then(*this, [this, state = input, request](const std::vector<PipelineFlowState>& otherStates) mutable {
                // Compute smoothed state.
                averageState(state, otherStates, request);
                return std::move(state);
            });
    }
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void SmoothTrajectoryModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    // Determine the current frame, preferably from the attribute stored with the pipeline flow state.
    // If the source frame attribute is not present, fall back to inferring it from the current animation time.
    int currentFrame = state.data() ? state.data()->sourceFrame() : -1;
    if(currentFrame < 0)
        currentFrame = request.modificationNode()->animationTimeToSourceFrame(request.time());
    AnimationTime time1 = request.modificationNode()->sourceFrameToAnimationTime(currentFrame);

    // If we are exactly on a source frame, there is no need to interpolate between two consecutive frames.
    if(time1 == request.time() && smoothingWindowSize() <= 1) {
        // The validity of the resulting state is restricted to the current animation time.
        state.intersectStateValidity(request.time());
        return;
    }

    if(smoothingWindowSize() == 1) {
        // Perform interpolation between two consecutive frames.
        int nextFrame = currentFrame + 1;
        AnimationTime time2 = request.modificationNode()->sourceFrameToAnimationTime(nextFrame);

        // Get the second frame.
        const PipelineFlowState& state2 = request.modificationNode()->evaluateInputSynchronous(PipelineEvaluationRequest(time2));

        // Perform the actual interpolation calculation.
        interpolateState(state, state2, request, time1, time2);
    }
    else {
        // Perform averaging of several frames. Determine frame interval.
        int startFrame = currentFrame - (smoothingWindowSize() - 1) / 2;
        int endFrame = currentFrame + smoothingWindowSize() / 2;

        // Obtain the range of input frames from the upstream pipeline.
        std::vector<PipelineFlowState> otherStates;
        otherStates.reserve(endFrame - startFrame);
        for(int frame = startFrame; frame <= endFrame; frame++) {
            if(frame != currentFrame) {
                AnimationTime time2 = request.modificationNode()->sourceFrameToAnimationTime(frame);
                otherStates.push_back(request.modificationNode()->evaluateInputSynchronous(PipelineEvaluationRequest(time2)));
            }
        }

        // Compute smoothed state.
        averageState(state, otherStates, request);
    }
}

/******************************************************************************
* Computes the interpolated state between two input states.
******************************************************************************/
void SmoothTrajectoryModifier::interpolateState(PipelineFlowState& state1, const PipelineFlowState& state2, const ModifierEvaluationRequest& request, AnimationTime time1, AnimationTime time2)
{
    OVITO_ASSERT(!isUndoRecording());

    // Make sure the obtained reference configuration is valid and ready to use.
    if(state2.status().type() == PipelineStatus::Error)
        throw Exception(tr("Input state for frame %1 is not available: %2").arg(request.modificationNode()->animationTimeToSourceFrame(time2)).arg(state2.status().text()));

    OVITO_ASSERT(time2 > time1);
    FloatType t = (FloatType)(request.time() - time1) / (time2 - time1);
    if(t < 0) t = 0;
    else if(t > 1) t = 1;

    const SimulationCell* cell1 = state1.getObject<SimulationCell>();
    const SimulationCell* cell2 = state2.getObject<SimulationCell>();

    // Interpolate particle positions.
    const Particles* particles1 = state1.expectObject<Particles>();
    const Particles* particles2 = state2.getObject<Particles>();
    if(!particles2)
        throw Exception(tr("Cannot interpolate between consecutive simulation frames, because the second frame contains no particles at all."));
    particles1->verifyIntegrity();
    particles2->verifyIntegrity();
    BufferReadAccess<Point3> posProperty2 = particles2->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> idProperty1 = particles1->getProperty(Particles::IdentifierProperty);
    BufferReadAccess<IdentifierIntType> idProperty2 = particles2->getProperty(Particles::IdentifierProperty);
    if((!idProperty1 || !idProperty2) && particles1->elementCount() != particles2->elementCount())
        throw Exception(tr("Cannot interpolate between consecutive simulation frames, because they contain different numbers of particles but no unique identifiers."));
    Particles* outputParticles = state1.makeMutable(particles1);
    BufferWriteAccess<Point3, access_mode::read_write> outputPositions = outputParticles->createProperty(DataBuffer::Initialized, Particles::PositionProperty);
    std::unordered_map<IdentifierIntType, size_t> idmap;
    if(idProperty1 && idProperty2 && !boost::equal(idProperty1, idProperty2)) {

        // Build ID-to-index map for second frame.
        size_t index = 0;
        for(auto id : idProperty2) {
            if(!idmap.insert(std::make_pair(id, index)).second)
                throw Exception(tr("Detected duplicate particle ID: %1. Cannot interpolate trajectories in this case.").arg(id));
            index++;
        }

        if(useMinimumImageConvention() && cell1) {
            auto id = idProperty1.cbegin();
            for(Point3& p1 : outputPositions) {
                auto mapEntry = idmap.find(*id);
                if(mapEntry != idmap.end()) {
                    Vector3 delta = cell1->wrapVector(posProperty2[mapEntry->second] - p1);
                    p1 += delta * t;
                }
                ++id;
            }
        }
        else {
            auto id = idProperty1.cbegin();
            for(Point3& p1 : outputPositions) {
                auto mapEntry = idmap.find(*id);
                if(mapEntry != idmap.end()) {
                    p1 += (posProperty2[mapEntry->second] - p1) * t;
                }
                ++id;
            }
        }
    }
    else {
        const Point3* p2 = posProperty2.cbegin();
        if(useMinimumImageConvention() && cell1) {
            for(Point3& p1 : outputPositions) {
                Vector3 delta = cell1->wrapVector((*p2++) - p1);
                p1 += delta * t;
            }
        }
        else {
            for(Point3& p1 : outputPositions) {
                p1 += ((*p2++) - p1) * t;
            }
        }
    }

    // Interpolate particle orientations.
    if(BufferReadAccess<QuaternionG> orientationProperty2 = particles2->getProperty(Particles::OrientationProperty)) {
        BufferWriteAccess<QuaternionG, access_mode::read_write> outputOrientations = outputParticles->createProperty(DataBuffer::Initialized, Particles::OrientationProperty);
        if(idProperty1 && idProperty2 && !boost::equal(idProperty1, idProperty2)) {
            auto id = idProperty1.cbegin();
            for(QuaternionG& q1 : outputOrientations) {
                auto mapEntry = idmap.find(*id);
                if(mapEntry != idmap.end()) {
                    const QuaternionG& q2 = orientationProperty2[mapEntry->second];
                    if(q1.dot(q2) < 0)
                        q1 = -q1;
                    q1 = QuaternionG::interpolateSafely(q1, q2, static_cast<GraphicsFloatType>(t));
                }
                ++id;
            }
        }
        else {
            const QuaternionG* q2 = orientationProperty2.cbegin();
            for(QuaternionG& q1 : outputOrientations) {
                if(q1.dot(*q2) < 0)
                    q1 = -q1;
                q1 = QuaternionG::interpolateSafely(q1, *q2++, static_cast<GraphicsFloatType>(t));
            }
        }
    }

    // Interpolate all scalar and continuous particle properties.
    for(const Property* property1 : particles1->properties()) {
        if(property1->dataType() == Property::Float32 && property1->componentCount() == 1) {
            const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
            if(property2 && property2->dataType() == property1->dataType() && property2->componentCount() == property1->componentCount()) {
                BufferWriteAccess<float, access_mode::read_write> data1 = outputParticles->makeMutable(property1);
                BufferReadAccess<float> data2(property2);
                if(idProperty1 && idProperty2 && !boost::equal(idProperty1, idProperty2)) {
                    auto id = idProperty1.cbegin();
                    for(auto& v1 : data1) {
                        auto mapEntry = idmap.find(*id);
                        if(mapEntry != idmap.end()) {
                            v1 = v1 * (1.0f - t) + data2[mapEntry->second] * t;
                        }
                        ++id;
                    }
                }
                else {
                    const auto* v2 = data2.cbegin();
                    for(auto& v1 : data1) {
                        v1 = v1 * (1.0f - t) + *v2++ * t;
                    }
                }

            }
        }
        else if(property1->dataType() == Property::Float64 && property1->componentCount() == 1) {
            const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
            if(property2 && property2->dataType() == property1->dataType() && property2->componentCount() == property1->componentCount()) {
                BufferWriteAccess<double, access_mode::read_write> data1 = outputParticles->makeMutable(property1);
                BufferReadAccess<double> data2(property2);
                if(idProperty1 && idProperty2 && !boost::equal(idProperty1, idProperty2)) {
                    auto id = idProperty1.cbegin();
                    for(auto& v1 : data1) {
                        auto mapEntry = idmap.find(*id);
                        if(mapEntry != idmap.end()) {
                            v1 = v1 * (1.0 - t) + data2[mapEntry->second] * t;
                        }
                        ++id;
                    }
                }
                else {
                    const auto* v2 = data2.cbegin();
                    for(auto& v1 : data1) {
                        v1 = v1 * (1.0 - t) + *v2++ * t;
                    }
                }
            }
        }
    }

    // Interpolate simulation cell vectors.
    if(cell1 && cell2) {
        const AffineTransformation cellMat1 = cell1->cellMatrix();
        const AffineTransformation delta = cell2->cellMatrix() - cellMat1;
        SimulationCell* outputCell = state1.expectMutableObject<SimulationCell>();
        outputCell->setCellMatrix(cellMat1 + delta * t);
    }

    // The validity of the interpolated state is restricted to the current animation time.
    state1.intersectStateValidity(request.time());
}

/******************************************************************************
* Computes the averaged state from several input states.
******************************************************************************/
void SmoothTrajectoryModifier::averageState(PipelineFlowState& state1, const std::vector<PipelineFlowState>& otherStates, const ModifierEvaluationRequest& request)
{
    OVITO_ASSERT(!isUndoRecording());

    // Get particle positions and simulation cell of the central frame.
    const SimulationCell* cell1 = state1.getObject<SimulationCell>();
    const Particles* particles1 = state1.expectObject<Particles>();
    particles1->verifyIntegrity();
    BufferReadAccessAndRef<Point3> posProperty1 = particles1->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> idProperty1 = particles1->getProperty(Particles::IdentifierProperty);

    // Create a modifiable copy of the particle coordinates array.
    Particles* outputParticles = state1.makeMutable(particles1);
    BufferWriteAccess<Point3, access_mode::discard_read_write> outputPositions = outputParticles->createProperty(Particles::PositionProperty);
    boost::fill(outputPositions, Point3::Origin());

    // Create output orientations array if smoothing particle orientations.
    BufferWriteAccess<QuaternionG, access_mode::read_write> outputOrientations = particles1->getProperty(Particles::OrientationProperty)
        ? outputParticles->createProperty(DataBuffer::Initialized, Particles::OrientationProperty)
        : nullptr;

    // Create copies of all scalar continuous particle properties.
    std::vector<BufferWriteAccess<float, access_mode::read_write>> outputScalarProperties32;
    std::vector<BufferWriteAccess<double, access_mode::read_write>> outputScalarProperties64;
    for(const Property* property : particles1->properties()) {
        if(property->componentCount() == 1) {
            if(property->dataType() == Property::Float32) {
                outputScalarProperties32.emplace_back(outputParticles->makeMutable(property));
            }
            else if(property->dataType() == Property::Float64) {
                outputScalarProperties64.emplace_back(outputParticles->makeMutable(property));
            }
        }
    }

    // For interpolating the simulation cell vectors.
    AffineTransformation averageCellMat;
    if(cell1)
        averageCellMat = cell1->cellMatrix();
    int cellCount = cell1 ? 1 : 0;

    // Keep track on a per-particle basis of how many different frames get averaged.
    // This is needed to support systems with varying particle counts.
    // Averaging only takes place over the interval of frames during which a particle exists.
    std::vector<int> particleStateCounts(outputParticles->elementCount(), 1);

    // Iterate over all frames within the averaging window (except the central frame).
    for(const PipelineFlowState& state2 : otherStates) {

        // Make sure the obtained reference configuration is valid and ready to use.
        if(state2.status().type() == PipelineStatus::Error)
            throw Exception(tr("An input frame for trajectory smoothing is unavailable: %1").arg(state2.status().text()));

        const Particles* particles2 = state2.getObject<Particles>();
        if(!particles2)
            continue;
        particles2->verifyIntegrity();
        BufferReadAccess<Point3> posProperty2 = particles2->expectProperty(Particles::PositionProperty);
        BufferReadAccess<IdentifierIntType> idProperty2 = particles2->getProperty(Particles::IdentifierProperty);

        // Sum up cell vectors.
        const SimulationCell* cell2 = cell1 ? state2.expectObject<SimulationCell>() : nullptr;
        if(cell2) {
            averageCellMat += cell2->cellMatrix();
            cellCount++;
        }
        bool wrapVectors = (useMinimumImageConvention() && cell2);
        auto psc = particleStateCounts.begin();

        if(idProperty1 && idProperty2 && !boost::equal(idProperty1, idProperty2)) {
            // Build id-to-index map.
            std::unordered_map<IdentifierIntType,size_t> idmap;
            size_t index = 0;
            for(auto id : idProperty2) {
                if(!idmap.insert(std::make_pair(id,index)).second)
                    throw Exception(tr("Detected duplicate particle ID: %1. Cannot smooth trajectories in this case.").arg(id));
                index++;
            }

            // Calculate mean displacements relative to current positions.
            const Point3* p1 = posProperty1.cbegin();
            auto id = idProperty1.cbegin();
            for(Point3& pout : outputPositions) {
                if(auto mapEntry = idmap.find(*id); mapEntry != idmap.end()) {
                    Vector3 delta = posProperty2[mapEntry->second] - (*p1);
                    pout += wrapVectors ? cell2->wrapVector(delta) : delta;
                    (*psc)++;
                }
                ++p1;
                ++id;
                ++psc;
            }

            // Average particle orientations over time.
            if(outputOrientations) {
                if(BufferReadAccess<QuaternionG> orientationProperty2 = particles2->getProperty(Particles::OrientationProperty)) {
                    auto id = idProperty1.cbegin();
                    for(QuaternionG& qout : outputOrientations) {
                        if(auto mapEntry = idmap.find(*id); mapEntry != idmap.end()) {
                            const QuaternionG& q2 = orientationProperty2[mapEntry->second];
                            if(qout.dot(q2) >= 0) {
                                qout.x() += q2.x();
                                qout.y() += q2.y();
                                qout.z() += q2.z();
                                qout.w() += q2.w();
                            }
                            else {
                                qout.x() -= q2.x();
                                qout.y() -= q2.y();
                                qout.z() -= q2.z();
                                qout.w() -= q2.w();
                            }
                        }
                        ++id;
                    }
                }
            }

            // Average all scalar continuous properties.
            for(auto& accessor1 : outputScalarProperties32) {
                Property* property1 = static_object_cast<Property>(accessor1.buffer());
                const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
                if(property2 && property2->dataType() == accessor1.dataType() && property2->componentCount() == accessor1.componentCount()) {
                    BufferReadAccess<float> accessor2(property2);
                    auto id = idProperty1.cbegin();
                    for(auto& v : accessor1) {
                        if(auto mapEntry = idmap.find(*id); mapEntry != idmap.end()) {
                            v += accessor2[mapEntry->second];
                        }
                        ++id;
                    }
                }
            }
            for(auto& accessor1 : outputScalarProperties64) {
                Property* property1 = static_object_cast<Property>(accessor1.buffer());
                const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
                if(property2 && property2->dataType() == accessor1.dataType() && property2->componentCount() == accessor1.componentCount()) {
                    BufferReadAccess<double> accessor2(property2);
                    auto id = idProperty1.cbegin();
                    for(auto& v : accessor1) {
                        if(auto mapEntry = idmap.find(*id); mapEntry != idmap.end()) {
                            v += accessor2[mapEntry->second];
                        }
                        ++id;
                    }
                }
            }
        }
        else {
            // Calculate mean displacements relative to current positions.
            const Point3* p1 = posProperty1.cbegin();
            const Point3* p2 = posProperty2.cbegin();
            auto pout = outputPositions.begin();
            for(auto pend = pout + std::min(posProperty1.size(), posProperty2.size()); pout != pend; ++pout, ++p1, ++p2, ++psc) {
                Vector3 delta = (*p2) - (*p1);
                *pout += wrapVectors ? cell2->wrapVector(delta) : delta;
                (*psc)++;
            }
            OVITO_ASSERT(p1 == posProperty1.cend() || p2 == posProperty2.cend());

            // Average particle orientations over time.
            if(outputOrientations) {
                if(BufferReadAccess<QuaternionG> orientationProperty2 = particles2->getProperty(Particles::OrientationProperty)) {
                    const auto* q2 = orientationProperty2.cbegin();
                    for(auto* qout = outputOrientations.begin(), *qend = qout + std::min(outputOrientations.size(), orientationProperty2.size()); qout != qend; ++qout, ++q2) {
                        if(qout->dot(*q2) >= 0) {
                            qout->x() += q2->x();
                            qout->y() += q2->y();
                            qout->z() += q2->z();
                            qout->w() += q2->w();
                        }
                        else {
                            qout->x() -= q2->x();
                            qout->y() -= q2->y();
                            qout->z() -= q2->z();
                            qout->w() -= q2->w();
                        }
                    }
                }
            }

            // Average all scalar continuous properties.
            for(auto& accessor1 : outputScalarProperties32) {
                Property* property1 = static_object_cast<Property>(accessor1.buffer());
                const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
                if(property2 && property2->dataType() == accessor1.dataType() && property2->componentCount() == accessor1.componentCount()) {
                    BufferReadAccess<float> accessor2(property2);
                    const auto* v2 = accessor2.cbegin();
                    for(auto* vout = accessor1.begin(), *vend = vout + std::min(accessor1.size(), accessor2.size()); vout != vend; ++vout, ++v2) {
                        *vout += *v2;
                    }
                }
            }
            for(auto& accessor1 : outputScalarProperties64) {
                Property* property1 = static_object_cast<Property>(accessor1.buffer());
                const Property* property2 = (property1->type() != 0) ? particles2->getProperty(property1->type()) : particles2->getProperty(property1->name());
                if(property2 && property2->dataType() == accessor1.dataType() && property2->componentCount() == accessor1.componentCount()) {
                    BufferReadAccess<double> accessor2(property2);
                    const auto* v2 = accessor2.cbegin();
                    for(auto* vout = accessor1.begin(), *vend = vout + std::min(accessor1.size(), accessor2.size()); vout != vend; ++vout, ++v2) {
                        *vout += *v2;
                    }
                }
            }
        }
    }

    // Calculate mean position from current position plus mean of displacement vector.
    auto psc = particleStateCounts.cbegin();
    const Point3* p1 = posProperty1.cbegin();
    for(Point3& p : outputPositions) {
        p /= (*psc++);
        p += (*p1++) - Point3::Origin();
    }
    OVITO_ASSERT(psc == particleStateCounts.cend());
    OVITO_ASSERT(p1 == posProperty1.cend());

    // Normalize orientation quaternions.
    if(outputOrientations) {
        for(QuaternionG& q : outputOrientations) {
            q.normalizeSafely();
        }
    }

    // Calculate means of the auxiliary properties.
    for(auto& accessor : outputScalarProperties32) {
        auto psc = particleStateCounts.cbegin();
        for(auto& v : accessor)
            v /= *psc++;
        OVITO_ASSERT(psc == particleStateCounts.cend());
    }
    for(auto& accessor : outputScalarProperties64) {
        auto psc = particleStateCounts.cbegin();
        for(auto& v : accessor)
            v /= *psc++;
        OVITO_ASSERT(psc == particleStateCounts.cend());
    }

    // Compute average of simulation cell vectors.
    if(cell1 && cellCount) {
        SimulationCell* outputCell = state1.expectMutableObject<SimulationCell>();
        outputCell->setCellMatrix(averageCellMat * (1.0 / cellCount));
    }

    // The validity of the interpolated state is restricted to the current animation time.
    state1.intersectStateValidity(request.time());
}

}   // End of namespace
