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
 * Identifies interfacial particles using ITIM or GITIM.
 */
class OVITO_PARTICLES_EXPORT InterfacialIdentificationModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(InterfacialIdentificationModifier, OOMetaClass)

public:

    enum InterfaceMethod {
        ITIM,
        GITIM,
    };
    Q_ENUM(InterfaceMethod);

    enum InterfaceNormalAxis {
        XAxis,
        YAxis,
        ZAxis,
    };
    Q_ENUM(InterfaceNormalAxis);

    static constexpr QStringView LayerCountsTableId = u"interfacial-layers";
    static constexpr QStringView InterfaceLayerPropertyName = u"Interface Layer";
    static constexpr QStringView InterfaceSidePropertyName = u"Interface Side";

    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(InterfaceMethod{ITIM}, method, setMethod, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{2.0}, probeSphereRadius, setProbeSphereRadius, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.4}, meshSpacing, setMeshSpacing, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, radiusScale, setRadiusScale, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, maxLayers, setMaxLayers, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(InterfaceNormalAxis{ZAxis}, normalAxis, setNormalAxis, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedParticles, setOnlySelectedParticles);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectInterfacialParticles, setSelectInterfacialParticles);
};

}   // End of namespace

