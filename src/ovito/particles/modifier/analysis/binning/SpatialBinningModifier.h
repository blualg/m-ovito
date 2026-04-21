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
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

class OVITO_PARTICLES_EXPORT SpatialBinningModifier : public Modifier
{
    class SpatialBinningModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(SpatialBinningModifier, SpatialBinningModifierClass)

public:

    enum ReductionOperation
    {
        Mean,
        Sum,
        SumDividedByBinVolume,
        Min,
        Max
    };
    Q_ENUM(ReductionOperation);

    enum BinDirection
    {
        CellVector1 = 0,
        CellVector2 = 1,
        CellVector3 = 2,
        CellVectors12 = 0 + (1 << 2),
        CellVectors13 = 0 + (2 << 2),
        CellVectors23 = 1 + (2 << 2)
    };
    Q_ENUM(BinDirection);

    static constexpr QStringView OutputIdentifier = u"binning";

    virtual void initializeModifier(const ModifierInitializationRequest& request) override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    [[nodiscard]] bool is1D() const;
    static bool is1D(BinDirection direction);
    static int binDirectionX(BinDirection direction);
    static int binDirectionY(BinDirection direction);

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PropertyReference{}, sourceProperty, setSourceProperty, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ReductionOperation{Mean}, reductionOperation, setReductionOperation, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, firstDerivative, setFirstDerivative, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(BinDirection{CellVector3}, binDirection, setBinDirection, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBinsX, setNumberOfBinsX, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBinsY, setNumberOfBinsY, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
