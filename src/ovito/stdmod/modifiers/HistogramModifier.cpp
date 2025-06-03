////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "HistogramModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(HistogramModifier);
OVITO_CLASSINFO(HistogramModifier, "DisplayName", "Histogram");
OVITO_CLASSINFO(HistogramModifier, "Description", "Compute the histogram or distribution of some quantity.");
OVITO_CLASSINFO(HistogramModifier, "ModifierCategory", "Analysis");
DEFINE_PROPERTY_FIELD(HistogramModifier, numberOfBins);
DEFINE_PROPERTY_FIELD(HistogramModifier, selectInRange);
DEFINE_PROPERTY_FIELD(HistogramModifier, selectionRangeStart);
DEFINE_PROPERTY_FIELD(HistogramModifier, selectionRangeEnd);
DEFINE_PROPERTY_FIELD(HistogramModifier, fixXAxisRange);
DEFINE_PROPERTY_FIELD(HistogramModifier, xAxisRangeStart);
DEFINE_PROPERTY_FIELD(HistogramModifier, xAxisRangeEnd);
DEFINE_PROPERTY_FIELD(HistogramModifier, fixYAxisRange);
DEFINE_PROPERTY_FIELD(HistogramModifier, yAxisRangeStart);
DEFINE_PROPERTY_FIELD(HistogramModifier, yAxisRangeEnd);
DEFINE_PROPERTY_FIELD(HistogramModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(HistogramModifier, onlySelectedElements);
DEFINE_PROPERTY_FIELD(HistogramModifier, normalizationMode);
SET_PROPERTY_FIELD_LABEL(HistogramModifier, numberOfBins, "Number of histogram bins");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, selectInRange, "Select value range");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, selectionRangeStart, "Selection range start");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, selectionRangeEnd, "Selection range end");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, fixXAxisRange, "Fix x-range");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, xAxisRangeStart, "X-range start");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, xAxisRangeEnd, "X-range end");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, fixYAxisRange, "Fix y-range");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, yAxisRangeStart, "Y-range start");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, yAxisRangeEnd, "Y-range end");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, sourceProperty, "Source property");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, onlySelectedElements, "Use only selected elements");
SET_PROPERTY_FIELD_LABEL(HistogramModifier, normalizationMode, "Normalization mode");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(HistogramModifier, numberOfBins, IntegerParameterUnit, 1, 100000);

/******************************************************************************
* Constructor.
******************************************************************************/
void HistogramModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particle properties by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void HistogramModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    // Use the first available property from the input state as data source when the modifier is newly created.
    if(!sourceProperty() && subject() && this_task::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                bestProperty = PropertyReference(property, (property->componentCount() > 1) ? 0 : -1);
            }
            setSourceProperty(bestProperty);
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void HistogramModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(HistogramModifier::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of HistogramModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> HistogramModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    if(!subject())
        throw Exception(tr("No data element type set."));
    if(!sourceProperty())
        throw Exception(tr("No input property selected."));

    // Look up the property container object.
    ConstDataObjectPath containerPath = state.expectObject(subject());
    const PropertyContainer* container = state.expectLeafObject(subject());
    container->verifyIntegrity();

    // Get the input property.
    QString errorDescription;
    ConstPropertyPtr property;
    int vecComponent;
    std::tie(property, vecComponent) = sourceProperty().findInContainerWithComponent(container, errorDescription);
    if(!property)
        throw Exception(std::move(errorDescription));

    // Get the input selection if filtering was enabled by the user.
    ConstPropertyPtr inputSelection;
    if(onlySelectedElements() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        inputSelection = container->expectProperty(Property::GenericSelectionProperty);
    }

    // Create storage for output selection.
    PropertyPtr outputSelection;
    if(selectInRange() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        // First make sure we can safely modify the property container.
        PropertyContainer* mutableContainer = state.expectMutableLeafObject(subject());
        // Add the selection property to the output container.
        outputSelection = mutableContainer->createProperty(DataBuffer::Uninitialized, Property::GenericSelectionProperty, containerPath);
    }

    // Create selection property for output.
    FloatType selectionRangeStart = this->selectionRangeStart();
    FloatType selectionRangeEnd = this->selectionRangeEnd();
    if(selectionRangeStart > selectionRangeEnd)
        std::swap(selectionRangeStart, selectionRangeEnd);

    // Create output data table.
    DataTable* table = state.createObject<DataTable>(
        QStringLiteral("histogram[%1]").arg(sourceProperty().nameWithComponent()),
        request.modificationNode(), DataTable::Histogram, sourceProperty().nameWithComponent());
    table->setAxisLabelX(sourceProperty().nameWithComponent());

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            property = std::move(property),
            inputSelection = std::move(inputSelection),
            outputSelection = std::move(outputSelection),
            selectionRangeStart, selectionRangeEnd,
            vecComponent,
            elementDescriptionName = container->getOOMetaClass().elementDescriptionName(),
            intervalStart = xAxisRangeStart(),
            intervalEnd = xAxisRangeEnd(),
            fixXAxisRange = fixXAxisRange(),
            numberOfBins = std::max(1, numberOfBins()),
            normalizationMode = normalizationMode(),
            table,
            createdByNode = request.modificationNodeWeak()]() mutable
    {
        size_t numSelected = 0;

        // Allocate output array for histogram.
        table->setElementCount(numberOfBins);
        Property* histogram = table->createProperty((property->size() != 0) ? DataBuffer::Uninitialized : DataBuffer::Initialized, QStringLiteral("Count"), Property::Int64);
        size_t histogramSizeMin1 = histogram->size() - 1;
        FloatType binSize = 0;

        if(property->size() > 0) {
#ifdef OVITO_USE_SYCL
            // Initialize histogram bins to zero.
            histogram->fillZero();

            property->forAnyType([&](auto _) {
                using T = decltype(_);

                // Access input value array (placeholder accessor).
                SyclBufferAccess<T*, access_mode::read> inputAcc(property);

                // Value range calculation.
                std::pair<sycl::buffer<T>, sycl::buffer<T>> intervalBuf =
                    fixXAxisRange
                        ?
                        std::make_pair(
                            detail::allocateSyclBuffer<T>(sycl::range<1>{1}),
                            detail::allocateSyclBuffer<T>(sycl::range<1>{1}))
                        :
                        inputAcc.minMax(vecComponent, inputSelection);

                if(fixXAxisRange) {
                    intervalBuf.first.get_host_access(sycl::write_only, sycl::no_init)[0] = static_cast<T>(intervalStart);
                    intervalBuf.second.get_host_access(sycl::write_only, sycl::no_init)[0] = static_cast<T>(intervalEnd);
                }

                // Histogram calculation.
                this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                    cgh.require(inputAcc);
                    sycl::accessor intervalStartAcc{intervalBuf.first, cgh, sycl::read_only};
                    sycl::accessor intervalEndAcc{intervalBuf.second, cgh, sycl::read_only};
                    SyclBufferAccess<int64_t, access_mode::read_write> histogramAcc(histogram, cgh);
                    SyclBufferAccess<SelectionIntType, access_mode::read> selectionAcc(inputSelection, cgh);
                    OVITO_SYCL_PARALLEL_FOR(cgh, HistogramModifier_kernel)(sycl::range(inputAcc.size()), [=](size_t i) {
                        if(!selectionAcc || selectionAcc[i]) {
                            const T intervalStart = intervalStartAcc[0];
                            const T intervalEnd = intervalEndAcc[0];
                            const T v = inputAcc[sycl::id<2>(i, vecComponent)];
                            if(v >= intervalStart && v <= intervalEnd) {
                                size_t binIndex = static_cast<size_t>(histogramAcc.size() * (static_cast<FloatType>(v - intervalStart) / (intervalEnd - intervalStart)));
                                sycl::atomic_ref<int64_t, sycl::memory_order::relaxed, sycl::memory_scope::device>(
                                    histogramAcc[sycl::max((size_t)0, sycl::min(binIndex, histogramSizeMin1))])
                                    .fetch_add((int64_t)1);
                            }
                        }
                    });
                });

                // Read out the computed interval range.
                if(!fixXAxisRange) {
                    intervalStart = static_cast<FloatType>(intervalBuf.first.get_host_access(sycl::read_only)[0]);
                    intervalEnd = static_cast<FloatType>(intervalBuf.second.get_host_access(sycl::read_only)[0]);
                }
                binSize = (intervalEnd - intervalStart) / histogram->size();

                // Element selection.
                if(outputSelection) {
                    sycl::buffer<size_t> numSelectedBuf(&numSelected, 1);
                    std::array<FloatType, 2> selectionRange = { selectionRangeStart, selectionRangeEnd };
                    sycl::buffer<FloatType> selectionRangeBuf(selectionRange);
                    this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {
                        cgh.require(inputAcc);
                        sycl::accessor selectionRangeAcc{selectionRangeBuf, cgh, sycl::read_only};
                        SyclBufferAccess<SelectionIntType, access_mode::discard_write> selectionOutAcc(outputSelection, cgh);
                        SyclBufferAccess<SelectionIntType, access_mode::read> selectionAcc(inputSelection, cgh);
#ifdef OVITO_USE_SYCL_ACPP
                        auto reduction = sycl::reduction(sycl::accessor{numSelectedBuf, cgh, sycl::no_init}, size_t{0}, sycl::plus<size_t>());
#else
                        auto reduction = sycl::reduction(numSelectedBuf, cgh, size_t{0}, sycl::plus<size_t>(), sycl::property::reduction::initialize_to_identity{});
#endif
                        OVITO_SYCL_PARALLEL_FOR(cgh, HistogramModifier_kernel_selection)(sycl::range(inputAcc.size()), reduction, [=](size_t i, auto& red) {
                            if(!selectionAcc || selectionAcc[i]) {
                                const T v = inputAcc[sycl::id<2>(i, vecComponent)];
                                if(v >= selectionRangeAcc[0] && v <= selectionRangeAcc[1]) {
                                    selectionOutAcc[i] = 1;
                                    red += (size_t)1;
                                }
                                else {
                                    selectionOutAcc[i] = 0;
                                }
                            }
                            else selectionOutAcc[i] = 0;
                        });
                    });
                }
            });
#else
            BufferReadAccess<SelectionIntType> inputSelectionAcc = inputSelection;
            BufferWriteAccess<SelectionIntType, access_mode::discard_write> outputSelectionAcc = outputSelection;
            BufferWriteAccess<int64_t, access_mode::discard_read_write> histogramAcc(histogram);
            std::fill(histogramAcc.begin(), histogramAcc.end(), 0);

            // Determine value range.
            if(!fixXAxisRange) {
                std::tie(intervalStart, intervalEnd) = property->minMax(vecComponent, inputSelection);
            }
            binSize = (intervalEnd - intervalStart) / histogram->size();

            property->forAnyType([&](auto _) {
                using T = decltype(_);

                BufferReadAccess<T*> accessor(property);
                // Perform binning.
                if(intervalEnd > intervalStart) {
                    const SelectionIntType* sel = inputSelectionAcc ? inputSelectionAcc.cbegin() : nullptr;
                    for(auto v : accessor.componentRange(vecComponent)) {
                        if(sel && !*sel++) continue;
                        if(v < intervalStart || v > intervalEnd)
                            continue;
                        size_t binIndex = (static_cast<FloatType>(v) - intervalStart) / binSize;
                        histogramAcc[std::clamp(binIndex, size_t{0}, histogramSizeMin1)]++;
                    }
                }
                else {
                    if(!inputSelection)
                        histogramAcc[0] = property->size();
                    else
                        histogramAcc[0] = inputSelection->nonzeroCount();
                }
                if(outputSelectionAcc) {
                    SelectionIntType* s = outputSelectionAcc.begin();
                    const SelectionIntType* sel = inputSelectionAcc ? inputSelectionAcc.cbegin() : nullptr;
                    for(auto v : accessor.componentRange(vecComponent)) {
                        if((!sel || *sel++) && v >= selectionRangeStart && v <= selectionRangeEnd) {
                            *s++ = 1;
                            numSelected++;
                        }
                        else *s++ = 0;
                    }
                }
            });
#endif
        }
        else {
            intervalStart = intervalEnd = 0;
        }
        table->setIntervalStart(intervalStart);
        table->setIntervalEnd(intervalEnd);

        // Normalize histogram.
        if(normalizationMode == NormalizationMode::RelativeFrequency) {
            Property* relativeFrequency = table->createProperty(DataBuffer::Uninitialized, QStringLiteral("Relative Frequency"), Property::FloatDefault);
            BufferReadAccess<int64_t> countAcc(histogram);
            BufferWriteAccess<FloatType, access_mode::discard_write> relFreqAcc(relativeFrequency);
            int64_t totalCount = std::max((int64_t)1, std::accumulate(countAcc.cbegin(), countAcc.cend(), (int64_t)0));
            std::transform(countAcc.cbegin(), countAcc.cend(), relFreqAcc.begin(),
                [&](int64_t count) { return (FloatType)count / totalCount; });
            table->setY(relativeFrequency);
        }
        else if(normalizationMode == NormalizationMode::ProbabilityDensity) {
            Property* probabilityDensity = table->createProperty(DataBuffer::Uninitialized, QStringLiteral("Probability Density"), Property::FloatDefault);
            if(binSize != 0) {
                BufferReadAccess<int64_t> countAcc(histogram);
                BufferWriteAccess<FloatType, access_mode::discard_write> densityAcc(probabilityDensity);
                int64_t totalCount = std::max((int64_t)1, std::accumulate(countAcc.cbegin(), countAcc.cend(), (int64_t)0));
                FloatType normalization = totalCount * binSize;
                std::transform(countAcc.cbegin(), countAcc.cend(), densityAcc.begin(),
                    [&](int64_t count) { return (FloatType)count / normalization; });
            }
            else probabilityDensity->fillZero();
            table->setY(probabilityDensity);
        }
        else {
            table->setY(histogram);
        }

        QString statusMessage;
        if(outputSelection) {
            statusMessage = tr("%1 %2 selected (%3%)")
                    .arg(numSelected)
                    .arg(elementDescriptionName)
                    .arg((FloatType)numSelected * 100 / std::max((size_t)1,outputSelection->size()), 0, 'f', 1);
            state.addAttribute(QStringLiteral("Histogram.num_selected"), QVariant::fromValue(numSelected), createdByNode);
        }
        state.setStatus(PipelineStatus(std::move(statusMessage)));

        return std::move(state);
    });
}

}   // End of namespace
