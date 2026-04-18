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

class TimeAveragingModificationNode;  // defined below

/**
 * \brief Computes time averages of attributes, data tables, element properties, or the simulation cell.
 */
class OVITO_STDMOD_EXPORT TimeAveragingModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(TimeAveragingModifier, OOMetaClass)

public:

    /// The kind of input quantity the modifier should average.
    enum TargetType {
        Attribute,
        Table,
        Property,
        Cell
    };
    Q_ENUM(TargetType);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Asks the modifier for the set of animation time intervals that should be cached by the upstream pipeline.
    virtual void inputCachingHints(ModifierEvaluationRequest& request) override;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
    /// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

    /// Is called by the ModificationNode to let the modifier adjust the time interval of a TargetChanged event
    /// received from the upstream pipeline before it is propagated to the downstream pipeline.
    virtual void restrictInputValidityInterval(TimeInterval& iv) const override;

    /// Returns a short piece of information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Returns the list of sampled source frames.
    std::vector<int> sampledFrames(const ModificationNode* modNode) const;

    /// Computes the cached average data by traversing the trajectory.
    Future<PipelineFlowState> computeAverageData(const ModifierEvaluationRequest& request, PipelineFlowState&& state);

    /// Creates the cached average of a scalar attribute.
    void computeAttributeAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const;

    /// Creates the cached average of a data table.
    void computeTableAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const;

    /// Creates the cached average of an element-wise property.
    void computePropertyAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const;

    /// Creates the cached average of the simulation cell.
    void computeCellAverage(TimeAveragingModificationNode* modNode, const ModifierEvaluationRequest& request, const std::vector<PipelineFlowState>& sampleStates) const;

    /// Applies the cached average data to the current pipeline state.
    PipelineFlowState applyCachedAverage(const ModifierEvaluationRequest& request, PipelineFlowState state) const;

    /// The kind of input quantity that should be averaged.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(TargetType{Property}, targetType, setTargetType, PROPERTY_FIELD_MEMORIZE);

    /// Name of the attribute to average when targetType == Attribute.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, attributeName, setAttributeName);

    /// Data table to average when targetType == Table.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, table, setTable);

    /// Property container holding the property to average when targetType == Property.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyContainerReference{}, propertyContainer, setPropertyContainer);

    /// Property to average when targetType == Property.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, property, setProperty);

    /// Enables a custom frame interval instead of averaging over the full trajectory.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, useCustomFrameInterval, setUseCustomFrameInterval);

    /// First source frame of the averaging interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalStart, setIntervalStart);

    /// Last source frame of the averaging interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, intervalEnd, setIntervalEnd);

    /// Source frame sampling stride.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, samplingFrequency, setSamplingFrequency, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the averaged output should overwrite the original quantity.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, overwrite, setOverwrite);
};

/**
 * Used by the TimeAveragingModifier to store cached averaging results.
 */
class OVITO_STDMOD_EXPORT TimeAveragingModificationNode : public ModificationNode
{
    OVITO_CLASS(TimeAveragingModificationNode)

public:

    /// Returns true if cached averaging data is available.
    bool hasCachedAverage() const {
        return averagedProperty() || averagedTable() || averagedCell() || averagedAttributeValue().isValid();
    }

    /// Clears all cached average data.
    void invalidateCachedAverage();

protected:

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// Cached averaged property data.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const Property>, averagedProperty, setAveragedProperty,
        PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// Optional identifier list describing the canonical order of the cached averaged property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const Property>, referenceIdentifiers, setReferenceIdentifiers,
        PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// Cached averaged data table.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const DataTable>, averagedTable, setAveragedTable,
        PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// Cached averaged simulation cell.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(DataOORef<const SimulationCell>, averagedCell, setAveragedCell,
        PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_DONT_SAVE_RECOMPUTABLE_DATA);

    /// Cached averaged scalar attribute value.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QVariant{}, averagedAttributeValue, setAveragedAttributeValue,
        PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_DONT_SERIALIZE);
};

}   // End of namespace
