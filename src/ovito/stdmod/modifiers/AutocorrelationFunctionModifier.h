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

#include <ovito/stdmod/StdMod.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/table/DataTable.h>

namespace Ovito {

class OVITO_STDMOD_EXPORT AutocorrelationFunctionModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(AutocorrelationFunctionModifier, OOMetaClass)

public:

    enum AnalysisMode {
        SignalAutocorrelation,
        VectorReorientation,
        SurvivalProbability
    };
    Q_ENUM(AnalysisMode);

    enum TargetType {
        Attribute,
        Table,
        Property,
        Cell
    };
    Q_ENUM(TargetType);

    enum SelectionMode {
        AllElements,
        SelectedAtTimeOrigin,
        SelectedAtBothTimes
    };
    Q_ENUM(SelectionMode);

    static QString correlationTableId() { return QStringLiteral("autocorrelation"); }

    void initializeObject(ObjectInitializationFlags flags);

    virtual void initializeModifier(const ModifierInitializationRequest& request) override;
    virtual void inputCachingHints(ModifierEvaluationRequest& request) override;
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request,
                                     PipelineEvaluationResult::EvaluationTypes& evaluationTypes,
                                     TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:

    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    std::vector<int> sampledFrames(const ModificationNode* modNode) const;
    Future<PipelineFlowState> computeCorrelationData(const ModifierEvaluationRequest& request, PipelineFlowState&& state);
    PipelineFlowState applyCachedResults(const ModifierEvaluationRequest& request, PipelineFlowState state) const;

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(AnalysisMode{SignalAutocorrelation}, analysisMode, setAnalysisMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(TargetType{Property}, targetType, setTargetType, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, attributeName, setAttributeName);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, table, setTable);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyContainerReference{}, propertyContainer, setPropertyContainer);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, property, setProperty);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, subtractMean, setSubtractMean, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, normalizeByZeroLag, setNormalizeByZeroLag, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{2}, legendreOrder, setLegendreOrder, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(SelectionMode{AllElements}, selectionMode, setSelectionMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, intermittency, setIntermittency, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomFrameInterval, setUseCustomFrameInterval);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalStart, setIntervalStart);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalEnd, setIntervalEnd);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, samplingFrequency, setSamplingFrequency, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, maxLag, setMaxLag, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, runRequestId, setRunRequestId, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

class OVITO_STDMOD_EXPORT AutocorrelationFunctionModificationNode : public ModificationNode
{
    OVITO_CLASS(AutocorrelationFunctionModificationNode)

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
