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
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>

namespace Ovito {

/**
 * \brief A modifier that performs the structure identification method developed by Ackland and Jones.
 *
 * See G. Ackland, PRB(2006)73:054104.
 */
class OVITO_PARTICLES_EXPORT AcklandJonesModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(AcklandJonesModifier)
    Q_CLASSINFO("DisplayName", "Ackland-Jones analysis");
    Q_CLASSINFO("Description", "Identify common crystalline structures based on local bond angles.");
    Q_CLASSINFO("ModifierCategory", "Structure identification");

public:

    /// The structure types recognized by the bond angle analysis.
    enum StructureType {
        OTHER = 0,              //< Unidentified structure
        FCC,                    //< Face-centered cubic
        HCP,                    //< Hexagonal close-packed
        BCC,                    //< Body-centered cubic
        ICO,                    //< Icosahedral structure

        NUM_STRUCTURE_TYPES     //< This just counts the number of defined structure types.
    };
    Q_ENUM(StructureType);

public:

    /// Constructor.
    Q_INVOKABLE AcklandJonesModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

private:

    /// Computes the modifier's results.
    class AcklandJonesAnalysisEngine : public StructureIdentificationEngine
    {
    public:

        /// Constructor.
        using StructureIdentificationEngine::StructureIdentificationEngine;

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    private:

        /// Determines the coordination structure of a single particle using the bond-angle analysis method.
        StructureType determineStructure(NearestNeighborFinder& neighFinder, size_t particleIndex);
    };
};

}   // End of namespace
