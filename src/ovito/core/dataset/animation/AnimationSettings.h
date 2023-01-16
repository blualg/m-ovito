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
#include "TimeInterval.h"

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
        virtual SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr overrideFieldDeserialization(const SerializedClassInfo::PropertyFieldInfo& field) const override;
    };

    OVITO_CLASS_META(AnimationSettings, AnimationSettingsClass)

public:

    /// \brief Constructor that initializes the object with default values.
    /// \param dataset The context dataset.
    Q_INVOKABLE AnimationSettings(ObjectCreationParams params);

    /// \brief Returns the time that corresponds to the current frame at which the time slider is positioned. 
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

    /// Returns whether the current animation interval consists of a one static frame only.
    bool isSingleFrame() const { return firstFrame() >= lastFrame(); }

    /// Returns the number of frames in the current animation interval.
    int numberOfFrames() const { return lastFrame() - firstFrame() + 1; }

public Q_SLOTS:

    /// \brief Sets the current animation time to the start of the animation interval.
    void jumpToAnimationStart();

    /// \brief Sets the current animation time to the end of the animation interval.
    void jumpToAnimationEnd();

    /// \brief Jumps to the next animation frame.
    void jumpToNextFrame();

    /// \brief Jumps to the previous animation frame.
    void jumpToPreviousFrame();

    /// Sets whether the animation is played back in a loop in the interactive viewports.
    void setLoopPlaybackSlot(bool loop) { setLoopPlayback(loop); }

    /// Recalculates the length of the animation interval to accommodate all loaded source animations
    /// in the scene.
    void adjustAnimationInterval();

Q_SIGNALS:

    /// This signal is emitted when the current animation frame has changed.
    void currentFrameChanged(int frame);

    /// This signal is emitted when the active animation interval has changed.
    void intervalChanged(int firstFrame, int lastFrame);

    /// This signal is emitted when the animation speed has changed.
    void speedChanged();

    /// This signal is emitted when the time to string conversion format has changed.
    void timeFormatChanged();

protected:

    /// \brief Is called when the value of a non-animatable property field of this RefMaker has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// \brief Saves the class' contents to an output stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// \brief Loads the class' contents from an input stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// \brief Creates a copy of this object.
    virtual OORef<RefTarget> clone(bool deepCopy, CloneHelper& cloneHelper) const override;

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
    /// trajectories in the scene.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, autoAdjustInterval, setAutoAdjustInterval);

    /// List of names assigned to animation frames.
    QMap<int,QString> _namedFrames;
};

}   // End of namespace
