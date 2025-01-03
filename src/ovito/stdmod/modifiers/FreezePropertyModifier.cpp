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
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "FreezePropertyModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FreezePropertyModifier);
OVITO_CLASSINFO(FreezePropertyModifier, "DisplayName", "Freeze property");
OVITO_CLASSINFO(FreezePropertyModifier, "Description", "Copy the values of a varying property from one trajectory frame to all others.");
OVITO_CLASSINFO(FreezePropertyModifier, "ModifierCategory", "Modification");
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, sourceProperty);
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, destinationProperty);
DEFINE_PROPERTY_FIELD(FreezePropertyModifier, freezeTime);
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, sourceProperty, "Property");
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, destinationProperty, "Destination property");
SET_PROPERTY_FIELD_LABEL(FreezePropertyModifier, freezeTime, "Freeze at frame");

IMPLEMENT_CREATABLE_OVITO_CLASS(FreezePropertyModificationNode);
OVITO_CLASSINFO(FreezePropertyModificationNode, "ClassNameAlias", "FreezePropertyModifierApplication");  // For backward compatibility with OVITO 3.9.2
DEFINE_REFERENCE_FIELD(FreezePropertyModificationNode, property);
DEFINE_REFERENCE_FIELD(FreezePropertyModificationNode, identifiers);
DEFINE_VECTOR_REFERENCE_FIELD(FreezePropertyModificationNode, cachedVisElements);
SET_MODIFICATION_NODE_TYPE(FreezePropertyModifier, FreezePropertyModificationNode);

/******************************************************************************
* Constructor.
******************************************************************************/
void FreezePropertyModifier::initializeObject(ObjectInitializationFlags flags)
{
    GenericPropertyModifier::initializeObject(flags);

    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier is being inserted
* into a pipeline.
******************************************************************************/
void FreezePropertyModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    GenericPropertyModifier::initializeModifier(request);

    // Use the first available particle property from the input state as data source when the modifier is newly created.
    if(!sourceProperty() && subject() && this_task::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).blockForResult();
        if(const PropertyContainer* container = input.getLeafObject(subject())) {
            for(const Property* property : container->properties()) {
                setSourceProperty(property);
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
    if(field == PROPERTY_FIELD(FreezePropertyModifier::sourceProperty) && !isBeingLoaded()) {
        // Changes of some the modifier's parameters affect the result of FreezePropertyModifier::getPipelineEditorShortInfo().
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> FreezePropertyModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // Check if we already have the frozen property available.
    if(FreezePropertyModificationNode* modNode = dynamic_object_cast<FreezePropertyModificationNode>(request.modificationNode())) {
        if(modNode->hasFrozenState(AnimationTime::fromFrame(freezeTime()))) {
            // Perform replacement of the property in the input pipeline state.
            return transferFrozenProperty(modNode, std::move(state));
        }
    }

    // Cannot make upstream requests in interactive mode - this would take too long.
    if(request.interactiveMode())
        throw Exception(tr("No stored property values available yet."));

    // Set up the upstream pipeline request for the freeze time.
    PipelineEvaluationRequest upstreamRequest = request;
    upstreamRequest.setTime(AnimationTime::fromFrame(freezeTime()));

    // Request the frozen state from the upstream pipeline.
    return request.modificationNode()->evaluateInput(upstreamRequest)
        .then(ObjectExecutor(this), [this, request, state = std::move(state)](const PipelineFlowState& frozenState) mutable {

            // Extract the property to freeze.
            if(FreezePropertyModificationNode* modNode = dynamic_object_cast<FreezePropertyModificationNode>(request.modificationNode())) {
                if(modNode->modifier() == this && sourceProperty() && subject()) {

                    const PropertyContainer* container = frozenState.expectLeafObject(subject());
                    if(const Property* property = sourceProperty().findInContainer(container)) {

                        // Cache the property to be frozen in the ModificationNode.
                        modNode->updateStoredData(property,
                            container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty)
                                ? container->getProperty(Property::GenericIdentifierProperty)
                                : nullptr,
                            frozenState.stateValidity());

                        // Perform the actual replacement of the property in the input pipeline state.
                        return transferFrozenProperty(modNode, std::move(state));
                    }
                    else {
                        throw Exception(tr("The property '%1' is not present in the input state.").arg(sourceProperty().name()));
                    }
                }
                modNode->invalidateFrozenState();
            }

            return std::move(state);
        });
}

/******************************************************************************
* Copies the stored property to the current pipeline state.
******************************************************************************/
PipelineFlowState FreezePropertyModifier::transferFrozenProperty(FreezePropertyModificationNode* modNode, PipelineFlowState state) const
{
    if(!subject())
        throw Exception(tr("No property type selected."));

    if(!sourceProperty()) {
        state.setStatus(PipelineStatus(PipelineStatus::Warning, tr("No source property selected.")));
        return state;
    }
    if(!destinationProperty())
        throw Exception(tr("No output property selected."));

    // Retrieve the property values stored in the ModificationNode.
    if(!modNode || !modNode->property())
        throw Exception(tr("No stored property values available."));

    // Look up the property container object.
    PropertyContainer* container = state.expectMutableLeafObject(subject());
    container->verifyIntegrity();

    // Get the property that will be overwritten by the stored one.
    Property* outputProperty;
    int destTypeId = destinationProperty().standardTypeId(&container->getOOMetaClass());
    if(destTypeId != 0) {
        outputProperty = container->createProperty(DataBuffer::Initialized, destTypeId);
        if(outputProperty->dataType() != modNode->property()->dataType()
            || outputProperty->componentCount() != modNode->property()->componentCount()
            || outputProperty->stride() != modNode->property()->stride())
            throw Exception(tr("Types of source property and output property are not compatible. Cannot restore saved property values."));
    }
    else {
        outputProperty = container->createProperty(DataBuffer::Initialized, destinationProperty().name(),
            modNode->property()->dataType(), modNode->property()->componentCount());
        outputProperty->setComponentNames(modNode->property()->componentNames());
    }
    OVITO_ASSERT(outputProperty->stride() == modNode->property()->stride());

    // Check if particle IDs are present and if the order of particles has changed
    // since we took the snapshot of the property values.
    BufferReadAccess<IdentifierIntType> idProperty = container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty)
        ? container->getProperty(Property::GenericIdentifierProperty)
        : nullptr;
    BufferReadAccess<IdentifierIntType> storedIds = modNode->identifiers();
    if(storedIds && idProperty && (idProperty.size() != storedIds.size() || !boost::equal(idProperty, storedIds))) {

        // Obtain ID-to-index map for frozen state.
        // The map needs to be built only once and can be cached in the modification node.
        if(modNode->idmap().empty()) {
            std::unordered_map<IdentifierIntType, size_t> idmap;
            size_t index = 0;
            for(auto id : storedIds) {
                if(!idmap.insert(std::make_pair(id, index)).second)
                    throw Exception(tr("Detected non-unique element ID %1 in frozen snapshot. Cannot map property values from frozen state to current state.").arg(id));
                index++;
            }
            // Store the ID-to-index map in the modification node.
            modNode->idmap().swap(idmap);
        }
        storedIds.reset();

        // Build index-to-index map for the current state.
        const std::unordered_map<IdentifierIntType, size_t>& idmap = modNode->idmap();
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
        modNode->property()->mappedCopyTo(*outputProperty, mapping);
    }
    else {
        storedIds.reset();
        idProperty.reset();

        // Make sure the number of elements didn't change when no IDs are defined.
        if(modNode->property()->size() != outputProperty->size())
            throw Exception(tr("Number of input elements has changed. Cannot restore saved property values. There were %1 elements when the snapshot was created. Now there are %2.").arg(modNode->property()->size()).arg(outputProperty->size()));

        if(outputProperty->dataType() == modNode->property()->dataType() && outputProperty->stride() == modNode->property()->stride())
            outputProperty->copyFrom(*modNode->property());
    }

    // Replace vis elements of output property with cached ones and cache any new elements.
    // This is required to avoid losing the output property's display settings
    // each time the modifier is re-evaluated or when serializing the modifier application.
    OORefVector<DataVis> currentVisElements = outputProperty->visElements();
    // Replace with cached vis elements if they are of the same class type.
    for(int i = 0; i < currentVisElements.size() && i < modNode->cachedVisElements().size(); i++) {
        if(currentVisElements[i]->getOOClass() == modNode->cachedVisElements()[i]->getOOClass()) {
            currentVisElements[i] = modNode->cachedVisElements()[i];
        }
    }
    outputProperty->setVisElements(currentVisElements);
    modNode->setCachedVisElements(std::move(currentVisElements));

    return state;
}

/******************************************************************************
* Makes a copy of the given source property and, optionally, of the provided
* particle identifier list, which will allow to restore the saved property
* values even if the order of particles changes.
******************************************************************************/
void FreezePropertyModificationNode::updateStoredData(const Property* property, const Property* identifiers, TimeInterval validityInterval)
{
    CloneHelper cloneHelper;
    setProperty(cloneHelper.cloneObject(property, false));
    setIdentifiers(cloneHelper.cloneObject(identifiers, false));
    _validityInterval = validityInterval;
    _idmap.clear();
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool FreezePropertyModificationNode::referenceEvent(RefTarget* source, const ReferenceEvent& event)
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
    return ModificationNode::referenceEvent(source, event);
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
        if(ModificationNode* node = someNode()) {
            QSet<Pipeline*> pipelines = node->pipelines(true);
            if(!pipelines.empty()) {
                if(SceneNode* sceneNode = (*pipelines.begin())->someSceneNode()) {
                    if(Scene* scene = sceneNode->scene()) {
                        if(scene->animationSettings()) {
                            int ticksPerFrame = (int)std::round(4800.0f / scene->animationSettings()->framesPerSecond());
                            setFreezeTime(freezeTime() / ticksPerFrame);
                        }
                    }
                }
            }
        }
    }
}

}   // End of namespace
