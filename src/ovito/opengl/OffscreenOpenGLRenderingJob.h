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
#include "OpenGLRenderingJob.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>

namespace Ovito {

/**
 * \brief A rendering job that uses OpenGL to render into an offscreen framebuffer.
 */
class OVITO_OPENGLRENDERER_EXPORT OffscreenOpenGLRenderingJob : public OpenGLRenderingJob
{
    OVITO_CLASS(OffscreenOpenGLRenderingJob)

public:

    /// Constructor creating a new QOffscreenSurface.
    void initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, int multisamplingLevel, bool orderIndependentTransparency);

    /// Called when this renderer is being destroyed.
    virtual void aboutToBeDeleted() override;

    /// Requests the rendering job to make its OpenGL context current, e.g. for releasing OpenGL resources that require an active context.
    [[nodiscard]] virtual OpenGLContextRestore activateContext() override;

private:

    /// Creates the QOffscreenSurface in the main thread.
    void createOffscreenSurface();

    /// Creates the QOpenGLContext for offscreen rendering (in any thread).
    QOpenGLContext& createOffscreenContext();

private:

    /// The offscreen surface used to render into an image buffer using OpenGL.
    std::unique_ptr<QOffscreenSurface> _offscreenSurface;

    /// The OpenGL rendering context used for offscreen rendering.
    std::optional<QOpenGLContext> _offscreenContext;
};

}   // End of namespace
