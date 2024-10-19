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

#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ScatterPlotModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ScatterPlotModifier);
OVITO_CLASSINFO(ScatterPlotModifier, "DisplayName", "Scatter plot");
OVITO_CLASSINFO(ScatterPlotModifier, "Description", "Generate a scatter plot from the values of two properties.");
OVITO_CLASSINFO(ScatterPlotModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectXAxisInRange);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectionXAxisRangeStart);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectionXAxisRangeEnd);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectYAxisInRange);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectionYAxisRangeStart);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, selectionYAxisRangeEnd);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, fixXAxisRange);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, xAxisRangeStart);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, xAxisRangeEnd);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, fixYAxisRange);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, yAxisRangeStart);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, yAxisRangeEnd);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, xAxisProperty);
DEFINE_PROPERTY_FIELD(ScatterPlotModifier, yAxisProperty);
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectXAxisInRange, "Select elements in x-range");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectionXAxisRangeStart, "Selection x-range start");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectionXAxisRangeEnd, "Selection x-range end");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectYAxisInRange, "Select elements in y-range");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectionYAxisRangeStart, "Selection y-range start");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, selectionYAxisRangeEnd, "Selection y-range end");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, fixXAxisRange, "Fix x-range");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, xAxisRangeStart, "X-range start");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, xAxisRangeEnd, "X-range end");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, fixYAxisRange, "Fix y-range");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, yAxisRangeStart, "Y-range start");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, yAxisRangeEnd, "Y-range end");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, xAxisProperty, "X-axis property");
SET_PROPERTY_FIELD_LABEL(ScatterPlotModifier, yAxisProperty, "Y-axis property");

/******************************************************************************
* Constructor.
******************************************************************************/
void ScatterPlotModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particle properties by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ScatterPlotModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    // Use the first available property from the input state as data source when the modifier is newly created.
    if((!xAxisProperty() || !yAxisProperty()) && subject() && this_task::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                bestProperty = PropertyReference(property, (property->componentCount() > 1) ? 0 : -1);
            }
            if(!xAxisProperty() && bestProperty) {
                setXAxisProperty(bestProperty);
            }
            if(!yAxisProperty() && bestProperty) {
                setYAxisProperty(bestProperty);
            }
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ScatterPlotModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if((field == PROPERTY_FIELD(ScatterPlotModifier::xAxisProperty) || field == PROPERTY_FIELD(ScatterPlotModifier::yAxisProperty)) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of ScatterPlotModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
 * Sends an event to all dependents of this RefTarget.
 ******************************************************************************/
void ScatterPlotModifier::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged && event.sender() == this) {
        auto field = static_cast<const TargetChangedEvent&>(event).field();
        if(field == PROPERTY_FIELD(ScatterPlotModifier::fixXAxisRange) || field == PROPERTY_FIELD(ScatterPlotModifier::fixYAxisRange) ||
            field == PROPERTY_FIELD(ScatterPlotModifier::xAxisRangeStart) || field == PROPERTY_FIELD(ScatterPlotModifier::xAxisRangeEnd) ||
            field == PROPERTY_FIELD(ScatterPlotModifier::yAxisRangeStart) || field == PROPERTY_FIELD(ScatterPlotModifier::yAxisRangeEnd)) {
            // Changes to the above parameters do not invalidate the modifier's results.
            // Intercept the change event and modify it such that it does not trigger a re-evaluation of the modifier.
            GenericPropertyModifier::notifyDependentsImpl(TargetChangedEvent(this, field, TimeInterval::infinite()));
            // Trigger a plot widget update in the modifier editor panel:
            notifyDependents(ReferenceEvent::PipelineCacheUpdated);
            return;
        }
    }
    GenericPropertyModifier::notifyDependentsImpl(event);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> ScatterPlotModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No data element type set."));
    if(!xAxisProperty())
        throw Exception(tr("No input property for x-axis selected."));
    if(!yAxisProperty())
        throw Exception(tr("No input property for y-axis selected."));

    // Look up the property container object.
    ConstDataObjectPath containerPath = state.expectObject(subject());
    const PropertyContainer* container = state.expectLeafObject(subject());
    container->verifyIntegrity();

    // Get the input properties.
    QString errorDescription;
    auto [xProperty, xVecComponent] = xAxisProperty().findInContainerWithComponent(container, errorDescription);
    if(!xProperty)
        throw Exception(std::move(errorDescription));
    auto [yProperty, yVecComponent] = yAxisProperty().findInContainerWithComponent(container, errorDescription);
    if(!yProperty)
        throw Exception(std::move(errorDescription));

    // Get selection ranges.
    FloatType selectionXAxisRangeStart = this->selectionXAxisRangeStart();
    FloatType selectionXAxisRangeEnd = this->selectionXAxisRangeEnd();
    FloatType selectionYAxisRangeStart = this->selectionYAxisRangeStart();
    FloatType selectionYAxisRangeEnd = this->selectionYAxisRangeEnd();
    if(selectionXAxisRangeStart > selectionXAxisRangeEnd)
        std::swap(selectionXAxisRangeStart, selectionXAxisRangeEnd);
    if(selectionYAxisRangeStart > selectionYAxisRangeEnd)
        std::swap(selectionYAxisRangeStart, selectionYAxisRangeEnd);

    // Create output selection.
    PropertyPtr outputSelection;
    if((selectXAxisInRange() || selectYAxisInRange()) && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        // First make sure we can safely modify the property container.
        PropertyContainer* mutableContainer = state.expectMutableLeafObject(subject());
        // Add the selection property to the output container.
        outputSelection = mutableContainer->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty, containerPath);
    }

    // Create output data table.
    DataTable* table = state.createObject<DataTable>(QStringLiteral("scatter"), request.modificationNode(),
        DataTable::Scatter, tr("%1 vs. %2").arg(yAxisProperty().nameWithComponent()).arg(xAxisProperty().nameWithComponent()));
    OVITO_ASSERT(table == state.getObjectBy<DataTable>(request.modificationNode(), QStringLiteral("scatter")));

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            xProperty = ConstPropertyPtr(xProperty),
            yProperty = ConstPropertyPtr(yProperty),
            xVecComponent,
            yVecComponent,
            xPropertyName = xAxisProperty().nameWithComponent(),
            yPropertyName = yAxisProperty().nameWithComponent(),
            outputSelection = std::move(outputSelection),
            selectXAxisInRange = selectXAxisInRange(),
            selectYAxisInRange = selectYAxisInRange(),
            selectionXAxisRangeStart,
            selectionXAxisRangeEnd,
            selectionYAxisRangeStart,
            selectionYAxisRangeEnd,
            table,
            elementDescriptionName = container->getOOMetaClass().elementDescriptionName(),
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        table->setElementCount(xProperty->size());

        // Collect X coordinates.
        Property* out_x = table->createProperty(DataBuffer::Uninitialized, xPropertyName, Property::FloatDefault);
        xProperty->copyComponentTo(BufferWriteAccess<FloatType, access_mode::discard_write>{out_x}.begin(), xVecComponent);

        // Collect Y coordinates.
        Property* out_y;
        if(yPropertyName != xPropertyName) {
            out_y = table->createProperty(DataBuffer::Uninitialized, yPropertyName, Property::FloatDefault);
            yProperty->copyComponentTo(BufferWriteAccess<FloatType, access_mode::discard_write>{out_y}.begin(), yVecComponent);
        }
        else {
            out_y = out_x;
        }

        table->setX(out_x);
        table->setY(out_y);

        if(outputSelection) {
            outputSelection->fill<SelectionIntType>(1);
            BufferWriteAccess<SelectionIntType, access_mode::write> outputSelectionAcc{outputSelection};
            size_t numSelected = outputSelection->size();

            if(selectXAxisInRange) {
                SelectionIntType* s = outputSelectionAcc.begin();
                for(const FloatType x : BufferReadAccess<FloatType>{out_x}) {
                    if(x < selectionXAxisRangeStart || x > selectionXAxisRangeEnd) {
                        *s = 0;
                        numSelected--;
                    }
                    ++s;
                }
            }

            if(selectYAxisInRange) {
                SelectionIntType* s = outputSelectionAcc.begin();
                for(const FloatType y : BufferReadAccess<FloatType>{out_y}) {
                    if(y < selectionYAxisRangeStart || y > selectionYAxisRangeEnd) {
                        if(*s) {
                            *s = 0;
                            numSelected--;
                        }
                    }
                    ++s;
                }
            }

            QString statusMessage = tr("%1 %2 selected (%3%)").arg(numSelected)
                        .arg(elementDescriptionName)
                        .arg((FloatType)numSelected * 100 / std::max((size_t)1, outputSelection->size()), 0, 'f', 1);
            state.setStatus(std::move(statusMessage));
        }

        return std::move(state);
    });
}

}   // End of namespace
