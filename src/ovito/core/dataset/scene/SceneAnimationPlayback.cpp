////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/PipelineSceneNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>
#include "SceneAnimationPlayback.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(SceneAnimationPlayback);

/******************************************************************************
* Constructor.
******************************************************************************/
SceneAnimationPlayback::SceneAnimationPlayback(UserInterface& userInterface) : ScenePreparation(userInterface)
{
    connect(this, &ScenePreparation::scenePreparationFinished, this, &SceneAnimationPlayback::scheduleNextAnimationFrame);
}

/******************************************************************************
* Starts playback of the animation in the viewports.
******************************************************************************/
void SceneAnimationPlayback::startAnimationPlayback(Scene* scene, FloatType playbackRate)
{
    // Do not start playback if animation interval does not contain more than a single frame.
    if(!scene || playbackRate == 0.0f || !scene->animationSettings() || scene->animationSettings()->isSingleFrame()) {
        scene = nullptr;
        playbackRate = 0.0f;
    }

    if(scene && playbackRate != 0 && !isPlaybackActive()) {
        _activePlaybackRate = playbackRate;

        // Commence the scene preparation.
        setScene(scene);

        // While animation playback is active, display only the final outcome of the pipeline evaluation.
        // Do not re-render viewports when intermediate results become available.
        userInterface().suspendPreliminaryViewportUpdates();

        Q_EMIT playbackChanged(true);

        AnimationSettings* animSettings = scene->animationSettings();
        if(_activePlaybackRate > 0) {
            if(animSettings->currentFrame() < animSettings->lastFrame())
                scheduleNextAnimationFrame();
            else
                continuePlaybackAtFrame(animSettings->firstFrame());
        }
        else {
            if(animSettings->currentFrame() > animSettings->firstFrame())
                scheduleNextAnimationFrame();
            else
                continuePlaybackAtFrame(animSettings->lastFrame());
        }
    }
    else {
        stopAnimationPlayback();
    }
}

/******************************************************************************
* Jumps to the given animation frame, then schedules the next frame as soon as
* the scene was completely shown.
******************************************************************************/
void SceneAnimationPlayback::continuePlaybackAtFrame(int frame)
{
    OVITO_ASSERT(scene());
    OVITO_ASSERT(scene()->animationSettings());

    // The following requires a valid execution context.
    if(!userInterface().handleExceptions([&] {

        // Move time slider to the next animation frame and request preparation of the scene for display.
        scene()->animationSettings()->setCurrentFrame(frame);

        if(isPlaybackActive()) {
            // Take time as we start to render the current frame.
            _frameRenderingTimer.start();

            // Once the scene is ready, schedule the next animation frame.
            restartPreparation();
        }
    })) {
        // Stop playback on error during time change.
        stopAnimationPlayback();
    }
}

/******************************************************************************
* Starts a timer to show the next animation frame.
******************************************************************************/
void SceneAnimationPlayback::scheduleNextAnimationFrame()
{
    if(!isPlaybackActive())
        return;

    if(!scene() || !scene()->animationSettings()) {
        stopAnimationPlayback();
        return;
    }

    if(!_nextFrameTimer.isActive()) {
        int playbackSpeed = scene()->animationSettings()->playbackSpeed();
        int timerSpeed = 1000 / std::abs(_activePlaybackRate);
        if(playbackSpeed > 1) timerSpeed /= playbackSpeed;
        else if(playbackSpeed < -1) timerSpeed *= -playbackSpeed;

        // Time period of a single animation frame.
        FloatType fps = scene()->animationSettings()->framesPerSecond();
        int msec = fps > 0.0f ? (int)(timerSpeed / fps) : 0;

        // Take into account how long it took to render the previous frame.
        if(_frameRenderingTimer.isValid()) {
            msec -= _frameRenderingTimer.elapsed();
        }

        _nextFrameTimer.start(std::max(msec, 0), Qt::CoarseTimer, this);
    }
}

/******************************************************************************
* Stops playback of the animation in the viewports.
******************************************************************************/
void SceneAnimationPlayback::stopAnimationPlayback()
{
    setScene(nullptr);
    _nextFrameTimer.stop();
    if(isPlaybackActive()) {
        _activePlaybackRate = 0;
        _frameRenderingTimer.invalidate();
        userInterface().resumePreliminaryViewportUpdates();
        Q_EMIT playbackChanged(false);
    }
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void SceneAnimationPlayback::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _nextFrameTimer.timerId()) {
        _nextFrameTimer.stop();

        // Check if the animation playback has been deactivated in the meantime.
        if(!isPlaybackActive())
            return;

        if(!scene() || !scene()->animationSettings()) {
            stopAnimationPlayback();
            return;
        }
        AnimationSettings* anim = scene()->animationSettings();

        // Add +/-N frames to current time.
        int newFrame = anim->currentFrame() + (_activePlaybackRate > 0 ? 1 : -1) * std::max(1, anim->playbackEveryNthFrame());

        // Loop back to first frame if end has been reached.
        if(newFrame > anim->lastFrame()) {
            if(anim->loopPlayback() && !anim->isSingleFrame()) {
                newFrame = anim->firstFrame();
            }
            else {
                newFrame = anim->lastFrame();
                userInterface().handleExceptions([&] {
                    scene()->animationSettings()->setCurrentFrame(newFrame);
                });
                stopAnimationPlayback();
            }
        }
        else if(newFrame < anim->firstFrame()) {
            if(anim->loopPlayback() && !anim->isSingleFrame()) {
                newFrame = anim->lastFrame();
            }
            else {
                newFrame = anim->firstFrame();
                userInterface().handleExceptions([&] {
                    scene()->animationSettings()->setCurrentFrame(newFrame);
                });
                stopAnimationPlayback();
            }
        }

        // Set new frame and continue playing.
        if(isPlaybackActive())
            continuePlaybackAtFrame(newFrame);
    }
    ScenePreparation::timerEvent(event);
}

}   // End of namespace
