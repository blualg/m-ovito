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

    OVITO_CLASSINFO("DisplayName", "Polyhedral template matching");
    OVITO_CLASSINFO("Description", "Identify structures using the PTM method and local crystal orientations.");
    OVITO_CLASSINFO("ModifierCategory", "Structure identification");

public:

    /// Constructor.
    explicit PolyhedralTemplateMatchingModifier(ObjectInitializationFlags flags);

protected:

    /// Creates a computation engine that will compute the modifier's results.
    virtual Future<EnginePtr> createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input) override;

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Analysis engine that performs the PTM.
    class PTMEngine : public StructureIdentificationEngine
    {
    public:

        /// Constructor.
        PTMEngine(const ModifierEvaluationRequest& request, ConstPropertyPtr positions, ElementOrderingFingerprint fingerprint, ConstPropertyPtr particleTypes, const SimulationCell* simCell,
                const OORefVector<ElementType>& structureTypes, const OORefVector<ElementType>& orderingTypes, ConstPropertyPtr selection,
                bool outputInteratomicDistance, bool outputOrientation, bool outputDeformationGradient);

        /// Computes the modifier's results.
        virtual void perform() override;

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        /// This method is called by the system whenever a parameter of the modifier changes.
        /// The method can be overridden by subclasses to indicate to the caller whether the engine object should be
        /// discarded (false) or may be kept in the cache, because the computation results are not affected by the changing parameter (true).
        virtual bool modifierChanged(const PropertyFieldEvent& event) override {
            // Avoid a recomputation if the user changes just the RMSD cutoff parameter.
            if(event.field() == PROPERTY_FIELD(rmsdCutoff))
                return true;
            return StructureIdentificationEngine::modifierChanged(event);
        }

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

    protected:

        /// Post-processes the per-particle structure types before they are output to the data pipeline.
        virtual PropertyPtr postProcessStructureTypes(const ModifierEvaluationRequest& request, const PropertyPtr& structures) override;

    private:

        /// The internal PTM algorithm object.
        /// Store it in an optional<> so that it can be released early in the cleanup() method.
        std::optional<PTMAlgorithm> _algorithm;

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
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, rmsdCutoff, setRmsdCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of the per-particle RMSD values.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, outputRmsd, setOutputRmsd);

    /// Controls the output of local interatomic distances.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, outputInteratomicDistance, setOutputInteratomicDistance, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of local orientations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, outputOrientation, setOutputOrientation, PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of elastic deformation gradients.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, outputDeformationGradient, setOutputDeformationGradient);

    /// Controls the output of alloy ordering types.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, outputOrderingTypes, setOutputOrderingTypes, PROPERTY_FIELD_MEMORIZE);

    /// Contains the list of ordering types recognized by this analysis modifier.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ElementType>, orderingTypes, setOrderingTypes);
};

}   // End of namespace
