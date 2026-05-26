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
 * Replaces selected molecules by one particle located at the molecule center of
 * mass while leaving unselected molecules atomistic.
 */
class OVITO_PARTICLES_EXPORT MoleculeCenterOfMassModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(MoleculeCenterOfMassModifier, OOMetaClass)

public:

    enum MoleculeSelectionSource {
        AllMolecules,
        CurrentParticleSelection,
        AtomTypes,
        Expression,
    };
    Q_ENUM(MoleculeSelectionSource);

    static constexpr QStringView IsCOMPropertyName = u"Is COM Particle";
    static constexpr QStringView SourceAtomCountPropertyName = u"Source Atom Count";

    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterChange() override { return true; }

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(MoleculeSelectionSource{AllMolecules}, selectionSource, setSelectionSource, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, selectedTypes, setSelectedTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, selectionExpression, setSelectionExpression, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
