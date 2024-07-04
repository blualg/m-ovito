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

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNode.h>
#include <ovito/crystalanalysis/objects/MicrostructurePhase.h>
#include <ovito/stdobj/simcell/PeriodicDomainObject.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Stores a collection of dislocation segments.
 */
class OVITO_CRYSTALANALYSIS_EXPORT DislocationNetwork : public PeriodicDomainObject
{
    OVITO_CLASS(DislocationNetwork)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the list of dislocation segments.
    const std::vector<DislocationSegment*>& segments() const { return _segments; }

    /// Returns the list of dislocation segments.
    std::vector<DislocationSegment*>& segments() { return _segments; }

    /// Allocates a new dislocation segment terminated by two nodes.
    DislocationSegment* createSegment(const ClusterVector& burgersVector);

    /// Removes a segment from the global list of segments.
    void discardSegment(DislocationSegment* segment);

    /// Smoothens and coarsens the dislocation lines.
    void smoothDislocationLines(int lineSmoothingLevel, FloatType linePointInterval);

    /// Adds a new crystal structures to the list.
    void addCrystalStructure(const MicrostructurePhase* structure) { _crystalStructures.push_back(this, PROPERTY_FIELD(crystalStructures), structure); }

    /// Removes a crystal structure.
    void removeCrystalStructure(int index) { _crystalStructures.remove(this, PROPERTY_FIELD(crystalStructures), index); }

    /// Returns the crystal structure with the given ID, or null if no such structure exists.
    const MicrostructurePhase* structureById(int id) const {
        for(const MicrostructurePhase* stype : crystalStructures())
            if(stype->numericId() == id)
                return stype;
        return nullptr;
    }

    /// Returns the crystal structure with the given name, or null if no such structure exists.
    const MicrostructurePhase* structureByName(const QString& name) const {
        for(const MicrostructurePhase* stype : crystalStructures())
            if(stype->name() == name)
                return stype;
        return nullptr;
    }

    /// Indicates whether this data object wants to be shown in the pipeline editor under the data source section.
    virtual PipelineEditorObjectListMode pipelineEditorObjectListMode() const override {
        return PipelineEditorObjectListMode::ShowIncludingSubObjects;
    }

    /// Creates an editable proxy object for this DataObject and synchronizes its parameters.
    virtual void updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const override;

protected:

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

    /// Is called when the value of a reference field of this object changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

private:

    /// Smoothes the sampling points of a dislocation line.
    static void smoothDislocationLine(int smoothingLevel, std::deque<Point3>& line, bool isLoop);

    /// Removes some of the sampling points from a dislocation line.
    static void coarsenDislocationLine(FloatType linePointInterval, const std::deque<Point3>& input, const std::deque<int>& coreSize, std::deque<Point3>& output, std::deque<int>& outputCoreSize, bool isClosedLoop, bool isInfiniteLine);

private:

    /// List of crystal structures.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(DataOORef<const MicrostructurePhase>, crystalStructures, setCrystalStructures);

    /// The associated cluster graph.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(DataOORef<const ClusterGraph>, clusterGraph, setClusterGraph);

    // Used to allocate memory for DislocationNode instances.
    MemoryPool<DislocationNode> _nodePool;

    /// The list of dislocation segments.
    std::vector<DislocationSegment*> _segments;

    /// Used to allocate memory for DislocationSegment objects.
    MemoryPool<DislocationSegment> _segmentPool;
};

}   // End of namespace
