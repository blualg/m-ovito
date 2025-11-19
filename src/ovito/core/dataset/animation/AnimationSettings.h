////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include "TimeInterval.h"
#include "AnimationFrameLabel.h"

namespace Ovito {

/**
 * \brief Stores the animation settings such as the animation length, current frame number, playback rate, etc.
 */
class OVITO_CORE_EXPORT AnimationSettings : public RefTarget
{
    /// Give this class its own metaclass.
    class AnimationSettingsClass : public RefTarget::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using RefTarget::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes are of the deserialization of a serialized property field that has been removed from the class.
        /// This is needed for backward compatibility with OVITO 3.7.
        virtual SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const override;
    };

    OVITO_CLASS_META(AnimationSettings, AnimationSettingsClass)

public:

    /// Returns the time that corresponds to the current frame at which the time slider is positioned.
    AnimationTime currentTime() const { return AnimationTime::fromFrame(currentFrame()); }

    /// Returns the labels assigned by the trajectory file loader to individual animation frames.
    const QMap<int, AnimationFrameLabel>& frameLabels() const { return _frameLabels; }

    /// Converts a time value to its string representation, i.e., a human-readable representation of the time value (usually the animation frame number).
    QString timeToString(AnimationTime time);

    /// Converts a string entered by a user to a time value. Throws an exception when a parsing error occurs.
    AnimationTime stringToTime(const QString& stringValue);

    /// Returns whether the current animation interval consists of a one static frame only.
    bool isSingleFrame() const { return firstFrame() >= lastFrame(); }

    /// Returns the number of frames in the current animation interval.
    int numberOfFrames() const { return lastFrame() - firstFrame() + 1; }

public Q_SLOTS:

    /// Sets the current animation time to the start of the animation interval.
    void jumpToAnimationStart();

    /// Sets the current animation time to the end of the animation interval.
    void jumpToAnimationEnd();

    /// Jumps to the next animation frame.
    void jumpToNextFrame();

    /// Jumps to the previous animation frame.
    void jumpToPreviousFrame();

    /// Sets whether the animation is played back in a loop in the interactive viewports.
    void setLoopPlaybackSlot(bool loop) { setLoopPlayback(loop); }

    /// Recalculates the length of the animation interval to accommodate all loaded source animations in the scene.
    void adjustAnimationInterval();

    /// Rebuilds the list of human-readable labels assigned to animation frames.
    void updateAnimationFrameLabels();

protected:

    /// Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Saves the class' contents to an output stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from an input stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

    /// Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

private:

    /// The current animation time.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{0}, currentFrame, setCurrentFrame, PROPERTY_FIELD_NO_UNDO);

    /// The start of the animation interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, firstFrame, setFirstFrame);

    /// The end of the animation interval.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, lastFrame, setLastFrame);

    /// The playback speed of the animation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{10}, framesPerSecond, setFramesPerSecond, PROPERTY_FIELD_MEMORIZE);

    /// The playback speed factor that is used for animation playback in the viewport.
    /// A value greater than 1 means that the animation is played at a speed higher
    /// than realtime.
    /// A value smaller than -1 that the animation is played at a speed lower than realtime.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, playbackSpeed, setPlaybackSpeed, PROPERTY_FIELD_MEMORIZE);

    /// Controls whether the animation is played back in a loop in the interactive viewports.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, loopPlayback, setLoopPlayback, PROPERTY_FIELD_MEMORIZE);

    /// Specifies the number of frames to skip when playing back the animation in the interactive viewports.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{1}, playbackEveryNthFrame, setPlaybackEveryNthFrame);

    /// Controls whether the animation interval is automatically adjusted to accommodate all loaded
    /// trajectories in the scene.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{true}, autoAdjustInterval, setAutoAdjustInterval);

    /// Controls whether the timeline preferentially shows simulation time values (if available) or zero-based trajectory frame numbers.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, preferSimulationTimeDisplay, setPreferSimulationTimeDisplay, PROPERTY_FIELD_MEMORIZE);

    /// List of labels assigned to animation frames.
    QMap<int, AnimationFrameLabel> _frameLabels;
};

}   // End of namespace
