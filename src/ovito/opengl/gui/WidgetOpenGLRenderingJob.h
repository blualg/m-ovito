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
#include <ovito/opengl/OpenGLRenderingJob.h>

#include <QOpenGLWidget>

namespace Ovito {

/**
 * \brief A rendering job that uses OpenGL to render into an QOpenGLWidget.
 */
class OVITO_OPENGLRENDERERGUI_EXPORT WidgetOpenGLRenderingJob : public OpenGLRenderingJob
{
    OVITO_CLASS(WidgetOpenGLRenderingJob)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, QOpenGLWidget* widget, std::shared_ptr<RendererResourceCache> visCache, int multisamplingLevel, bool orderIndependentTransparency);

    /// Requests the rendering job to make its OpenGL context current, e.g. for releasing OpenGL resources that require an active context.
    [[nodiscard]] virtual OpenGLContextRestore activateContext() override {
        OpenGLContextRestore restore;
        OVITO_ASSERT(_widget);
        if(_widget)
            _widget->makeCurrent();
        return restore;
    }

private:

    /// The widget we are rendering into.
    QPointer<QOpenGLWidget> _widget;
};

}   // End of namespace
