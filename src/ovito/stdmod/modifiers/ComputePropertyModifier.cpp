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
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "ComputePropertyModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ComputePropertyModifierDelegate);

IMPLEMENT_OVITO_CLASS(ComputePropertyModifier);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, expressions);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, outputProperty);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, onlySelectedElements);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, useMultilineFields);
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, expressions, "Expressions");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, outputProperty, "Output property");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, onlySelectedElements, "Compute only for selected elements");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, useMultilineFields, "Expand field(s)");

IMPLEMENT_OVITO_CLASS(ComputePropertyModifierApplication);
DEFINE_VECTOR_REFERENCE_FIELD(ComputePropertyModifierApplication, cachedVisElements);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModifierApplication, inputVariableNames);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModifierApplication, delegateInputVariableNames);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModifierApplication, inputVariableTable);
SET_PROPERTY_FIELD_CHANGE_EVENT(ComputePropertyModifierApplication, inputVariableNames, ReferenceEvent::ObjectStatusChanged);
SET_PROPERTY_FIELD_CHANGE_EVENT(ComputePropertyModifierApplication, inputVariableTable, ReferenceEvent::ObjectStatusChanged);
SET_MODIFIER_APPLICATION_TYPE(ComputePropertyModifier, ComputePropertyModifierApplication);

/******************************************************************************
* Constructs a new instance of this class.
******************************************************************************/
ComputePropertyModifier::ComputePropertyModifier(ObjectInitializationFlags flags) : AsynchronousDelegatingModifier(flags),
    _expressions(QStringList("0")),
    _onlySelectedElements(false),
    _useMultilineFields(false)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Let this modifier act on particles by default.
        createDefaultModifierDelegate(ComputePropertyModifierDelegate::OOClass(), QStringLiteral("ParticlesComputePropertyModifierDelegate"));
        // Set default output property.
        if(delegate())
            setOutputProperty(PropertyReference(delegate()->inputContainerClass(), QStringLiteral("My property")));
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ComputePropertyModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(ComputePropertyModifier::outputProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of ComputePropertyModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    AsynchronousDelegatingModifier::propertyChanged(field);
}

/******************************************************************************
* Sets the number of vector components of the property to create.
******************************************************************************/
void ComputePropertyModifier::setPropertyComponentCount(int newComponentCount)
{
    if(newComponentCount < expressions().size()) {
        setExpressions(expressions().mid(0, newComponentCount));
    }
    else if(newComponentCount > expressions().size()) {
        QStringList newList = expressions();
        while(newList.size() < newComponentCount)
            newList.append("0");
        setExpressions(newList);
    }
    if(delegate())
        delegate()->setComponentCount(newComponentCount);
}

/******************************************************************************
* Sets the number of expressions based on the selected output property.
******************************************************************************/
void ComputePropertyModifier::adjustPropertyComponentCount()
{
    if(delegate() && outputProperty().type() != PropertyObject::GenericUserProperty)
        setPropertyComponentCount(delegate()->inputContainerClass()->standardPropertyComponentCount(outputProperty().type()));
    else
        setPropertyComponentCount(1);
}

/******************************************************************************
* Returns the vector component names of the selected output property.
******************************************************************************/
QStringList ComputePropertyModifier::propertyComponentNames() const
{
    if(!outputProperty().isNull() && outputProperty().type() != PropertyObject::GenericUserProperty) {
        return outputProperty().containerClass()->standardPropertyComponentNames(outputProperty().type());
    }
    return {};
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ComputePropertyModifier::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(AsynchronousDelegatingModifier::delegate) && !isAboutToBeDeleted() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        setOutputProperty(outputProperty().convertToContainerClass(delegate() ? delegate()->inputContainerClass() : nullptr));
        if(delegate()) delegate()->setComponentCount(expressions().size());
    }
    AsynchronousDelegatingModifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the
* modifier's results.
******************************************************************************/
Future<AsynchronousModifier::EnginePtr> ComputePropertyModifier::createEngine(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    ComputePropertyModifierApplication* myModApp = dynamic_object_cast<ComputePropertyModifierApplication>(request.modApp());

    // Get the delegate object that will take of the specific details.
    if(!delegate())
        throw Exception(tr("No delegate set for the compute property modifier."));

    // Look up the property container which we will operate on.
    ConstDataObjectPath objectPath = input.expectObject(delegate()->inputContainerRef());
    const PropertyContainer* container = static_object_cast<PropertyContainer>(objectPath.back());
    if(outputProperty().containerClass() != delegate()->inputContainerClass())
        throw Exception(tr("Property %1 to be computed is not a %2 property.").arg(outputProperty().name()).arg(delegate()->inputContainerClass()->elementDescriptionName()));
    container->verifyIntegrity();

    // Get the number of input elements.
    size_t nelements = container->elementCount();

    // Get input selection property and existing property data.
    ConstPropertyPtr selectionProperty;
    if(onlySelectedElements() && container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericSelectionProperty)) {
        selectionProperty = container->getProperty(PropertyObject::GenericSelectionProperty);
        if(!selectionProperty)
            throw Exception(tr("Compute property modifier has been restricted to selected elements, but no selection was previously defined."));
    }

    // Prepare output property.
    PropertyPtr outp;
    const PropertyObject* existingProperty = outputProperty().findInContainer(container);
    if(existingProperty && existingProperty->componentCount() == propertyComponentCount()) {
        // Copy existing data.
        outp = CloneHelper().cloneObject(existingProperty, false);

        // Reset cached vis elements.
        if(myModApp)
            myModApp->setCachedVisElements({});
    }
    else {
        // Allocate new data array.
        if(outputProperty().type() != PropertyObject::GenericUserProperty) {
            outp = container->getOOMetaClass().createStandardProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, nelements, outputProperty().type(), objectPath);
        }
        else if(!outputProperty().name().isEmpty() && propertyComponentCount() > 0) {
            outp = container->getOOMetaClass().createUserProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, nelements, PropertyObject::FloatDefault, propertyComponentCount(), outputProperty().name());
        }
        else {
            throw Exception(tr("Output property of compute property modifier has not been specified."));
        }

        if(myModApp) {
            // Replace vis elements of output property with cached ones and cache any new vis elements.
            // This is required to avoid losing the output property's display settings
            // each time the modifier is re-evaluated or when serializing the modifier.
            OORefVector<DataVis> currentVisElements = outp->visElements();
            // Replace with cached vis elements if they are of the same class type.
            for(int i = 0; i < currentVisElements.size() && i < myModApp->cachedVisElements().size(); i++) {
                if(currentVisElements[i]->getOOClass() == myModApp->cachedVisElements()[i]->getOOClass()) {
                    currentVisElements[i] = myModApp->cachedVisElements()[i];
                }
            }
            outp->setVisElements(currentVisElements);
            myModApp->setCachedVisElements(std::move(currentVisElements));
        }
    }
    if(propertyComponentCount() != outp->componentCount())
        throw Exception(tr("Number of expressions does not match component count of output property."));

    TimeInterval validityInterval = input.stateValidity();

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    auto engine = delegate()->createEngine(request, input,
            objectPath, std::move(outp),
            std::move(selectionProperty),
            expressions());

    // Determine if math expressions are time-dependent, i.e. if they reference the animation
    // frame number. If yes, then we have to restrict the validity interval of the computation
    // to the current time.
    if(engine->isTimeDependent()) {
        TimeInterval iv = engine->validityInterval();
        iv.intersect(request.time());
        engine->setValidityInterval(iv);
    }

    // Store the list of input variables in the ModifierApplication so that the UI component can display it to the user.
    if(myModApp) {
        myModApp->setInputVariableNames(engine->inputVariableNames());
        myModApp->setDelegateInputVariableNames(engine->delegateInputVariableNames());
        myModApp->setInputVariableTable(engine->inputVariableTable());
        delegate()->notifyDependents(ReferenceEvent::ObjectStatusChanged);
        this->notifyDependents(ReferenceEvent::ObjectStatusChanged);
        myModApp->notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    return engine;
}

/******************************************************************************
* Creates and initializes a computation engine that will compute the
* modifier's results.
******************************************************************************/
std::shared_ptr<ComputePropertyModifierDelegate::PropertyComputeEngine> ComputePropertyModifierDelegate::createEngine(
                const ModifierEvaluationRequest& request,
                const PipelineFlowState& input,
                const ConstDataObjectPath& containerPath,
                PropertyPtr outputProperty,
                ConstPropertyPtr selectionProperty,
                QStringList expressions)
{
    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    return std::make_shared<PropertyComputeEngine>(
            request,
            input.stateValidity(),
            input,
            containerPath,
            std::move(outputProperty),
            std::move(selectionProperty),
            std::move(expressions),
            request.time().frame(), // Note: Using global animation frame here, because that's what the user expects.
            std::make_unique<PropertyExpressionEvaluator>());
}

/******************************************************************************
* Constructor.
******************************************************************************/
ComputePropertyModifierDelegate::PropertyComputeEngine::PropertyComputeEngine(
        const ModifierEvaluationRequest& request,
        const TimeInterval& validityInterval,
        const PipelineFlowState& input,
        const ConstDataObjectPath& containerPath,
        PropertyPtr outputProperty,
        const PropertyObject* selectionProperty,
        QStringList expressions,
        int frameNumber,
        std::unique_ptr<PropertyExpressionEvaluator> evaluator) :
    AsynchronousModifier::Engine(request, validityInterval),
    _selection(selectionProperty),
    _expressions(std::move(expressions)),
    _frameNumber(frameNumber),
    _outputProperty(std::move(outputProperty)),
    _evaluator(std::move(evaluator)),
    _outputArray(_outputProperty, !selectionProperty)
{
    OVITO_ASSERT(_expressions.size() == this->outputProperty()->componentCount());

    // Initialize expression evaluator.
    _evaluator->initialize(_expressions, input, containerPath, _frameNumber);
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void ComputePropertyModifierDelegate::PropertyComputeEngine::perform()
{
    setProgressText(tr("Computing property '%1'").arg(outputProperty()->name()));
    setProgressMaximum(outputProperty()->size());

    BufferReadAccess<SelectionIntType> selectionAccessor(selection());

    // Parallelized loop over all data elements.
    parallelForChunksWithProgress(outputProperty()->size(), [this, &selectionAccessor](size_t startIndex, size_t count, ProgressingTask& operation) {
        PropertyExpressionEvaluator::Worker worker(*_evaluator);

        size_t endIndex = startIndex + count;
        size_t componentCount = outputProperty()->componentCount();
        for(size_t elementIndex = startIndex; elementIndex < endIndex; elementIndex++) {

            // Update progress indicator.
            if((elementIndex % 1024) == 0)
                operation.incrementProgressValue(1024);

            // Exit if operation was canceled.
            if(operation.isCanceled())
                return;

            // Skip unselected particles if requested.
            if(selectionAccessor && !selectionAccessor[elementIndex])
                continue;

            for(size_t component = 0; component < componentCount; component++) {

                // Compute expression value.
                FloatType value = worker.evaluate(elementIndex, component);

                // Store results in output property.
                outputArray().set(elementIndex, component, value);
            }
        }
    });

    // Release data that is no longer needed to reduce memory footprint.
    releaseWorkingData();
}

/******************************************************************************
* Returns the list of available input variables.
******************************************************************************/
QStringList ComputePropertyModifierDelegate::PropertyComputeEngine::inputVariableNames() const
{
    if(_evaluator) {
        return _evaluator->inputVariableNames();
    }
    else {
        return {};
    }
}

/******************************************************************************
* This method is called by the system whenever a parameter of the modifier changes.
* The method can be overridden by subclasses to indicate to the caller whether the engine object should be
* discarded or may be kept in the cache, because the computation results are not affected by the changing parameter.
******************************************************************************/
bool ComputePropertyModifierDelegate::PropertyComputeEngine::modifierChanged(const PropertyFieldEvent& event)
{
    // Do not recompute results if just the 'useMultilineFields' option is toggled by the user.
    if(event.field() == PROPERTY_FIELD(ComputePropertyModifier::useMultilineFields))
        return true; // This return value tells the system to hold on to the cached engine object.

    return AsynchronousModifier::Engine::modifierChanged(event);
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void ComputePropertyModifierDelegate::PropertyComputeEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(request.modifier());

    if(!modifier->delegate())
        throw Exception(tr("No delegate set for the Compute Property modifier."));

    // Look up the container we are operating on.
    PropertyContainer* container = state.expectMutableLeafObject(modifier->delegate()->inputContainerRef());

    // Create the output property object in the container.
    container->createProperty(outputProperty());
}

}   // End of namespace
