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
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include "ColorCodingModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesColorCodingModifierDelegate);

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
DEFINE_REFERENCE_FIELD(ColorCodingModifier, startValueController);
DEFINE_REFERENCE_FIELD(ColorCodingModifier, endValueController);
DEFINE_REFERENCE_FIELD(ColorCodingModifier, colorGradient);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, colorOnlySelected);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, keepSelection);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, autoAdjustRange);
DEFINE_PROPERTY_FIELD(ColorCodingModifier, sourceProperty);
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, startValueController, "Start value");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, endValueController, "End value");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, colorGradient, "Color gradient");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, colorOnlySelected, "Color only selected elements");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, keepSelection, "Keep selection");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, autoAdjustRange, "Automatically adjust range");
SET_PROPERTY_FIELD_LABEL(ColorCodingModifier, sourceProperty, "Source property");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ColorCodingModifier::ColorCodingModifier(ObjectInitializationFlags flags) : DelegatingModifier(flags),
    _colorOnlySelected(false),
    _keepSelection(true),
    _autoAdjustRange(false)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        setColorGradient(OORef<ColorCodingHSVGradient>::create());
        setStartValueController(ControllerManager::createFloatController());
        setEndValueController(ControllerManager::createFloatController());

        // Let this modifier act on particles by default.
        createDefaultModifierDelegate(ColorCodingModifierDelegate::OOClass(), QStringLiteral("ParticlesColorCodingModifierDelegate"));

        // When the modifier is created by a Python script, enable automatic range adjustment.
        if(ExecutionContext::isScripting()) {
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
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval ColorCodingModifier::validityInterval(const ModifierEvaluationRequest& request) const
{
    TimeInterval iv = DelegatingModifier::validityInterval(request);
    if(!autoAdjustRange()) {
        if(startValueController()) iv.intersect(startValueController()->validityInterval(request.time()));
        if(endValueController()) iv.intersect(endValueController()->validityInterval(request.time()));
    }
    return iv;
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ColorCodingModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(ColorCodingModifier::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of ColorCodingModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
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
    if(sourceProperty().isNull() && delegate() && ExecutionContext::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInputSynchronous(request);
        if(const PropertyContainer* container = input.getLeafObject(delegate()->inputContainerRef())) {
            PropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                bestProperty = PropertyReference(delegate()->inputContainerClass(), property, (property->componentCount() > 1) ? 0 : -1);
            }
            if(!bestProperty.isNull())
                setSourceProperty(bestProperty);
        }

        // Automatically adjust value range to input.
        adjustRange(request.time());
    }
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ColorCodingModifier::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    // Whenever the delegate of this modifier is being replaced, update the source property reference.
    if(field == PROPERTY_FIELD(DelegatingModifier::delegate) && !isBeingLoaded() && !isBeingDeleted() && !isUndoingOrRedoing()) {
        setSourceProperty(sourceProperty().convertToContainerClass(delegate() ? delegate()->inputContainerClass() : nullptr));
    }
    DelegatingModifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the
* modifier's results.
******************************************************************************/
Future<ModifierEnginePtr> ColorCodingModifier::createEngineInternal(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    if(!colorGradient())
        throw Exception(tr("No color gradient has been selected."));

    if(!delegate())
        throw Exception(tr("No delegate set for the color coding modifier."));

    // Get the source property.
    const PropertyReference& sourceProperty = this->sourceProperty();
    if(sourceProperty.isNull())
        throw Exception(tr("No source property was set as input for color coding."));

    // Look up the selected property container.
    ConstDataObjectPath containerPath = input.expectObject(delegate()->inputContainerRef());
    const PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Check if the source property is the right kind of property.
    if(sourceProperty.containerClass() != &container->getOOMetaClass()) {
        throw Exception(tr("Color coding modifier was set to operate on '%1', but the selected input is a '%2' property.")
                            .arg(delegate()->getOOMetaClass().pythonDataName())
                            .arg(sourceProperty.containerClass()->propertyClassDisplayName()));
    }

    // Make sure input data structure is ok.
    container->verifyIntegrity();

    ConstPropertyPtr property = sourceProperty.findInContainer(container);
    if(!property)
        throw Exception(tr("The property with the name '%1' does not exist.").arg(sourceProperty.name()));
    if(sourceProperty.vectorComponent() >= (int)property->componentCount())
        throw Exception(tr("The vector component is out of range. The property '%1' has only %2 values per data element.").arg(sourceProperty.name()).arg(property->componentCount()));
    int vecComponent = std::max(0, sourceProperty.vectorComponent());

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selection;
    if(colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        selection = container->getProperty(Property::GenericSelectionProperty);

    // Get modifier's parameter.
    FloatType startValue = 0, endValue = 0;
    TimeInterval validityInterval = input.stateValidity();
    if(!autoAdjustRange()) {
        if(startValueController()) startValue = startValueController()->getFloatValue(request.time(), validityInterval);
        if(endValueController()) endValue = endValueController()->getFloatValue(request.time(), validityInterval);
    }

    return std::make_shared<ColorCodingModifierDelegate::ColorCodingEngine>(
            request,
            validityInterval,
            colorGradient(),
            std::move(containerPath),
            std::move(property),
            vecComponent,
            std::move(selection),
            startValue, endValue,
            autoAdjustRange(),
            delegate()->outputColorPropertyId());
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void ColorCodingModifierDelegate::ColorCodingEngine::perform()
{
    // Create the color output property.
    const PropertyContainer* container = static_object_cast<PropertyContainer>(_containerPath.back());
    _colors = container->getOOMetaClass().createStandardProperty(_selection ? DataBuffer::Initialized : DataBuffer::Uninitialized, _input->size(), _outputColorPropertyId, _containerPath);

    // Determine input value range.
    FloatType startValue = _minValue;
    FloatType endValue = _maxValue;
    if(_autoAdjustRange) {
        _minValue = std::numeric_limits<FloatType>::max();
        _maxValue = std::numeric_limits<FloatType>::lowest();

        // Iterate over the property array to find the lowest/highest value.
        std::tie(_minValue, _maxValue) = _input->minMax(_vectorComponent, _selection);

        // If the range is valid. It may be not if the property is empty or no elements are selected.
        if(_minValue != std::numeric_limits<FloatType>::max()) {
            startValue = _minValue;
            endValue = _maxValue;
        }
        else {
            _minValue =  std::numeric_limits<FloatType>::infinity();
            _maxValue = -std::numeric_limits<FloatType>::infinity();
        }
    }

    // Clamp to finite range.
    if(!std::isfinite(startValue)) startValue = std::numeric_limits<FloatType>::lowest();
    if(!std::isfinite(endValue)) endValue = std::numeric_limits<FloatType>::max();
    const FloatType intervalRange = endValue - startValue;

#ifdef OVITO_USE_SYCL
    if(_colors->size() != 0) {
        // Create color lookup table.
        ConstDataBufferPtr colorMap = _gradient->getColorMap();

        ExecutionContext::current().ui().taskManager().syclQueue().submit([&](sycl::handler& cgh) {

            // Access selection flags array (optional).
            SyclBufferAccess<const SelectionIntType, access_mode::read> selectionAcc(_selection, cgh);
            // Access color lookup table.
            SyclBufferAccess<const ColorG, access_mode::read> colorMapAcc(colorMap, cgh);
            // Access output array.
            SyclBufferAccess<ColorG, access_mode::write> outputAcc(_colors, cgh, _selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);

            // Duplicate templated code for different input data types.
            _input->forAnyType([&](auto _) {
                using T = decltype(_);
                SyclBufferAccess<const T*, access_mode::read> inputAcc(_input, cgh);

                OVITO_SYCL_PARALLEL_FOR(cgh, ColorCodingModifierDelegate_apply)(sycl::range(_input->size()), [=](size_t i) {
                    if(selectionAcc.empty() || selectionAcc[i]) {
                        // Get input value.
                        const GraphicsFloatType value = static_cast<FloatType>(inputAcc[sycl::id<2>(i, _vectorComponent)]);

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
                        if(std::isnan(t)) t = 0;
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
    BufferWriteAccess<ColorG, access_mode::write> colorAcc(_colors, _selection ? DataBuffer::Initialized : DataBuffer::Uninitialized);
    BufferReadAccess<SelectionIntType> selectionAcc(_selection);

    bool result = _input->forEach(_vectorComponent, [&](size_t i, auto value) {
        if(selectionAcc && !selectionAcc[i])
            return;

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
        colorAcc[i] = _gradient->valueToColor(t);
    });
    if(!result)
        throw Exception(tr("The property '%1' has an invalid or non-numeric data type.").arg(_input->name()));
#endif

    // Release data that is no longer needed.
    _gradient.reset();
    _containerPath.clear();
    _input.reset();
    _selection.reset();
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ColorCodingModifierDelegate::ColorCodingEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    ColorCodingModifier* modifier = static_object_cast<ColorCodingModifier>(request.modifier());

    if(modifier->autoAdjustRange() && std::isfinite(_minValue)) {
        state.setAttribute(QStringLiteral("ColorCoding.RangeMin"), _minValue, request.modificationNode());
        state.setAttribute(QStringLiteral("ColorCoding.RangeMax"), _maxValue, request.modificationNode());
    }

    // Look up the property container.
    DataObjectPath objectPath = state.expectMutableObject(modifier->delegate()->inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());

    if(_orderingFingerprint.hasChanged(container))
        throw Exception(tr("Cached modifier results are obsolete, because the number or the storage order of input elements has changed."));

    // Output color property.
    container->createProperty(_colors);

    // Clear selection if requested.
    if(modifier->colorOnlySelected() && !modifier->keepSelection()) {
        if(const Property* selection = container->getProperty(Property::GenericSelectionProperty))
            container->removeProperty(selection);
    }
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
    const Property* property = sourceProperty().findInContainer(container);
    if(!property)
        return false;

    // Verify input property.
    if(sourceProperty().vectorComponent() >= (int)property->componentCount())
        return false;
    if(property->size() == 0)
        return false;
    int vecComponent = std::max(0, sourceProperty().vectorComponent());

    // Get the input selection property if coloring is restricted to the currently selected elements.
    ConstPropertyPtr selection = (colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty))
        ? container->getProperty(Property::GenericSelectionProperty) : nullptr;

    // Iterate over the property array to find the lowest/highest value.
    auto [minValue, maxValue] = property->minMax(vecComponent, selection);

    // If the range is valid. It may be not if the property is empty or no elements are selected.
    if(minValue == std::numeric_limits<FloatType>::max())
        return false;

    // Clamp to finite range.
    if(!std::isfinite(minValue)) minValue = std::numeric_limits<FloatType>::lowest();
    if(!std::isfinite(maxValue)) maxValue = std::numeric_limits<FloatType>::max();

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
    PipelineEvaluationRequest request(time);
    for(ModificationNode* node : nodes()) {
        const PipelineFlowState& inputState = node->evaluateInputSynchronous(request);

        // Determine the minimum and maximum values of the selected property.
        success |= determinePropertyValueRange(inputState, minValue, maxValue);
    }
    if(!success)
        return false;

    // Adjust range of color coding.
    if(startValueController())
        startValueController()->setFloatValue(time, minValue);
    if(endValueController())
        endValueController()->setFloatValue(time, maxValue);

    return true;
}

/******************************************************************************
* Sets the start and end value to the minimum and maximum value of the selected
* particle or bond property determined over the entire animation sequence.
******************************************************************************/
bool ColorCodingModifier::adjustRangeGlobal(int startFrame, int endFrame)
{
    this_task::setProgressMaximum(endFrame - startFrame + 1);

    FloatType minValue = std::numeric_limits<FloatType>::max();
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();

    // Loop over all animation frames, evaluate data pipeline, and determine
    // minimum and maximum values.
    for(int frame = startFrame; frame <= endFrame && !this_task::isCanceled(); frame++) {
        this_task::setProgressText(tr("Analyzing frame %1").arg(frame));

        for(ModificationNode* node : nodes()) {

            // Evaluate data pipeline up to this color coding modifier.
            SharedFuture<PipelineFlowState> stateFuture = node->evaluateInput(PipelineEvaluationRequest(AnimationTime::fromFrame(frame)));
            if(!stateFuture.waitForFinished())
                break;

            // Determine min/max value of the selected property.
            determinePropertyValueRange(stateFuture.result(), minValue, maxValue);
        }
        this_task::incrementProgressValue(1);
    }

    if(!this_task::isCanceled()) {
        // Adjust range of color coding to the min/max values.
        if(minValue != std::numeric_limits<FloatType>::max())
            setStartValue(minValue);
        if(maxValue != std::numeric_limits<FloatType>::lowest())
            setEndValue(maxValue);

        return true;
    }
    return false;
}

/******************************************************************************
* Swaps the minimum and maximum values to reverse the color scale.
******************************************************************************/
void ColorCodingModifier::reverseRange()
{
    // Swap controllers for start and end value.
    OORef<Controller> oldStartValue = startValueController();
    setStartValueController(endValueController());
    setEndValueController(std::move(oldStartValue));
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void ColorCodingModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    Future<ModifierEnginePtr> engineFuture = createEngineInternal(request, state);
    OVITO_ASSERT(engineFuture.isFinished());

    ModifierEnginePtr engine = engineFuture.result();
    engine->perform();
    engine->applyResults(request, state);
}

}   // End of namespace
