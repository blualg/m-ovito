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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(Scene);
DEFINE_REFERENCE_FIELD(Scene, animationSettings);
DEFINE_REFERENCE_FIELD(Scene, selection);
DEFINE_PROPERTY_FIELD(Scene, orbitCenterMode);
DEFINE_PROPERTY_FIELD(Scene, userOrbitCenter);
SET_PROPERTY_FIELD_LABEL(Scene, animationSettings, "Animation Settings");
SET_PROPERTY_FIELD_LABEL(Scene, selection, "Selection");

/******************************************************************************
* Constructor.
******************************************************************************/
Scene::Scene(ObjectCreationParams params, AnimationSettings* animationSettings) : SceneNode(params),
	_orbitCenterMode(ORBIT_SELECTION_CENTER),
	_userOrbitCenter(Point3::Origin())
{
	setNodeName("Scene");
	setAnimationSettings(animationSettings);

	// The root node does not need a transformation controller.
	setTransformationController(nullptr);

	// Create child objects for animation settings and node selection set.
	if(params.createSubObjects()) {
		if(!this->animationSettings())
			setAnimationSettings(OORef<AnimationSettings>::create(params));
		setSelection(OORef<SelectionSet>::create(params));
	}
}

/******************************************************************************
* Searches the scene for a node with the given name.
******************************************************************************/
SceneNode* Scene::getNodeByName(const QString& nodeName) const
{
	SceneNode* result = nullptr;
	visitChildren([nodeName, &result](SceneNode* node) -> bool {
		if(node->nodeName() == nodeName) {
			result = node;
			return false;
		}
		return true;
	});
	return result;
}

/******************************************************************************
* Generates a name for a node that is unique throughout the scene.
******************************************************************************/
QString Scene::makeNameUnique(QString baseName) const
{
	// Remove any existing digits from end of base name.
	if(baseName.size() > 2 &&
		baseName.at(baseName.size()-1).isDigit() && baseName.at(baseName.size()-2).isDigit())
		baseName.chop(2);

	// Keep appending different numbers until we arrive at a unique name.
	for(int i = 1; ; i++) {
		QString newName = baseName + QString::number(i).rightJustified(2, '0');
		if(getNodeByName(newName) == nullptr)
			return newName;
	}
}

/******************************************************************************
* Is called when a RefTarget referenced by this object has generated an event.
******************************************************************************/
bool Scene::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
	if(event.type() == ReferenceEvent::AnimationFramesChanged && !isBeingLoaded()) {
		// Automatically adjust scene's animation interval to length of loaded source animations.
		if(animationSettings() && animationSettings()->autoAdjustInterval()) {
			UndoSuspender noUndo;
			animationSettings()->adjustAnimationInterval();
		}
	}
	else if(event.type() == ReferenceEvent::RequestGoToAnimationTime) {
		int frame = static_cast<const RequestGoToAnimationTimeEvent&>(event).time().frame();
		if(animationSettings() && frame >= animationSettings()->firstFrame() && frame <= animationSettings()->lastFrame())
			animationSettings()->setCurrentFrame(frame);
	}

	return SceneNode::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void Scene::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(selection)) {
		Q_EMIT selectionSetReplaced(selection());
	}
	else if(field == PROPERTY_FIELD(animationSettings)) {
		OVITO_ASSERT(oldTarget == nullptr || newTarget == nullptr); // Note: Replacing the animation settings of a scene is not yet supported.
	}
	SceneNode::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void Scene::propertyChanged(const PropertyFieldDescriptor* field)
{
	if(field == PROPERTY_FIELD(orbitCenterMode) || field == PROPERTY_FIELD(userOrbitCenter)) {
		Q_EMIT cameraOrbitCenterChanged();
	}
	SceneNode::propertyChanged(field);
}

}	// End of namespace
