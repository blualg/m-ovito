////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
//  Copyright 2019 Henrik Andersen Sveinsson
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
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

#include <boost/math/special_functions/spherical_harmonic.hpp>
#include <boost/numeric/ublas/matrix.hpp>

namespace Ovito {

/**
 * \brief This modifier implements the Chill+ algorithm [Nguyen & Molinero, J. Phys. Chem. B 2015, 119, 9369-9376]
 *        for identifying various water phases.
 */
class OVITO_PARTICLES_EXPORT ChillPlusModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(ChillPlusModifier)

    Q_CLASSINFO("DisplayName", "Chill+");
    Q_CLASSINFO("Description", "Identify hexagonal ice, cubic ice, hydrate and other arrangements of water molecules.");
#ifndef OVITO_QML_GUI
    Q_CLASSINFO("ModifierCategory", "Structure identification");
#else
    Q_CLASSINFO("ModifierCategory", "-");
#endif

public:

    /// The structure types recognized by the Chill+ algorithm.
    enum StructureType {
        OTHER = 0,              //< Unidentified structure
        HEXAGONAL_ICE,          //< Hexagonal ice
        CUBIC_ICE,              //< Cubic ice
        INTERFACIAL_ICE,        //< Interfacial ice
        HYDRATE,                //< Hydrate
        INTERFACIAL_HYDRATE,    //< Interfacial hydrate

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

    /// Constructor.
    Q_INVOKABLE ChillPlusModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

private:

    /// Computes the modifier's results.
    class ChillPlusEngine : public StructureIdentificationEngine
    {
    public:

        /// Constructor.
        ChillPlusEngine(const ModifierEvaluationRequest& request, ParticleOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, const OORefVector<ElementType>& structureTypes, ConstPropertyPtr selection, FloatType cutoff) :
            StructureIdentificationEngine(request, fingerprint, positions, simCell, structureTypes, selection),
            _cutoff(cutoff) {}

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        /// Returns the value of the cutoff parameter.
        FloatType cutoff() const { return _cutoff; }

    private:

        /// Implementation of the identification algorithm.
        StructureType determineStructure(CutoffNeighborFinder& neighFinder, size_t particleIndex);

        /// Helper method.
        static std::complex<float> compute_q_lm(CutoffNeighborFinder& neighFinder, size_t particleIndex, int, int);

        /// Helper method.
        static std::pair<float, float> polar_asimuthal(const Vector3& delta);

        const FloatType _cutoff;
        boost::numeric::ublas::matrix<std::complex<float>> q_values;
    };

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
