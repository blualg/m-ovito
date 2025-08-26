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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationTask.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/ForEach.h>
#include <ovito/core/utilities/concurrent/LaunchTask.h>
#include "UnwrapTrajectoriesModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(UnwrapTrajectoriesModifier);
OVITO_CLASSINFO(UnwrapTrajectoriesModifier, "DisplayName", "Unwrap trajectories");
OVITO_CLASSINFO(UnwrapTrajectoriesModifier, "Description", "Unwrap particle coordinates at periodic cell boundaries and generate continuous trajectories.");
OVITO_CLASSINFO(UnwrapTrajectoriesModifier, "ModifierCategory", "Modification");
IMPLEMENT_CREATABLE_OVITO_CLASS(UnwrapTrajectoriesModificationNode);
OVITO_CLASSINFO(UnwrapTrajectoriesModificationNode, "ClassNameAlias", "UnwrapTrajectoriesModifierApplication");  // For backward compatibility with OVITO 3.9.2
SET_MODIFICATION_NODE_TYPE(UnwrapTrajectoriesModifier, UnwrapTrajectoriesModificationNode);

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool UnwrapTrajectoriesModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>() && input.containsObject<SimulationCell>();
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void UnwrapTrajectoriesModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Unless the unwrapping information has already been computed (up to the current time), indicate that we will do different
    // computations depending on whether the pipeline is evaluated in interactive mode or not.
    UnwrapTrajectoriesModificationNode* modNode = dynamic_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode());
    if(modNode && request.time() > modNode->unwrappedUpToTime()) {
        if(request.interactiveMode())
            evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
        else
            evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
    }
}

/******************************************************************************
 * Launches an asynchronous task to evaluate the node's modifier.
 ******************************************************************************/
SharedFuture<PipelineFlowState> UnwrapTrajectoriesModificationNode::launchModifierEvaluation(ModifierEvaluationRequest&& request, SharedFuture<PipelineFlowState> inputFuture)
{
    using UnwrapTrajectoriesTask = ModifierEvaluationTask<UnwrapTrajectoriesModifier, SharedFuture<void>>;

    return launchTask(
        std::make_shared<UnwrapTrajectoriesTask>(std::move(request), _unwrapWeakFuture.lock()),
        std::move(inputFuture));
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> UnwrapTrajectoriesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state, SharedFuture<void> unwrapFuture)
{
    if(!state)
        return std::move(state);

    // If the periodic image flags property is present, use it to unwrap particle positions.
    if(state.expectObject<Particles>()->getProperty(Particles::PeriodicImageProperty)) {
        // The actual work can be performed in a separate thread.
        return asyncLaunch([state = std::move(state)]() mutable {

            // Simulation cell is needed for this.
            const SimulationCell* cell = state.expectObject<SimulationCell>();

            // Make a modifiable copy of the particles object.
            Particles* outputParticles = state.expectMutableObject<Particles>();
            outputParticles->verifyIntegrity();

            // Perform the coordinate unwrapping.
            outputParticles->unwrapCoordinates(*cell);

            state.setStatus(tr("Unwrapping particle coordinates using stored PBC image flags."));
            return std::move(state);
        });
    }

    UnwrapTrajectoriesModificationNode* modNode = dynamic_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode());
    if(!modNode)
        throw Exception(tr("Internal error: The UnwrapTrajectoriesModifier is not associated with a valid UnwrapTrajectoriesModificationNode."));

    if(!request.interactiveMode()) {
        // Without the periodic image flags information, we have to scan the particle trajectories
        // from beginning to end before making them continuous.
        return modNode->detectPeriodicCrossings(request, std::move(unwrapFuture)).then(ObjectExecutor(modNode), [state = std::move(state), request]() mutable {
            static_object_cast<UnwrapTrajectoriesModificationNode>(request.modificationNode())->unwrapParticleCoordinates(request, state);
            return std::move(state);
        });
    }
    else {
        modNode->unwrapParticleCoordinates(request, state);
        return std::move(state);
    }
}

/******************************************************************************
* Processes all frames of the input trajectory to detect periodic crossings
* of the particles.
******************************************************************************/
SharedFuture<void> UnwrapTrajectoriesModificationNode::detectPeriodicCrossings(const ModifierEvaluationRequest& request, SharedFuture<void> unwrapFuture)
{
    OVITO_ASSERT(request.modificationNode() == this);

    if(!unwrapFuture || unwrapFuture.isCanceled()) {

        // Determine the range of animation frames to be processed.
        int startFrame = 0;
        int endFrame = numberOfSourceFrames();
        if(endFrame <= 1 || (unwrappedUpToTime() != AnimationTime::negativeInfinity() && animationTimeToSourceFrame(unwrappedUpToTime()) >= endFrame - 1))
            return Future<void>::createImmediateEmpty();
        auto inputFrameRange = boost::irange(startFrame, endFrame);
        request.modificationNode()->setStatus(tr("Processing %1 trajectory frames...").arg(boost::size(inputFrameRange)));
        auto progress = std::make_unique<TaskProgress>(this_task::ui());
        progress->setText(tr("Unwrapping particle trajectories"));
        progress->setMaximum(boost::size(inputFrameRange));

        // Iterate over all frames of the input range in sequential order.
        unwrapFuture = for_each_sequential(
            std::move(inputFrameRange),
            DeferredObjectExecutor(this), // Require deferred execution
            // Requests the next frame from the upstream pipeline.
            [request=request](int frame) mutable -> SharedFuture<PipelineFlowState> {
                request.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                return request.modificationNode()->evaluateInput(request).asFuture();
            },
            // This object processes each frame's data.
            WorkingData{this, std::move(progress)});

        // Keep a weak reference to the task.
        _unwrapWeakFuture = unwrapFuture;
    }
    return unwrapFuture;
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
    _mostRecentPbcShifts.clear();
    _mostRecentIdMap.clear();
    _unwrapWeakFuture.reset();
}

/******************************************************************************
* Sends an event to all dependents of this RefTarget.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        // Do not discard stored information if the modifier is turned off and on again by the user.
        if(static_cast<const TargetChangedEvent&>(event).field() != PROPERTY_FIELD(Modifier::isEnabled) || event.sender() != modifier()) {
            // Throw away precomputed information when the modifier or the upstream pipeline change.
            invalidateUnwrapData();
        }
    }
    ModificationNode::notifyDependentsImpl(event);
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

    // The pipeline system may call evaluateModifier() with an outdated trajectory frame, which doesn't match the current
    // animation time when doing an interactive viewport update. This would lead to artifacts, because particles might get unwrapped even though they haven't crossed
    // a periodic cell boundary yet. To avoid this from happening, we try to determine the true animation time of the
    // current input data collection and use it for looking up the unwrap information.
    AnimationTime time = request.time();
    if(request.interactiveMode()) {
        int sourceFrame = state.data()->sourceFrame();
        if(sourceFrame != -1)
            time = request.modificationNode()->sourceFrameToAnimationTime(sourceFrame);
    }

    // Check if periodic cell boundary crossing have been precomputed or not.
    if(time > unwrappedUpToTime()) {
        if(this_task::isInteractive())
            state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("Particle crossings of periodic cell boundaries have not been determined yet.")));
        else
            throw Exception(tr("Particle crossings of periodic cell boundaries have not been determined yet or the requested trajectory frame is out of range. Cannot unwrap trajectories in frame %1.").arg(time.frame()));
        return;
    }

    // Reverse any cell shear flips made by LAMMPS.
    if(!unflipRecords().empty() && time >= unflipRecords().front().time) {
        auto iter = unflipRecords().rbegin();
        while(iter->time > time) {
            ++iter;
            OVITO_ASSERT(iter != unflipRecords().rend());
        }
        const std::array<int,3>& flipState = iter->flipState;
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
    const AffineTransformation cellMatrix = simCell->cellMatrix();

    // Make a modifiable copy of the particles object.
    Particles* outputParticles = state.expectMutableObject<Particles>();
    const size_t particleCount = outputParticles->elementCount();

    // Make a modifiable copy of the particle position property.
    BufferWriteAccess<Point3, access_mode::read_write> posProperty = outputParticles->expectMutableProperty(Particles::PositionProperty);

    // The accumulated PBC shift vector for each particle.
    std::vector<Vector3I> pbcShifts;

    // Get particle identifiers.
    bool allUnwrapped = true;
    if(BufferReadAccess<IdentifierIntType> identifierProperty = outputParticles->getProperty(Particles::IdentifierProperty)) {

        // Build id-to-index map.
        std::unordered_map<IdentifierIntType, size_t> idMap;
        idMap.reserve(particleCount);
        size_t index = 0;
        for(auto id : identifierProperty)
            idMap.emplace(id, index++);

        // Preparation step, which might reuse the most recent PBC shift vectors.
        auto startAt = unwrapRecords().cbegin();
        pbcShifts.resize(particleCount, Vector3I::Zero());
        if(!_mostRecentPbcShifts.empty() && _mostRecentTime <= time) {
            // Determine how many records we can skip in the time-sorted list, i.e., which records are already accounted for in the most recent shift vectors.
            startAt = std::upper_bound(unwrapRecords().cbegin(), unwrapRecords().cend(), _mostRecentTime, [](const AnimationTime& a, const UnwrapRecord& b) { return a < b.time; });
            OVITO_ASSERT(startAt == unwrapRecords().cend() || startAt->time > _mostRecentTime);
            // Remap most recent shift vectors to current particle ordering.
            for(const auto& [id, index] : _mostRecentIdMap) {
                OVITO_ASSERT(index >= 0 && index < _mostRecentPbcShifts.size());
                if(auto iter = idMap.find(id); iter != idMap.end()) {
                    pbcShifts[iter->second] = _mostRecentPbcShifts[index];
                }
            }
            allUnwrapped = false;
        }

        // Accumulate the shifts by stepping through the unwrap records up to the current time.
        for(auto record = startAt; record != unwrapRecords().cend(); ++record) {
            if(record->time > time)
                break;
            if(auto iter = idMap.find(record->id); iter != idMap.end()) {
                pbcShifts[iter->second][record->dimension] += record->direction;
                allUnwrapped = false;
            }
        }

        _mostRecentIdMap = std::move(idMap);
    }
    else {
        // Preparation step, which might reuse the most recent PBC shift vectors.
        auto startAt = unwrapRecords().cbegin();
        if(_mostRecentPbcShifts.empty() || _mostRecentTime > time) {
            pbcShifts.resize(particleCount, Vector3I::Zero());
        }
        else {
            // Determine how many records we can skip in the time-sorted list, i.e., which records are already accounted for in the most recent shift vectors.
            startAt = std::upper_bound(unwrapRecords().cbegin(), unwrapRecords().cend(), _mostRecentTime, [](const AnimationTime& a, const UnwrapRecord& b) { return a < b.time; });
            OVITO_ASSERT(startAt == unwrapRecords().cend() || startAt->time > _mostRecentTime);
            pbcShifts = std::move(_mostRecentPbcShifts);
            pbcShifts.resize(particleCount, Vector3I::Zero());
            allUnwrapped = false;
        }

        // Accumulate the shifts by stepping through the unwrap records up to the current time.
        for(auto record = startAt; record != unwrapRecords().cend(); ++record) {
            if(record->time > time)
                break;
            if(record->id < particleCount) {
                OVITO_ASSERT(record->id >= 0);
                pbcShifts[record->id][record->dimension] += record->direction;
                allUnwrapped = false;
            }
        }

        _mostRecentIdMap.clear();
    }

    if(!allUnwrapped) {
        // Apply accumulated shift vectors to obtain unwrapped particle coordinates.
        auto pbcShift = pbcShifts.cbegin();
        for(Point3& p : posProperty) {
            if(*pbcShift != Vector3I::Zero())
                p += cellMatrix * pbcShift->toDataType<FloatType>();
            ++pbcShift;
        }
        OVITO_ASSERT(pbcShift == pbcShifts.cend());

        // Unwrap bonds by adjusting their PBC shift vectors.
        if(outputParticles->bonds()) {
            if(BufferReadAccess<ParticleIndexPair> topologyProperty = outputParticles->bonds()->getProperty(Bonds::TopologyProperty)) {
                BufferWriteAccess<Vector3I, access_mode::read_write> periodicImageProperty = outputParticles->makeBondsMutable()->createProperty(DataBuffer::Initialized, Bonds::PeriodicImageProperty);
                for(size_t bondIndex = 0; bondIndex < topologyProperty.size(); bondIndex++) {
                    size_t particleIndex1 = topologyProperty[bondIndex][0];
                    size_t particleIndex2 = topologyProperty[bondIndex][1];
                    if(particleIndex1 >= posProperty.size() || particleIndex2 >= posProperty.size())
                        continue;
                    periodicImageProperty[bondIndex] += pbcShifts[particleIndex1] - pbcShifts[particleIndex2];
                }
            }
        }
    }

    _mostRecentTime = time;
    _mostRecentPbcShifts = std::move(pbcShifts);
}

/******************************************************************************
* Calculates the information that is needed to unwrap particle coordinates.
******************************************************************************/
void UnwrapTrajectoriesModificationNode::WorkingData::operator()(int frame, const PipelineFlowState& state)
{
    if(!_modNode->_unwrapWeakFuture.lock())
        return;

    AnimationTime time = _modNode->sourceFrameToAnimationTime(frame);

    // Get simulation cell geometry and boundary conditions.
    const SimulationCell* cell = state.getObject<SimulationCell>();
    if(!cell)
        throw Exception(tr("Unwrap operation requires a simulation cell. None defined at trajectory frame %1.").arg(frame));
    if(!cell->hasPbcCorrected())
        throw Exception(tr("No periodic boundary conditions set for the simulation cell."));
    AffineTransformation reciprocalCellMatrix = cell->inverseMatrix();

    const Particles* particles = state.getObject<Particles>();
    if(!particles)
        throw Exception(tr("Input data contains no particles at frame %1.").arg(frame));
    particles->verifyIntegrity();
    BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
    BufferReadAccess<IdentifierIntType> identifierProperty = particles->getProperty(Particles::IdentifierProperty);

    // Special handling of cell flips in LAMMPS, which occur whenever a tilt factor exceeds +/-50%.
    if(cell->matrix()(1,0) == 0 && cell->matrix()(2,0) == 0 && cell->matrix()(2,1) == 0 && cell->matrix()(0,0) > 0 && cell->matrix()(1,1) > 0) {
        if(_previousCell) {
            std::array<int, 3> flipState = _currentFlipState;
            // Detect discontinuities in the three tilt factors of the cell.
            if(cell->hasPbc(0)) {
                FloatType xy1 = _previousCell->matrix()(0,1) / _previousCell->matrix()(0,0);
                FloatType xy2 = cell->matrix()(0,1) / cell->matrix()(0,0);
                if(auto flip_xy = std::lround(xy2 - xy1))
                    flipState[0] -= flip_xy;
                if(!cell->is2D()) {
                    FloatType xz1 = _previousCell->matrix()(0,2) / _previousCell->matrix()(0,0);
                    FloatType xz2 = cell->matrix()(0,2) / cell->matrix()(0,0);
                    if(auto flip_xz = std::lround(xz2 - xz1))
                        flipState[1] -= flip_xz;
                }
            }
            if(cell->hasPbc(1) && !cell->is2D()) {
                FloatType yz1 = _previousCell->matrix()(1,2) / _previousCell->matrix()(1,1);
                FloatType yz2 = cell->matrix()(1,2) / cell->matrix()(1,1);
                if(auto flip_yz = std::lround(yz2 - yz1))
                    flipState[2] -= flip_yz;
            }
            // Emit a timeline record whever a flipping occurred.
            if(flipState != _currentFlipState)
                _modNode->_unflipRecords.push_back({time, flipState});
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

    const std::array<bool, 3> pbcFlags = cell->pbcFlagsCorrected();

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
                if(pbcFlags[dim]) {
                    if(auto shift = std::lround(delta[dim])) {
                        // Create a new record when the particle has crossed a periodic cell boundary.
                        _modNode->_unwrapRecords.push_back({result.first->first, time, (qint8)dim, (qint16)shift});
                    }
                }
            }
            result.first->second = rp;
        }
        index++;
    }

    _modNode->_unwrappedUpToTime = time;
    _progress->incrementValueNoCancel();
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
        stream << item.id;
        stream << item.time;
        stream << item.dimension;
        stream << item.direction;
    }
    stream.writeSizeT(unflipRecords().size());
    for(const auto& item : unflipRecords()) {
        stream << item.time;
        stream << item.flipState[0];
        stream << item.flipState[1];
        stream << item.flipState[2];
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
    _unwrapRecords.resize(numItems);
    for(UnwrapRecord& item : _unwrapRecords) {
        stream >> item.id >> item.time >> item.dimension >> item.direction;
    }
    // For backward compatibility with OVITO 3.10, which used a std::unordered_map for storing the unwrap records:
    boost::sort(_unwrapRecords, [](const UnwrapRecord& a, const UnwrapRecord& b) { return a.time < b.time; });
    if(version >= 1) {
        stream.readSizeT(numItems);
        _unflipRecords.resize(numItems);
        for(UnflipRecord& item : _unflipRecords) {
            stream >> item.time >> item.flipState[0] >> item.flipState[1] >> item.flipState[2];
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
            if(SceneNode* sceneNode = (*pipelines.begin())->someSceneNode()) {
                if(Scene* scene = sceneNode->scene()) {
                    if(scene->animationSettings()) {
                        int ticksPerFrame = std::lround(4800.0f / scene->animationSettings()->framesPerSecond());
                        _unwrappedUpToTime = AnimationTime::fromFrame(_unwrappedUpToTime.ticks() / ticksPerFrame);
                        for(auto& record : _unwrapRecords) {
                            record.time = AnimationTime::fromFrame(record.time.ticks() / ticksPerFrame);
                        }
                        for(auto& record : _unflipRecords) {
                            record.time = AnimationTime::fromFrame(record.time.ticks() / ticksPerFrame);
                        }
                    }
                }
            }
        }
    }
}

}   // End of namespace
