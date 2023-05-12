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
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "FreezePropertyModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(FreezePropertyModifier);
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, destinationProperty);
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, freezeTime);
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, destinationProperty, "Destination property");
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, freezeTime, "Freeze at frame");

IMPLEMENT_OVITO_CLASS(FreezePropertyModifierApplication);
DEFINE_REFERENCE_FIELD(FreezePropertyModifierApplication, property);
DEFINE_REFERENCE_FIELD(FreezePropertyModifierApplication, identifiers);
DEFINE_VECTOR_REFERENCE_FIELD(FreezePropertyModifierApplication, cachedVisElements);
SET_MODIFIER_APPLICATION_TYPE(FreezePropertyModifier, FreezePropertyModifierApplication);

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
FreezePropertyModifier::FreezePropertyModifier(ObjectInitializationFlags flags) : GenericPropertyModifier(flags),
    _freezeTime(0)
{
    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("ParticlesObject"));
}

/******************************************************************************
* This method is called by the system when the modifier is being inserted
* into a pipeline.
******************************************************************************/
void FreezePropertyModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    // Use the first available particle property from the input state as data source when the modifier is newly created.
    if(sourceProperty().isNull() && subject() && ExecutionContext::isInteractive()) {
        const PipelineFlowState& input = request.modApp()->evaluateInputSynchronous(request);
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            for(const PropertyObject* property : container->properties()) {
                setSourceProperty(PropertyReference(subject().dataClass(), property));
                setDestinationProperty(sourceProperty());
                break;
            }
        }
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void FreezePropertyModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // Whenever the selected property class of this modifier changes, update the property references accordingly.
    if(field == PROPERTY_FIELD(GenericPropertyModifier::subject) && !isBeingLoaded() && !isAboutToBeDeleted() && !isUndoingOrRedoing()) {
        setSourceProperty(sourceProperty().convertToContainerClass(subject().dataClass()));
        setDestinationProperty(destinationProperty().convertToContainerClass(subject().dataClass()));
    }
    else if(field == PROPERTY_FIELD(FreezePropertyModifier::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of FreezePropertyModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> FreezePropertyModifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    // Check if we already have the frozen property available.
    if(FreezePropertyModifierApplication* myModApp = dynamic_object_cast<FreezePropertyModifierApplication>(request.modApp())) {
        if(myModApp->hasFrozenState(AnimationTime::fromFrame(freezeTime()))) {
            // Perform replacement of the property in the input pipeline state.
            PipelineFlowState output = input;
            evaluateSynchronous(request, output);
            return std::move(output);
        }
    }

    // Set up the upstream pipeline request.
    PipelineEvaluationRequest upstreamRequest = request;
    upstreamRequest.setTime(AnimationTime::fromFrame(freezeTime()));

    // Request the frozen state from the upstream pipeline.
    return request.modApp()->evaluateInput(upstreamRequest)
        .then(*this, [this, request, modApp = OORef<ModifierApplication>(request.modApp()), state = input](const PipelineFlowState& frozenState) mutable {

            // Extract the input property.
            if(FreezePropertyModifierApplication* myModApp = dynamic_object_cast<FreezePropertyModifierApplication>(request.modApp())) {
                if(myModApp->modifier() == this && !sourceProperty().isNull() && subject()) {

                    const PropertyContainer* container = frozenState.expectLeafObject(subject());
                    if(const PropertyObject* property = sourceProperty().findInContainer(container)) {

                        // Cache the property to be frozen in the ModifierApplication.
                        myModApp->updateStoredData(property,
                            container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericIdentifierProperty)
                                ? container->getProperty(PropertyObject::GenericIdentifierProperty)
                                : nullptr,
                            frozenState.stateValidity());

                        // Perform the actual replacement of the property in the input pipeline state.
                        evaluateSynchronous(request, state);
                        return std::move(state);
                    }
                    else {
                        throw Exception(tr("The property '%1' is not present in the input state.").arg(sourceProperty().name()));
                    }
                }
                myModApp->invalidateFrozenState();
            }

            return std::move(state);
        });
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void FreezePropertyModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    if(!subject())
        throw Exception(tr("No property type selected."));

    if(sourceProperty().isNull()) {
        state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("No source property selected.")));
        return;
    }
    if(destinationProperty().isNull())
        throw Exception(tr("No output property selected."));

    // Retrieve the property values stored in the ModifierApplication.
    FreezePropertyModifierApplication* myModApp = dynamic_object_cast<FreezePropertyModifierApplication>(request.modApp());
    if(!myModApp || !myModApp->property())
        throw Exception(tr("No stored property values available."));

    // Look up the property container object.
    PropertyContainer* container = state.expectMutableLeafObject(subject());
    container->verifyIntegrity();

    // Get the property that will be overwritten by the stored one.
    PropertyObject* outputProperty;
    if(destinationProperty().type() != PropertyObject::GenericUserProperty) {
        outputProperty = container->createProperty(DataBuffer::Initialized, destinationProperty().type());
        if(outputProperty->dataType() != myModApp->property()->dataType()
            || outputProperty->componentCount() != myModApp->property()->componentCount()
            || outputProperty->stride() != myModApp->property()->stride())
            throw Exception(tr("Types of source property and output property are not compatible. Cannot restore saved property values."));
    }
    else {
        outputProperty = container->createProperty(DataBuffer::Initialized, destinationProperty().name(),
            myModApp->property()->dataType(), myModApp->property()->componentCount());
        outputProperty->setComponentNames(myModApp->property()->componentNames());
    }
    OVITO_ASSERT(outputProperty->stride() == myModApp->property()->stride());

    // Check if particle IDs are present and if the order of particles has changed
    // since we took the snapshot of the property values.
    ConstPropertyAccess<qlonglong> idProperty = container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericIdentifierProperty)
        ? container->getProperty(PropertyObject::GenericIdentifierProperty)
        : nullptr;
    ConstPropertyAccess<qlonglong> storedIds = myModApp->identifiers();
    if(storedIds && idProperty && (idProperty.size() != storedIds.size() || !boost::equal(idProperty, storedIds))) {

        // Build ID-to-index map.
        std::unordered_map<qlonglong,size_t> idmap;
        size_t index = 0;
        for(auto id : storedIds) {
            if(!idmap.insert(std::make_pair(id, index)).second)
                throw Exception(tr("Detected duplicate element ID %1 in saved snapshot. Cannot apply saved property values.").arg(id));
            index++;
        }
        storedIds.reset();

        // Build index-to-index map.
        std::vector<size_t> mapping(outputProperty->size());
        auto id = idProperty.cbegin();
        for(size_t& mappedIndex : mapping) {
            auto mapEntry = idmap.find(*id++);
            if(mapEntry == idmap.end())
                throw Exception(tr("Detected new element ID %1, which didn't exist when the snapshot was created. Cannot restore saved property values.").arg(*id));
            mappedIndex = mapEntry->second;
        }
        idProperty.reset();

        // Copy and reorder property data.
        myModApp->property()->mappedCopyTo(*outputProperty, mapping);
    }
    else {
        // Make sure the number of elements didn't change when no IDs are defined.
        if(myModApp->property()->size() != outputProperty->size())
            throw Exception(tr("Number of input elements has changed. Cannot restore saved property values. There were %1 elements when the snapshot was created. Now there are %2.").arg(myModApp->property()->size()).arg(outputProperty->size()));

        if(outputProperty->dataType() == myModApp->property()->dataType() && outputProperty->stride() == myModApp->property()->stride())
            outputProperty->copyFrom(*myModApp->property());
    }

    // Replace vis elements of output property with cached ones and cache any new elements.
    // This is required to avoid losing the output property's display settings
    // each time the modifier is re-evaluated or when serializing the modifier application.
    OORefVector<DataVis> currentVisElements = outputProperty->visElements();
    // Replace with cached vis elements if they are of the same class type.
    for(int i = 0; i < currentVisElements.size() && i < myModApp->cachedVisElements().size(); i++) {
        if(currentVisElements[i]->getOOClass() == myModApp->cachedVisElements()[i]->getOOClass()) {
            currentVisElements[i] = myModApp->cachedVisElements()[i];
        }
    }
    outputProperty->setVisElements(currentVisElements);
    myModApp->setCachedVisElements(std::move(currentVisElements));
}

/******************************************************************************
* Makes a copy of the given source property and, optionally, of the provided
* particle identifier list, which will allow to restore the saved property
* values even if the order of particles changes.
******************************************************************************/
void FreezePropertyModifierApplication::updateStoredData(const PropertyObject* property, const PropertyObject* identifiers, TimeInterval validityInterval)
{
    CloneHelper cloneHelper;
    setProperty(cloneHelper.cloneObject(property, false));
    setIdentifiers(cloneHelper.cloneObject(identifiers, false));
    _validityInterval = validityInterval;
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool FreezePropertyModifierApplication::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == input()) {
            if(FreezePropertyModifier* mod = dynamic_object_cast<FreezePropertyModifier>(modifier())) {
                if(static_cast<const TargetChangedEvent&>(event).unchangedInterval().contains(AnimationTime::fromFrame(mod->freezeTime())) == false) {
                    // Invalidate cached state.
                    invalidateFrozenState();
                    notifyTargetChanged();
                    return false;
                }
            }
        }
        else if(source == modifier()) {
            invalidateFrozenState();
        }
    }
    return ModifierApplication::referenceEvent(source, event);
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void FreezePropertyModifier::loadFromStreamComplete(ObjectLoadStream& stream)
{
    GenericPropertyModifier::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.7:
    // Convert legacy time value from ticks to frames. This requires access to the AnimationSettings object, which is stored in the scene.
    if(stream.formatVersion() <= 30008) {
        if(ModifierApplication* modApp = someModifierApplication()) {
            QSet<PipelineSceneNode*> pipelines = modApp->pipelines(true);
            if(!pipelines.empty()) {
                if(Scene* scene = (*pipelines.begin())->scene()) {
                    if(scene->animationSettings()) {
                        int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
                        setFreezeTime(freezeTime() / ticksPerFrame);
                    }
                }
            }
        }
    }
}

}   // End of namespace
