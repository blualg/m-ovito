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
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

class TimeSeriesModificationNode;

/**
 * \brief Samples a trajectory quantity across multiple frames and plots it as a data table.
 */
class OVITO_STDMOD_EXPORT TimeSeriesModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(TimeSeriesModifier, OOMetaClass)

public:

    enum TargetType {
        Attribute,
        Table,
        Property,
        Cell
    };
    Q_ENUM(TargetType);

    enum ReductionMode {
        Mean,
        Sum,
        Minimum,
        Maximum
    };
    Q_ENUM(ReductionMode);

    enum XAxisMode {
        Frame,
        AnimationTime
    };
    Q_ENUM(XAxisMode);

    void initializeObject(ObjectInitializationFlags flags);
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;
    virtual void inputCachingHints(ModifierEvaluationRequest& request) override;
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:
    std::vector<int> sampledFrames(const ModificationNode* modNode) const;
    Future<PipelineFlowState> computeSeriesData(const ModifierEvaluationRequest& request, PipelineFlowState&& state);
    PipelineFlowState applyCachedSeries(const ModifierEvaluationRequest& request, PipelineFlowState state) const;

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(TargetType{Property}, targetType, setTargetType, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, attributeName, setAttributeName);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, table, setTable);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyContainerReference{}, propertyContainer, setPropertyContainer);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, property, setProperty);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(ReductionMode{Mean}, reductionMode, setReductionMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(XAxisMode{Frame}, xAxisMode, setXAxisMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomFrameInterval, setUseCustomFrameInterval);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalStart, setIntervalStart);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalEnd, setIntervalEnd);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, samplingFrequency, setSamplingFrequency, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, runRequestId, setRunRequestId,
        PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

/**
 * Stores the cached time-series output table.
 */
class OVITO_STDMOD_EXPORT TimeSeriesModificationNode : public ModificationNode
{
    OVITO_CLASS(TimeSeriesModificationNode)

public:
    bool hasCachedSeries() const { return seriesTable() != nullptr; }
    void invalidateCachedSeries();

protected:
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const DataTable>, seriesTable, setSeriesTable,
        PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, completedRunRequestId, setCompletedRunRequestId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, cacheGenerationId, setCacheGenerationId,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

}   // End of namespace
