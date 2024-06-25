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


#include <ovito/gui/base/GUIBase.h>
#include <ovito/gui/vpwidget/WidgetViewportWindow.h>
#include <ovito/opengl/OpenGLRenderer.h>
#include <ovito/opengl/OpenGLRenderingFrameBuffer.h>
#include "OpenGLPickingMap.h"

#include <QOpenGLWidget>

namespace Ovito {

/**
 * \brief The interactive viewport window implementation of the OpenGL renderer.
 */
class OVITO_OPENGLRENDERERWINDOW_EXPORT OpenGLViewportWindow : public WidgetViewportWindow
{
    Q_OBJECT
    OVITO_CLASS(OpenGLViewportWindow)

public:

    /// Determines the object that is located under the given mouse cursor position.
    virtual std::optional<PickResult> pick(const QPointF& pos) override;

    /// Releases the renderer resources held by the viewport's surface and picking renderers.
    virtual void releaseResources() override;

protected:

    /// Creates the UI widget that is associated with this viewport window.
    virtual QWidget* createWidget(QWidget* parent) override;

    /// Creates the rendering job that renders the contents of the viewport window.
    virtual OORef<RenderingJob> createRenderingJob() override;

    /// Newly renders the window contents after the frame graph has been regenerated.
    virtual void rerender() override;

    /// Returns the QOpenGLWidget that is associated with this viewport window.
    QOpenGLWidget* widget() const { return static_cast<QOpenGLWidget*>(WidgetViewportWindow::widget()); }

    /// Is called whenever the widget needs to be painted.
    void paint();

private:

    /// The abstract frame buffer for on-screen rendering into the QOpenGLWidget.
    OORef<AbstractRenderingFrameBuffer> _visualFrameBuffer;

    /// The abstract frame buffer for off-screen rendering into the object picking buffer.
    OORef<AbstractRenderingFrameBuffer> _pickingFrameBuffer;

    /// Manages the information obtained from an object picking render pass.
    std::shared_ptr<OpenGLPickingMap> _objectPickingMap = std::make_shared<OpenGLPickingMap>();
};

}   // End of namespace
