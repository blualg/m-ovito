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
 * \brief Calculates an angular probability density from two user-defined vectors
 *        spanning a pair of molecules.
 */
class OVITO_PARTICLES_EXPORT PairMolecularAngleModifier : public Modifier
{
    class PairMolecularAngleModifierClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;

        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(PairMolecularAngleModifier, PairMolecularAngleModifierClass)

public:

    enum EndpointMolecule
    {
        Molecule1,
        Molecule2
    };
    Q_ENUM(EndpointMolecule);

    enum AngleSamplingMode
    {
        AllVectorCombinations,
        SmallestAnglePerPair
    };
    Q_ENUM(AngleSamplingMode);

    enum PairRoleMode
    {
        OrderedMoleculePairs,
        UniqueMoleculePairs
    };
    Q_ENUM(PairRoleMode);

    static constexpr QStringView TableIdentifier = u"pair-molecular-angle-distribution";

    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, molecule1SiteTypes, setMolecule1SiteTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, molecule1SiteExpression, setMolecule1SiteExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, molecule2SiteTypes, setMolecule2SiteTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, molecule2SiteExpression, setMolecule2SiteExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{4}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(EndpointMolecule{Molecule1}, direction1StartMolecule, setDirection1StartMolecule, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction1StartTypes, setDirection1StartTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction1StartExpression, setDirection1StartExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(EndpointMolecule{Molecule2}, direction1EndMolecule, setDirection1EndMolecule, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction1EndTypes, setDirection1EndTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction1EndExpression, setDirection1EndExpression, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(EndpointMolecule{Molecule1}, direction2StartMolecule, setDirection2StartMolecule, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction2StartTypes, setDirection2StartTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction2StartExpression, setDirection2StartExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(EndpointMolecule{Molecule1}, direction2EndMolecule, setDirection2EndMolecule, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction2EndTypes, setDirection2EndTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, direction2EndExpression, setDirection2EndExpression, PROPERTY_FIELD_MEMORIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(AngleSamplingMode{SmallestAnglePerPair}, angleSamplingMode, setAngleSamplingMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PairRoleMode{OrderedMoleculePairs}, pairRoleMode, setPairRoleMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{180}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, onlySelectedParticles, setOnlySelectedParticles, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
