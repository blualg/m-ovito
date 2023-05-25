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
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/util/ParticleOrderingFingerprint.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/pipeline/AsynchronousModifier.h>

namespace Ovito::Particles {

/**
 * \brief Calculates the centrosymmetry parameter (CSP) for particles.
 */
class OVITO_PARTICLES_EXPORT CentroSymmetryModifier : public AsynchronousModifier
{
    /// Give this modifier class its own metaclass.
    class CentroSymmetryModifierClass : public AsynchronousModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using AsynchronousModifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(CentroSymmetryModifier, CentroSymmetryModifierClass)

    Q_CLASSINFO("DisplayName", "Centrosymmetry parameter");
    Q_CLASSINFO("Description", "Calculate the lattice centrosymmetry parameter for each particle.");
    Q_CLASSINFO("ModifierCategory", "Structure identification");

public:

    enum CSPMode {
        ConventionalMode,   ///< Performs the conventional CSP.
        MatchingMode,       ///< Performs the minimum-weight matching CSP.
    };
    Q_ENUM(CSPMode);

    /// The maximum number of neighbors that can be taken into account to compute the CSP.
    enum { MAX_CSP_NEIGHBORS = 32 };

public:

    /// Constructor.
    Q_INVOKABLE CentroSymmetryModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Computes the centrosymmetry parameter of a single particle.
    static FloatType computeCSP(NearestNeighborFinder& neighList, size_t particleIndex, CSPMode mode);

private:

    /// Computes the modifier's results.
    class CentroSymmetryEngine : public Engine
    {
    public:

        /// Constructor.
        CentroSymmetryEngine(const ModifierEvaluationRequest& request, ParticleOrderingFingerprint fingerprint, ConstPropertyPtr positions, ConstPropertyPtr selection, const SimulationCellObject* simCell, int nneighbors, CSPMode mode, DataOORef<DataTable> histogram) :
            Engine(request),
            _nneighbors(nneighbors),
            _mode(mode),
            _positions(std::move(positions)),
            _selection(std::move(selection)),
            _simCell(simCell),
            _csp(ParticlesObject::OOClass().createStandardProperty(DataBuffer::Uninitialized, fingerprint.particleCount(), ParticlesObject::CentroSymmetryProperty)),
            _inputFingerprint(std::move(fingerprint)),
            _histogram(std::move(histogram)) {}

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        /// Returns the property storage that contains the computed per-particle CSP values.
        const PropertyPtr& csp() const { return _csp; }

        /// Returns the property storage that contains the input particle positions.
        const ConstPropertyPtr& positions() const { return _positions; }

        /// Returns the property storage that contains the particle selection (optional).
        const ConstPropertyPtr& selection() const { return _selection; }

        /// Returns the simulation cell data.
        const DataOORef<const SimulationCellObject>& cell() const { return _simCell; }

    private:

        const int _nneighbors;
        const CSPMode _mode;
        DataOORef<const SimulationCellObject> _simCell;
        ConstPropertyPtr _positions;
        ConstPropertyPtr _selection;
        const PropertyPtr _csp;
        ParticleOrderingFingerprint _inputFingerprint;

        /// The computed distribution of the CSP values.
        DataOORef<DataTable> _histogram;
    };

    /// Specifies the number of nearest neighbors to take into account when computing the CSP.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, numNeighbors, setNumNeighbors, PROPERTY_FIELD_MEMORIZE);

    /// Controls how the CSP is performed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(CSPMode, mode, setMode, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether analysis should take into account only selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, onlySelectedParticles, setOnlySelectedParticles);
};

}   // End of namespace
