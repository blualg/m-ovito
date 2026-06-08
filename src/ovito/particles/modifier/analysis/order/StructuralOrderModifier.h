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
 * \brief Computes structural order metrics for particle configurations.
 */
class OVITO_PARTICLES_EXPORT StructuralOrderModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(StructuralOrderModifier, OOMetaClass)

public:

    enum OrderParameter
    {
        TranslationalEntropyOrder = 0,
        OrientationalEntropyOrder = 1,
        TetrahedralOrderParameter = 2,
        RadialTetrahedralOrderParameter = 3,
        LocalStructureIndexOrderParameter = 4,
        VoronoiLocalDensityOrderParameter = 5
    };
    Q_ENUM(OrderParameter);

    enum LocalOrderTargetMode
    {
        CurrentParticles = 0,
        SitesWithinReferenceCutoff = 1
    };
    Q_ENUM(LocalOrderTargetMode);

    static constexpr QStringView ProfileTableIdentifier = u"structural-order-profile";

    void initializeObject(ObjectInitializationFlags flags);

    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(OrderParameter{TranslationalEntropyOrder}, orderParameter, setOrderParameter, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{6.0}, cutoff, setCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, radialBins, setRadialBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{36}, angularBins, setAngularBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{100}, distributionBins, setDistributionBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{2.76}, tetrahedralReferenceDistance, setTetrahedralReferenceDistance, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.7}, localStructureIndexCutoff, setLocalStructureIndexCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(LocalOrderTargetMode{CurrentParticles}, localOrderTargetMode, setLocalOrderTargetMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, referenceTypes, setReferenceTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, referenceExpression, setReferenceExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QStringLiteral("O"), localSiteTypes, setLocalSiteTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, localSiteExpression, setLocalSiteExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.5}, localShellCutoff, setLocalShellCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelected, setOnlySelected);
};

}  // namespace Ovito
