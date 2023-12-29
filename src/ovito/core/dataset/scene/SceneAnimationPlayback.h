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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include "ScenePreparation.h"

namespace Ovito {

/**
 * \brief This object requests the next frames during animation playback in the interactive viewports.
 */
class OVITO_CORE_EXPORT SceneAnimationPlayback : public ScenePreparation
{
    OVITO_CLASS(SceneAnimationPlayback)
    Q_OBJECT

public:

    /// Constructor.
    explicit SceneAnimationPlayback(UserInterface& userInterface);

    /// Returns whether the animation is currently being played back in the viewports.
    bool isPlaybackActive() const { return _activePlaybackRate != 0; }

public Q_SLOTS:

    /// Starts playback of the animation.
    void startAnimationPlayback(Scene* scene, FloatType playbackRate = FloatType(1));

    /// Stops playback of the animation.
    void stopAnimationPlayback();

Q_SIGNALS:

    /// This signal is emitted when the animation playback is started or stopped.
    void playbackChanged(bool active);

private Q_SLOTS:

    /// Starts a timer to show the next animation frame.
    void scheduleNextAnimationFrame();

protected:

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// Jumps to the given animation frame, then schedules the next frame as soon as the scene was completely shown.
    void continuePlaybackAtFrame(int frame);

    /// Indicates that the animation is currently being played back in the interactive viewports.
    FloatType _activePlaybackRate = 0;

    /// Measures how long it took to load, compute, and render the current animation frame.
    QElapsedTimer _frameRenderingTimer;

    /// Timer for scheduling the next animation frame.
    QBasicTimer _nextFrameTimer;
};

}   // End of namespace
