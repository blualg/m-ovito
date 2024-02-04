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


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Bonds.h>
#include <ovito/stdobj/util/ElementOrderingFingerprint.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Extends the current particle selection by adding particles to the selection
 *        that are neighbors of an already selected particle.
 */
class OVITO_PARTICLES_EXPORT ExpandSelectionModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(ExpandSelectionModifier, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "Expand selection");
    OVITO_CLASSINFO("Description", "Select particles that are neighbors of already selected particles.");
    OVITO_CLASSINFO("ModifierCategory", "Selection");

public:

    enum ExpansionMode {
        BondedNeighbors,    ///< Expands the selection to particles that are bonded to an already selected particle.
        CutoffRange,        ///< Expands the selection to particles that within a cutoff range of an already selected particle.
        NearestNeighbors,   ///< Expands the selection to the N nearest particles of already selected particles.
    };
    Q_ENUM(ExpansionMode);

    /// Compile-time constant for the maximum number of nearest neighbors that can be taken into account.
    enum { MAX_NEAREST_NEIGHBORS = 30 };

public:

    /// Constructor.
    explicit ExpandSelectionModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<ModifierEnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

private:

    /// The modifier's compute engine.
    class ExpandSelectionEngine : public ModifierEngine
    {
    public:

        /// Constructor.
        ExpandSelectionEngine(const ModifierEvaluationRequest& request, ElementOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, ConstPropertyPtr inputSelection, int numIterations) :
            ModifierEngine(request),
            _numIterations(numIterations),
            _positions(std::move(positions)),
            _simCell(simCell),
            _inputSelection(inputSelection),
            _outputSelection(inputSelection.makeCopy()),
            _inputFingerprint(std::move(fingerprint)) {}

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Performs one iteration of the expansion.
        virtual void expandSelection() = 0;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        const PropertyPtr& outputSelection() { return _outputSelection; }

        void setOutputSelection(PropertyPtr ptr) { _outputSelection = std::move(ptr); }

        size_t numSelectedParticlesInput() const { return _numSelectedParticlesInput; }

        size_t numSelectedParticlesOutput() const { return _numSelectedParticlesOutput; }

        void setNumSelectedParticlesInput(size_t count) { _numSelectedParticlesInput = count; }

        void setNumSelectedParticlesOutput(size_t count) { _numSelectedParticlesOutput = count; }

        const DataOORef<const SimulationCell>& simCell() const { return _simCell; }

        const ConstPropertyPtr& positions() const { return _positions; }

        const ConstPropertyPtr& inputSelection() const { return _inputSelection; }

    protected:

        const int _numIterations;
        DataOORef<const SimulationCell> _simCell;
        ConstPropertyPtr _positions;
        ConstPropertyPtr _inputSelection;
        PropertyPtr _outputSelection;
        size_t _numSelectedParticlesInput;
        size_t _numSelectedParticlesOutput;
        ElementOrderingFingerprint _inputFingerprint;
    };

    /// Computes the expanded selection by using the nearest neighbor criterion.
    class ExpandSelectionNearestEngine : public ExpandSelectionEngine
    {
    public:

        /// Constructor.
        ExpandSelectionNearestEngine(const ModifierEvaluationRequest& request, ElementOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, ConstPropertyPtr inputSelection, int numIterations, int numNearestNeighbors) :
            ExpandSelectionEngine(request, std::move(fingerprint), std::move(positions), simCell, std::move(inputSelection), numIterations),
            _numNearestNeighbors(numNearestNeighbors) {}

        /// Expands the selection by one step.
        virtual void expandSelection() override;

    private:

        const int _numNearestNeighbors;
    };

    /// Computes the expanded selection when using a cutoff range criterion.
    class ExpandSelectionCutoffEngine : public ExpandSelectionEngine
    {
    public:

        /// Constructor.
        ExpandSelectionCutoffEngine(const ModifierEvaluationRequest& request, ElementOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, ConstPropertyPtr inputSelection, int numIterations, FloatType cutoff) :
            ExpandSelectionEngine(request, std::move(fingerprint), std::move(positions), simCell, std::move(inputSelection), numIterations),
            _cutoffRange(cutoff) {}

        /// Expands the selection by one step.
        virtual void expandSelection() override;

    private:

        const FloatType _cutoffRange;
    };

    /// Computes the expanded selection when using bonds.
    class ExpandSelectionBondedEngine : public ExpandSelectionEngine
    {
    public:

        /// Constructor.
        ExpandSelectionBondedEngine(const ModifierEvaluationRequest& request, ElementOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, ConstPropertyPtr inputSelection, int numIterations, ConstPropertyPtr bondTopology) :
            ExpandSelectionEngine(request, std::move(fingerprint), std::move(positions), simCell, std::move(inputSelection), numIterations),
            _bondTopology(std::move(bondTopology)) {}

        /// Expands the selection by one step.
        virtual void expandSelection() override;

    private:

        ConstPropertyPtr _bondTopology;
    };

private:

    /// The expansion mode.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ExpansionMode, mode, setMode, PROPERTY_FIELD_MEMORIZE);

    /// The selection cutoff range.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cutoffRange, setCutoffRange, PROPERTY_FIELD_MEMORIZE);

    /// The number of nearest neighbors to select.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, numNearestNeighbors, setNumNearestNeighbors, PROPERTY_FIELD_MEMORIZE);

    /// The number of expansion steps to perform.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int, numberOfIterations, setNumberOfIterations);
};

}   // End of namespace
