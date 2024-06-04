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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include "ComputePropertyModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ComputePropertyModifierDelegate);

IMPLEMENT_CREATABLE_OVITO_CLASS(ComputePropertyModifier);
OVITO_CLASSINFO(ComputePropertyModifier, "DisplayName", "Compute property");
OVITO_CLASSINFO(ComputePropertyModifier, "Description", "Enter a user-defined formula to set properties of particles, bonds and other elements.");
OVITO_CLASSINFO(ComputePropertyModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, expressions);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, outputProperty);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, onlySelectedElements);
DEFINE_PROPERTY_FIELD(ComputePropertyModifier, useMultilineFields);
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, expressions, "Expressions");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, outputProperty, "Output property");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, onlySelectedElements, "Compute only for selected elements");
SET_PROPERTY_FIELD_LABEL(ComputePropertyModifier, useMultilineFields, "Expand field(s)");

IMPLEMENT_CREATABLE_OVITO_CLASS(ComputePropertyModificationNode);
DEFINE_VECTOR_REFERENCE_FIELD(ComputePropertyModificationNode, cachedVisElements);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModificationNode, inputVariableNames);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModificationNode, delegateInputVariableNames);
DEFINE_RUNTIME_PROPERTY_FIELD(ComputePropertyModificationNode, inputVariableTable);
SET_MODIFICATION_NODE_TYPE(ComputePropertyModifier, ComputePropertyModificationNode);
OVITO_CLASSINFO(ComputePropertyModificationNode, "ClassNameAlias", "ComputePropertyModifierApplication");  // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
void ComputePropertyModifier::initializeObject(ObjectInitializationFlags flags)
{
    DelegatingModifier::initializeObject(flags);

    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        // Let this modifier act on particles by default.
        createDefaultModifierDelegate(ComputePropertyModifierDelegate::OOClass(), QStringLiteral("ParticlesComputePropertyModifierDelegate"));

        // Set default output property.
        if(delegate())
            setOutputProperty(QStringLiteral("My property"));
    }
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
    if(delegate()) {
        int typeId = outputProperty().standardTypeId(delegate()->inputContainerClass());
        if(typeId != 0) {
            setPropertyComponentCount(delegate()->inputContainerClass()->standardPropertyComponentCount(typeId));
            return;
        }
    }
    setPropertyComponentCount(1);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ComputePropertyModifier::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(DelegatingModifier::delegate) && !isBeingDeleted() && !isBeingLoaded() && !isUndoingOrRedoing()) {
        if(delegate())
            delegate()->setComponentCount(expressions().size());
    }
    DelegatingModifier::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ComputePropertyModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(ComputePropertyModifier::outputProperty) && !isBeingLoaded()) {
        // Changes to some the modifier's parameters affect the result of ComputePropertyModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    DelegatingModifier::propertyChanged(field);
}

/******************************************************************************
 * Sends an event to all dependents of this RefTarget.
 ******************************************************************************/
void ComputePropertyModifier::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged && event.sender() == this) {
        if(static_cast<const TargetChangedEvent&>(event).field() == PROPERTY_FIELD(ComputePropertyModifier::useMultilineFields)) {
            // Changes to the 'useMultilineFields' option do not invalidate the modifier's results.
            // Intercept the change event and modify it such that it does not trigger a re-evaluation of the modifier.
            DelegatingModifier::notifyDependentsImpl(TargetChangedEvent(this, PROPERTY_FIELD(ComputePropertyModifier::useMultilineFields), TimeInterval::infinite()));
            return;
        }
    }
    DelegatingModifier::notifyDependentsImpl(event);
}

/******************************************************************************
 * Checks if math expressions are time-dependent, i.e. whether they involve the animation frame number.
 ******************************************************************************/
bool ComputePropertyModifierDelegate::isExpressionTimeDependent(ComputePropertyModifier* modifier) const
{
    for(const QString& expression : modifier->expressions()) {
        // This is a very simple check for the presence of the word "Frame" in the expression.
        // It's not perfect, but should catch all relevant cases (maybe more).
        if(expression.contains(QLatin1String("Frame")))
            return true;
    }
    return false;
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void ComputePropertyModifierDelegate::preevaluateDelegate(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Determine whether math expressions are time-dependent, i.e. whether they involve the current animation
    // frame number. If so, then we have to restrict the validity interval of the computation results
    // to the current animation time.
    if(isExpressionTimeDependent(static_object_cast<ComputePropertyModifier>(request.modifier()))) {
        validityInterval.intersect(request.time());
    }

    // Indicate that we do a different computation depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
 * Applies this modifier delegate to the data.
 ******************************************************************************/
Future<PipelineFlowState> ComputePropertyModifierDelegate::apply(const ModifierEvaluationRequest& request, PipelineFlowState&& state, const PipelineFlowState& originalState, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
    const ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(request.modifier());
    ComputePropertyModificationNode* modNode = static_object_cast<ComputePropertyModificationNode>(request.modificationNode());

    // Look up the property container which we will operate on. Make sure we can safely modify it.
    DataObjectPath containerPath = state.expectMutableObject(inputContainerRef());
    PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Make sure input data structure is ok.
    container->verifyIntegrity();

    // Get input selection property if computation is restricted to selected elements.
    ConstPropertyPtr selectionProperty;
    if(modifier->onlySelectedElements() && container->getOOMetaClass().isValidStandardPropertyId(Property::GenericSelectionProperty)) {
        selectionProperty = container->getProperty(Property::GenericSelectionProperty);
        if(!selectionProperty)
            throw Exception(tr("Compute property modifier has been restricted to selected elements, but no selection was previously defined."));
    }

    // In interactive mode, do not perform a real computation. Instead, used an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            ConstDataObjectPath containerPathCached = cachedState.getObject(inputContainerRef());
            if(!containerPathCached.empty()) {
                DataOORef<const PropertyContainer> containerCached = static_object_cast<PropertyContainer>(containerPathCached.back());
                if(const Property* propertyCached = modifier->outputProperty().findInContainer(containerCached)) {
                    return asyncLaunch([state = std::move(state), container, containerPath = std::move(containerPath), propertyCached, containerCached = std::move(containerCached)]() mutable {
                        container->tryToAdoptProperties(containerCached, {propertyCached}, containerPath);
                        return std::move(state);
                    });
                }
            }
        }
        return std::move(state);
    }

    // Prepare output property.
    PropertyPtr outputProperty;
    const Property* existingProperty = modifier->outputProperty().findInContainer(container);
    if(existingProperty && existingProperty->componentCount() == modifier->propertyComponentCount()) {
        outputProperty = container->makePropertyMutable(existingProperty, selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        modNode->setCachedVisElements({});
    }
    else {
        // Allocate new data array.
        int typeId = modifier->outputProperty().standardTypeId(&container->getOOMetaClass());
        if(typeId != 0) {
            outputProperty = container->createProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, typeId, containerPath);
        }
        else if(modifier->outputProperty() && modifier->propertyComponentCount() > 0) {
            outputProperty = container->createProperty(selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized, modifier->outputProperty().name(), Property::FloatDefault, modifier->propertyComponentCount());
        }
        else {
            throw Exception(tr("Output property of compute property modifier has not been specified."));
        }

        // Replace vis elements of output property with cached ones and cache any new vis elements.
        // This is required to avoid losing the output property's display settings
        // each time the modifier is re-evaluated or when serializing the modifier.
        OORefVector<DataVis> currentVisElements = outputProperty->visElements();
        // Replace with cached vis elements if they are of the same class type.
        for(int i = 0; i < currentVisElements.size() && i < modNode->cachedVisElements().size(); i++) {
            if(currentVisElements[i]->getOOClass() == modNode->cachedVisElements()[i]->getOOClass()) {
                currentVisElements[i] = modNode->cachedVisElements()[i];
            }
        }
        outputProperty->setVisElements(currentVisElements);
        modNode->setCachedVisElements(std::move(currentVisElements));
    }
    if(modifier->propertyComponentCount() != outputProperty->componentCount())
        throw Exception(tr("Number of expressions does not match component count of output property."));

    // Launch computations in a separate thread.
    return performComputation(modifier, modNode, std::move(state), originalState, std::move(outputProperty), std::move(selectionProperty), request.time().frame());
}

/******************************************************************************
 * Launches the actual computations.
 ******************************************************************************/
Future<PipelineFlowState> ComputePropertyModifierDelegate::performComputation(
    const ComputePropertyModifier* modifier,
    ComputePropertyModificationNode* modNode,
    PipelineFlowState state,
    const PipelineFlowState& originalState,
    PropertyPtr outputProperty,
    ConstPropertyPtr selectionProperty,
    int frame) const
{
    // Initialize expression evaluator.
    auto evaluator = std::make_unique<PropertyExpressionEvaluator>();
    evaluator->initialize(modifier->expressions(), originalState, originalState.expectObject(inputContainerRef()), frame);

    // Store the list of input variables in the ModificationNode so that the UI component can display it to the user.
    modNode->setInputVariableNames(evaluator->inputVariableNames());
    modNode->setInputVariableTable(evaluator->inputVariableTable());

    // Notify the UI component that the list of variables should be refreshed.
    modifier->notifyDependents(ReferenceEvent::ObjectStatusChanged);
    modNode->notifyDependents(ReferenceEvent::ObjectStatusChanged);

    // The actual computation can be performed in a separate worker thread.
    return asyncLaunch([
            state = std::move(state),
            outputProperty = std::move(outputProperty),
            selectionProperty = std::move(selectionProperty),
            evaluator = std::move(evaluator)]() mutable
    {
        this_task::setProgressText(tr("Computing property '%1'").arg(outputProperty->name()));

        RawBufferAccess<access_mode::write> outputAccessor(outputProperty, selectionProperty ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        BufferReadAccess<SelectionIntType> selectionAccessor(selectionProperty);

        EnumerableThreadSpecific<PropertyExpressionEvaluator::Worker> expressionWorkers;
        size_t componentCount = outputAccessor.componentCount();

        parallelForInnerOuter(outputProperty->size(), 10000, [&](auto&& iterate) {
            PropertyExpressionEvaluator::Worker& worker = expressionWorkers.create(*evaluator);
            iterate([&](size_t i) {

                // Skip unselected particles if requested.
                if(selectionAccessor && !selectionAccessor[i])
                    return;

                for(size_t component = 0; component < componentCount; component++) {
                    // Compute expression value.
                    FloatType value = worker.evaluate(i, component);

                    // Store results in output property.
                    outputAccessor.set(i, component, value);
                }
            });
        });

        return std::move(state);
    });
}

}   // End of namespace
