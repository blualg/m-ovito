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
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/table/DataTable.h>
#include "PTMAlgorithm.h"

namespace Ovito {

/**
 * \brief A modifier that uses the Polyhedral Template Matching (PTM) method to identify
 *        local coordination structures.
 */
class OVITO_PARTICLES_EXPORT PolyhedralTemplateMatchingModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(PolyhedralTemplateMatchingModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if the user changes just the RMSD cutoff parameter.
        if(event.field() == PROPERTY_FIELD(rmsdCutoff) || event.field() == PROPERTY_FIELD(outputRmsd))
            return true;
        return StructureIdentificationModifier::shouldKeepPartialResultsAfterChange(event);
    }

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) override;

private:

    /// Analysis engine that performs the PTM.
    class PTMEngine : public StructureIdentificationModifier::Algorithm
    {
    public:

        /// Constructor.
        PTMEngine(PropertyPtr structures, size_t particleCount, ConstPropertyPtr particleTypes, const OORefVector<ElementType>& orderingTypes,
                bool outputInteratomicDistance, bool outputOrientation, bool outputDeformationGradient);

        /// Performs the atomic structure classification.
        virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) override;

        /// Obtains the modifier parameters that are relevant for the post-processing phase (phase II).
        /// The method is called by the StructureIdentificationModifier in the main thread before phase II begins to
        /// store the modifier's parameters in a std::any container that will be passed to the postProcessStructureTypes() and computeStructureStatistics() methods.
        virtual std::any getModifierParameters(StructureIdentificationModifier* modifier) const override {
            return std::make_pair(
                static_object_cast<PolyhedralTemplateMatchingModifier>(modifier)->rmsdCutoff(),
                static_object_cast<PolyhedralTemplateMatchingModifier>(modifier)->outputRmsd());
        }

        /// Post-processes the per-particle structure types before they are output to the data pipeline.
        virtual PropertyPtr postProcessStructureTypes(const PropertyPtr& structures, const std::any& modifierParameters) const override;

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const override;

        const PropertyPtr& rmsd() const { return _rmsd; }
        const PropertyPtr& interatomicDistances() const { return _interatomicDistances; }
        const PropertyPtr& orientations() const { return _orientations; }
        const PropertyPtr& deformationGradients() const { return _deformationGradients; }
        const PropertyPtr& orderingTypes() const { return _orderingTypes; }
        const PropertyPtr& correspondences() const { return _correspondences; }

        /// Returns the RMSD value range of the histogram.
        FloatType rmsdHistogramRange() const { return _rmsdHistogramRange; }

        /// Returns the histogram of computed RMSD values.
        const PropertyPtr& rmsdHistogram() const { return _rmsdHistogram; }

    private:

        /// The internal PTM algorithm object.
        /// Store it in an optional<> so that it can be released early.
        std::optional<PTMAlgorithm> _algorithm{std::in_place};

        // Modifier outputs:
        const PropertyPtr _rmsd;
        const PropertyPtr _interatomicDistances;
        const PropertyPtr _orientations;
        const PropertyPtr _deformationGradients;
        const PropertyPtr _orderingTypes;
        const PropertyPtr _correspondences;
        PropertyPtr _rmsdHistogram;
        FloatType _rmsdHistogramRange;
    };

private:

    /// The RMSD cutoff.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.1}, rmsdCutoff, setRmsdCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of the per-particle RMSD values.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, outputRmsd, setOutputRmsd);

    /// Controls the output of local interatomic distances.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outputInteratomicDistance, setOutputInteratomicDistance, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of local orientations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outputOrientation, setOutputOrientation, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of elastic deformation gradients.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, outputDeformationGradient, setOutputDeformationGradient);

    /// Controls the output of alloy ordering types.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, outputOrderingTypes, setOutputOrderingTypes, PROPERTY_FIELD_MEMORIZE);

    /// Contains the list of ordering types recognized by this analysis modifier.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ElementType>, orderingTypes, setOrderingTypes);
};

}   // End of namespace
