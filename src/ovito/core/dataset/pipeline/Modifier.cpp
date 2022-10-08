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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(Modifier);
DEFINE_PROPERTY_FIELD(Modifier, isEnabled);
DEFINE_PROPERTY_FIELD(Modifier, title);
SET_PROPERTY_FIELD_LABEL(Modifier, isEnabled, "Enabled");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, isEnabled, ReferenceEvent::TargetEnabledOrDisabled);
SET_PROPERTY_FIELD_LABEL(Modifier, title, "Name");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
Modifier::Modifier(ObjectCreationParams params) : RefTarget(params),
	_isEnabled(true)
{
}

/******************************************************************************
* Determines the time interval over which a computed pipeline state will remain valid.
******************************************************************************/
TimeInterval Modifier::validityInterval(const ModifierEvaluationRequest& request) const
{
	return TimeInterval::infinite();
}

/******************************************************************************
* Create a new modifier application that refers to this modifier instance.
******************************************************************************/
OORef<ModifierApplication> Modifier::createModifierApplication()
{
	// Look which ModifierApplication class has been registered for this Modifier class.
	for(OvitoClassPtr clazz = &getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
		if(OvitoClassPtr modAppClass = ModifierApplication::registry().getModAppClass(clazz)) {
			if(!modAppClass->isDerivedFrom(ModifierApplication::OOClass()))
				throwException(tr("The modifier application class %1 assigned to the Modifier-derived class %2 is not derived from ModifierApplication.").arg(modAppClass->name(), clazz->name()));
#ifdef OVITO_DEBUG
			for(OvitoClassPtr superClazz = clazz->superClass(); superClazz != nullptr; superClazz = superClazz->superClass()) {
				if(OvitoClassPtr modAppSuperClass = ModifierApplication::registry().getModAppClass(superClazz)) {
					if(!modAppClass->isDerivedFrom(*modAppSuperClass))
						throwException(tr("The modifier application class %1 assigned to the Modifier-derived class %2 is not derived from the ModifierApplication specialization %3.").arg(modAppClass->name(), clazz->name(), modAppSuperClass->name()));
				}
			}
#endif
			return static_object_cast<ModifierApplication>(modAppClass->createInstance(dataset()));
		}
	}
	return OORef<ModifierApplication>::create(dataset());
}

/******************************************************************************
* Returns the number of animation frames this modifier provides.
******************************************************************************/
int Modifier::numberOfOutputFrames(ModifierApplication* modApp) const 
{ 
	OVITO_ASSERT(modApp);
	if(PipelineObject* input = modApp->input())
		return input->numberOfSourceFrames();
	return 1;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> Modifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
	PipelineFlowState output = input;
	if(output)
		evaluateSynchronous(request, output);
	return Future<PipelineFlowState>::createImmediate(std::move(output));
}

/******************************************************************************
* Returns the list of applications associated with this modifier.
******************************************************************************/
QVector<ModifierApplication*> Modifier::modifierApplications() const
{
	QVector<ModifierApplication*> apps;
	visitDependents([&](RefMaker* dependent) {
        ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent);
		if(modApp != nullptr && modApp->modifier() == this)
			apps.push_back(modApp);
	});
	return apps;
}

/******************************************************************************
* Returns one of the applications of this modifier in a pipeline.
******************************************************************************/
ModifierApplication* Modifier::someModifierApplication() const
{
	ModifierApplication* result = nullptr;
	visitDependents([&](RefMaker* dependent) {
        ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent);
		if(modApp != nullptr && modApp->modifier() == this)
			result = modApp;
	});
	return result;
}

/******************************************************************************
* Returns the current status of the modifier's applications.
******************************************************************************/
PipelineStatus Modifier::globalStatus() const
{
	// Combine the status values of all ModifierApplications into a single status.
	PipelineStatus result;
	for(ModifierApplication* modApp : modifierApplications()) {
		PipelineStatus s = modApp->status();

		if(result.text().isEmpty())
			result.setText(s.text());
		else if(s.text() != result.text())
			result.setText(result.text() + QStringLiteral("\n") + s.text());

		if(s.type() == PipelineStatus::Error)
			result.setType(PipelineStatus::Error);
		else if(result.type() != PipelineStatus::Error && s.type() == PipelineStatus::Warning)
			result.setType(PipelineStatus::Warning);
	}
	return result;
}

}	// End of namespace
