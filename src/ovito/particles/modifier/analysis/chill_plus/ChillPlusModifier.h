////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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

    OVITO_CLASSINFO("DisplayName", "Chill+");
    OVITO_CLASSINFO("Description", "Identify hexagonal ice, cubic ice, hydrate and other arrangements of water molecules.");
    OVITO_CLASSINFO("ModifierCategory", "Structure identification");

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
    explicit ChillPlusModifier(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) override {
        return std::make_shared<ChillPlusAlgorithm>(std::move(structures), cutoff());
    }

    /// Computes the modifier's results.
    class ChillPlusAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        ChillPlusAlgorithm(PropertyPtr structures, FloatType cutoff) :
            Algorithm(std::move(structures)),
            _cutoff(cutoff) {}

        /// Performs the atomic structure classification.
        virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

        /// Returns the value of the cutoff parameter.
        FloatType cutoff() const { return _cutoff; }

    private:

        /// Implementation of the identification algorithm.
        static StructureType determineStructure(const CutoffNeighborFinder& neighFinder, size_t particleIndex, const boost::numeric::ublas::matrix<std::complex<float>>& q_values);

        const FloatType _cutoff;
    };

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
