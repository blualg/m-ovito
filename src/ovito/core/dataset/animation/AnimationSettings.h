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
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include "TimeInterval.h"

namespace Ovito {

/**
 * \brief Stores the animation settings such as the animation length, current frame number, playback rate, etc.
 */
class OVITO_CORE_EXPORT AnimationSettings : public RefTarget
{
	OVITO_CLASS(AnimationSettings)

public:

	/// \brief Constructor that initializes the object with default values.
	/// \param dataset The context dataset.
	Q_INVOKABLE AnimationSettings(ObjectCreationParams params);

	/// \brief Returns the animation time that corresponds to the current animation frame at which the time slider is positioned. 
	AnimationTime currentTime() const { return AnimationTime::fromFrame(currentFrame()); }

    /// \brief Returns the list of names assigned to animation frames.
    const QMap<int,QString>& namedFrames() const { return _namedFrames; }

	/// \brief Converts a time value to its string representation.
	/// \param time Some animation time value.
	/// \return A human-readable representation of the time value (usually the animation frame number).
	QString timeToString(AnimationTime time);

	/// \brief Converts a string entered by a user to a time value.
	/// \param stringValue The string representation of a time value (typically the animation frame number).
	/// \return The animation time.
	/// \throw Exception when a parsing error occurs.
	AnimationTime stringToTime(const QString& stringValue);

	/// \brief Indicates that the animation time has recently been changed via setTime(), and the scene
	///        is still being prepared for displaying the new frame.
	bool isTimeChanging() const { return _isTimeChanging; }

	/// Returns whether the animation is currently being played back in the viewports.
	bool isPlaybackActive() const { return _activePlaybackRate != 0; }

	/// Returns whether the current animation interval consists of a one static frame only.
	bool isSingleFrame() const { return firstFrame() >= lastFrame(); }

	/// \brief Suspends updates of the viewports whenever preliminary data pipeline results are available.
	void suspendPreliminaryViewportUpdates() { _preliminaryViewportUpdatesSuspendCount++; }

	/// \brief Resumes updates of the viewports whenever preliminary data pipeline results are available.
	void resumePreliminaryViewportUpdates() {
		OVITO_ASSERT_MSG(_preliminaryViewportUpdatesSuspendCount > 0, "AnimationSettings::resumePreliminaryViewportUpdates()", "resumePreliminaryViewportUpdates() has been called more often than suspendPreliminaryViewportUpdates().");
		_preliminaryViewportUpdatesSuspendCount--;
	}

	/// Returns whether viewports should be updated whenever preliminary pipeline results are available.  
	bool arePreliminaryViewportUpdatesSuspended() const { return isPlaybackActive() || _preliminaryViewportUpdatesSuspendCount > 0; }

public Q_SLOTS:

	/// \brief Sets the current animation time to the start of the animation interval.
	void jumpToAnimationStart();

	/// \brief Sets the current animation time to the end of the animation interval.
	void jumpToAnimationEnd();

	/// \brief Jumps to the next animation frame.
	void jumpToNextFrame();

	/// \brief Jumps to the previous animation frame.
	void jumpToPreviousFrame();

	/// \brief Starts playback of the animation in the viewports.
	void startAnimationPlayback(FloatType playbackRate = FloatType(1));

	/// \brief Stops playback of the animation in the viewports.
	void stopAnimationPlayback();

	/// \brief Starts or stops animation playback in the viewports.
	void setAnimationPlayback(bool on);

	/// Sets whether the animation is played back in a loop in the interactive viewports.
    void setLoopPlaybackSlot(bool loop) { setLoopPlayback(loop); }

	/// Recalculates the length of the animation interval to accommodate all loaded source animations
	/// in the scene.
	void adjustAnimationInterval();

Q_SIGNALS:

	/// This signal is emitted when the current animation frame has changed.
	void currentFrameChanged(int frame);

	/// This signal is emitted when the scene becomes ready after the current animation frame has changed.
	void currentFrameChangeComplete();

	/// This signal is emitted when the active animation interval has changed.
	void intervalChanged(int firstFrame, int lastFrame);

	/// This signal is emitted when the animation speed has changed.
	void speedChanged();

	/// This signal is emitted when the time to string conversion format has changed.
	void timeFormatChanged();

	/// This signal is emitted when the animation playback is started or stopped.
	void playbackChanged(bool active);

private Q_SLOTS:

	/// \brief Is called when the current animation time has changed.
	void onCurrentFrameChanged();

	/// \brief Timer callback used during animation playback.
	void onPlaybackTimer();

	/// Starts a timer to show the next animation frame.
	void scheduleNextAnimationFrame();

protected:

	/// \brief Is called when the value of a non-animatable property field of this RefMaker has changed.
	virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

	/// \brief Saves the class' contents to an output stream.
	virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

	/// \brief Loads the class' contents from an input stream.
	virtual void loadFromStream(ObjectLoadStream& stream) override;

	/// \brief Creates a copy of this object.
	virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

	/// Jumps to the given animation frame, then schedules the next frame as soon as the scene was completely shown.
	void continuePlaybackAtFrame(int frame);

private:

	/// The current animation time.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, currentFrame, setCurrentFrame, PROPERTY_FIELD_NO_UNDO);

	/// The start of the animation interval.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, firstFrame, setFirstFrame);

	/// The end of the animation interval.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, lastFrame, setLastFrame);

	/// The playback speed of the animation.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType, framesPerSecond, setFramesPerSecond, PROPERTY_FIELD_MEMORIZE);

	/// The playback speed factor that is used for animation playback in the viewport.
	/// A value greater than 1 means that the animation is played at a speed higher
	/// than realtime.
	/// A value smaller than -1 that the animation is played at a speed lower than realtime.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int, playbackSpeed, setPlaybackSpeed, PROPERTY_FIELD_MEMORIZE);

	/// Controls whether the animation is played back in a loop in the interactive viewports.
	DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, loopPlayback, setLoopPlayback, PROPERTY_FIELD_MEMORIZE);

	/// Specifies the number of frames to skip when playing back the animation in the interactive viewports.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(int, playbackEveryNthFrame, setPlaybackEveryNthFrame);

	/// Controls whether the animation interval is automatically adjusted to accommodate all loaded
	/// source animations in the scene.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, autoAdjustInterval, setAutoAdjustInterval);

    /// List of names assigned to animation frames.
    QMap<int,QString> _namedFrames;

	/// Indicates that the animation has been changed, and the scene is still being prepared for display of the new frame.
	bool _isTimeChanging = false;

	/// Counts the number of times preliminary viewport updates have been suspended.
	int _preliminaryViewportUpdatesSuspendCount = 0;

	/// Indicates that the animation is currently being played back in the viewports.
	FloatType _activePlaybackRate = 0;

	/// Task that prepares the scene after an animation time change.
	SharedFuture<> _sceneReadyFuture;

	/// Measures how long it took to load, compute, and render the current animation frame.
	QElapsedTimer _frameRenderingTimer;
};

/**
 * \brief A helper class that suspends preliminary viewport updates while it exists.
 *
 * \sa AnimationSettings
 */
class OVITO_CORE_EXPORT PreliminaryViewportUpdatesSuspender
{
public:

	/// Suspends the automatic generation of animation keys by calling AnimationSettings::suspendPreliminaryViewportUpdates().
	/// \param animSettings The animation settings object.
	PreliminaryViewportUpdatesSuspender(AnimationSettings* animSettings) : _animSettings(animSettings) {
		animSettings->suspendPreliminaryViewportUpdates();
	}

	/// Resumes the automatic generation of animation keys by calling AnimationSettings::resumePreliminaryViewportUpdates().
	~PreliminaryViewportUpdatesSuspender() {
		if(_animSettings) _animSettings->resumePreliminaryViewportUpdates();
	}

private:

	QPointer<AnimationSettings> _animSettings;
};

}	// End of namespace
