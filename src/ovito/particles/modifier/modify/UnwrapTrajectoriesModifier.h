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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

/**
 * \brief This modifier unwraps the positions of particles that have crossed a periodic boundary
 *        in order to generate continuous trajectories.
 */
class OVITO_PARTICLES_EXPORT UnwrapTrajectoriesModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class UnwrapTrajectoriesModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(UnwrapTrajectoriesModifier, UnwrapTrajectoriesModifierClass)
    OVITO_CLASSINFO("DisplayName", "Unwrap trajectories");
    OVITO_CLASSINFO("Description", "Unwrap particle coordinates at periodic cell boundaries and generate continuous trajectories.");
    OVITO_CLASSINFO("ModifierCategory", "Modification");

public:

    /// Constructor.
    using Modifier::Modifier;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;
};

/**
 * Used by the UnwrapTrajectoriesModifier to store the information for unfolding the particle trajectories.
 */
class OVITO_PARTICLES_EXPORT UnwrapTrajectoriesModificationNode : public ModificationNode
{
    OVITO_CLASS(UnwrapTrajectoriesModificationNode)
    OVITO_CLASSINFO("ClassNameAlias", "UnwrapTrajectoriesModifierApplication");  // For backward compatibility with OVITO 3.9.2

public:

    /// Data structure holding the precomputed information that is needed to unwrap the particle trajectories.
    /// For each crossing of a particle through a periodic cell boundary, the map contains one entry specifying
    /// the particle's unique ID, the time of the crossing, the spatial dimension and the direction (positive or negative).
    using UnwrapData = std::unordered_multimap<qlonglong, std::tuple<AnimationTime, qint8, qint16>>; // Note: using qlonglong instead of IdentifierIntType here fore backward file compatibility with OVITO 3.8

    /// Data structure holding the precomputed information that is needed to undo flipping of sheared simulation cells in LAMMPS.
    using UnflipData = std::vector<std::pair<AnimationTime, std::array<int,3>>>;

    /// Constructor.
    using ModificationNode::ModificationNode;

    /// Indicates the animation time up to which trajectories have already been unwrapped.
    AnimationTime unwrappedUpToTime() const { return _unwrappedUpToTime; }

    /// Returns the list of particle crossings through periodic cell boundaries.
    const UnwrapData& unwrapRecords() const { return _unwrapRecords; }

    /// Returns the list of detected cell flips.
    const UnflipData& unflipRecords() const { return _unflipRecords; }

    /// Processes all frames of the input trajectory to detect periodic crossings of the particles.
    SharedFuture<> detectPeriodicCrossings(const ModifierEvaluationRequest& request);

    /// Unwraps the current particle coordinates.
    void unwrapParticleCoordinates(const ModifierEvaluationRequest& request, PipelineFlowState& state);

    /// Rescales the times of all animation keys from the old animation interval to the new interval.
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

protected:

    /// Saves the class' contents to an output stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from an input stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// \brief Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// Throws away the precomputed unwrapping information and interrupts
    /// any computation currently in progress.
    void invalidateUnwrapData();

private:

    /// The operation that processes all trajectory frames in the background to detect periodic crossings of particles.
    SharedFuture<> _unwrapOperation;

    /// The animation time up to which trajectories have already been unwrapped so far.
    AnimationTime _unwrappedUpToTime = AnimationTime::negativeInfinity();

    /// The list of particle crossings through periodic cell boundaries.
    UnwrapData _unwrapRecords;

    /// The list of detected cell flips.
    UnflipData _unflipRecords;

    /// Working state used during processing of the input trajectory.
    struct WorkingData {
        UnwrapTrajectoriesModificationNode* _modNode;
        std::unordered_map<qlonglong,Point3> _previousPositions;
        DataOORef<const SimulationCell> _previousCell;
        std::array<int,3> _currentFlipState{{0,0,0}};

        /// Calculates the information that is needed to unwrap particle coordinates.
        void operator()(int frame, const PipelineFlowState& state);
    };
};

}   // End of namespace
