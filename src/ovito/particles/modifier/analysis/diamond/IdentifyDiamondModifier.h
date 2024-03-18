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
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

namespace Ovito {

/**
 * \brief A modifier that identifies local diamond structures.
 */
class OVITO_PARTICLES_EXPORT IdentifyDiamondModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(IdentifyDiamondModifier)

    OVITO_CLASSINFO("DisplayName", "Identify diamond structure");
    OVITO_CLASSINFO("Description", "Identify particles arranged in cubic and hexagonal diamond structures.");
    OVITO_CLASSINFO("ModifierCategory", "Structure identification");

public:

    /// The structure types recognized by the modifier.
    enum StructureType {
        OTHER = 0,                  //< Unidentified structure
        CUBIC_DIAMOND,              //< Cubic diamond structure
        CUBIC_DIAMOND_FIRST_NEIGH,  //< First neighbor of a cubic diamond atom
        CUBIC_DIAMOND_SECOND_NEIGH, //< Second neighbor of a cubic diamond atom
        HEX_DIAMOND,                //< Hexagonal diamond structure
        HEX_DIAMOND_FIRST_NEIGH,    //< First neighbor of a hexagonal diamond atom
        HEX_DIAMOND_SECOND_NEIGH,   //< Second neighbor of a hexagonal diamond atom

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

public:

    /// Constructor.
    explicit IdentifyDiamondModifier(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) override {
        return std::make_shared<DiamondIdentificationAlgorithm>(std::move(structures));
    }

private:

    /// Analysis engine that performs the structure identification
    class DiamondIdentificationAlgorithm : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        using Algorithm::Algorithm;

        /// Performs the atomic structure classification.
        virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;
    };
};

}   // End of namespace
