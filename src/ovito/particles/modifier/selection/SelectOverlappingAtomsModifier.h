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
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Modifier that removes overlapping
 */
class OVITO_PARTICLES_EXPORT SelectOverlappingAtomsModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_PARTICLES_EXPORT SelectOverlappingAtomsModifierClass : public Modifier::OOMetaClass
    {
    public:
        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(SelectOverlappingAtomsModifier, SelectOverlappingAtomsModifierClass)

public:
    /// Modifies the input data.
    [[nodiscard]] virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request,
                                                                     PipelineFlowState&& state) override;

private:
    /// The maximum distance between two atoms to be considered overlapping.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(float{0.05}, overlapDistance, setOverlapDistance, PROPERTY_FIELD_MEMORIZE);

    /// Apply to selected particles only
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, applyToSelection, setApplyToSelection, PROPERTY_FIELD_MEMORIZE);

    /// Keep single particle
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, keepOne, setKeepOne, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito