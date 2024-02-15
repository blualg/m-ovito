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

#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Handles the ACTION_VIEWPORT_MAXIMIZE command.
******************************************************************************/
void ActionManager::on_ViewportMaximize_triggered()
{
    userInterface().handleExceptions([&] {
        ViewportConfiguration* vpconf = dataset()->viewportConfig();
        if(vpconf->maximizedViewport()) {
            vpconf->setMaximizedViewport(nullptr);
        }
        else if(vpconf->activeViewport()) {
            vpconf->setMaximizedViewport(vpconf->activeViewport());
        }
        // Remember which viewport was maximized across program sessions.
        // The same viewport will be maximized next time OVITO is started.
        ViewportSettings::getSettings().setDefaultMaximizedViewportType(vpconf->maximizedViewport() ? vpconf->maximizedViewport()->viewType() : Viewport::VIEW_NONE);
        ViewportSettings::getSettings().save();
    });
}

/******************************************************************************
* Handles the ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS command.
******************************************************************************/
void ActionManager::on_ViewportZoomSceneExtents_triggered()
{
    ViewportConfiguration* vpconf = dataset()->viewportConfig();

    userInterface().handleExceptions([&] {
        if(vpconf->activeViewport() && !QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            vpconf->activeViewport()->notifyDependents(Viewport::ZoomToSceneExtentsRequested);
        }
        else {
            for(Viewport* vp : vpconf->viewports())
                vp->notifyDependents(Viewport::ZoomToSceneExtentsRequested);
        }
    });
}

/******************************************************************************
* Handles the ACTION_VIEWPORT_ZOOM_SCENE_EXTENTS_ALL command.
******************************************************************************/
void ActionManager::on_ViewportZoomSceneExtentsAll_triggered()
{
    userInterface().handleExceptions([&] {
        for(Viewport* vp : dataset()->viewportConfig()->viewports())
            vp->notifyDependents(Viewport::ZoomToSceneExtentsRequested);
    });
}

/******************************************************************************
* Handles the ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS command.
******************************************************************************/
void ActionManager::on_ViewportZoomSelectionExtents_triggered()
{
    userInterface().handleExceptions([&] {
        ViewportConfiguration* vpconf = dataset()->viewportConfig();
        if(vpconf->activeViewport() && !QGuiApplication::keyboardModifiers().testFlag(Qt::ControlModifier))
            vpconf->activeViewport()->notifyDependents(Viewport::ZoomToSelectionExtentsRequested);
        else {
            for(Viewport* vp : vpconf->viewports())
                vp->notifyDependents(Viewport::ZoomToSelectionExtentsRequested);
        }
    });
}

/******************************************************************************
* Handles the ACTION_VIEWPORT_ZOOM_SELECTION_EXTENTS_ALL command.
******************************************************************************/
void ActionManager::on_ViewportZoomSelectionExtentsAll_triggered()
{
    userInterface().handleExceptions([&] {
        for(Viewport* vp : dataset()->viewportConfig()->viewports())
            vp->notifyDependents(Viewport::ZoomToSelectionExtentsRequested);
    });
}

}   // End of namespace
