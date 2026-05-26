////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * \brief Calculates predefined derived particle properties and simple reduced quantities.
 */
class OVITO_PARTICLES_EXPORT CalculatePropertyModifier : public Modifier
{
    class CalculatePropertyModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(CalculatePropertyModifier, CalculatePropertyModifierClass)

public:

    enum PropertyType
    {
        DipoleDirection,
        ManualMolecularDirection,
        KineticEnergy,
        ParticleExpression,
        VectorExpression,
        PairExpression,
        PairDistances
    };
    Q_ENUM(PropertyType);

    enum GroupingMode
    {
        NoGrouping,
        GroupByMolecule
    };
    Q_ENUM(GroupingMode);

    enum ReductionOperation
    {
        NoReduction,
        SumReduction,
        MeanReduction,
        MinReduction,
        MaxReduction
    };
    Q_ENUM(ReductionOperation);

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

private:

    /// Predefined property recipe to evaluate.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PropertyType{ParticleExpression}, propertyType, setPropertyType, PROPERTY_FIELD_MEMORIZE);

    /// Optional grouping applied before writing the output property.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(GroupingMode{NoGrouping}, groupingMode, setGroupingMode, PROPERTY_FIELD_MEMORIZE);

    /// Reduction applied to scalar per-particle calculations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ReductionOperation{NoReduction}, reductionOperation, setReductionOperation, PROPERTY_FIELD_MEMORIZE);

    /// Source selector type list used for manual direction and pair-distance calculations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, fromTypes, setFromTypes, PROPERTY_FIELD_MEMORIZE);

    /// Source selector expression override.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, fromExpression, setFromExpression, PROPERTY_FIELD_MEMORIZE);

    /// Target selector type list used for manual direction and pair-distance calculations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, toTypes, setToTypes, PROPERTY_FIELD_MEMORIZE);

    /// Target selector expression override.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, toExpression, setToExpression, PROPERTY_FIELD_MEMORIZE);

    /// Expression used by the generic particle-expression mode.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, expression, setExpression, PROPERTY_FIELD_MEMORIZE);

    /// Optional multiline script consisting of line-by-line assignments.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, script, setScript, PROPERTY_FIELD_MEMORIZE);

    /// X-component expression used by the vector-expression mode.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, expressionX, setExpressionX, PROPERTY_FIELD_MEMORIZE);

    /// Y-component expression used by the vector-expression mode.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, expressionY, setExpressionY, PROPERTY_FIELD_MEMORIZE);

    /// Z-component expression used by the vector-expression mode.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, expressionZ, setExpressionZ, PROPERTY_FIELD_MEMORIZE);

    /// Name of the custom output property written by scalar calculations.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, outputPropertyName, setOutputPropertyName, PROPERTY_FIELD_MEMORIZE);

    /// Restricts the calculation to selected particles or molecules touched by the upstream particle selection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelectedParticles, setOnlySelectedParticles, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
