////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//  Copyright 2020 Peter Mahler Larsen
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


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

class GrainSegmentationEngine;  // defined in GrainSegmentationEngine.h

/*
 * Decomposes a polycrystalline microstructure into individual grains.
 */
class OVITO_CRYSTALANALYSIS_EXPORT GrainSegmentationModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_CRYSTALANALYSIS_EXPORT GrainSegmentationModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(GrainSegmentationModifier, GrainSegmentationModifierClass)

public:

    enum MergeAlgorithm {
        GraphClusteringAutomatic,   ///< Use graph clustering algorithm to build merge sequence and choose threshold adaptively.
        GraphClusteringManual,      ///< Use graph clustering algorithm to build merge sequence and let user choose merge threshold.
        MinimumSpanningTree,        ///< Use minimum spanning tree algorithm to build merge sequence.
    };
    Q_ENUM(MergeAlgorithm);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if one of these parameters changes.
        if(event.field() == PROPERTY_FIELD(colorParticlesByGrain)
                || event.field() == PROPERTY_FIELD(mergingThreshold)
                || event.field() == PROPERTY_FIELD(minGrainAtomCount)
                || event.field() == PROPERTY_FIELD(orphanAdoption))
            return true;
        return Modifier::shouldKeepPartialResultsAfterChange(event);
    }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The merging algorithm to use.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(MergeAlgorithm{GraphClusteringAutomatic}, mergeAlgorithm, setMergeAlgorithm);

    /// Controls whether to handle coherent crystal interfaces.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, handleCoherentInterfaces, setHandleCoherentInterfaces);

    /// Controls the amount of noise allowed inside a grain.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0.0}, mergingThreshold, setMergingThreshold);

    /// The minimum number of crystalline atoms per grain.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{100}, minGrainAtomCount, setMinGrainAtomCount);

    /// Controls whether to adopt orphan atoms
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, orphanAdoption, setOrphanAdoption, PROPERTY_FIELD_MEMORIZE);

    /// The visual element for rendering the bonds created by the modifier.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<BondsVis>, bondsVis, setBondsVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE);

    /// Controls the output of bonds by the modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, outputBonds, setOutputBonds);

    /// Controls the coloring of particles by the modifier.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, colorParticlesByGrain, setColorParticlesByGrain, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
