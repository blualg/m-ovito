////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/utilities/concurrent/TaskManager.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include "ColorCodingModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ColorCodingModifierDelegate);

IMPLEMENT_OVITO_CLASS(ColorCodingModifier);
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
        const PipelineFlowState& input = request.modApp()->evaluateInputSynchronous(request);
        if(const PropertyContainer* container = input.getLeafObject(delegate()->inputContainerRef())) {
            PropertyReference bestProperty;
            for(const PropertyObject* property : container->properties()) {
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
    if(field == PROPERTY_FIELD(DelegatingModifier::delegate) && !isBeingLoaded() && !isAboutToBeDeleted() && !isUndoingOrRedoing()) {
        setSourceProperty(sourceProperty().convertToContainerClass(delegate() ? delegate()->inputContainerClass() : nullptr));
    }
    DelegatingModifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
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
    const PropertyObject* property = sourceProperty().findInContainer(container);
    if(!property)
        return false;

    // Verify input property.
    if(sourceProperty().vectorComponent() >= (int)property->componentCount())
        return false;
    if(property->size() == 0)
        return false;
    int vecComponent = std::max(0, sourceProperty().vectorComponent());

    // Iterate over the property array to find the lowest/highest value.
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();
    FloatType minValue = std::numeric_limits<FloatType>::max();
    property->forEach(vecComponent, [&](size_t i, auto v) {
            if(v > maxValue) maxValue = v;
            if(v < minValue) minValue = v;
        });
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
    for(ModifierApplication* modApp : modifierApplications()) {
        const PipelineFlowState& inputState = modApp->evaluateInputSynchronous(request);

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
bool ColorCodingModifier::adjustRangeGlobal(MainThreadOperation& operation, int startFrame, int endFrame)
{
    operation.setProgressMaximum(endFrame - startFrame + 1);

    FloatType minValue = std::numeric_limits<FloatType>::max();
    FloatType maxValue = std::numeric_limits<FloatType>::lowest();

    // Loop over all animation frames, evaluate data pipeline, and determine
    // minimum and maximum values.
    for(int frame = startFrame; frame <= endFrame && !operation.isCanceled(); frame++) {
        operation.setProgressText(tr("Analyzing frame %1").arg(frame));

        for(ModifierApplication* modApp : modifierApplications()) {

            // Evaluate data pipeline up to this color coding modifier.
            SharedFuture<PipelineFlowState> stateFuture = modApp->evaluateInput(PipelineEvaluationRequest(AnimationTime::fromFrame(frame)));
            if(!stateFuture.waitForFinished())
                break;

            // Determine min/max value of the selected property.
            determinePropertyValueRange(stateFuture.result(), minValue, maxValue);
        }
        operation.incrementProgressValue(1);
    }

    if(!operation.isCanceled()) {
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

#ifdef OVITO_QML_GUI
/******************************************************************************
* Returns the class name of the selected color gradient.
******************************************************************************/
QString ColorCodingModifier::colorGradientType() const
{
    return colorGradient() ? colorGradient()->getOOClass().name() : QString();
}

/******************************************************************************
* Assigns a new color gradient based on its class name.
******************************************************************************/
void ColorCodingModifier::setColorGradientType(const QString& typeName)
{
    OvitoClassPtr descriptor = PluginManager::instance().findClass(QString(), typeName);
    if(!descriptor) {
        qWarning() << "setColorGradientType: Color gradient class" << typeName << "does not exist.";
        return;
    }
    OORef<ColorCodingGradient> gradient = static_object_cast<ColorCodingGradient>(descriptor->createInstance());
    if(gradient) {
        setColorGradient(std::move(gradient));
#ifndef OVITO_DISABLE_QSETTINGS
        QSettings settings;
        settings.beginGroup(ColorCodingModifier::OOClass().plugin()->pluginId());
        settings.beginGroup(ColorCodingModifier::OOClass().name());
        settings.setValue(PROPERTY_FIELD(ColorCodingModifier::colorGradient).identifier(),
                QVariant::fromValue(OvitoClass::encodeAsString(descriptor)));
#endif
    }
}
#endif

/******************************************************************************
* Applies the modifier operation to the data in a pipeline flow state.
******************************************************************************/
PipelineStatus ColorCodingModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState& state, const PipelineFlowState& inputState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(request.modifier());

    if(!mod->colorGradient())
        throw Exception(tr("No color gradient has been selected."));

    // Get the source property.
    const PropertyReference& sourceProperty = mod->sourceProperty();
    int vecComponent;
    if(sourceProperty.isNull())
        throw Exception(tr("No source property was set as input for color coding."));

    // Look up the selected property container. Make sure we can safely modify it.
    DataObjectPath objectPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());

    // Check if the source property is the right kind of property.
    if(sourceProperty.containerClass() != &container->getOOMetaClass())
        throw Exception(tr("Color coding modifier was set to operate on '%1', but the selected input is a '%2' property.")
            .arg(getOOMetaClass().pythonDataName()).arg(sourceProperty.containerClass()->propertyClassDisplayName()));

    // Make sure input data structure is ok.
    container->verifyIntegrity();

    ConstPropertyPtr property = sourceProperty.findInContainer(container);
    if(!property)
        throw Exception(tr("The property with the name '%1' does not exist.").arg(sourceProperty.name()));
    if(sourceProperty.vectorComponent() >= (int)property->componentCount())
        throw Exception(tr("The vector component is out of range. The property '%1' has only %2 values per data element.").arg(sourceProperty.name()).arg(property->componentCount()));
    vecComponent = std::max(0, sourceProperty.vectorComponent());

    // Get the selection property if enabled by the user.
    ConstPropertyPtr selectionProperty;
    if(mod->colorOnlySelected() && container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
        if(const PropertyObject* selPropertyObj = container->getProperty(PropertyObject::GenericSelectionProperty)) {
            selectionProperty = selPropertyObj;

            // Clear selection if requested.
            if(!mod->keepSelection()) {
                container->removeProperty(selPropertyObj);
            }
        }
    }

    // Get modifier's parameter values.
    FloatType startValue = 0, endValue = 0;

    if(mod->autoAdjustRange()) {
        FloatType minValue = std::numeric_limits<FloatType>::max();
        FloatType maxValue = std::numeric_limits<FloatType>::lowest();
        if(mod->determinePropertyValueRange(state, minValue, maxValue)) {
            startValue = minValue;
            endValue = maxValue;
            state.setAttribute(QStringLiteral("ColorCoding.RangeMin"), minValue, request.modApp());
            state.setAttribute(QStringLiteral("ColorCoding.RangeMax"), maxValue, request.modApp());
        }
    }
    else {
        if(mod->startValueController()) startValue = mod->startValueController()->getFloatValue(request.time(), state.mutableStateValidity());
        if(mod->endValueController()) endValue = mod->endValueController()->getFloatValue(request.time(), state.mutableStateValidity());
    }

    // Clamp to finite range.
    if(!std::isfinite(startValue)) startValue = std::numeric_limits<FloatType>::lowest();
    if(!std::isfinite(endValue)) endValue = std::numeric_limits<FloatType>::max();

    // Create the color output property.
    BufferAccess<ColorG> colorProperty = container->createProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, outputColorPropertyId(), objectPath);

    BufferAccess<const SelectionIntType> selection(selectionProperty.get());
    bool result = property->forEach(vecComponent, [&](size_t i, auto v) {
        if(selection && !selection[i])
            return;

        // Compute linear interpolation.
        GraphicsFloatType t;
        if(startValue == endValue) {
            if(v == startValue) t = GraphicsFloatType(0.5);
            else if(v > startValue) t = GraphicsFloatType(1.0);
            else t = GraphicsFloatType(0.0);
        }
        else t = static_cast<GraphicsFloatType>((v - startValue) / (endValue - startValue));

        // Clamp values.
        if(std::isnan(t)) t = 0;
        else if(t == std::numeric_limits<GraphicsFloatType>::infinity()) t = 1;
        else if(t == -std::numeric_limits<GraphicsFloatType>::infinity()) t = 0;
        else if(t < 0) t = 0;
        else if(t > 1) t = 1;

        colorProperty[i] = mod->colorGradient()->valueToColor(t);
    });
    if(!result)
        throw Exception(tr("The property '%1' has an invalid or non-numeric data type.").arg(property->name()));
#endif

    return PipelineStatus::Success;
}

}   // End of namespace
