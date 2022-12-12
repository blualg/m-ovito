////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/opengl/OpenGLSceneRenderer.h>
#include <ovito/opengl/PickingOpenGLSceneRenderer.h>
#include "OpenGLViewportWindow.h"

namespace Ovito {

OVITO_REGISTER_VIEWPORT_WINDOW_IMPLEMENTATION(OpenGLViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
OpenGLViewportWindow::OpenGLViewportWindow(Viewport* vp, UserInterface* userInterface, QWidget* parentWidget) : 
		QOpenGLWidget(parentWidget),
		BaseViewportWindow(*userInterface, vp)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);

	// Create the viewport renderer.
	_viewportRenderer = OORef<OpenGLSceneRenderer>::create();
	_viewportRenderer->setInteractive(true);

	// Create the object picking renderer.
	_pickingRenderer = OORef<PickingOpenGLSceneRenderer>::create();
	_pickingRenderer->setInteractive(true);

	// Make sure the viewport window releases its resources before the application shuts down, e.g.
	// due to a Python script error.
	connect(QCoreApplication::instance(), &QObject::destroyed, this, [this]() { releaseResources(); });

	// Re-render window whenever requested by the system.
	connect(&scenePreparation(), &ScenePreparation::viewportUpdateRequest, this, &OpenGLViewportWindow::renderLater);
}

/******************************************************************************
* Destructor.
******************************************************************************/
OpenGLViewportWindow::~OpenGLViewportWindow() 
{
	releaseResources();
}

/******************************************************************************
* Is called once before the first call to paintGL() or resizeGL().
******************************************************************************/
void OpenGLViewportWindow::initializeGL()
{
	// Determine OpenGL vendor string so other parts of the code can decide
	// which OpenGL features are safe to use.
	OpenGLSceneRenderer::determineOpenGLInfo();
}

/******************************************************************************
* Releases the renderer resources held by the viewport's surface and picking renderers. 
******************************************************************************/
void OpenGLViewportWindow::releaseResources()
{
	// Release any OpenGL resources held by the viewport renderers.
	if(_viewportRenderer && _viewportRenderer->currentResourceFrame()) {
		makeCurrent();
		OpenGLResourceManager::instance()->releaseResourceFrame(_viewportRenderer->currentResourceFrame());
		_viewportRenderer->setCurrentResourceFrame(0);
	}
	if(_pickingRenderer && _pickingRenderer->currentResourceFrame()) {
		makeCurrent();
		OpenGLResourceManager::instance()->releaseResourceFrame(_pickingRenderer->currentResourceFrame());
		_pickingRenderer->setCurrentResourceFrame(0);
	}
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void OpenGLViewportWindow::renderLater()
{
	_updateRequested = true;
	update();
}

/******************************************************************************
* If an update request is pending for this viewport window, immediately
* processes it and redraw the window contents.
******************************************************************************/
void OpenGLViewportWindow::processViewportUpdate()
{
	if(_updateRequested) {
		OVITO_ASSERT_MSG(!userInterface().isRenderingInteractiveViewports(), "OpenGLViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");
		repaint();
	}
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
ViewportPickResult OpenGLViewportWindow::pick(const QPointF& pos)
{
	ViewportPickResult result;

	// Cannot perform picking while viewport is not visible or currently rendering or when updates are disabled.
	if(isVisible() && !userInterface().isRenderingInteractiveViewports() && !userInterface().areViewportUpdatesSuspended() && pickingRenderer()) {
		OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = 0;
		try {
			if(pickingRenderer()->isRefreshRequired()) {
				// A dataset is required for rendering.
				if(DataSet* dataset = userInterface().datasetContainer().currentSet()) {
					// Request a new frame from the resource manager for this render pass.
					previousResourceFrame = pickingRenderer()->currentResourceFrame();
					pickingRenderer()->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());
					pickingRenderer()->setPrimaryFramebuffer(defaultFramebufferObject());

					// Let the viewport do the actual rendering work.
					viewport()->renderInteractive(userInterface(), dataset, pickingRenderer());
				}
				else {
					return result; // Return null result if no dataset is available.
				}
			}

			// Query which object is located at the given window position.
			const QPoint pixelPos = (pos * devicePixelRatio()).toPoint();
			const PickingOpenGLSceneRenderer::ObjectPickingRecord* objInfo;
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
			userInterface().reportError(ex);
		}

		// Release the resources created by the OpenGL renderer during the last render pass before the current pass.
		if(previousResourceFrame)
			OpenGLResourceManager::instance()->releaseResourceFrame(previousResourceFrame);
	}
	return result;
}

/******************************************************************************
* Handles show events.
******************************************************************************/
void OpenGLViewportWindow::showEvent(QShowEvent* event)
{
	if(!event->spontaneous())
		update();
	QOpenGLWidget::showEvent(event);
}

/******************************************************************************
* Handles hide events.
******************************************************************************/
void OpenGLViewportWindow::hideEvent(QHideEvent* event)
{
	// Release all renderer resources when the window becomes hidden.
	releaseResources();

	QOpenGLWidget::hideEvent(event);
}

/******************************************************************************
* Is called whenever the widget needs to be painted.
******************************************************************************/
void OpenGLViewportWindow::paintGL()
{
	_updateRequested = false;

	// Do nothing if windows has been detached from its viewport.
	if(!viewport())
		return;

	OVITO_ASSERT_MSG(!userInterface().isRenderingInteractiveViewports(), "OpenGLViewportWindow::paintGL()", "Recursive viewport repaint detected.");

	// Do not re-enter rendering function of the same viewport.
	if(userInterface().isRenderingInteractiveViewports())
		return;

	QSurfaceFormat format = context()->format();
	// OpenGL in a VirtualBox machine Windows guest reports "2.1 Chromium 1.9" as version string, which is
	// not correctly parsed by Qt. We have to workaround this.
	if(OpenGLSceneRenderer::openGLVersion().startsWith("2.1 ")) {
		format.setMajorVersion(2);
		format.setMinorVersion(1);
	}

	// Invalidate picking buffer every time the visible contents of the viewport change.
	_pickingRenderer->resetPickingBuffer();

	// A dataset is required for rendering.
	DataSet* dataset = userInterface().datasetContainer().currentSet();
	if(!dataset)
		return;

	if(!userInterface().areViewportUpdatesSuspended()) {

		if(format.majorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MAJOR || (format.majorVersion() == OVITO_OPENGL_MINIMUM_VERSION_MAJOR && format.minorVersion() < OVITO_OPENGL_MINIMUM_VERSION_MINOR)) {
			// Avoid infinite recursion.
			static bool errorMessageShown = false;
			if(!errorMessageShown) {
				errorMessageShown = true;
				userInterface().exitWithFatalError(Exception(tr(
						"The OpenGL graphics driver installed on this system does not support OpenGL version %6.%7 or newer.\n\n"
						"Ovito requires modern graphics hardware and up-to-date graphics drivers to display 3D content. Your current system configuration is not compatible with Ovito and the application will quit now.\n\n"
						"To avoid this error, please install the newest graphics driver of the hardware vendor or, if necessary, consider replacing your graphics card with a newer model.\n\n"
						"The installed OpenGL graphics driver reports the following information:\n\n"
						"OpenGL vendor: %1\n"
						"OpenGL renderer: %2\n"
						"OpenGL version: %3.%4 (%5)\n\n"
						"Ovito requires at least OpenGL version %6.%7.")
						.arg(QString(OpenGLSceneRenderer::openGLVendor()))
						.arg(QString(OpenGLSceneRenderer::openGLRenderer()))
						.arg(format.majorVersion())
						.arg(format.minorVersion())
						.arg(QString(OpenGLSceneRenderer::openGLVersion()))
						.arg(OVITO_OPENGL_MINIMUM_VERSION_MAJOR)
						.arg(OVITO_OPENGL_MINIMUM_VERSION_MINOR)
					));
			}
			return;
		}
		
#ifdef Q_OS_WIN
		if(OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 2000" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 3000" || OpenGLSceneRenderer::openGLRenderer() == "Intel(R) HD Graphics 4400") {
			// Avoid infinite recursion.
			static bool errorMessageShown = false;
			if(!errorMessageShown) {
				errorMessageShown = true;
				userInterface().exitWithFatalError(Exception(tr(
						"The graphics chip installed in this system is not compatible with OVITO, unfortunately.\n\n"
						"Intel(R) HD Graphics, an integrated graphics chip released in the years 2010/2011/2012, does not support the specific OpenGL functions required by OVITO. "
						"There is no known workaround to make OVITO work on systems with this particular graphics unit. Please use OVITO on a computer with a more modern graphics processor.\n\n"
						"Detected graphics interface:\n\n"
						"OpenGL vendor: %1\n"
						"OpenGL renderer: %2\n"
						"OpenGL version: %3.%4 (%5)")
						.arg(QString(OpenGLSceneRenderer::openGLVendor()))
						.arg(QString(OpenGLSceneRenderer::openGLRenderer()))
						.arg(format.majorVersion())
						.arg(format.minorVersion())
						.arg(QString(OpenGLSceneRenderer::openGLVersion()))
					));
			}
			return;
		}
#endif

		// Request a new frame from the resource manager for this render pass.
		OpenGLResourceManager::ResourceFrameHandle previousResourceFrame = _viewportRenderer->currentResourceFrame();
		_viewportRenderer->setCurrentResourceFrame(OpenGLResourceManager::instance()->acquireResourceFrame());
		_viewportRenderer->setPrimaryFramebuffer(defaultFramebufferObject());

		try {
			// Let the Viewport class do the actual rendering work.
			viewport()->renderInteractive(userInterface(), dataset, _viewportRenderer);
		}
		catch(Exception& ex) {
			ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewport contents. The program will quit."));

			QString openGLReport;
			QTextStream stream(&openGLReport, QIODevice::WriteOnly | QIODevice::Text);
			stream << "OpenGL version: " << context()->format().majorVersion() << QStringLiteral(".") << context()->format().minorVersion() << "\n";
			stream << "OpenGL profile: " << (context()->format().profile() == QSurfaceFormat::CoreProfile ? "core" : (context()->format().profile() == QSurfaceFormat::CompatibilityProfile ? "compatibility" : "none")) << "\n";
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
		// Make sure viewport is refreshed as soon as updates get re-enabled.
		userInterface().updateViewports();
	}
}

}	// End of namespace
