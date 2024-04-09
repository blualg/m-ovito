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
#include <ovito/particles/objects/Bonds.h>
#include <ovito/mesh/surface/SurfaceMeshBuilder.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief A modifier that creates coordination polyhedra around atoms.
 */
class OVITO_PARTICLES_EXPORT CoordinationPolyhedraModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class CoordinationPolyhedraModifierClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(CoordinationPolyhedraModifier, CoordinationPolyhedraModifierClass)

public:

    /// Constructor.
    explicit CoordinationPolyhedraModifier(ObjectInitializationFlags flags);

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates whether the interactive viewports should be updated after a parameter of the the modifier has
    /// been changed and before the entire pipeline is recomputed.
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

private:

    /// The vis element for rendering the polyhedra.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SurfaceMeshVis>, surfaceMeshVis, setSurfaceMeshVis, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE | PROPERTY_FIELD_OPEN_SUBEDITOR);

    /// Controls whether property values should be copied over from the input particles to the generated mesh vertices and mesh regions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, transferParticleProperties, setTransferParticleProperties);
};

}   // End of namespace
