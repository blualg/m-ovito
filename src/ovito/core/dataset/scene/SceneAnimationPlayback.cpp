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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/ScenePreparation.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/UserInterface.h>
#include "SceneAnimationPlayback.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SceneAnimationPlayback);
DEFINE_REFERENCE_FIELD(SceneAnimationPlayback, scene);

/******************************************************************************
* Constructor.
******************************************************************************/
SceneAnimationPlayback::SceneAnimationPlayback(UserInterface& userInterface) : _userInterface(userInterface)
{
    // This facility requires a Qt event loop.
    Application::instance()->createQtApplication(false);
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
            // Take time as we start to prepare and render the current frame.
            _frameRenderingTimer.start();

            // Wait for the scene to be fully prepared and rendered in all viewports, then schedule the next animation frame.
            _numPendingViewportWindows = 0;

            // Visit all viewports that display the current scene.
            scene()->visitDependents([&](RefMaker* dependent) {
                if(Viewport* viewport = dynamic_object_cast<Viewport>(dependent)) {
                    // Visit all windows that are associated with the viewport.
                    viewport->visitDependents([&](RefMaker* dependent) {
                        if(ViewportWindow* window = dynamic_object_cast<ViewportWindow>(dependent)) {
                            if(window->viewport() == viewport && window->isVisible()) {
                                _numPendingViewportWindows++;
                                connect(window, &ViewportWindow::frameRenderComplete, this, &SceneAnimationPlayback::viewportWindowComplete);
                                connect(window, &ViewportWindow::viewportWindowHidden, this, &SceneAnimationPlayback::viewportWindowComplete);
                            }
                        }
                    });
                }
            });

            // Just in case there are zero viewport windows to wait for.
            if(_numPendingViewportWindows == 0)
                scheduleNextAnimationFrame();
        }
    })) {
        // Stop playback on error during time change.
        stopAnimationPlayback();
    }
}

/******************************************************************************
* Receives notification from a ViewportWindow that scene rendering is complete.
******************************************************************************/
void SceneAnimationPlayback::viewportWindowComplete()
{
    // Stop receiving further notifications from this window.
    ViewportWindow* window = qobject_cast<ViewportWindow*>(sender());
    OVITO_ASSERT(window);
    window->disconnect(this);

    // We have may stopped waiting for the viewports in the meantime.
    if(_numPendingViewportWindows == 0)
        return;

    // Check if the last viewport window we have been waiting for.
    if(--_numPendingViewportWindows == 0)
        scheduleNextAnimationFrame();
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

    QObject::timerEvent(event);
}

}   // End of namespace
