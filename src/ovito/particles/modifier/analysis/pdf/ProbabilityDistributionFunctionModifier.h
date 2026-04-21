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
 * \brief Computes probability distribution functions for explicit bond, angle, and dihedral topology.
 */
class OVITO_PARTICLES_EXPORT ProbabilityDistributionFunctionModifier : public Modifier
{
    class ProbabilityDistributionFunctionModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(ProbabilityDistributionFunctionModifier, ProbabilityDistributionFunctionModifierClass)

public:

    enum DistributionMode
    {
        BondLength,
        BondAngle,
        DihedralAngle
    };
    Q_ENUM(DistributionMode);

    static constexpr QStringView TableIdentifier = u"probability-distribution-function";

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

protected:

    /// Is called when the value of a property field of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Chooses which topology quantity is sampled.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(DistributionMode{BondLength}, mode, setMode, PROPERTY_FIELD_MEMORIZE);

    /// Controls the number of histogram bins.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Lower bound of the histogram range.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, rangeStart, setRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Upper bound of the histogram range.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{5}, rangeEnd, setRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Restricts the analysis to interactions whose participating particles are all selected.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelected, setOnlySelected, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
