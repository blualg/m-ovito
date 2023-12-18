////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
 * \brief This modifier wraps around the atomic positions in directions with periodic boundary
 *        conditions.
 */
class OVITO_PARTICLES_EXPORT WrapPeriodicImagesModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class WrapPeriodicImagesModifierClass : public ModifierClass
    {
    public:

        /// Inherit constructor from base class.
        using ModifierClass::ModifierClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(WrapPeriodicImagesModifier, WrapPeriodicImagesModifierClass)
    OVITO_CLASSINFO("DisplayName", "Wrap at periodic boundaries");
    OVITO_CLASSINFO("Description", "Fold particle coordinates back into the periodic simulation box.");
    OVITO_CLASSINFO("ModifierCategory", "Modification");

public:

    /// Constructor.
    using Modifier::Modifier;

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;
};

}   // End of namespace
