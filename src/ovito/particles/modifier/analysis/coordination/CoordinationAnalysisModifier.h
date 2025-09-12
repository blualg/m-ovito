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
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief This modifier computes the coordination number of each particle (i.e. number of neighbors within a given cutoff range)
 *        as well as the radial pair distribution function (RDF) of the system.
 */
class OVITO_PARTICLES_EXPORT CoordinationAnalysisModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class CoordinationAnalysisModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;

        /// Name of the output table
        constexpr static const char* tableName = "coordination-rdf";
    };

    OVITO_CLASS_META(CoordinationAnalysisModifier, CoordinationAnalysisModifierClass)

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

    /// Controls the cutoff radius for the neighbor lists.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.2}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Controls the number of RDF histogram bins.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Controls the computation of partials RDFs.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, computePartialRDF, setComputePartialRDF, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the modifier acts only on currently selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelected, setOnlySelected);

    /// The particle property that is used as the source for type classification when computing partial RDFs.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, typeProperty, setTypeProperty);
};

}   // End of namespace
