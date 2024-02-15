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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/viewport/WidgetViewportWindow.h>
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>

#include <QOpenGLWidget>

namespace Ovito {

/**
 * \brief The viewport window implementation of the OpenGL renderer.
 */
class OVITO_OPENGLRENDERERGUI_EXPORT OpenGLViewportWindow : public WidgetViewportWindow
{
    Q_OBJECT
    OVITO_CLASS(OpenGLViewportWindow)

public:

    /// Constructor.
    OpenGLViewportWindow();

    /// Destructor.
    virtual ~OpenGLViewportWindow() { releaseResources(); }

    /// Returns the renderer generating an offscreen image of the scene used for object picking.
    PickingOpenGLSceneRenderer* pickingRenderer() const { return _pickingRenderer; }

    /// Determines the object that is located under the given mouse cursor position.
    virtual ViewportPickResult pick(const QPointF& pos) override;

    /// Releases the renderer resources held by the viewport's surface and picking renderers.
    virtual void releaseResources() override;

	/// Returns the best QImage pixel format to be used for textures and image primitives.
	virtual QImage::Format preferredImageFormat() const override { return _viewportRenderer->preferredImageFormat(); }

protected:

    /// Creates the UI widget that is associated with this viewport window.
    virtual QWidget* createWidget(QWidget* parent) override;

    /// This is called after the frame graph has been updated to render the viewport contents on screen.
    virtual void refreshDisplay() override;

    /// Lets the renderer implementation perform post-processing of a newly generated frame graph.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) override {
        _viewportRenderer->postprocessFrameGraph(frameGraph);
    }

    /// Returns the QOpenGLWidget that is associated with this viewport window.
    QOpenGLWidget* widget() const { return static_cast<QOpenGLWidget*>(WidgetViewportWindow::widget()); }

    /// Is called whenever the widget needs to be painted.
    void paint();

private:

    /// This is the renderer of the interactive viewport.
    OORef<OpenGLSceneRenderer> _viewportRenderer;

    /// This renderer generates an offscreen rendering of the scene that allows picking of objects.
    OORef<PickingOpenGLSceneRenderer> _pickingRenderer;
};

}   // End of namespace
