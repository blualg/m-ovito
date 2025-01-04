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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief This modifier computes the atomic volume and the Voronoi indices of particles.
 */
class OVITO_PARTICLES_EXPORT VoronoiAnalysisModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class VoronoiAnalysisModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(VoronoiAnalysisModifier, VoronoiAnalysisModifierClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

private:

    /// Computes the modifier's results.
    class VoronoiAnalysisEngine
    {
    public:

        /// Constructor.
        VoronoiAnalysisEngine(OOWeakRef<const PipelineNode> createdByNode, size_t particleCount, ConstPropertyPtr positions, ConstPropertyPtr selection, ConstPropertyPtr particleIdentifiers, ConstPropertyPtr radii,
                            const SimulationCell* simCell, SurfaceMesh* polyhedraMesh,
                            bool computeIndices, bool computeBonds, FloatType edgeThreshold, FloatType faceThreshold, FloatType relativeFaceThreshold, OORef<BondsVis> bondsVis) :
            _createdByNode(createdByNode),
            _positions(positions),
            _selection(std::move(selection)),
            _particleIdentifiers(std::move(particleIdentifiers)),
            _radii(std::move(radii)),
            _simCell(simCell),
            _edgeThreshold(edgeThreshold),
            _faceThreshold(faceThreshold),
            _relativeFaceThreshold(relativeFaceThreshold),
            _computeBonds(computeBonds),
            _coordinationNumbers(Particles::OOClass().createStandardProperty(DataBuffer::Initialized, particleCount, Particles::CoordinationProperty)),
            _atomicVolumes(Particles::OOClass().createUserProperty(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1, QStringLiteral("Atomic Volume"))),
            _cavityRadii(Particles::OOClass().createUserProperty(DataBuffer::Initialized, particleCount, Property::FloatDefault, 1, QStringLiteral("Cavity Radius"))),
            _maxFaceOrders(computeIndices ? Particles::OOClass().createUserProperty(DataBuffer::Initialized, particleCount, Property::Int32, 1, QStringLiteral("Max Face Order")) : nullptr),
            _polyhedraMesh(polyhedraMesh),
            _bondsVis(std::move(bondsVis)) {}

        /// Computes the modifier's results.
        void perform();

        /// Injects the computed results into the data pipeline.
        void applyResults(PipelineFlowState& state);

        /// Returns the property storage that contains the computed coordination numbers.
        const PropertyPtr& coordinationNumbers() const { return _coordinationNumbers; }

        /// Returns the property storage that contains the computed atomic volumes.
        const PropertyPtr& atomicVolumes() const { return _atomicVolumes; }

        /// Returns the property storage that contains the computed cavity radii.
        const PropertyPtr& cavityRadii() const { return _cavityRadii; }

        /// Returns the property storage that contains the computed Voronoi indices.
        const PropertyPtr& voronoiIndices() const { return _voronoiIndices; }

        /// Returns the property storage that contains the maximum face order for each particle.
        const PropertyPtr& maxFaceOrders() const { return _maxFaceOrders; }

        /// Returns the volume sum of all Voronoi cells computed by the modifier.
        std::atomic<double>& voronoiVolumeSum() { return _voronoiVolumeSum; }

        /// Returns the maximum number of edges of any Voronoi face.
        std::atomic<int>& maxFaceOrder() { return _maxFaceOrder; }

        /// Returns the generated nearest neighbor bonds.
        std::vector<Bond>& bonds() { return _bonds; }

        const SimulationCell* simCell() const { return _simCell; }
        const ConstPropertyPtr& positions() const { return _positions; }
        const ConstPropertyPtr& selection() const { return _selection; }
        const OOWeakRef<const PipelineNode>& createdByNode() const { return _createdByNode; }

    private:

        const FloatType _edgeThreshold;
        const FloatType _faceThreshold;
        const FloatType _relativeFaceThreshold;
        DataOORef<const SimulationCell> _simCell;
        ConstPropertyPtr _radii;
        ConstPropertyPtr _positions;
        ConstPropertyPtr _selection;
        ConstPropertyPtr _particleIdentifiers;
        bool _computeBonds;
        OOWeakRef<const PipelineNode> _createdByNode;
        OORef<BondsVis> _bondsVis;

        const PropertyPtr _coordinationNumbers;
        const PropertyPtr _atomicVolumes;
        const PropertyPtr _cavityRadii;
        PropertyPtr _voronoiIndices;
        const PropertyPtr _maxFaceOrders;
        std::vector<Bond> _bonds;
        PropertyPtr _bondVoronoiOrder;

        /// The volume sum of all Voronoi cells.
        std::atomic<double> _voronoiVolumeSum{0.0};

        /// The maximum number of edges of a Voronoi face.
        std::atomic<int> _maxFaceOrder{0};

        /// A surface mesh representing the computed polyhedral Voronoi cells.
        SurfaceMesh* _polyhedraMesh;

        /// The total volume of the simulation cell.
        FloatType _simulationBoxVolume;

        /// Maximum length of Voronoi index vectors produced by this modifier.
        constexpr static int FaceOrderStorageLimit = 32;
    };

    /// Controls whether the modifier takes into account only selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelected, setOnlySelected);

    /// Controls whether the modifier takes into account particle radii.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useRadii, setUseRadii);

    /// Controls whether the modifier computes Voronoi indices.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, computeIndices, setComputeIndices);

    /// The minimum length for an edge to be counted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, edgeThreshold, setEdgeThreshold);

    /// The minimum area for a face to be counted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, faceThreshold, setFaceThreshold);

    /// The minimum area for a face to be counted relative to the total polyhedron surface.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, relativeFaceThreshold, setRelativeFaceThreshold);

    /// Controls whether the modifier outputs nearest neighbor bonds.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, computeBonds, setComputeBonds);

    /// Controls whether the modifier outputs Voronoi polyhedra.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, computePolyhedra, setComputePolyhedra);

    /// The vis element for rendering the bonds.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<BondsVis>, bondsVis, setBondsVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE);

    /// The vis element for rendering the polyhedral Voronoi cells.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SurfaceMeshVis>, polyhedraVis, setPolyhedraVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

}   // End of namespace
