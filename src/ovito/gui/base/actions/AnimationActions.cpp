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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Handles the ACTION_GOTO_START_OF_ANIMATION command.
******************************************************************************/
void ActionManager::on_AnimationGotoStart_triggered()
{
    if(AnimationSettings* anim = userInterface().datasetContainer().activeAnimationSettings())
        anim->jumpToAnimationStart();
}

/******************************************************************************
* Handles the ACTION_GOTO_END_OF_ANIMATION command.
******************************************************************************/
void ActionManager::on_AnimationGotoEnd_triggered()
{
    if(AnimationSettings* anim = userInterface().datasetContainer().activeAnimationSettings())
        anim->jumpToAnimationEnd();
}

/******************************************************************************
* Handles the ACTION_GOTO_PREVIOUS_FRAME command.
******************************************************************************/
void ActionManager::on_AnimationGotoPreviousFrame_triggered()
{
    if(AnimationSettings* anim = userInterface().datasetContainer().activeAnimationSettings())
        anim->jumpToPreviousFrame();
}

/******************************************************************************
* Handles the ACTION_GOTO_NEXT_FRAME command.
******************************************************************************/
void ActionManager::on_AnimationGotoNextFrame_triggered()
{
    if(AnimationSettings* anim = userInterface().datasetContainer().activeAnimationSettings())
        anim->jumpToNextFrame();
}

/******************************************************************************
* Handles the ACTION_START_ANIMATION_PLAYBACK command.
******************************************************************************/
void ActionManager::on_AnimationStartPlayback_triggered()
{
    if(!getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK)->isChecked())
        getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK)->trigger();
}

/******************************************************************************
* Handles the ACTION_STOP_ANIMATION_PLAYBACK command.
******************************************************************************/
void ActionManager::on_AnimationStopPlayback_triggered()
{
    if(getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK)->isChecked())
        getAction(ACTION_TOGGLE_ANIMATION_PLAYBACK)->trigger();
}

}   // End of namespace
