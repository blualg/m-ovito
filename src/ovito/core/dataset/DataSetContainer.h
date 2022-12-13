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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/scene/SceneAnimationPlayback.h>
#include <ovito/core/oo/RefMaker.h>

namespace Ovito {

/**
 * \brief Manages the DataSet being edited.
 */
class OVITO_CORE_EXPORT DataSetContainer : public RefMaker
{
	OVITO_CLASS(DataSetContainer)

#ifdef OVITO_QML_GUI
	Q_PROPERTY(Ovito::DataSet* currentSet READ currentSet WRITE setCurrentSet NOTIFY dataSetChanged)
#endif

public:

	/// \brief Constructor.
	explicit DataSetContainer(TaskManager& taskManager, UserInterface& userInterface);

	/// \brief Destructor.
	virtual ~DataSetContainer();

	/// Returns the manager of asynchronous tasks associated with this container.
	TaskManager& taskManager() { return _taskManager; }

	/// Returns the abstract user interface this container is part of.
	UserInterface& userInterface() { return _userInterface; }

	/// Creates an empty dataset and makes it the current dataset.
	DataSet* newDataset();

	/// Loads the given session state file and makes it the current dataset.
	void loadDataset(const QString& filename);

	/// Returns the currently active scene.
	Scene* activeScene() const { return _activeScene; }

	/// Returns the currently active animation settings.
	AnimationSettings* activeAnimationSettings() const { return _activeAnimationSettings; }

	/// Returns the currently scene node selection set.
	SelectionSet* activeSelectionSet() const { return _activeSelectionSet; }

	/// Returns the currently active viewport.
	Viewport* activeViewport() const { return _activeViewport; }

	/// Returns the current time of the active animation settings object.
	AnimationTime currentAnimationTime() const { return activeAnimationSettings() ? activeAnimationSettings()->currentTime() : AnimationTime(0); }

	/// Returns the active animation frame interval.
	std::pair<int, int> currentAnimationInterval() const { return activeAnimationSettings() ? std::make_pair(activeAnimationSettings()->firstFrame(), activeAnimationSettings()->lastFrame()) : std::make_pair(0, 0); }

	/// Returns whether the animation is currently being played back in the interactive viewports.
	bool isPlaybackActive() const { return _animationPlayback && _animationPlayback->isPlaybackActive(); }

public Q_SLOTS:

	/// \brief Starts playback of the animation in the viewports.
	void startAnimationPlayback(FloatType playbackRate = FloatType(1)) { createAnimationPlayback()->startAnimationPlayback(activeScene(), playbackRate); }

	/// \brief Stops playback of the animation in the viewports.
	void stopAnimationPlayback() { if(_animationPlayback) _animationPlayback->stopAnimationPlayback(); }

	/// \brief Starts or stops animation playback in the viewports.
	void setAnimationPlayback(bool on);

Q_SIGNALS:

	/// Is emitted when a another dataset has become the active dataset.
	void dataSetChanged(DataSet* newDataSet);

	/// \brief Is emitted when nodes have been added or removed from the current selection set.
	/// \param selection The current selection set.
	/// \note This signal is NOT emitted when a node in the selection set has changed.
	/// \note In contrast to the selectionChangeComplete() signal this signal is emitted
	///       for every node that is added to or removed from the selection set. That is,
	///       a call to SelectionSet::addAll() for example will generate multiple selectionChanged()
	///       events but only a single selectionChangeComplete() event.
    void selectionChanged(SelectionSet* selection);

	/// \brief This signal is emitted after all changes to the selection set have been completed.
	/// \param selection The current selection set.
	/// \note This signal is NOT emitted when a node in the selection set has changed.
	/// \note In contrast to the selectionChange() signal this signal is emitted
	///       only once after the selection set has been changed. That is,
	///       a call to SelectionSet::addAll() for example will generate multiple selectionChanged()
	///       events but only a single selectionChangeComplete() event.
    void selectionChangeComplete(SelectionSet* selection);

	/// \brief This signal is emitted whenever the current selection set has been replaced by another one.
	/// \note This signal is NOT emitted when nodes are added or removed from the current selection set.
    void selectionSetReplaced(SelectionSet* newSelectionSet);

	/// \brief This signal is emitted whenever the current viewport configuration of current dataset has been replaced by a new one.
	/// \note This signal is NOT emitted when the parameters of the current viewport configuration change.
    void viewportConfigReplaced(ViewportConfiguration* newViewportConfiguration);

	/// \brief This signal is emitted when another viewport became active.
	void activeViewportChanged(Viewport* activeViewport);

	/// \brief This signal is emitted whenever the active scene of current dataset has been replaced by a new one.
	/// \note This signal is NOT emitted when the contents of the current scene change.
    void sceneReplaced(Scene* newScene);

	/// \brief This signal is emitted whenever the current animation settings of the current dataset have been replaced by new ones.
	/// \note This signal is NOT emitted when the parameters of the current animation settings object change.
    void animationSettingsReplaced(AnimationSettings* newAnimationSettings);

	/// \brief This signal is emitted whenever the current render settings of this dataset
	///        have been replaced by new ones.
	/// \note This signal is NOT emitted when parameters of the current render settings object change.
    void renderSettingsReplaced(RenderSettings* newRenderSettings);

	/// \brief This signal is emitted when the current animation frame has changed or if the current animation settings have been replaced.
    void currentFrameChanged(int newFrame);

	/// \brief This signal is emitted whenever the length of the active animation interval changes.
	void animationIntervalChanged(int firstFrame, int lastFrame);

	/// \brief This signal is emitted when the time to string conversion format has changed.
	void timeFormatChanged();

	/// \brief This signal is emitted whenever the file path of the active dataset changes.
	void filePathChanged(const QString& filePath);

	/// This signal is emitted when the animation playback is started or stopped.
	void playbackChanged(bool active);

protected:

	/// Is called when the value of a reference field of this RefMaker changes.
	virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

	/// Create the animation playback helper object on demand.
	SceneAnimationPlayback* createAnimationPlayback();

protected Q_SLOTS:

	/// This handler is called when another viewport configuration becomes the active one.
    void onViewportConfigReplaced(ViewportConfiguration* viewportConfig);

	/// This handler is called when another viewport becomes the active one.
	void onActiveViewportChanged(Viewport* activeViewport);

	/// This handler is called when another scene becomes the active one.
    void onSceneReplaced(Scene* newScene);

	/// This handler is called when another selection set becomes the active one.
    void onSelectionSetReplaced(SelectionSet* newSelectionSet);

	/// This handler is called when another animation settings object becomes the active one.
    void onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings);

private:

	/// The current dataset being edited by the user.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<DataSet>, currentSet, setCurrentSet, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE);

	/// The manager of asynchronous tasks associated with this container.
	TaskManager& _taskManager;

	/// The abstract user interface this container is part of.
	UserInterface& _userInterface;

	/// Reference to the currently active scene.
	OORef<Scene> _activeScene;

	/// Reference to the currently active animation settings.
	OORef<AnimationSettings> _activeAnimationSettings;

	/// Reference to the currently scene node selection set.
	OORef<SelectionSet> _activeSelectionSet;

	/// Reference to the currently selected viewport.
	OORef<Viewport> _activeViewport;

	/// Helper object responsible for playing back the frames of the animation in the interactive viewports.
	OORef<SceneAnimationPlayback> _animationPlayback;

	QMetaObject::Connection _selectionSetReplacedConnection;
	QMetaObject::Connection _selectionSetChangedConnection;
	QMetaObject::Connection _selectionSetChangeCompleteConnection;
	QMetaObject::Connection _viewportConfigReplacedConnection;
	QMetaObject::Connection _activeViewportChangedConnection;
	QMetaObject::Connection _sceneReplacedConnection;
	QMetaObject::Connection _renderSettingsReplacedConnection;
	QMetaObject::Connection _animationCurrentFrameChangedConnection;
	QMetaObject::Connection _animationIntervalChangedConnection;
	QMetaObject::Connection _timeFormatChangedConnection;
	QMetaObject::Connection _filePathChangedConnection;
};

}	// End of namespace
