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


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/particles/modifier/analysis/StructureIdentificationModifier.h>
#include <ovito/crystalanalysis/modifier/dxa/StructureAnalysis.h>

namespace Ovito {

/*
 * Extracts dislocation lines from a crystal.
 */
class OVITO_CRYSTALANALYSIS_EXPORT ElasticStrainModifier : public StructureIdentificationModifier
{
    OVITO_CLASS(ElasticStrainModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

protected:

    /// Creates the engine that will perform the structure identification.
    virtual std::shared_ptr<Algorithm> createAlgorithm(const ModifierEvaluationRequest& request, const PipelineFlowState& input, PropertyPtr structures) override;

private:

    /// The type of crystal to be analyzed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(StructureAnalysis::LatticeStructureType{StructureAnalysis::LATTICE_FCC}, inputCrystalStructure, setInputCrystalStructure, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether atomic deformation gradient tensors should be computed and stored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, calculateDeformationGradients, setCalculateDeformationGradients, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether atomic strain tensors should be computed and stored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, calculateStrainTensors, setCalculateStrainTensors, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the calculated strain tensors should be pushed forward to the spatial reference frame.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, pushStrainTensorsForward, setPushStrainTensorsForward, PROPERTY_FIELD_MEMORIZE);

    /// The lattice parameter of ideal crystal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, latticeConstant, setLatticeConstant, PROPERTY_FIELD_MEMORIZE);

    /// The c/a ratio of the ideal crystal.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{std::sqrt(8.0/3.0)}, axialRatio, setAxialRatio, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
