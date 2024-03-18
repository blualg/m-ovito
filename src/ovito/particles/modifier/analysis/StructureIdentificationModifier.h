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
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/util/ElementOrderingFingerprint.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

#include <any>

namespace Ovito {

/**
 * \brief Base class for modifiers that assign a structure type to each particle.
 */
class OVITO_PARTICLES_EXPORT StructureIdentificationModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_PARTICLES_EXPORT StructureIdentificationModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(StructureIdentificationModifier, StructureIdentificationModifierClass)

public:

    /// Abstract base class for structure identification algorithms.
    class OVITO_PARTICLES_EXPORT Algorithm
    {
    public:

        /// Constructor.
        Algorithm(PropertyPtr structures) : _structures(std::move(structures)) {}

        /// Performs the atomic structure classification.
        virtual void identifyStructures(const Particles* particles, const SimulationCell* simulationCell, const Property* selection) = 0;

        /// Obtains the modifier parameters that are relevant for the post-processing phase (phase II).
        /// The method is called by the StructureIdentificationModifier in the main thread before phase II begins to
        /// store the modifier's parameters in a std::any container that will be passed to the postProcessStructureTypes() and computeStructureStatistics() methods.
        virtual std::any getModifierParameters(StructureIdentificationModifier* modifier) const { return {}; }

        /// Gives subclasses the possibility to post-process per-particle structure types.
        virtual PropertyPtr postProcessStructureTypes(const PropertyPtr& structures, const std::any& modifierParameters) const { return structures; }

        /// Computes the structure identification statistics.
        virtual std::vector<int64_t> computeStructureStatistics(const Property* structures, PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode, const std::any& modifierParameters) const;

        /// Returns the property storage for the computed per-particle structure types.
        const PropertyPtr& structures() const { return _structures; }

        /// Returns whether a given structural type is enabled for identification.
        bool typeIdentificationEnabled(int typeId) const {
            OVITO_ASSERT(typeId >= 0);
            if(typeId >= structures()->elementTypes().size())
                return false;
            OVITO_ASSERT(structures()->elementTypes()[typeId]->numericId() == typeId);
            return structures()->elementTypes()[typeId]->enabled();
        }

        /// Returns an array of boolean flags indicating for which structure types identification is enabled.
        template<size_t N>
        std::array<bool, N> typesToIdentify() const {
            std::array<bool, N> arr;
            arr.fill(false);
            for(const ElementType* t : structures()->elementTypes())
                if(t->enabled() && t->numericId() >= 0 && t->numericId() < N)
                    arr[t->numericId()] = true;
            return arr;
        }

    private:

        PropertyPtr _structures;
    };

public:

    /// Constructor.
    explicit StructureIdentificationModifier(ObjectInitializationFlags flags);

    /// This function is called by the pipeline system before a new modifier evaluation begins.
    virtual bool preEvaluationRun(const ModifierEvaluationRequest& request, PipelineEvaluationResult& result) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if the user toggles just the color-by-type option.
        if(event.field() == PROPERTY_FIELD(colorByType))
            return true;
        return Modifier::shouldKeepPartialResultsAfterChange(event);
    }

    /// Returns an existing structure type managed by the modifier.
    ElementType* structureTypeById(int id) const {
        for(ElementType* type : structureTypes())
            if(type->numericId() == id) return type;
        return nullptr;
    }

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) = 0;

    /// Adopts existing computation results for an interactive pipeline evaluation.
    virtual void reuseCachedState(const ModifierEvaluationRequest& request, Particles* particles, PipelineFlowState& output, const PipelineFlowState& cachedState);

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Inserts a structure type into the list.
    void addStructureType(ElementType* type) {
        // Make sure the numeric type ID is unique.
        OVITO_ASSERT(std::none_of(structureTypes().begin(), structureTypes().end(), [&](const ElementType* t) { return t->numericId() == type->numericId(); }));
        _structureTypes.push_back(this, PROPERTY_FIELD(structureTypes), type);
    }

    /// Create an instance of the ParticleType class to represent a structure type.
    ElementType* createStructureType(int id, ParticleType::PredefinedStructureType predefType);

private:

    /// Contains the list of structure types recognized by this analysis modifier.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ElementType>, structureTypes, setStructureTypes);

    /// Controls whether analysis should take into account only selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, onlySelectedParticles, setOnlySelectedParticles);

    /// Controls whether the modifier colors particles based on their type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, colorByType, setColorByType);
};

}   // End of namespace
