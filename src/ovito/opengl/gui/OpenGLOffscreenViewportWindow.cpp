////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>
#include "OpenGLOffscreenViewportWindow.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
OpenGLOffscreenViewportWindow::OpenGLOffscreenViewportWindow(Viewport* vp, const QSize& initialSize, std::function<void(QImage)> imageCallback) : 
	BaseViewportWindow(*this, vp),
	UserInterface(*vp->dataset()->container(), vp->taskManager()),
	_inputManager(nullptr, *this),
	_imageCallback(std::move(imageCallback))
{
	OVITO_ASSERT(vp);
	OVITO_ASSERT(qApp);
	OVITO_ASSERT(QThread::currentThread() == qApp->thread());

	// Assign our internal input manager to the UserInterface object.
	setViewportInputManager(&_inputManager);

	// Create a OpenGL context for rendering to an offscreen buffer.
	// The context should share its resources with interactive viewport renderers (only when operating in the same thread).
	if(QOpenGLContext::globalShareContext() && QThread::currentThread() == QOpenGLContext::globalShareContext()->thread())
		_offscreenContext.setShareContext(QOpenGLContext::globalShareContext());
	if(!_offscreenContext.create())
		throw Exception(tr("Failed to create OpenGL context for offscreen rendering. Please make sure OVITO is able to access the OpenGL graphics interface. On Linux systems, a running display manager may be necessary for it."));
	
	// Create an offscreen rendering surface.
	_offscreenSurface = new QOffscreenSurface(nullptr, this);
	_offscreenSurface->setFormat(_offscreenContext.format());
	_offscreenSurface->create(); 
	if(!_offscreenSurface->isValid())
		throw Exception(tr("Failed to create offscreen OpenGL rendering surface."));

	// Make the context current.
	if(!_offscreenContext.makeCurrent(_offscreenSurface))
		throw Exception(tr("Failed to make OpenGL context current."));

	// Determine OpenGL vendor string so other parts of the code can decide
	// which OpenGL features are safe to use.
	OpenGLSceneRenderer::determineOpenGLInfo();

	// Create offscreen framebuffer.
	QOpenGLFramebufferObjectFormat framebufferFormat;
	framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	_framebufferObject = std::make_unique<QOpenGLFramebufferObject>(initialSize, framebufferFormat);
	if(!_framebufferObject->isValid())
		throw Exception(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));

	_offscreenContext.doneCurrent();

	// Create the viewport renderer.
	_viewportRenderer = OORef<OpenGLSceneRenderer>::create(viewport()->dataset());
	_viewportRenderer->setInteractive(true);

	// Create the object picking renderer.
	_pickingRenderer = OORef<PickingOpenGLSceneRenderer>::create(viewport()->dataset());
	_pickingRenderer->setInteractive(true);

	// Tell the renderers about the FBO we are rendering into.
	_viewportRenderer->setPrimaryFramebuffer(_framebufferObject->handle());
	_pickingRenderer->setPrimaryFramebuffer(_framebufferObject->handle());

	// Render the window for the first time.
	renderLater();
}

/******************************************************************************
* Destructor.
******************************************************************************/
OpenGLOffscreenViewportWindow::~OpenGLOffscreenViewportWindow() 
{
	releaseResources();
}

/******************************************************************************
* Releases the renderer resources held by the viewport's surface and picking renderers. 
******************************************************************************/
void OpenGLOffscreenViewportWindow::releaseResources()
{
	// Release any OpenGL resources held by the viewport renderers.
	if(_viewportRenderer && _viewportRenderer->currentResourceFrame()) {
		makeOpenGLContextCurrent();
		OpenGLResourceManager::instance()->releaseResourceFrame(_viewportRenderer->currentResourceFrame());
		_viewportRenderer->setCurrentResourceFrame(0);
	}
	if(_pickingRenderer && _pickingRenderer->currentResourceFrame()) {
		makeOpenGLContextCurrent();
		OpenGLResourceManager::instance()->releaseResourceFrame(_pickingRenderer->currentResourceFrame());
		_pickingRenderer->setCurrentResourceFrame(0);
	}
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void OpenGLOffscreenViewportWindow::renderLater()
{
	if(!_repaintTimer.isActive())
		_repaintTimer.start(0, this);
}

/******************************************************************************
* If an update request is pending for this viewport window, immediately
* processes it and redraw the window contents.
******************************************************************************/
void OpenGLOffscreenViewportWindow::processViewportUpdate()
{
	if(_immediateViewportUpdatesEnabled && _repaintTimer.isActive()) {
		OVITO_ASSERT_MSG(!viewport()->isRendering(), "OpenGLOffscreenViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");
		OVITO_ASSERT_MSG(!viewport()->dataset()->viewportConfig()->isRendering(), "OpenGLOffscreenViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");
		renderViewport();
	}
}

/******************************************************************************
* Handles timer events of the object.
******************************************************************************/
void OpenGLOffscreenViewportWindow::timerEvent(QTimerEvent* event)
{
	if(event->timerId() == _repaintTimer.timerId()) {
		renderViewport();
	}
	QObject::timerEvent(event);
}

/******************************************************************************
* Changes the size of the offscreen window.
******************************************************************************/
void OpenGLOffscreenViewportWindow::setSize(const QSize& size)
{
	if(_framebufferObject->size() == size)
		return;

	// Make the context current.
	if(!_offscreenContext.makeCurrent(_offscreenSurface))
		throw Exception(tr("Failed to make OpenGL context current."));

	// Recreate offscreen framebuffer.
	QOpenGLFramebufferObjectFormat framebufferFormat;
	framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	_framebufferObject = std::make_unique<QOpenGLFramebufferObject>(size, framebufferFormat);
	if(!_framebufferObject->isValid())
		throw Exception(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));

	// Tell the renderers about the new FBO.
	_viewportRenderer->setPrimaryFramebuffer(_framebufferObject->handle());
	_pickingRenderer->setPrimaryFramebuffer(_framebufferObject->handle());

	renderLater();
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
ViewportPickResult OpenGLOffscreenViewportWindow::pick(const QPointF& pos)
{
	ViewportPickResult result;

	// Cannot perform picking while viewport is not visible or currently rendering or when updates are disabled.
	if(isVisible() && !viewport()->isRendering() && !viewport()->dataset()->viewportConfig()->isSuspended() && pickingRenderer()) {
		OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = 0;
		try {
			if(pickingRenderer()->isRefreshRequired()) {
				// Request a new frame from the resource manager for this render pass.
				previousResourceFrame = pickingRenderer()->currentResourceFrame();
				pickingRenderer()->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());

				// Let the viewport do the actual rendering work.
				viewport()->renderInteractive(pickingRenderer());
			}

			// Query which object is located at the given window position.
			const QPoint pixelPos = (pos * devicePixelRatio()).toPoint();
			const SceneRenderer::ObjectPickingRecord* objInfo;
			quint32 subobjectId;
			std::tie(objInfo, subobjectId) = pickingRenderer()->objectAtLocation(pixelPos);
			if(objInfo) {
				result.setPipelineNode(objInfo->objectNode);
				result.setPickInfo(objInfo->pickInfo);
				result.setHitLocation(pickingRenderer()->worldPositionFromLocation(pixelPos));
				result.setSubobjectId(subobjectId);
			}
		}
		catch(const Exception& ex) {
			ex.reportError();
		}

		// Release the resources created by the OpenGL renderer during the last render pass before the current pass.
		if(previousResourceFrame)
			OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
	}
	return result;
}

/******************************************************************************
* Is called whenever the widget needs to be painted.
******************************************************************************/
void OpenGLOffscreenViewportWindow::renderViewport()
{
	_repaintTimer.stop();

	// Do nothing if windows has been detached from its viewport.
	if(!viewport() || !viewport()->dataset())
		return;

	OVITO_ASSERT_MSG(!viewport()->isRendering(), "OpenGLOffscreenViewportWindow::renderViewport()", "Recursive viewport repaint detected.");
	OVITO_ASSERT_MSG(!viewport()->dataset()->viewportConfig()->isRendering(), "OpenGLOffscreenViewportWindow::renderViewport()", "Recursive viewport repaint detected.");

	// Do not re-enter rendering function of the same viewport.
	if(viewport()->isRendering())
		return;

	// Invalidate picking buffer every time the visible contents of the viewport change.
	_pickingRenderer->resetPickingBuffer();

	if(!viewport()->dataset()->viewportConfig()->isSuspended()) {

		// Request a new frame from the resource manager for this render pass.
		OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = _viewportRenderer->currentResourceFrame();
		_viewportRenderer->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());

		try {
			// Make the context current.
			if(!_offscreenContext.makeCurrent(_offscreenSurface))
				throw Exception(tr("Failed to make OpenGL context current."));

			// Bind OpenGL buffer.
			if(!_framebufferObject->bind())
				throw Exception(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

			// Let the Viewport class do the actual rendering work.
			viewport()->renderInteractive(_viewportRenderer);

			// Flush the contents to the FBO before extracting image.
			_offscreenContext.swapBuffers(_offscreenSurface);

			// Fetch rendered image from OpenGL framebuffer.
			QImage renderedImage = _framebufferObject->toImage();

			// Invoke callback function with the rendered image.
			if(_imageCallback)
				_imageCallback(std::move(renderedImage));
		}
		catch(Exception& ex) {
			if(ex.context() == nullptr) ex.setContext(viewport()->dataset());
			ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewport contents."));

			QString openGLReport;
			QTextStream stream(&openGLReport, QIODevice::WriteOnly | QIODevice::Text);
			stream << "OpenGL version: " << _offscreenContext.format().majorVersion() << QStringLiteral(".") << _offscreenContext.format().minorVersion() << "\n";
			stream << "OpenGL profile: " << (_offscreenContext.format().profile() == QSurfaceFormat::CoreProfile ? "core" : (_offscreenContext.format().profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
			stream << "OpenGL vendor: " << QString(OpenGLSceneRenderer::openGLVendor()) << "\n";
			stream << "OpenGL renderer: " << QString(OpenGLSceneRenderer::openGLRenderer()) << "\n";
			stream << "OpenGL version string: " << QString(OpenGLSceneRenderer::openGLVersion()) << "\n";
			stream << "OpenGL shading language: " << QString(OpenGLSceneRenderer::openGLSLVersion()) << "\n";
			stream << "OpenGL shader programs: " << QOpenGLShaderProgram::hasOpenGLShaderPrograms() << "\n";
			ex.appendDetailMessage(openGLReport);

			userInterface().exitWithFatalError(ex);
		}

		// Release the resources created by the OpenGL renderer during the last render pass before the current pass.
		if(previousResourceFrame) {
			OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
		}
	}
	else {
		// Make sure viewport gets refreshed as soon as updates are enabled again.
		viewport()->dataset()->viewportConfig()->updateViewports();
	}
}

}	// End of namespace
