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

#include <ovito/core/Core.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportWindowInterface.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/DataBufferAccess.h>
#include <ovito/core/app/UserInterface.h>

namespace Ovito {

/******************************************************************************
* Returns the global viewport window class registry.
******************************************************************************/
ViewportWindowInterface::Registry& ViewportWindowInterface::registry()
{
	static Registry singleton;
	return singleton;
}

/******************************************************************************
* Constructor which associates this window with the given viewport instance.
******************************************************************************/
ViewportWindowInterface::ViewportWindowInterface(UserInterface& userInterface, Viewport* vp) : 
	_userInterface(userInterface),
	_viewport(vp)
{
	OVITO_ASSERT(vp);

	// Associate the viewport with this window.
	if(vp)
		vp->setWindow(this);
}

/******************************************************************************
* Destructor.
******************************************************************************/
ViewportWindowInterface::~ViewportWindowInterface()
{
	// Detach from Viewport instance.
	if(viewport())
		viewport()->setWindow(nullptr);
}

/******************************************************************************
* Makes the viewport window delete itself.
******************************************************************************/
void ViewportWindowInterface::destroyViewportWindow() 
{ 
	OVITO_ASSERT(_viewport != nullptr); 
	_viewport->setWindow(nullptr); 
	_viewport = nullptr;
}

/******************************************************************************
* Render the axis tripod symbol in the corner of the viewport that indicates
* the coordinate system orientation.
******************************************************************************/
void ViewportWindowInterface::renderOrientationIndicator(SceneRenderer* renderer)
{
	const FloatType tripodSize = 80.0;			// device-independent pixels
	const FloatType tripodArrowSize = 0.17; 	// percentage of the above value.

	// Set up projection matrix.
	QSize imageSize = renderer->viewportRect().size();
	const FloatType tripodPixelSize = tripodSize * renderer->devicePixelRatio();
	Matrix4 viewportScalingTM = Matrix4::Identity();
	viewportScalingTM(0,0) = tripodPixelSize / imageSize.width();
	viewportScalingTM(1,1) = tripodPixelSize / imageSize.height();
	viewportScalingTM(0,3) = -1.0 + viewportScalingTM(0,0);
	viewportScalingTM(1,3) = -1.0 + viewportScalingTM(1,1);
	ViewProjectionParameters projParams = viewport()->projectionParams();
	projParams.projectionMatrix = viewportScalingTM * Matrix4::ortho(-1.4, 1.4, -1.4, 1.4, -2.0, 2.0);
	projParams.inverseProjectionMatrix = projParams.projectionMatrix.inverse();
	projParams.viewMatrix.setIdentity();
	projParams.inverseViewMatrix.setIdentity();
	projParams.isPerspective = false;
	renderer->setProjParams(projParams);
	renderer->setWorldTransform(AffineTransformation::Identity());

	// Turn off depth-testing.
	renderer->setDepthTestEnabled(false);

    static const ColorA axisColors[3] = { ColorA(1.0, 0.0, 0.0), ColorA(0.0, 1.0, 0.0), ColorA(0.4, 0.4, 1.0) };
	static const QString labels[3] = { QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z") };

	// Create line primitive for the coordinate axis arrows.
	if(!_orientationTripodGeometry.colors()) {
		DataBufferAccessAndRef<ColorA> vertexColors = DataBufferPtr::create(18, DataBuffer::Float, 4);
		std::fill(vertexColors.begin() + 0,  vertexColors.begin() + 6,  axisColors[0]);
		std::fill(vertexColors.begin() + 6,  vertexColors.begin() + 12, axisColors[1]);
		std::fill(vertexColors.begin() + 12, vertexColors.end(),        axisColors[2]);
		_orientationTripodGeometry.setColors(vertexColors.take());
	}

	// Update geometry of coordinate axis arrows.
	DataBufferAccessAndRef<Point3> vertices = DataBufferPtr::create(18, DataBuffer::Float, 3);
	for(size_t axis = 0, index = 0; axis < 3; axis++) {
		Vector3 dir = viewport()->projectionParams().viewMatrix.column(axis).normalized();
		vertices[index++] = Point3::Origin();
		vertices[index++] = Point3::Origin() + dir;
		vertices[index++] = Point3::Origin() + dir;
		vertices[index++] = Point3::Origin() + (dir + tripodArrowSize * Vector3(dir.y() - dir.x(), -dir.x() - dir.y(), dir.z()));
		vertices[index++] = Point3::Origin() + dir;
		vertices[index++] = Point3::Origin() + (dir + tripodArrowSize * Vector3(-dir.y() - dir.x(), dir.x() - dir.y(), dir.z()));
	}
	// To avoid unnecessary GPU traffic, keep old data buffer in place if contents haven't changed.
	ConstDataBufferPtr newPositions = vertices.take();
	if(!_orientationTripodGeometry.positions() || !newPositions->equals(*_orientationTripodGeometry.positions()))
		_orientationTripodGeometry.setPositions(std::move(newPositions));

	// Render coordinate axis arrows.
	renderer->renderLines(_orientationTripodGeometry);

	// Render x,y,z labels.
	for(int axis = 0; axis < 3; axis++) {

		// Create a rendering buffer that is responsible for rendering the text label.
		if(_orientationTripodLabels[axis].text().isEmpty()) {
			_orientationTripodLabels[axis].setFont(ViewportSettings::getSettings().viewportFont());
			_orientationTripodLabels[axis].setColor(axisColors[axis]);
			_orientationTripodLabels[axis].setText(labels[axis]);
			_orientationTripodLabels[axis].setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		}

		Point3 p = Point3::Origin() + viewport()->projectionParams().viewMatrix.column(axis).resized(1.23);
		Point3 ndcPoint = projParams.projectionMatrix * p;
		_orientationTripodLabels[axis].setPositionViewport(renderer, {ndcPoint.x(), ndcPoint.y()});
		renderer->renderText(_orientationTripodLabels[axis]);
	}

	// Restore old rendering attributes.
	renderer->setDepthTestEnabled(true);
}

/******************************************************************************
* Renders the frame on top of the scene that indicates the visible rendering area.
******************************************************************************/
void ViewportWindowInterface::renderRenderFrame(SceneRenderer* renderer)
{
	// The render frame in viewport coordinates.
	Box2 frameRect = viewport()->renderFrameRect(userInterface().datasetContainer().currentSet());
	if(frameRect.isEmpty())
		return;

	// Create a 1x1 pixel semi-transparent image, which is used to fill reactangular areas with a uniform color.
	static QImage image;
	if(image.isNull()) {
		image = QImage(1, 1, QImage::Format_ARGB32);
		image.fill(0xA0A0A0A0);
	}

	// Fill area around frame rectangle with semi-transparent color.
	ImagePrimitive primitive;
	primitive.setImage(image);
	
	// Render four rectangles, which form the frame.
	primitive.setRectViewport(renderer, Box2({-1,-1}, {frameRect.minc.x(),1}));
	renderer->renderImage(primitive);
	primitive.setRectViewport(renderer, Box2({frameRect.maxc.x(),-1}, {1,1}));
	renderer->renderImage(primitive);
	primitive.setRectViewport(renderer, Box2({frameRect.minc.x(),-1}, {frameRect.maxc.x(),frameRect.minc.y()}));
	renderer->renderImage(primitive);
	primitive.setRectViewport(renderer, Box2({frameRect.minc.x(),frameRect.maxc.y()}, {frameRect.maxc.x(),1}));
	renderer->renderImage(primitive);
}

/******************************************************************************
* Renders the viewport caption text.
******************************************************************************/
QRectF ViewportWindowInterface::renderViewportTitle(SceneRenderer* renderer, bool hoverState)
{
	TextPrimitive primitive;
	primitive.setAlignment(Qt::AlignLeft | Qt::AlignTop);

	if(hoverState) {
		QFont font = ViewportSettings::getSettings().viewportFont();
		font.setUnderline(true);
		primitive.setFont(font);
	}
	else {
		primitive.setFont(ViewportSettings::getSettings().viewportFont());
	}

	QString str = viewport()->viewportTitle();
	if(viewport()->renderPreviewMode())
		str += Viewport::tr(" (preview)");
#ifdef OVITO_DEBUG
	str += QStringLiteral(" [%1]").arg(++_renderDebugCounter);
#endif
	primitive.setText(str);
	Color textColor = Viewport::viewportColor(ViewportSettings::COLOR_VIEWPORT_CAPTION);
	if(viewport()->renderPreviewMode() && textColor == renderer->renderSettings().backgroundColor())
		textColor = Vector3(1,1,1) - (Vector3)textColor;
	primitive.setColor(textColor);

	Point2 pos = Point2(2, 2) * renderer->devicePixelRatio();
	primitive.setPositionWindow(pos);
	renderer->renderText(primitive);

	// Compute the area covered by the caption text.
	QRectF textBounds = primitive.queryBounds(renderer);
	textBounds.moveTo(QPointF(2,2));
	textBounds.setWidth(std::max(textBounds.width() / renderer->devicePixelRatio(), 30.0));
	textBounds.setHeight(textBounds.height() / renderer->devicePixelRatio());
	textBounds.adjust(-2, -2, 2, 2);
	return textBounds;
}

}	// End of namespace
