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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include "UnwrapTrajectoriesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(UnwrapTrajectoriesModifier);

IMPLEMENT_CREATABLE_OVITO_CLASS(UnwrapTrajectoriesModificationNode);
SET_MODIFICATION_NODE_TYPE(UnwrapTrajectoriesModifier, UnwrapTrajectoriesModificationNode);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool UnwrapTrajectoriesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> UnwrapTrajectoriesModifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    if(input) {
        if(UnwrapTrajectoriesModificationNode* unwrapModNode = dynamic_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode())) {

            // If the periodic image flags property is present, use it to unwrap particle positions.
            const Particles* inputParticles = input.expectObject<Particles>();
            if(inputParticles->getProperty(Particles::PeriodicImageProperty)) {
                PipelineFlowState output = input;
                unwrapModNode->unwrapParticleCoordinates(request, output);
                return output;
            }

            // Without the periodic image flags information, we have to scan the particle trajectories
            // from beginning to end before making them continuous.
            return unwrapModNode->detectPeriodicCrossings(request).then(*unwrapModNode, [state = input, request]() mutable {
                static_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode())->unwrapParticleCoordinates(request, state);
                return std::move(state);
            });
        }
    }
    return input;
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void UnwrapTrajectoriesModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    if(!state) return;

    // The pipeline system may call evaluateSynchronous() with an outdated trajectory frame, which doesn't match the current
    // animation time. This would lead to artifacts, because particles might get unwrapped even though they haven't crossed
    // a periodic cell boundary yet. To avoid this from happening, we try to determine the true animation time of the
    // current input data collection and use it for looking up the unwrap information.
    AnimationTime time = request.time();
    int sourceFrame = state.data()->sourceFrame();
    if(sourceFrame != -1)
        time = request.modificationNode()->sourceFrameToAnimationTime(sourceFrame);

    if(UnwrapTrajectoriesModificationNode* unwrapModNode = dynamic_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode())) {
        unwrapModNode->unwrapParticleCoordinates(request, state);
    }
}

/******************************************************************************
* Processes all frames of the input trajectory to detect periodic crossings
* of the particles.
******************************************************************************/
SharedFuture<> UnwrapTrajectoriesModificationNode::detectPeriodicCrossings(const ModifierEvaluationRequest& request)
{
    if(_unwrapOperation.isValid() == false || _unwrapOperation.isCanceled()) {

        // Determine the range of animation frames to be processed.
        int startFrame = 0;
        if(unwrappedUpToTime() != AnimationTime::negativeInfinity())
            startFrame = animationTimeToSourceFrame(unwrappedUpToTime());
        int endFrame = std::max(numberOfSourceFrames(), startFrame);
        auto inputFrameRange = boost::irange(startFrame, endFrame);

        // Iterate over all frames of the input range in sequential order.
        _unwrapOperation = for_each_sequential(
            std::move(inputFrameRange),
            ObjectExecutor(this, true), // Require deferred execution of each frame
            // Requests the next frame from the upstream pipeline.
            [this](int frame) {
                return evaluateInput(PipelineEvaluationRequest(sourceFrameToAnimationTime(frame)));
            },
            // This object processes each frame's data.
            WorkingData{this});

        // Display progress in the UI.
        OVITO_ASSERT(_unwrapOperation.task()->isProgressingTask());
        static_cast<ProgressingTask*>(_unwrapOperation.task().get())->setProgressText(tr("Unwrapping particle trajectories"));
        ExecutionContext::current().ui().taskManager().registerFuture(_unwrapOperation);
        registerActiveFuture(_unwrapOperation);
    }
    return _unwrapOperation;
}

/******************************************************************************
* Throws away the precomputed unwrapping information and interrupts
* any computation currently in progress.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::invalidateUnwrapData()
{
    _unwrappedUpToTime = AnimationTime::negativeInfinity();
    _unwrapRecords.clear();
    _unflipRecords.clear();
    _unwrapOperation.reset();
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool UnwrapTrajectoriesModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged && source == input()) {
        invalidateUnwrapData();
    }
    return ModificationNode::referenceEvent(source, event);
}

/******************************************************************************
* Gets called when the data object of the node has been replaced.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(input)) {
        invalidateUnwrapData();
    }
    ModificationNode::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Rescales the times of all animation keys from the old animation interval to the new interval.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval)
{
    ModificationNode::rescaleTime(oldAnimationInterval, newAnimationInterval);
    invalidateUnwrapData();
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::unwrapParticleCoordinates(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    const Particles* inputParticles = state.expectObject<Particles>();
    inputParticles->verifyIntegrity();

    // If the periodic image flags particle property is present, use it to unwrap particle positions.
    if(BufferReadAccess<Vector3I> particlePeriodicImageProperty = inputParticles->getProperty(Particles::PeriodicImageProperty)) {
        // Get current simulation cell.
        const SimulationCell* cell = state.expectObject<SimulationCell>();

        // Make a modifiable copy of the particles object.
        Particles* outputParticles = state.expectMutableObject<Particles>();

        // Make a modifiable copy of the particle position property.
        BufferWriteAccess<Point3, access_mode::read_write> posProperty = outputParticles->expectMutableProperty(Particles::PositionProperty);
        const Vector3I* pbcShift = particlePeriodicImageProperty.cbegin();
        for(Point3& p : posProperty) {
            p += cell->cellMatrix() * (*pbcShift++).toDataType<FloatType>();
        }

        // Unwrap bonds by adjusting their PBC shift vectors.
        if(outputParticles->bonds()) {
            if(BufferReadAccess<ParticleIndexPair> topologyProperty = outputParticles->bonds()->getProperty(Bonds::TopologyProperty)) {
                BufferWriteAccess<Vector3I, access_mode::read_write> periodicImageProperty = outputParticles->makeBondsMutable()->createProperty(DataBuffer::Initialized, Bonds::PeriodicImageProperty);
                for(size_t bondIndex = 0; bondIndex < topologyProperty.size(); bondIndex++) {
                    size_t particleIndex1 = topologyProperty[bondIndex][0];
                    size_t particleIndex2 = topologyProperty[bondIndex][1];
                    if(particleIndex1 >= particlePeriodicImageProperty.size() || particleIndex2 >= particlePeriodicImageProperty.size())
                        continue;
                    const Vector3I& particleShift1 = particlePeriodicImageProperty[particleIndex1];
                    const Vector3I& particleShift2 = particlePeriodicImageProperty[particleIndex2];
                    periodicImageProperty[bondIndex] += particleShift1 - particleShift2;
                }
            }
        }

        // After unwrapping the particles, the PBC image flags are obsolete.
        // It's time to remove the particle property.
        outputParticles->removeProperty(outputParticles->getProperty(Particles::PeriodicImageProperty));

        state.setStatus(tr("Unwrapping particle positions using stored PBC image information."));

        return;
    }

    // Check if periodic cell boundary crossing have been precomputed or not.
    if(request.time() > unwrappedUpToTime()) {
        if(ExecutionContext::isInteractive())
            state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("Particle crossings of periodic cell boundaries have not been determined yet.")));
        else
            throw Exception(tr("Particle crossings of periodic cell boundaries have not been determined yet or the requested trajectory frame is out of range. Cannot unwrap trajectories at this time."));
        return;
    }

    // Reverse any cell shear flips made by LAMMPS.
    if(!unflipRecords().empty() && request.time() >= unflipRecords().front().first) {
        auto iter = unflipRecords().rbegin();
        while(iter->first > request.time()) {
            ++iter;
            OVITO_ASSERT(iter != unflipRecords().rend());
        }
        const std::array<int,3>& flipState = iter->second;
        SimulationCell* simCellObj = state.expectMutableObject<SimulationCell>();
        AffineTransformation cell = simCellObj->cellMatrix();
        cell.column(2) += cell.column(0) * flipState[1] + cell.column(1) * flipState[2];
        cell.column(1) += cell.column(0) * flipState[0];
        simCellObj->setCellMatrix(cell);
    }

    if(unwrapRecords().empty())
        return;

    // Get current simulation cell.
    const SimulationCell* simCell = state.expectObject<SimulationCell>();

    // Make a modifiable copy of the particles object.
    Particles* outputParticles = state.expectMutableObject<Particles>();

    // Make a modifiable copy of the particle position property.
    BufferWriteAccess<Point3, access_mode::read_write> posProperty = outputParticles->expectMutableProperty(Particles::PositionProperty);

    // Get particle identifiers.
    BufferReadAccess<IdentifierIntType> identifierProperty = outputParticles->getProperty(Particles::IdentifierProperty);
    if(identifierProperty && identifierProperty.size() != posProperty.size())
        identifierProperty.reset();

    // Compute unwrapped particle coordinates.
    qlonglong index = 0;
    for(Point3& p : posProperty) {
        auto range = unwrapRecords().equal_range(identifierProperty ? identifierProperty[index] : index);
        bool shifted = false;
        Vector3 pbcShift = Vector3::Zero();
        for(auto iter = range.first; iter != range.second; ++iter) {
            if(std::get<0>(iter->second) <= request.time()) {
                pbcShift[std::get<1>(iter->second)] += std::get<2>(iter->second);
                shifted = true;
            }
        }
        if(shifted) {
            p += simCell->matrix() * pbcShift;
        }
        index++;
    }

    // Unwrap bonds by adjusting their PBC shift vectors.
    if(outputParticles->bonds()) {
        if(BufferReadAccess<ParticleIndexPair> topologyProperty = outputParticles->bonds()->getProperty(Bonds::TopologyProperty)) {
            BufferWriteAccess<Vector3I, access_mode::read_write> periodicImageProperty = outputParticles->makeBondsMutable()->createProperty(DataBuffer::Initialized, Bonds::PeriodicImageProperty);
            for(size_t bondIndex = 0; bondIndex < topologyProperty.size(); bondIndex++) {
                size_t particleIndex1 = topologyProperty[bondIndex][0];
                size_t particleIndex2 = topologyProperty[bondIndex][1];
                if(particleIndex1 >= posProperty.size() || particleIndex2 >= posProperty.size())
                    continue;

                Vector3I& pbcShift = periodicImageProperty[bondIndex];
                auto range1 = unwrapRecords().equal_range(identifierProperty ? identifierProperty[particleIndex1] : particleIndex1);
                auto range2 = unwrapRecords().equal_range(identifierProperty ? identifierProperty[particleIndex2] : particleIndex2);
                for(auto iter = range1.first; iter != range1.second; ++iter) {
                    if(std::get<0>(iter->second) <= request.time()) {
                        pbcShift[std::get<1>(iter->second)] += std::get<2>(iter->second);
                    }
                }
                for(auto iter = range2.first; iter != range2.second; ++iter) {
                    if(std::get<0>(iter->second) <= request.time()) {
                        pbcShift[std::get<1>(iter->second)] -= std::get<2>(iter->second);
                    }
                }
            }
        }
    }
}

/******************************************************************************
* Calculates the information that is needed to unwrap particle coordinates.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::WorkingData::operator()(int frame, const PipelineFlowState& state)
{
    AnimationTime time = _modNode->sourceFrameToAnimationTime(frame);

    // Get simulation cell geometry and boundary conditions.
    const SimulationCell* cell = state.getObject<SimulationCell>();
    if(!cell)
        throw Exception(tr("Input data contains no simulation cell information at frame %1.").arg(frame));
    if(!cell->hasPbcCorrected())
        throw Exception(tr("No periodic boundary conditions set for the simulation cell."));
    AffineTransformation reciprocalCellMatrix = cell->inverseMatrix();

    const Particles* particles = state.getObject<Particles>();
    if(!particles)
        throw Exception(tr("Input data contains no particles at frame %1.").arg(frame));
    BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
    if(identifierProperty && identifierProperty.size() != posProperty.size())
        identifierProperty.reset();

    // Special handling of cell flips in LAMMPS, which occur whenever a tilt factor exceeds +/-50%.
    if(cell->matrix()(1,0) == 0 && cell->matrix()(2,0) == 0 && cell->matrix()(2,1) == 0 && cell->matrix()(0,0) > 0 && cell->matrix()(1,1) > 0) {
        if(_previousCell) {
            std::array<int,3> flipState = _currentFlipState;
            // Detect discontinuities in the three tilt factors of the cell.
            if(cell->hasPbc(0)) {
                FloatType xy1 = _previousCell->matrix()(0,1) / _previousCell->matrix()(0,0);
                FloatType xy2 = cell->matrix()(0,1) / cell->matrix()(0,0);
                if(int flip_xy = (int)std::round(xy2 - xy1))
                    flipState[0] -= flip_xy;
                if(!cell->is2D()) {
                    FloatType xz1 = _previousCell->matrix()(0,2) / _previousCell->matrix()(0,0);
                    FloatType xz2 = cell->matrix()(0,2) / cell->matrix()(0,0);
                    if(int flip_xz = (int)std::round(xz2 - xz1))
                        flipState[1] -= flip_xz;
                }
            }
            if(cell->hasPbc(1) && !cell->is2D()) {
                FloatType yz1 = _previousCell->matrix()(1,2) / _previousCell->matrix()(1,1);
                FloatType yz2 = cell->matrix()(1,2) / cell->matrix()(1,1);
                if(int flip_yz = (int)std::round(yz2 - yz1))
                    flipState[2] -= flip_yz;
            }
            // Emit a timeline record whever a flipping occurred.
            if(flipState != _currentFlipState)
                _modNode->_unflipRecords.emplace_back(time, flipState);
            _currentFlipState = flipState;
        }
        _previousCell = cell;
        // Unflip current simulation cell.
        if(_currentFlipState != std::array<int,3>{{0,0,0}}) {
            AffineTransformation newCellMatrix = cell->matrix();
            newCellMatrix(0,1) += cell->matrix()(0,0) * _currentFlipState[0];
            newCellMatrix(0,2) += cell->matrix()(0,0) * _currentFlipState[1];
            newCellMatrix(1,2) += cell->matrix()(1,1) * _currentFlipState[2];
            reciprocalCellMatrix = newCellMatrix.inverse();
        }
    }

    qlonglong index = 0;
    for(const Point3& p : posProperty) {
        Point3 rp = reciprocalCellMatrix * p;
        // Try to insert new position of particle into map.
        // If an old position already exists, insertion will fail and we can
        // test whether the particle did cross a periodic cell boundary.
        auto result = _previousPositions.insert(std::make_pair(identifierProperty ? identifierProperty[index] : index, rp));
        if(!result.second) {
            Vector3 delta = result.first->second - rp;
            for(size_t dim = 0; dim < 3; dim++) {
                if(cell->hasPbcCorrected(dim)) {
                    int shift = (int)std::round(delta[dim]);
                    if(shift != 0) {
                        // Create a new record when particle has crossed a periodic cell boundary.
                        _modNode->_unwrapRecords.emplace(result.first->first, std::make_tuple(time, (qint8)dim, (qint16)shift));
                    }
                }
            }
            result.first->second = rp;
        }
        index++;
    }

    _modNode->_unwrappedUpToTime = time;
    _modNode->setStatus(tr("Processed input trajectory frame %1 of %2.").arg(frame).arg(_modNode->numberOfSourceFrames()));
}

/******************************************************************************
* Saves the class' contents to an output stream.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    ModificationNode::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x02);
    stream << unwrappedUpToTime();
    stream.endChunk();
    stream.beginChunk(0x02);
    stream.writeSizeT(unwrapRecords().size());
    for(const auto& item : unwrapRecords()) {
        OVITO_STATIC_ASSERT((std::is_same<qlonglong, qint64>::value));
        stream << item.first;
        stream << std::get<0>(item.second);
        stream << std::get<1>(item.second);
        stream << std::get<2>(item.second);
    }
    stream.writeSizeT(unflipRecords().size());
    for(const auto& item : unflipRecords()) {
        stream << item.first;
        stream << std::get<0>(item.second);
        stream << std::get<1>(item.second);
        stream << std::get<2>(item.second);
    }
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from an input stream.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::loadFromStream(ObjectLoadStream& stream)
{
    ModificationNode::loadFromStream(stream);
    stream.expectChunk(0x02);
    stream >> _unwrappedUpToTime;
    stream.closeChunk();
    int version = stream.expectChunkRange(0x01, 1);
    size_t numItems = stream.readSizeT();
    _unwrapRecords.reserve(numItems);
    for(size_t i = 0; i < numItems; i++) {
        UnwrapData::key_type particleId;
        std::tuple_element_t<0, UnwrapData::mapped_type> time;
        std::tuple_element_t<1, UnwrapData::mapped_type> dim;
        std::tuple_element_t<2, UnwrapData::mapped_type> direction;
        stream >> particleId >> time >> dim >> direction;
        _unwrapRecords.emplace(particleId, std::make_tuple(time, dim, direction));
    }
    if(version >= 1) {
        stream.readSizeT(numItems);
        _unflipRecords.reserve(numItems);
        for(size_t i = 0; i < numItems; i++) {
            UnflipData::value_type item;
            stream >> item.first >> item.second[0] >> item.second[1] >> item.second[2];
            _unflipRecords.push_back(item);
        }
    }
    stream.closeChunk();
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::loadFromStreamComplete(ObjectLoadStream& stream)
{
    ModificationNode::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.7:
    // Convert legacy time values from ticks to frames. This requires access to the AnimationSettings object, which is stored at scene level.
    if(stream.formatVersion() <= 30008) {
        QSet<Pipeline*> pipelines = this->pipelines(true);
        if(!pipelines.empty()) {
            if(Scene* scene = (*pipelines.begin())->scene()) {
                if(scene->animationSettings()) {
                    int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
                    _unwrappedUpToTime = AnimationTime::fromFrame(_unwrappedUpToTime.ticks() / ticksPerFrame);
                    for(auto& record : _unwrapRecords) {
                        std::get<0>(record.second) = AnimationTime::fromFrame(std::get<0>(record.second).ticks() / ticksPerFrame);
                    }
                    for(auto& record : _unflipRecords) {
                        std::get<0>(record) = AnimationTime::fromFrame(std::get<0>(record).ticks() / ticksPerFrame);
                    }
                }
            }
        }
    }
}

}   // End of namespace
