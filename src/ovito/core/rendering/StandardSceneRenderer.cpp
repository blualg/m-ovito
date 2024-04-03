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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "StandardSceneRenderer.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(StandardSceneRenderer);
DEFINE_PROPERTY_FIELD(StandardSceneRenderer, antialiasingLevel);
SET_PROPERTY_FIELD_LABEL(StandardSceneRenderer, antialiasingLevel, "Antialiasing level");
SET_PROPERTY_FIELD_UNITS_AND_RANGE(StandardSceneRenderer, antialiasingLevel, IntegerParameterUnit, 1, 6);

/******************************************************************************
* Constructor.
******************************************************************************/
StandardSceneRenderer::StandardSceneRenderer(ObjectInitializationFlags flags) : SceneRenderer(flags),
    _antialiasingLevel(3)
{
}

/******************************************************************************
* Prepares the renderer for rendering one or more frames.
******************************************************************************/
void StandardSceneRenderer::startRender(const QSize& frameBufferSize)
{
    SceneRenderer::startRender(frameBufferSize);

    // Create the internal renderer implementation.
    OvitoClassPtr rendererClass = {};

#ifndef OVITO_DISABLE_QSETTINGS
    // Did user select Vulkan as the standard graphics interface?
    QSettings applicationSettings;
    if(applicationSettings.value("rendering/selected_graphics_api").toString() == "Vulkan")
        rendererClass = PluginManager::instance().findClass("VulkanRenderer", "OffscreenVulkanSceneRenderer");
#endif

    // Use OpenGL renderer as the default implementation.
    if(!rendererClass)
        rendererClass = PluginManager::instance().findClass("OpenGLRenderer", "OffscreenOpenGLSceneRenderer");

    // Instantiate the renderer implementation.
    if(!rendererClass)
        throw Exception(tr("The OffscreenOpenGLSceneRenderer class is not available. Please make sure the OpenGLRenderer plugin is installed correctly."));
    if(!_internalRenderer || &_internalRenderer->getOOClass() != rendererClass) {
        _internalRenderer = static_object_cast<SceneRenderer>(rendererClass->createInstance());
    }

    // Pass supersampling level requested by the user to the renderer implementation.
    _internalRenderer->setMultisamplingLevel(std::max(1, antialiasingLevel()));

    _internalRenderer->startRender(frameBufferSize);
}

/******************************************************************************
* Renders the current animation frame.
******************************************************************************/
void StandardSceneRenderer::renderFrame(FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    // Delegate rendering work to implementation class.
    _internalRenderer->renderFrame(frameGraph, viewportRect, frameBuffer);
}

/******************************************************************************
* Is called after rendering has finished.
******************************************************************************/
void StandardSceneRenderer::endRender()
{
    if(_internalRenderer) {
        // Call implementation class.
        _internalRenderer->endRender();
    }
    SceneRenderer::endRender();
}

}   // End of namespace
