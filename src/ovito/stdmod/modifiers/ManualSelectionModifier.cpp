////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>
#include "ManualSelectionModifier.h"

namespace Ovito::StdMod {

IMPLEMENT_OVITO_CLASS(ManualSelectionModifier);

IMPLEMENT_OVITO_CLASS(ManualSelectionModifierApplication);
SET_MODIFIER_APPLICATION_TYPE(ManualSelectionModifier, ManualSelectionModifierApplication);
DEFINE_REFERENCE_FIELD(ManualSelectionModifierApplication, selectionSet);
SET_PROPERTY_FIELD_LABEL(ManualSelectionModifierApplication, selectionSet, "Element selection set");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
ManualSelectionModifier::ManualSelectionModifier(ObjectCreationParams params) : GenericPropertyModifier(params)
{
	// Operate on particles by default.
	setDefaultSubject(QStringLiteral("Particles"), QStringLiteral("ParticlesObject"));
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void ManualSelectionModifier::initializeModifier(const ModifierInitializationRequest& request)
{
	Modifier::initializeModifier(request);

	// Take a snapshot of the existing selection state at the time the modifier is created.
	if(!getSelectionSet(request.modApp(), false)) {
		resetSelection(request.modApp(), request.modApp()->evaluateInputSynchronous(request));
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
		for(ModifierApplication* modApp : modifierApplications()) {
			resetSelection(modApp, modApp->evaluateInputSynchronous(request));
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
	ElementSelectionSet* selectionSet = getSelectionSet(request.modApp(), false);
	if(!selectionSet)
		throw Exception(tr("No stored selection set available. Please reset the selection state."));

	if(subject()) {
		PropertyContainer* container = state.expectMutableLeafObject(subject());
		container->verifyIntegrity();

		PipelineStatus status = selectionSet->applySelection(
				container->createProperty(PropertyObject::GenericSelectionProperty),
				container->getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericIdentifierProperty) ?
					container->getProperty(PropertyObject::GenericIdentifierProperty) : nullptr);

		state.setStatus(std::move(status));
	}
}

/******************************************************************************
* Returns the selection set object stored in the ModifierApplication, or, if
* it does not exist, creates one.
******************************************************************************/
ElementSelectionSet* ManualSelectionModifier::getSelectionSet(ModifierApplication* modApp, bool createIfNotExist)
{
	ManualSelectionModifierApplication* myModApp = dynamic_object_cast<ManualSelectionModifierApplication>(modApp);
	if(!myModApp)
		throw Exception(tr("Manual selection modifier is not associated with a ManualSelectionModifierApplication."));

	ElementSelectionSet* selectionSet = myModApp->selectionSet();
	if(!selectionSet && createIfNotExist)
		myModApp->setSelectionSet(selectionSet = OORef<ElementSelectionSet>::create());

	return selectionSet;
}

/******************************************************************************
* Adopts the selection state from the modifier's input.
******************************************************************************/
void ManualSelectionModifier::resetSelection(ModifierApplication* modApp, const PipelineFlowState& state)
{
	if(subject()) {
		const PropertyContainer* container = state.expectLeafObject(subject());
		getSelectionSet(modApp, true)->resetSelection(container);
	}
}

/******************************************************************************
* Selects all elements.
******************************************************************************/
void ManualSelectionModifier::selectAll(ModifierApplication* modApp, const PipelineFlowState& state)
{
	if(subject()) {
		const PropertyContainer* container = state.expectLeafObject(subject());
		getSelectionSet(modApp, true)->selectAll(container);
	}
}

/******************************************************************************
* Deselects all elements.
******************************************************************************/
void ManualSelectionModifier::clearSelection(ModifierApplication* modApp, const PipelineFlowState& state)
{
	if(subject()) {
		const PropertyContainer* container = state.expectLeafObject(subject());
		getSelectionSet(modApp, true)->clearSelection(container);
	}
}

/******************************************************************************
* Inverts the selection state of all elements.
******************************************************************************/
void ManualSelectionModifier::invertSelection(ModifierApplication* modApp, const PipelineFlowState& state)
{
	if(subject()) {
		const PropertyContainer* container = state.expectLeafObject(subject());
		getSelectionSet(modApp, true)->invertSelection(container);
	}
}

/******************************************************************************
* Toggles the selection state of a single element.
******************************************************************************/
void ManualSelectionModifier::toggleElementSelection(ModifierApplication* modApp, const PipelineFlowState& state, size_t elementIndex)
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
void ManualSelectionModifier::setSelection(ModifierApplication* modApp, const PipelineFlowState& state, const boost::dynamic_bitset<>& selection, ElementSelectionSet::SelectionMode mode)
{
	if(subject()) {
		const PropertyContainer* container = state.expectLeafObject(subject());
		getSelectionSet(modApp, true)->setSelection(container, selection, mode);
	}
}

}	// End of namespace
