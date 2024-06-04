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


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/stdobj/table/DataTable.h>

namespace Ovito {

/**
 * \brief This modifier computes a value histogram for a property.
 */
class OVITO_STDMOD_EXPORT HistogramModifier : public GenericPropertyModifier
{
    OVITO_CLASS(HistogramModifier)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Set start and end value of the x-axis.
    void setXAxisRange(FloatType start, FloatType end) {
        setXAxisRangeStart(start);
        setXAxisRangeEnd(end);
    }

    /// Set start and end value of the y-axis.
    void setYAxisRange(FloatType start, FloatType end) {
        setYAxisRangeStart(start);
        setYAxisRangeEnd(end);
    }

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override { return sourceProperty().nameWithComponent(); }

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The property that serves as data source of the histogram.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty, setSourceProperty);

    /// Controls the number of histogram bins.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{200}, numberOfBins, setNumberOfBins, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether elements within the specified range should be selected.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectInRange, setSelectInRange);

    /// Controls the start value of the selection interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, selectionRangeStart, setSelectionRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the selection interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, selectionRangeEnd, setSelectionRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the x-axis of the histogram should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixXAxisRange, setFixXAxisRange);

    /// Controls the start value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeStart, setXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, xAxisRangeEnd, setXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the range of the y-axis of the histogram should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixYAxisRange, setFixYAxisRange);

    /// Controls the start value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeStart, setYAxisRangeStart, PROPERTY_FIELD_MEMORIZE);

    /// Controls the end value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, yAxisRangeEnd, setYAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the modifier should take into account only selected elements.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedElements, setOnlySelectedElements);
};

}   // End of namespace
