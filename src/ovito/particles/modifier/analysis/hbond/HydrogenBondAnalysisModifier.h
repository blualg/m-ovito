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
#include <ovito/core/dataset/pipeline/ModificationNode.h>

namespace Ovito {

class OVITO_PARTICLES_EXPORT HydrogenBondAnalysisModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(HydrogenBondAnalysisModifier, OOMetaClass)

public:

    enum DefinitionMode
    {
        FixedGeometry = 0,
        PMFDerived = 1
    };
    Q_ENUM(DefinitionMode);

    static QString countTableId() { return QStringLiteral("hydrogen-bond-counts"); }
    static QString observationTableId() { return QStringLiteral("hydrogen-bond-observations"); }
    static QString pmfTableId() { return QStringLiteral("hydrogen-bond-pmf"); }

    void initializeObject(ObjectInitializationFlags flags);

    virtual void inputCachingHints(ModifierEvaluationRequest& request) override;
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

private:

    std::vector<int> sampledFrames(const ModificationNode* modNode) const;
    Future<PipelineFlowState> computeHydrogenBondData(const ModifierEvaluationRequest& request, PipelineFlowState&& state);
    PipelineFlowState applyCachedResults(const ModifierEvaluationRequest& request, PipelineFlowState state) const;

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, donorTypes, setDonorTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, hydrogenTypes, setHydrogenTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, acceptorTypes, setAcceptorTypes, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.2}, donorHydrogenCutoff, setDonorHydrogenCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(DefinitionMode{FixedGeometry}, definitionMode, setDefinitionMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.0}, donorAcceptorCutoff, setDonorAcceptorCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{30}, angleCutoff, setAngleCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.0}, pmfDistanceMinimum, setPmfDistanceMinimum, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{5.0}, pmfDistanceMaximum, setPmfDistanceMaximum, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.0}, pmfThetaMinimum, setPmfThetaMinimum, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{180.0}, pmfThetaMaximum, setPmfThetaMaximum, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{80}, pmfDistanceBins, setPmfDistanceBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{72}, pmfAngleBins, setPmfAngleBins, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomFrameInterval, setUseCustomFrameInterval);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalStart, setIntervalStart);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalEnd, setIntervalEnd);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, samplingFrequency, setSamplingFrequency, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, runRequestId, setRunRequestId, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

class OVITO_PARTICLES_EXPORT HydrogenBondAnalysisModificationNode : public ModificationNode
{
    OVITO_CLASS(HydrogenBondAnalysisModificationNode)

public:

    bool hasCachedResults() const { return cachedResults() != nullptr; }
    void invalidateCachedResults();

protected:

    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const DataCollection>, cachedResults, setCachedResults,
        PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_SUB_ANIM);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, cachedWarningText, setCachedWarningText,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, completedRunRequestId, setCompletedRunRequestId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, cacheGenerationId, setCacheGenerationId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

}   // End of namespace
