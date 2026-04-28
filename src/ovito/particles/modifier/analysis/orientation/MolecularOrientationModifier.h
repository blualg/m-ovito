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
 * \brief Evaluates the orientation of molecules around chosen reference atom types.
 */
class OVITO_PARTICLES_EXPORT MolecularOrientationModifier : public Modifier
{
    class MolecularOrientationModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(MolecularOrientationModifier, MolecularOrientationModifierClass)

public:

    enum DirectionMode
    {
        DipoleDirection,
        ManualMolecularDirection,
        MatchingPairVector
    };
    Q_ENUM(DirectionMode);

    static constexpr QStringView TableIdentifier = u"molecular-orientation-distribution";
    static constexpr QStringView DescriptorIdentifier = u"molecular-orientation-descriptors";

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

private:

    /// Chooses how the per-molecule orientation vector is constructed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(DirectionMode{DipoleDirection}, directionMode, setDirectionMode, PROPERTY_FIELD_MEMORIZE);

    /// Source particle type used for manual molecular directions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, fromTypeId, setFromTypeId, PROPERTY_FIELD_MEMORIZE);

    /// Target particle type used for manual molecular directions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, toTypeId, setToTypeId, PROPERTY_FIELD_MEMORIZE);

    /// Comma-separated list of reference atom types.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, referenceTypes, setReferenceTypes, PROPERTY_FIELD_MEMORIZE);

    /// Comma-separated list of anchor atom types.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, anchorTypes, setAnchorTypes, PROPERTY_FIELD_MEMORIZE);

    /// Cutoff radius used to identify candidate reference atoms around each molecule.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{5}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);

    /// Number of bins in the orientation-angle histogram.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{180}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Restricts the analysis to molecules touched by the upstream particle selection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelectedParticles, setOnlySelectedParticles, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
