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
 * \brief Generates reusable vector, angle, or dihedral descriptor elements from atom selectors.
 */
class OVITO_PARTICLES_EXPORT GeometryDescriptorModifier : public Modifier
{
    class GeometryDescriptorModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(GeometryDescriptorModifier, GeometryDescriptorModifierClass)

public:

    enum DescriptorMode
    {
        VectorDescriptor,
        AngleDescriptor,
        DihedralDescriptor
    };
    Q_ENUM(DescriptorMode);

    enum AtomRoleSelectionMode
    {
        TypeExpressionSelection,
        TemplateMoleculeAtomIds
    };
    Q_ENUM(AtomRoleSelectionMode);

    static constexpr QStringView DescriptorContainerIdentifier = u"geometry-descriptors";
    static constexpr QStringView TableIdentifier = u"geometry-descriptor-values";

    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(DescriptorMode{VectorDescriptor}, descriptorMode, setDescriptorMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(AtomRoleSelectionMode{TypeExpressionSelection}, atomRoleSelectionMode, setAtomRoleSelectionMode, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom1Types, setAtom1Types, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom1Expression, setAtom1Expression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom2Types, setAtom2Types, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom2Expression, setAtom2Expression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom3Types, setAtom3Types, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom3Expression, setAtom3Expression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom4Types, setAtom4Types, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, atom4Expression, setAtom4Expression, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, templateMoleculeId, setTemplateMoleculeId, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, atom1ParticleId, setAtom1ParticleId, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{2}, atom2ParticleId, setAtom2ParticleId, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{3}, atom3ParticleId, setAtom3ParticleId, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{4}, atom4ParticleId, setAtom4ParticleId, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, normalizeVectors, setNormalizeVectors, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelectedParticles, setOnlySelectedParticles, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
