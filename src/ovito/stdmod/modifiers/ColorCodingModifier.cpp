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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/app/PluginManager.h>
#include "ColorCodingModifier.h"
#include <chrono>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesColorCodingModifierDelegate);
OVITO_CLASSINFO(LinesColorCodingModifierDelegate, "DisplayName", "Lines");
OVITO_CLASSINFO(LinesColorCodingModifierDelegate, "ClassNameAlias", "TrajectoryColorCodingModifierDelegate");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
 * Indicates which data objects in the given input data collection the modifier
 * delegate is able to operate on.
 ******************************************************************************/
QVector<DataObjectReference> LinesColorCodingModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all (trajectory) lines objects in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(Lines::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

IMPLEMENT_ABSTRACT_OVITO_CLASS(ColorCodingModifierDelegate);

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorCodingModifier);
OVITO_CLASSINFO(ColorCodingModifier, "DisplayName", "Color coding");
OVITO_CLASSINFO(ColorCodingModifier, "Description", "Colors elements based on property values.");
OVITO_CLASSINFO(ColorCodingModifier, "ModifierCategory", "Coloring");
DEFINE_PROPERTY_FIELD(ColorCodingModifier, startValue);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, endValue);
DEFINE_REFERENCE_FIELD(ColorCodingModifier, colorGradient);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, colorOnlySelected);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, keepSelection);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, autoAdjustRange);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, symmetricRange);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, sourceProperty);
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, startValue, "Start value");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, endValue, "End value");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, colorGradient, "Color gradient");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, colorOnlySelected, "Color only selected elements");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, keepSelection, "Keep selection");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, autoAdjustRange, "Automatic range");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, symmetricRange, "Symmetric range");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, sourceProperty, "Source property");

/******************************************************************************
* Constructor.
******************************************************************************/
void ColorCodingModifier::initializeObject(ObjectInitializationFlags flags)
{
    DelegatingModifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setColorGradient(OORef<ColorCodingHSVGradient>::create());

        // Let this modifier act on particles by default.
        createDefaultModifierDelegate(ColorCodingModifierDelegate::OOClass(), QStringLiteral("ParticlesColorCodingModifierDelegate"));

        // When the modifier is created by a Python script, enable automatic range adjustment.
        if(this_task::isScripting()) {
            setAutoAdjustRange(true);
        }
        else {
#ifndef OVITO_DISABLE_QSETTINGS
            // Load the default gradient type set by the user.
            QSettings settings;
            settings.beginGroup(ColorCodingModifier::OOClass().plugin()->pluginId());
            settings.beginGroup(ColorCodingModifier::OOClass().name());
            QString typeString = settings.value(PROPERTY_FIELD(colorGradient)->identifier()).toString();
            if(!typeString.isEmpty()) {
                try {
                    OvitoClassPtr gradientType = OvitoClass::decodeFromString(typeString);
                    if(!colorGradient() || colorGradient()->getOOClass() != *gradientType) {
                        OORef<ColorCodingGradient> gradient = dynamic_object_cast<ColorCodingGradient>(gradientType->createInstance(flags));
                        if(gradient) setColorGradient(gradient);
                    }
                }
                catch(...) {}
            }
#endif

            // In the GUI environment, we let the modifier clear the selection by default
            // in order to make the newly assigned colors visible.
            setKeepSelection(false);
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ColorCodingModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of ColorCodingModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }
    else if(field == PROPERTY_FIELD(endValue) && symmetricRange() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        // If symmetric range is active, keep start and end value in sync.
        setStartValue(-endValue());
    }
    else if(field == PROPERTY_FIELD(symmetricRange) && symmetricRange() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        // If symmetric range is activated, symmetrize existing range.
        FloatType range = std::max(std::abs(startValue()), std::abs(endValue()));

        // Maintain the original order of start and end
        bool inverted = startValue() > endValue();
        // Update start and end to be symmetric
        setStartValue(inverted ? range : -range);
        setEndValue(inverted ? -range : range);
    }

    DelegatingModifier::propertyChanged(field);
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ColorCodingModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    DelegatingModifier::initializeModifier(request);

    // When the modifier is inserted, automatically select the most recently added property from the input.
    if(!sourceProperty() && delegate() && this_task::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).result();
        if(const PropertyContainer* container = input.getLeafObject(delegate()->inputContainerRef())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                bestProperty = PropertyReference(property, (property->componentCount() > 1) ? 0 : -1);
            }
            setSourceProperty(bestProperty);
        }

        // Automatically adjust value range to input.
        adjustRange(request.time());
    }
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ColorCodingModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const ColorCodingModifier* modifier = static_object_cast<ColorCodingModifier>(request.modifier());

    if(!modifier->colorGradient())
        throw Exception(tr("No color gradient has been selected."));

    // Get the source property.
    const PropertyReference& sourceProperty = modifier->sourceProperty();
    if(!sourceProperty)
        throw Exception(tr("No source property was set as input for color coding."));

    // Look up the selected property container. Make sure we can safely modify it.
    DataObjectPath containerPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Make sure input data structure is ok.
    container->verifyIntegrity();

    // Look up input property in container.
    ConstPropertyPtr property;
    int vectorComponent;
    QString errorDescription;
    std::tie(property, vectorComponent) = sourceProperty.findInContainerWithComponent(container, errorDescription);
    if(!property)
        throw Exception(std::move(errorDescription));

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selection;
    if(modifier->colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        selection = container->getProperty(Property::GenericSelectionProperty);

    // Get the modifier's parameters.
    FloatType startValue = 0, endValue = 0;
    TimeInterval validityInterval = state.stateValidity();
    if(!modifier->autoAdjustRange()) {
        startValue = modifier->startValue();
        endValue = modifier->endValue();
    }

    // Clear selection if requested.
    if(modifier->colorOnlySelected() && !modifier->keepSelection() && selection) {
        container->removeProperty(selection);
    }

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([request, state = std::move(state), containerPath = std::move(containerPath), selection = std::move(selection),
                        property = std::move(property), vectorComponent, outputColorPropertyId = outputColorPropertyId(), startValue,
                        endValue, autoAdjustRange = modifier->autoAdjustRange(), symmetricRange = modifier->symmetricRange(),
                        gradient = OORef<ColorCodingGradient>(modifier->colorGradient())]() mutable {
        // Create the color output property.
        PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());
        PropertyPtr colors = container->createProperty(selection ? DataBuffer::Initialized : DataBuffer::Uninitialized,
                                                       outputColorPropertyId, containerPath);

        // Determine input value range.
        if(autoAdjustRange) {
            startValue = std::numeric_limits<FloatType>::max();
            endValue = std::numeric_limits<FloatType>::lowest();

            // Iterate over the property array to find the lowest/highest value.
#if defined(_MSC_VER) && !defined(__clang__)
            // Workaround for msvc where std::tie does not update / replace startValue and endValue.
            // Assigning the return value (a temporary std::pair) to the tuple of references created by std::tie does not work
            // because we're trying to bind temporary values to non-temporary references (startValue and endValue).
            std::pair<FloatType, FloatType> minMax = property->minMax(vectorComponent, selection);
            startValue = minMax.first;
            endValue = minMax.second;
#else
            std::tie(startValue, endValue) = property->minMax(vectorComponent, selection);
#endif
            // If the range is valid. It may be not if the property is empty or no elements are selected.
            if(startValue != std::numeric_limits<FloatType>::max()) {
                if(symmetricRange) {
                    FloatType range = std::max(std::abs(startValue), std::abs(endValue));
                    startValue = -range;
                    endValue = range;
                }
                state.setAttribute(QStringLiteral("ColorCoding.RangeMin"), startValue, request.modificationNode());
                state.setAttribute(QStringLiteral("ColorCoding.RangeMax"), endValue, request.modificationNode());
            }
            else {
                startValue = std::numeric_limits<FloatType>::infinity();
                endValue = -std::numeric_limits<FloatType>::infinity();
            }
        }

        // Clamp to finite range.
        if(!std::isfinite(startValue))
            startValue = std::numeric_limits<FloatType>::lowest();
        if(!std::isfinite(endValue))
            endValue = std::numeric_limits<FloatType>::max();
        const FloatType intervalRange = endValue - startValue;

#ifdef OVITO_USE_SYCL
        if(colors->size() != 0) {
            // Create color lookup table.
            ConstDataBufferPtr colorMap = gradient->getColorMap();

            this_task::ui()->taskManager().syclQueue().submit([&](sycl::handler& cgh) {

                // Access selection flags array (optional).
                SyclBufferAccess<const SelectionIntType, access_mode::read> selectionAcc(selection, cgh);
                // Access color lookup table.
                SyclBufferAccess<const ColorG, access_mode::read> colorMapAcc(colorMap, cgh);
                // Access output array.
                SyclBufferAccess<ColorG, access_mode::write> outputAcc(colors, cgh, selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);

                // Duplicate templated code for different input data types.
                property->forAnyType([&](auto _) {
                    using T = decltype(_);
                    SyclBufferAccess<const T*, access_mode::read> inputAcc(property, cgh);

                    OVITO_SYCL_PARALLEL_FOR(cgh, ColorCodingModifierDelegate_apply)(sycl::range(property->size()), [=](size_t i) {
                        if(selectionAcc.empty() || selectionAcc[i]) {
                            // Get input value.
                            const GraphicsFloatType value = static_cast<FloatType>(inputAcc[sycl::id<2>(i, vectorComponent)]);

                            // Map input value to [0,1] range.
                            GraphicsFloatType t;
                            if(intervalRange != 0) {
                                t = (value - startValue) / intervalRange;
                            }
                            else {
                                if(value == startValue) t = GraphicsFloatType(0.5);
                                else if(value > startValue) t = 1;
                                else t = 0;
                            }

                            // Clamp value.
                            if(sycl::isnan(t))
                                t = 0;
                            else if(t ==  std::numeric_limits<GraphicsFloatType>::infinity()) t = 1;
                            else if(t == -std::numeric_limits<GraphicsFloatType>::infinity()) t = 0;
                            else if(t < 0) t = 0;
                            else if(t > 1) t = 1;

                            // Map scalar to RGB color. Perform linear interpolation of two adjacent colors.
                            GraphicsFloatType x = t * (GraphicsFloatType)(colorMapAcc.size() - 1);
                            int x0 = static_cast<int>(x);
                            int x1 = x0 + 1;
                            const ColorG& c0 = colorMapAcc[x0];
                            const ColorG& c1 = (x1 != colorMapAcc.size()) ? colorMapAcc[x1] : c0;
                            GraphicsFloatType w0 = (GraphicsFloatType)x1 - x;
                            GraphicsFloatType w1 = x - (GraphicsFloatType)x0;
                            outputAcc[i] = w0 * c0 + w1 * c1;
                        }
                    });
                });
            });
        }
#else
        BufferWriteAccess<ColorG, access_mode::write> colorAcc(colors, selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        BufferReadAccess<SelectionIntType> selectionAcc(selection);

        property->forAnyType([&](auto _) {
            using T = decltype(_);
            BufferReadAccess<T*> valueAcc(property);
            parallelFor<false>(colors->size(), 4096, [&](size_t i) {
                if(selectionAcc && !selectionAcc[i])
                    return;
                auto value = valueAcc.get(i, vectorComponent);

                // Map input value to [0,1] range.
                GraphicsFloatType t;
                if(intervalRange != 0) {
                    t = static_cast<GraphicsFloatType>((value - startValue) / intervalRange);
                }
                else {
                    if(value == startValue) t = GraphicsFloatType(0.5);
                    else if(value > startValue) t = 1;
                    else t = 0;
                }

                // Clamp value.
                if(std::isnan(t)) t = 0;
                else if(t ==  std::numeric_limits<GraphicsFloatType>::infinity()) t = 1;
                else if(t == -std::numeric_limits<GraphicsFloatType>::infinity()) t = 0;
                else if(t < 0) t = 0;
                else if(t > 1) t = 1;

                // Map scalar to RGB color.
                colorAcc[i] = gradient->valueToColor(t);
            });
        });
        colorAcc.reset();
        selectionAcc.reset();
#endif

        return std::move(state);
    });
}

/******************************************************************************
* Determines the range of values in the input data for the selected property.
******************************************************************************/
bool ColorCodingModifier::determinePropertyValueRange(const PipelineFlowState& state, FloatType& min, FloatType& max) const
{
    if(!delegate())
        return false;

    // Look up the selected property container.
    ConstDataObjectPath objectPath = state.getObject(delegate()->inputContainerRef());
    if(objectPath.empty())
        return false;
    const PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());

    // Look up the selected property.
    QString errorDescription;
    auto [property, vecComponent] = sourceProperty().findInContainerWithComponent(container, errorDescription);
    if(!property)
        return false;

    // Verify input property.
    if(property->size() == 0)
        return false;

    // Get the input selection property if coloring is restricted to the currently selected elements.
    ConstPropertyPtr selection = (colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        ? container->getProperty(Property::GenericSelectionProperty) : nullptr;

    // Iterate over the property array to find the lowest/highest value.
    auto [minValue, maxValue] = property->minMax(vecComponent, selection);

    // If the range is valid. It may be not if the property is empty or no elements are selected.
    if(minValue == std::numeric_limits<FloatType>::max())
        return false;

    // Determine global min/max values over all animation frames.
    if(minValue < min) min = minValue;
    if(maxValue > max) max = maxValue;

    return true;
}

/******************************************************************************
* Sets the start and end value to the minimum and maximum value
* in the selected input property. Returns true if successful.
******************************************************************************/
bool ColorCodingModifier::adjustRange(AnimationTime time)
{
    FloatType minValue = std::numeric_limits<FloatType>::max();
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();

    // Loop over all input data.
    bool success = false;
    for(ModificationNode* node : nodes()) {
        const PipelineFlowState& inputState = node->evaluateInput(PipelineEvaluationRequest(time, false, false)).result();

        // Determine the minimum and maximum values of the selected property.
        success |= determinePropertyValueRange(inputState, minValue, maxValue);
    }
    if(!success)
        return false;

    // Symmetrize range.
    if(symmetricRange()) {
        FloatType range = std::max(std::abs(maxValue), std::abs(minValue));
        minValue = -range;
        maxValue = range;
    }

    // Adjust range of color coding.
    setStartValue(minValue);
    setEndValue(maxValue);

    return true;
}

/******************************************************************************
* Sets the start and end value to the minimum and maximum value of the selected
* particle or bond property determined over the entire animation sequence.
******************************************************************************/
void ColorCodingModifier::adjustRangeGlobal(int startFrame, int endFrame)
{
    this_task::setProgressMaximum(endFrame - startFrame + 1);

    FloatType minValue = std::numeric_limits<FloatType>::max();
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();

    // Loop over all animation frames, evaluate data pipeline, and determine
    // minimum and maximum values.
    for(int frame = startFrame; frame <= endFrame; frame++) {
        this_task::setProgressText(tr("Analyzing frame %1").arg(frame));

        for(ModificationNode* node : nodes()) {

            // Evaluate data pipeline up to this color coding modifier.
            PipelineEvaluationResult stateFuture = node->evaluateInput(PipelineEvaluationRequest(AnimationTime::fromFrame(frame)));

            // Determine min/max value of the selected property.
            determinePropertyValueRange(stateFuture.result(), minValue, maxValue);
        }
        this_task::incrementProgressValue(1);
        this_task::throwIfCanceled();
    }

    // Symmetrize range.
    if(symmetricRange() && minValue != std::numeric_limits<FloatType>::max() && maxValue != std::numeric_limits<FloatType>::lowest()) {
        FloatType range = std::max(std::abs(maxValue), std::abs(minValue));
        minValue = -range;
        maxValue = range;
    }

    // Adjust range of color coding to the min/max values.
    if(minValue != std::numeric_limits<FloatType>::max())
        setStartValue(minValue);
    if(maxValue != std::numeric_limits<FloatType>::lowest())
        setEndValue(maxValue);
}

/******************************************************************************
* Swaps the minimum and maximum values to reverse the color scale.
******************************************************************************/
void ColorCodingModifier::reverseRange()
{
    // Swap start and end value.
    auto temp = startValue();
    setStartValue(endValue());
    setEndValue(temp);
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized animation controller field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.10.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr ColorCodingModifier::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // The ColorCodingModifier used to manage animation controllers for start and end value parameters in OVITO 3.10 and before.
    if(field.identifier == "startValueController" && field.definingClass == &ColorCodingModifier::OOClass()) {
        return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
            OVITO_ASSERT(field.isReferenceField);
            stream.expectChunk(0x02);
            OORef<Controller> controller = stream.loadObject<Controller>();
            stream.closeChunk();
            // Need to wait until the animation keys of the controller have been completely loaded.
            // Only then it is safe to query the controller for its value.
            stream.registerPostLoadCallback([modifier = static_cast<ColorCodingModifier*>(&owner), controller = std::move(controller)]() {
                modifier->setStartValue(controller->getFloatValue(AnimationTime(0)));
            });
        };
    }
    else if(field.identifier == "endValueController" && field.definingClass == &ColorCodingModifier::OOClass()) {
        return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
            OVITO_ASSERT(field.isReferenceField);
            stream.expectChunk(0x02);
            OORef<Controller> controller = stream.loadObject<Controller>();
            stream.closeChunk();
            // Need to wait until the animation keys of the controller have been completely loaded.
            // Only then it is safe to query the controller for its value.
            stream.registerPostLoadCallback([modifier = static_cast<ColorCodingModifier*>(&owner), controller = std::move(controller)]() {
                modifier->setEndValue(controller->getFloatValue(AnimationTime(0)));
            });
        };
    }
    return DelegatingModifier::OOMetaClass::overrideFieldDeserialization(stream, field);
}

}   // End of namespace
