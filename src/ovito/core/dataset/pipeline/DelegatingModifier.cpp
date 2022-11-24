////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/core/Core.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "DelegatingModifier.h"
#include "AsynchronousDelegatingModifier.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ModifierDelegate);
DEFINE_PROPERTY_FIELD(ModifierDelegate, isEnabled);
DEFINE_PROPERTY_FIELD(ModifierDelegate, inputDataObject);
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, isEnabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ModifierDelegate, inputDataObject, "Data object");

IMPLEMENT_OVITO_CLASS(DelegatingModifier);
DEFINE_REFERENCE_FIELD(DelegatingModifier, delegate);

IMPLEMENT_OVITO_CLASS(MultiDelegatingModifier);
DEFINE_VECTOR_REFERENCE_FIELD(MultiDelegatingModifier, delegates);

/******************************************************************************
* Returns the modifier to which this delegate belongs.
******************************************************************************/
Modifier* ModifierDelegate::modifier() const
{
	Modifier* result = nullptr;
	visitDependents([&](RefMaker* dependent) {
		if(DelegatingModifier* modifier = dynamic_object_cast<DelegatingModifier>(dependent)) {
			if(modifier->delegate() == this) result = modifier;
		}
		else if(MultiDelegatingModifier* modifier = dynamic_object_cast<MultiDelegatingModifier>(dependent)) {
			if(modifier->delegates().contains(const_cast<ModifierDelegate*>(this))) result = modifier;
		}
		else if(AsynchronousDelegatingModifier* modifier = dynamic_object_cast<AsynchronousDelegatingModifier>(dependent)) {
			if(modifier->delegate() == this) result = modifier;
		}
	});
	return result;
}

#ifdef OVITO_QML_GUI
/******************************************************************************
* Asks the delegate whether it can operate on  the given input pipeline state.
******************************************************************************/
bool ModifierDelegate::canOperateOnInput(ModifierApplication* modApp) const 
{
	if(modApp) {
		const PipelineFlowState& input = modApp->evaluateInputSynchronous(dataset()->animationSettings()->time());
		return !getOOMetaClass().getApplicableObjects(input).empty();
	}
	return false;
}
#endif

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval DelegatingModifier::validityInterval(const ModifierEvaluationRequest& request) const
{
	TimeInterval iv = Modifier::validityInterval(request);

	if(delegate() && delegate()->isEnabled())
		iv.intersect(delegate()->validityInterval(request));

	return iv;
}

/******************************************************************************
* Creates a default delegate for this modifier.
******************************************************************************/
void DelegatingModifier::createDefaultModifierDelegate(const OvitoClass& delegateType, const QString& defaultDelegateTypeName, ObjectCreationParams params)
{
	OVITO_ASSERT(delegateType.isDerivedFrom(ModifierDelegate::OOClass()));

	// Find the delegate type that corresponds to the given name string.
	for(OvitoClassPtr clazz : PluginManager::instance().listClasses(delegateType)) {
		if(clazz->name() == defaultDelegateTypeName) {
			OORef<ModifierDelegate> delegate = static_object_cast<ModifierDelegate>(clazz->createInstance(params));
			setDelegate(delegate);
			break;
		}
	}
	OVITO_ASSERT_MSG(delegate(), "DelegatingModifier::createDefaultModifierDelegate", qPrintable(QStringLiteral("There is no delegate class named '%1' inheriting from %2.").arg(defaultDelegateTypeName).arg(delegateType.name())));
}

/******************************************************************************
* Asks the metaclass whether the modifier can be applied to the given input data.
******************************************************************************/
bool DelegatingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
	if(!ModifierClass::isApplicableTo(input)) return false;

	// Check if there is any modifier delegate that could handle the input data.
	for(const ModifierDelegate::OOMetaClass* clazz : PluginManager::instance().metaclassMembers<ModifierDelegate>(delegateMetaclass())) {
		if(clazz->getApplicableObjects(input).empty() == false)
			return true;
	}
	return false;
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void DelegatingModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	// Apply the modifier delegate to the input data.
	applyDelegate(request, state);
}

/******************************************************************************
* Lets the modifier's delegate operate on a pipeline flow state.
******************************************************************************/
void DelegatingModifier::applyDelegate(const ModifierEvaluationRequest& request, PipelineFlowState& state, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
	OVITO_ASSERT(!isUndoRecording());
	OVITO_ASSERT(request.modApp()->modifier() == this);

	if(!delegate() || !delegate()->isEnabled())
		return;

	// Skip function if not applicable.
	if(delegate()->getOOMetaClass().getApplicableObjects(state).empty())
		throw Exception(tr("The modifier's pipeline input does not contain the expected kind of data."));

	// Call the delegate function.
	PipelineStatus delegateStatus = delegate()->apply(request, state, state, additionalInputs);

	// Append status text and code returned by the delegate function to the status returned to our caller.
	PipelineStatus status = state.status();
	if(status.type() == PipelineStatus::Success || delegateStatus.type() == PipelineStatus::Error)
		status.setType(delegateStatus.type());
	if(!delegateStatus.text().isEmpty()) {
		if(!status.text().isEmpty())
			status.setText(status.text() + QStringLiteral("\n") + delegateStatus.text());
		else
			status.setText(delegateStatus.text());
	}
	if(delegateStatus.shortInfo().isValid()) {
		status.setShortInfo(delegateStatus.shortInfo());
	}
	state.setStatus(std::move(status));
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval MultiDelegatingModifier::validityInterval(const ModifierEvaluationRequest& request) const
{
	TimeInterval iv = Modifier::validityInterval(request);

	for(const ModifierDelegate* delegate : delegates()) {
		if(delegate && delegate->isEnabled()) {
			iv.intersect(delegate->validityInterval(request));
		}
	}

	return iv;
}

/******************************************************************************
* Creates the list of delegate objects for this modifier.
******************************************************************************/
void MultiDelegatingModifier::createModifierDelegates(const OvitoClass& delegateType, ObjectCreationParams params)
{
	OVITO_ASSERT(delegateType.isDerivedFrom(ModifierDelegate::OOClass()));

	// Generate the list of delegate objects.
	if(delegates().empty()) {
		for(OvitoClassPtr clazz : PluginManager::instance().listClasses(delegateType)) {
			_delegates.push_back(this, PROPERTY_FIELD(delegates), static_object_cast<ModifierDelegate>(clazz->createInstance(params)));
		}
	}
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool MultiDelegatingModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
	if(!ModifierClass::isApplicableTo(input)) return false;

	// Check if there is any modifier delegate that could handle the input data.
	for(const ModifierDelegate::OOMetaClass* clazz : PluginManager::instance().metaclassMembers<ModifierDelegate>(delegateMetaclass())) {
		if(clazz->getApplicableObjects(input).empty() == false)
			return true;
	}
	return false;
}

/******************************************************************************
* Modifies the input data synchronously.
******************************************************************************/
void MultiDelegatingModifier::evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
	// Apply all enabled modifier delegates to the input data.
	applyDelegates(request, state);
}

/******************************************************************************
* Lets the registered modifier delegates operate on a pipeline flow state.
******************************************************************************/
void MultiDelegatingModifier::applyDelegates(const ModifierEvaluationRequest& request, PipelineFlowState& state, const std::vector<std::reference_wrapper<const PipelineFlowState>>& additionalInputs)
{
	OVITO_ASSERT(!isUndoRecording());
	OVITO_ASSERT(request.modApp()->modifier() == this);

	// Make a shallow copy of the input pipeline state.
	PipelineFlowState inputState = state;

	for(ModifierDelegate* delegate : delegates()) {

		// Skip function if not applicable.
		if(!state.data() || !delegate || !delegate->isEnabled() || delegate->getOOMetaClass().getApplicableObjects(*state.data()).empty())
			continue;

		// Call the delegate function.
		PipelineStatus delegateStatus = delegate->apply(request, state, inputState, additionalInputs);

		// Append status text and code returned by the delegate function to the status returned to our caller.
		PipelineStatus status = state.status();
		if(status.type() == PipelineStatus::Success || delegateStatus.type() == PipelineStatus::Error)
			status.setType(delegateStatus.type());
		if(!delegateStatus.text().isEmpty()) {
			if(!status.text().isEmpty())
				status.setText(status.text() + QStringLiteral("\n") + delegateStatus.text());
			else
				status.setText(delegateStatus.text());
		}
		state.setStatus(std::move(status));
	}
}

}	// End of namespace
