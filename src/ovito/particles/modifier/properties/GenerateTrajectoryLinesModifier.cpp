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
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
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

/**
 * Helper class that builds up the trajectory lines by sampling the particle positions.
*/
class TrajectoryGenerator : public AsynchronousTask<DataOORef<const Lines>>
{
public:

    /// Constructor.
    TrajectoryGenerator(bool transferParticleProperties, bool onlySelectedParticles, bool unwrapTrajectories, const PropertyReference& particleProperty, OORef<LinesVis> trajectoryVis, ModificationNode* modNode) :
        _transferParticleProperties(transferParticleProperties),
        _onlySelectedParticles(onlySelectedParticles),
        _unwrapTrajectories(unwrapTrajectories),
        _particleProperty(particleProperty),
        _trajectoryVis(std::move(trajectoryVis)),
        _modNode(modNode) {}

    /// Adds a new frame of particle positions to the collected trajectory data.
    void addFrame(int frame, const PipelineFlowState& state) {
        const Particles* particles = state.getObject<Particles>();
        if(!particles)
            throw Exception(GenerateTrajectoryLinesModifier::tr("There are no particles in the modifier's input at frame %1.").arg(frame));
        particles->verifyIntegrity();

        // Collect the list of frames which have sampled so far.
        int timeIndex = _sampleFrames.size();
        _sampleFrames.push_back(frame);

        if(timeIndex == 0) {
            // Store the first frame of the trajectory for later use.
            _firstState = state;

            // Determine which input particles to collect.
            if(_onlySelectedParticles) {
                if(BufferReadAccess<SelectionIntType> selectionProperty = particles->getProperty(Particles::SelectionProperty)) {
                    BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                    if(identifierProperty && identifierProperty.size() == selectionProperty.size()) {
                        const auto* s = selectionProperty.cbegin();
                        for(auto id : identifierProperty)
                            if(*s++) _selectedIdentifiers.insert(id);
                    }
                    else {
                        const auto* s = selectionProperty.cbegin();
                        for(size_t index = 0; index < selectionProperty.size(); index++)
                            if(*s++) _selectedIndices.push_back(index);
                    }
                }
                if(_selectedIndices.empty() && _selectedIdentifiers.empty())
                    throw Exception(GenerateTrajectoryLinesModifier::tr("Particle selection has not been defined or is empty in first trajectory frame."));
            }
        }

        // Get the particle property to be sampled.
        RawBufferReadAccess particleSamplingProperty;
        if(_transferParticleProperties) {
            if(!_particleProperty)
                throw Exception(GenerateTrajectoryLinesModifier::tr("Please select a particle property to be sampled."));
            particleSamplingProperty = _particleProperty.findInContainer(particles);
            if(!particleSamplingProperty)
                throw Exception(GenerateTrajectoryLinesModifier::tr("The particle property '%1' to be sampled and transferred to the trajectory lines does not exist (at frame %2). "
                    "Perhaps you need to restrict the sampling time interval to those times where the property is available.").arg(_particleProperty.name()).arg(frame));
        }

        BufferReadAccess<Point3> posProperty = particles->expectProperty(Particles::PositionProperty);
        if(_onlySelectedParticles) {
            if(!_selectedIdentifiers.empty()) {
                BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
                if(!identifierProperty)
                    throw Exception(GenerateTrajectoryLinesModifier::tr("Input particles do not have identifiers at frame %1.").arg(frame));

                // Create a mapping from IDs to indices.
                std::unordered_map<int64_t, size_t> idmap;
                size_t index = 0;
                for(auto id : identifierProperty)
                    idmap.insert(std::make_pair(id, index++));

                for(auto id : _selectedIdentifiers) {
                    if(auto entry = idmap.find(id); entry != idmap.end()) {
                        _pointData.push_back(posProperty[entry->second]);
                        _timeData.push_back(timeIndex);
                        _idData.push_back(id);
                        if(particleSamplingProperty) {
                            const auto* dataBegin = particleSamplingProperty.cdata(entry->second, 0);
                            _samplingPropertyData.insert(_samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
                        }
                    }
                }
            }
            else {
                // Add coordinates of selected particles by index.
                for(auto index : _selectedIndices) {
                    if(index < posProperty.size()) {
                        _pointData.push_back(posProperty[index]);
                        _timeData.push_back(timeIndex);
                        _idData.push_back(index);
                        if(particleSamplingProperty) {
                            const auto* dataBegin = particleSamplingProperty.cdata(index, 0);
                            _samplingPropertyData.insert(_samplingPropertyData.end(), dataBegin, dataBegin + particleSamplingProperty.stride());
                        }
                    }
                }
            }
        }
        else {
            // Add coordinates of all particles.
            _pointData.insert(_pointData.end(), posProperty.cbegin(), posProperty.cend());
            BufferReadAccess<int64_t> identifierProperty = particles->getProperty(Particles::IdentifierProperty);
            if(identifierProperty && identifierProperty.size() == posProperty.size()) {
                // Particles with IDs.
                _idData.insert(_idData.end(), identifierProperty.cbegin(), identifierProperty.cend());
            }
            else {
                // Particles without IDs.
                _idData.resize(_idData.size() + posProperty.size());
                std::iota(_idData.begin() + _timeData.size(), _idData.end(), 0);
            }
            _timeData.resize(_timeData.size() + posProperty.size(), timeIndex);
            if(particleSamplingProperty)
                _samplingPropertyData.insert(_samplingPropertyData.end(), particleSamplingProperty.cdata(), particleSamplingProperty.cdata() + particleSamplingProperty.size() * particleSamplingProperty.stride());
        }

        // Obtain the simulation cell geometry at the current animation time.
        if(_unwrapTrajectories) {
            if(const SimulationCell* cell = state.getObject<SimulationCell>()) {
                _cells.push_back(cell);
            }
            else {
                _cells.push_back({});
            }
        }
    }

    /// Builds the trajectory lines from the collected data.
    virtual void perform() override {

        // Sort vertex data to obtain continuous trajectory lines.
        this_task::setProgressMaximum(0);
        this_task::setProgressText(GenerateTrajectoryLinesModifier::tr("Sorting trajectory data"));
        std::vector<size_t> permutation(_pointData.size());
        std::iota(permutation.begin(), permutation.end(), (size_t)0);
        std::sort(permutation.begin(), permutation.end(), [&](size_t a, size_t b) {
            if(_idData[a] < _idData[b]) return true;
            if(_idData[a] > _idData[b]) return false;
            return _timeData[a] < _timeData[b];
        });
        this_task::throwIfCanceled();

        // Create the lines data container object.
        DataOORef<Lines> trajectoryLines = DataOORef<Lines>::create();
        trajectoryLines->setTitle(GenerateTrajectoryLinesModifier::tr("Particle trajectories"));
        trajectoryLines->setIdentifier(_firstState.generateUniqueIdentifier<Lines>(QStringLiteral("trajectories")));

        // Copy re-ordered trajectory points.
        trajectoryLines->setElementCount(_pointData.size());
        BufferWriteAccess<Point3, access_mode::discard_read_write> trajPosProperty =
            trajectoryLines->createProperty(Lines::PositionProperty);
        auto piter = permutation.cbegin();
        for(Point3& p : trajPosProperty) {
            p = _pointData[*piter++];
        }

        // Copy re-ordered trajectory time stamps.
        BufferWriteAccess<int32_t, access_mode::discard_write> trajTimeProperty =
            trajectoryLines->createProperty(Lines::SampleTimeProperty);
        piter = permutation.cbegin();
        for(int& t : trajTimeProperty) {
            t = _sampleFrames[_timeData[*piter++]];
        }

        // Copy re-ordered trajectory ids.
        BufferWriteAccess<int64_t, access_mode::discard_read_write> trajIdProperty =
            trajectoryLines->createProperty(Lines::SectionProperty);
        piter = permutation.cbegin();
        for(int64_t& id : trajIdProperty) {
            id = _idData[*piter++];
        }

        // Create the trajectory line property receiving the sampled particle property values.
        if(_transferParticleProperties && _particleProperty && !_particleProperty.isStandardProperty(&Particles::OOClass(), Particles::PositionProperty)) {
            if(const Property* inputProperty = _particleProperty.findInContainer(_firstState.expectObject<Particles>())) {
                OVITO_ASSERT(_samplingPropertyData.size() == inputProperty->stride() * trajectoryLines->elementCount());
                if(_samplingPropertyData.size() != inputProperty->stride() * trajectoryLines->elementCount())
                    throw Exception(GenerateTrajectoryLinesModifier::tr("Sampling buffer size mismatch. Sampled particle property '%1' seems to have a varying component count.").arg(inputProperty->name()));

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
                const auto* src = _samplingPropertyData.data();
                auto* dst = samplingProperty.data();
                size_t stride = samplingProperty.stride();
                piter = permutation.cbegin();
                for(size_t mapping : permutation) {
                    OVITO_ASSERT(stride * (mapping + 1) <= _samplingPropertyData.size());
                    std::memcpy(dst, src + stride * mapping, stride);
                    dst += stride;
                }
            }
        }
        this_task::throwIfCanceled();

        // Unwrap trajectory vertices at periodic boundaries of the simulation cell.
        if(_unwrapTrajectories && _pointData.size() >= 2 && !_cells.empty() && _cells.front() && _cells.front()->hasPbcCorrected()) {
            this_task::setProgressText(GenerateTrajectoryLinesModifier::tr("Unwrapping trajectory lines"));
            this_task::setProgressMaximum(trajPosProperty.size() - 1);
            Point3* pos = trajPosProperty.begin();
            piter = permutation.cbegin();
            const int64_t* id = trajIdProperty.cbegin();
            for(auto pos_end = pos + trajPosProperty.size() - 1; pos != pos_end; ++pos, ++piter, ++id) {
                this_task::incrementProgressValue();
                if(id[0] == id[1]) {
                    const SimulationCell* cell1 = _cells[_timeData[piter[0]]];
                    const SimulationCell* cell2 = _cells[_timeData[piter[1]]];
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

        // Assign the visual element, which renders the trajectory lines.
        trajectoryLines->setVisElement(std::move(_trajectoryVis));
        // Pass generated trajectory lines to the main thread.
        setResult(std::move(trajectoryLines));

        // Release working data at the end of the operation.
        _firstState.reset();
        decltype(_sampleFrames)().swap(_sampleFrames);
        decltype(_selectedIndices)().swap(_selectedIndices);
        decltype(_selectedIdentifiers)().swap(_selectedIdentifiers);
        decltype(_pointData)().swap(_pointData);
        decltype(_timeData)().swap(_timeData);
        decltype(_idData)().swap(_idData);
        decltype(_samplingPropertyData)().swap(_samplingPropertyData);
        decltype(_cells)().swap(_cells);
    }

private:

    const bool _transferParticleProperties;
    const bool _onlySelectedParticles;
    const bool _unwrapTrajectories;
    const PropertyReference _particleProperty;
    OORef<LinesVis> _trajectoryVis;
    OOWeakRef<const RefTarget> _modNode;
    PipelineFlowState _firstState;
    std::vector<int> _sampleFrames;
    std::vector<size_t> _selectedIndices;
    std::set<int64_t> _selectedIdentifiers;
    std::vector<Point3> _pointData;
    std::vector<int32_t> _timeData;
    std::vector<int64_t> _idData;
    std::vector<DataBuffer::Byte> _samplingPropertyData;
    std::vector<DataOORef<const SimulationCell>> _cells;
};

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> GenerateTrajectoryLinesModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Need our type of ModificationNode.
    GenerateTrajectoryLinesModificationNode* modNode = dynamic_object_cast<GenerateTrajectoryLinesModificationNode>(request.modificationNode());
    if(!modNode)
        return std::move(state);

    // Get a reference to the SharedFuture storing the results of a previous run.
    SharedFuture<DataOORef<const Lines>>& samplingOperation = modNode->_samplingOperation;

    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = modNode->getCachedPipelineNodeOutput(request.time(), true)) {
            // Adopt all data objects computed by the modifier from the cached state.
            for(const DataObject* obj : cachedState.data()->objects()) {
                if(obj->createdByNode() == modNode)
                    state.addObject(obj);
            }
        }
        return std::move(state);
    }

    // Is there an ongoing or already completed operation that we can reuse?
    if(!samplingOperation || samplingOperation.isCanceled()) {
        // If not, start a new sampling operation that loops over all trajectory frames.

        // Determine time interval over which to generate trajectories and which need to be fetched from the upstream pipeline.
        int startFrame = useCustomInterval() ? customIntervalStart() : 0;
        int endFrame = useCustomInterval() ? (customIntervalEnd() + 1) : modNode->numberOfSourceFrames();
        if(endFrame <= startFrame || everyNthFrame() < 1)
            throw Exception(tr("Trajectory range contains zero frames. Cannot generate trajectory lines over this time interval."));

        // Loop over all input animation frames and gather particle position data.
        Future<std::shared_ptr<TrajectoryGenerator>> future = for_each_sequential(
            boost::irange(startFrame, endFrame, everyNthFrame()),
            ObjectExecutor(modNode, true), // Request deferred execution
            [request = request](int frame) mutable -> SharedFuture<PipelineFlowState> {
                this_task::setProgressText(tr("Generating trajectory lines"));
                // Evaluate upstream pipeline at current frame.
                request.setTime(request.modificationNode()->sourceFrameToAnimationTime(frame));
                return request.modificationNode()->evaluateInput(request).asFuture();
            },
            [](int frame, const PipelineFlowState& state, auto& generator) {
                generator->addFrame(frame, state);
            },
            std::make_shared<TrajectoryGenerator>(
                transferParticleProperties(),
                onlySelectedParticles(),
                unwrapTrajectories(),
                particleProperty(),
                trajectoryVis(),
                modNode
            ));

        // After each frame of the input trajectory has been processed, build the final lines.
        samplingOperation = std::move(future).then([](std::shared_ptr<TrajectoryGenerator>&& generator) {
            return launchTask(std::move(generator));
        });

        // Let the modification node indicate its activity in the UI.
        modNode->registerActiveFuture(samplingOperation);
    }

    // Pass generated trajectory lines to the pipeline.
    return samplingOperation.then(*modNode, [state = std::move(state)](const DataOORef<const Lines>& trajectoryLines) mutable {
        state.addObject(trajectoryLines);
        return std::move(state);
    });
}

/******************************************************************************
* Sends an event to all dependents of this RefTarget.
******************************************************************************/
void GenerateTrajectoryLinesModificationNode::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        // Throw away precomputed trajectories when the modifier or the upstream pipeline change.
        // This also discards the stored trajectory lines in case the modifier is turned off by the user.
        _samplingOperation.reset();
    }
    ModificationNode::notifyDependentsImpl(event);
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
