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
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>
#include "ManualSelectionModifier.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ManualSelectionModifier);

IMPLEMENT_CREATABLE_OVITO_CLASS(ManualSelectionModificationNode);
SET_MODIFICATION_NODE_TYPE(ManualSelectionModifier, ManualSelectionModificationNode);
DEFINE_REFERENCE_FIELD(ManualSelectionModificationNode, selectionSet);
SET_PROPERTY_FIELD_LABEL(ManualSelectionModificationNode, selectionSet, "Element selection set");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ManualSelectionModifier::ManualSelectionModifier(ObjectInitializationFlags flags) : GenericPropertyModifier(flags)
{
    // Operate on particles by default.
    setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("Particles"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ManualSelectionModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    // Take a snapshot of the existing selection state at the time the modifier is created.
    if(!getSelectionSet(request.modificationNode(), false)) {
        resetSelection(request.modificationNode(), request.modificationNode()->evaluateInputSynchronous(request));
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ManualSelectionModifier::propertyChanged(const PropertyFieldDescriptor* field)
{
    // Whenever the subject of this modifier is changed, reset the selection.
    if(field == PROPERTY_FIELD(GenericPropertyModifier::subject) && !isBeingLoaded() && !isUndoingOrRedoing() && ExecutionContext::isInteractive()) {
        PipelineEvaluationRequest request(ExecutionContext::current().ui().datasetContainer().currentAnimationTime());
        for(ModificationNode* node : nodes()) {
            resetSelection(node, node->evaluateInputSynchronous(request));
        }
    }
    GenericPropertyModifier::propertyChanged(field);
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void ManualSelectionModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    // Retrieve the selection stored in the modifier application.
    ElementSelectionSet* selectionSet = getSelectionSet(request.modificationNode(), false);
    if(!selectionSet)
        throw Exception(tr("No stored selection set available. Please reset the selection state."));

    if(subject()) {
        PropertyContainer* container = state.expectMutableLeafObject(subject());
        container->verifyIntegrity();

        PipelineStatus status = selectionSet->applySelection(
                container->createProperty(Property::GenericSelectionProperty),
                container->getOOMetaClass().isValidStandardPropertyId(Property::GenericIdentifierProperty) ?
                    container->getProperty(Property::GenericIdentifierProperty) : nullptr);

        state.setStatus(std::move(status));
    }
}

/******************************************************************************
* Returns the selection set object stored in the ModificationNode, or, if
* it does not exist, creates one.
******************************************************************************/
ElementSelectionSet* ManualSelectionModifier::getSelectionSet(ModificationNode* modApp, bool createIfNotExist)
{
    ManualSelectionModificationNode* myModApp = dynamic_object_cast<ManualSelectionModificationNode>(modApp);
    if(!myModApp)
        throw Exception(tr("Manual selection modifier is not associated with a ManualSelectionModificationNode."));

    ElementSelectionSet* selectionSet = myModApp->selectionSet();
    if(!selectionSet && createIfNotExist)
        myModApp->setSelectionSet(selectionSet = OORef<ElementSelectionSet>::create());

    return selectionSet;
}

/******************************************************************************
* Adopts the selection state from the modifier's input.
******************************************************************************/
void ManualSelectionModifier::resetSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->resetSelection(container);
    }
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ManualSelectionModifier::selectAll(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->selectAll(container);
    }
}

/******************************************************************************
* Deselects all elements.
******************************************************************************/
void ManualSelectionModifier::clearSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->clearSelection(container);
    }
}

/******************************************************************************
* Inverts the selection state of all elements.
******************************************************************************/
void ManualSelectionModifier::invertSelection(ModificationNode* modApp, const PipelineFlowState& state)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->invertSelection(container);
    }
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ManualSelectionModifier::toggleElementSelection(ModificationNode* modApp, const PipelineFlowState& state, size_t elementIndex)
{
    ElementSelectionSet* selectionSet = getSelectionSet(modApp, false);
    if(!selectionSet)
        throw Exception(tr("No stored selection set available. Please reset the selection state."));
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        selectionSet->toggleElement(container, elementIndex);
    }
}

/******************************************************************************
* Replaces the selection.
******************************************************************************/
void ManualSelectionModifier::setSelection(ModificationNode* modApp, const PipelineFlowState& state, const boost::dynamic_bitset<>& selection, ElementSelectionSet::SelectionMode mode)
{
    if(subject()) {
        const PropertyContainer* container = state.expectLeafObject(subject());
        getSelectionSet(modApp, true)->setSelection(container, selection, mode);
    }
}

}   // End of namespace
