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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluation.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/utilities/concurrent/TaskWatcher.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief This represents an entire scene node tree.
 */
class OVITO_CORE_EXPORT Scene : public SceneNode
{
	OVITO_CLASS(Scene)
	Q_CLASSINFO("ClassNameAlias", "RootSceneNode");	// For backward compatibility with OVITO 3.7.11.

public:

	enum OrbitCenterMode {
		ORBIT_SELECTION_CENTER,		///< Take the center of mass of the current selection as orbit center.
									///< If there is no selection, use scene bounding box.
		ORBIT_USER_DEFINED			///< Use the orbit center set by the user.
	};
	Q_ENUM(OrbitCenterMode);

public:

	/// \brief Creates an empty scene.
	Q_INVOKABLE Scene(ObjectCreationParams params, AnimationSettings* animationSettings = nullptr);

	/// \brief Destructor.
	virtual ~Scene();

	/// \brief Searches the scene for a node with the given name.
	/// \param nodeName The name to look for.
	/// \return The scene node or \c nullptr, if there is no node with the given name.
	SceneNode* getNodeByName(const QString& nodeName) const;

	/// \brief Generates a name for a node that is unique throughout the scene.
	/// \param baseName A base name that will be made unique by appending a number.
	/// \return The generated unique name.
	QString makeNameUnique(QString baseName) const;

	/// \brief Returns the bounding box of the scene.
	/// \param time The time at which the bounding box should be computed.
	/// \return An world axis-aligned box that contains the bounding boxes of all child nodes.
	virtual Box3 localBoundingBox(AnimationTime time, TimeInterval& validity) const override { return Box3(); }

	/// \brief Returns whether this is the root scene node.
	virtual bool isRootNode() const override { return true; }

	/// \brief Deletes all child nodes of the scene.
	void clear() {
		while(!children().empty())
			children().back()->deleteNode();
	}

	/// \brief Returns a future that is triggered once all data pipelines in the scene
	///        have been completely evaluated at the current animation time.
	SharedFuture<> whenReady();

	/// \brief Returns the current location around which the viewport camera orbits.
	Point3 orbitCenter(Viewport* vp) const;

Q_SIGNALS:

	/// \brief Is emitted whenever the scene is being made ready for rendering after it was changed in some way.
	void scenePreparationStarted();

	/// \brief Is emitted whenever the scene became ready for rendering.
	void scenePreparationFinished();

	/// This signal is emitted when the camera orbit center has been moved to a different location.
	void cameraOrbitCenterChanged();

	/// \brief This signal is emitted whenever the current node selection set of this scene has been replaced by another one.
	/// \note This signal is NOT emitted when nodes are added or removed from the scene.
    void selectionSetReplaced(SelectionSet* newSelectionSet);

protected:

	/// Sends an event to all dependents of this RefTarget.
	virtual void notifyDependentsImpl(const ReferenceEvent& event) override;

	/// Is called when a RefTarget referenced by this object has generated an event.
	virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

	/// Is called when the value of a reference field of this RefMaker changes.
	virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

	/// Is called when the value of a property of this object has changed.
	virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private Q_SLOTS:

	/// Is called when the evaluation of a pipeline in the scene has finished.
	void pipelineEvaluationFinished();

	/// Is called whenver viewport updates are resumed.
	void onViewportUpdatesResumed();

private:

	/// Requests the (re-)evaluation of all data pipelines in the current scene.
	Q_INVOKABLE void makeReady(bool forceReevaluation);

	/// Requests the (re-)evaluation of all data pipelines next time execution returns to the event loop.
	void makeReadyLater(bool forceReevaluation) { 
		QMetaObject::invokeMethod(this, "makeReady", Qt::QueuedConnection, Q_ARG(bool, forceReevaluation));
	}

private:

	/// The animation timeline of this scene.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<AnimationSettings>, animationSettings, setAnimationSettings, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

	/// The current object selection set.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SelectionSet>, selection, setSelection, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

	/// Controls around which point the viewport camera should orbit.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(OrbitCenterMode, orbitCenterMode, setOrbitCenterMode, PROPERTY_FIELD_NO_UNDO);

	/// Position of the orbiting center picked by the user.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Point3, userOrbitCenter, setUserOrbitCenter, PROPERTY_FIELD_NO_UNDO);

	/// The promise to make the scene ready.
	Promise<> _sceneReadyPromise;

	/// The last animation frame at which the scene was made ready.
	int _sceneReadyFrame;

	/// The current pipeline evaluation that is in progress.
	PipelineEvaluationFuture _pipelineEvaluation;

	/// The watcher object that is used to monitor the evaluation of data pipelines in the scene.
	TaskWatcher _pipelineEvaluationWatcher;

	/// Qt connection to the ViewportConfiguration::viewportUpdateResumed() signal.
	QMetaObject::Connection _viewportUpdateResumedConnection;
};

}	// End of namespace
