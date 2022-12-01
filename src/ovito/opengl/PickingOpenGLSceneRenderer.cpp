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
#include <ovito/core/viewport/ViewportWindowInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/rendering/RenderSettings.h>
#include "PickingOpenGLSceneRenderer.h"
#include "OpenGLDepthTextureBlitter.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(PickingOpenGLSceneRenderer);

/******************************************************************************
* Constructor.
******************************************************************************/
PickingOpenGLSceneRenderer::PickingOpenGLSceneRenderer(ObjectCreationParams params) : OffscreenInteractiveOpenGLSceneRenderer(params) 
{
	setPicking(true);
}

/******************************************************************************
* Renders the current animation frame.
******************************************************************************/
bool PickingOpenGLSceneRenderer::renderFrame(const QRect& viewportRect, MainThreadOperation& operation)
{
	// Clear previous object records.
	resetPickingBuffer();

	// Let the base class do the main rendering work.
	if(!OffscreenInteractiveOpenGLSceneRenderer::renderFrame(viewportRect, operation))
		return false;

	if(framebufferObject()) {
		// Fetch rendered image from the OpenGL framebuffer.
		QSize size = framebufferObject()->size();

#ifndef Q_OS_WASM
		// Acquire OpenGL depth buffer data.
		// The depth information is used to compute the XYZ coordinate of the point under the mouse cursor.
		_depthBufferBits = glformat().depthBufferSize();
		if(_depthBufferBits == 16) {
			_depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLushort));
			glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, _depthBuffer.get());
		}
		else if(_depthBufferBits == 24) {
			_depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLuint));
			while(glGetError() != GL_NO_ERROR);
			glReadPixels(0, 0, size.width(), size.height(), 0x84F9 /*GL_DEPTH_STENCIL*/, 0x84FA /*GL_UNSIGNED_INT_24_8*/, _depthBuffer.get());
			if(glGetError() != GL_NO_ERROR) {
				glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_FLOAT, _depthBuffer.get());
				_depthBufferBits = 0;
			}
		}
		else if(_depthBufferBits == 32) {
			_depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLuint));
			glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, _depthBuffer.get());
		}
		else {
			_depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLfloat));
			glReadPixels(0, 0, size.width(), size.height(), GL_DEPTH_COMPONENT, GL_FLOAT, _depthBuffer.get());
			_depthBufferBits = 0;
		}
#endif
	}
	else {
		// Create a temporary OpenGL framebuffer.
		QOpenGLFramebufferObjectFormat framebufferFormat;
		QSize size = viewport()->window()->viewportWindowDeviceSize();
		QOpenGLFramebufferObject framebufferObject(size, framebufferFormat);

		// Clear OpenGL error state and verify validity of framebuffer.
		while(this->glGetError() != GL_NO_ERROR);
		if(!framebufferObject.isValid())
			throw RendererException(tr("Failed to create OpenGL framebuffer object for offscreen rendering."));

		// Bind OpenGL framebuffer.
		if(!framebufferObject.bind())
			throw RendererException(tr("Failed to bind OpenGL framebuffer object for offscreen rendering."));

		// Reset OpenGL context state.
		this->glDisable(GL_CULL_FACE);
		this->glDisable(GL_STENCIL_TEST);
		this->glDisable(GL_BLEND);
		this->glDisable(GL_DEPTH_TEST);

		// Transfer depth buffer to the color buffer so that the pixel data can be read.
		// WebGL1 doesn't allow direct reading the data of a depth texture.
		OpenGLDepthTextureBlitter blitter;
		blitter.create();
		blitter.bind();
		blitter.blit(depthTextureId());
		blitter.release();

		// Read depth buffer contents from the color attachment of the framebuffer.
		// Depth values are encoded as RGB values in each pixel.
		_depthBufferBits = 24;
		_depthBuffer = std::make_unique<quint8[]>(size.width() * size.height() * sizeof(GLuint));
		OVITO_CHECK_OPENGL(this, this->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, _depthBuffer.get()));
	}

	return true;
}

/******************************************************************************
* Resets the internal state of the picking renderer and clears the stored object records.
******************************************************************************/
void PickingOpenGLSceneRenderer::resetPickingBuffer()
{
	discardFramebufferImage();

	OffscreenInteractiveOpenGLSceneRenderer::resetPickingBuffer();
}

/******************************************************************************
* Returns the object record and the sub-object ID for the object at the given pixel coordinates.
******************************************************************************/
std::tuple<const SceneRenderer::ObjectPickingRecord*, quint32> PickingOpenGLSceneRenderer::objectAtLocation(const QPoint& pos) const
{
	if(!framebufferImage().isNull()) {
		if(pos.x() >= 0 && pos.x() < framebufferImage().width() && pos.y() >= 0 && pos.y() < framebufferImage().height()) {
			QPoint mirroredPos(pos.x(), framebufferImage().height() - 1 - pos.y());
			QRgb pixel = framebufferImage().pixel(mirroredPos);
			quint32 red = qRed(pixel);
			quint32 green = qGreen(pixel);
			quint32 blue = qBlue(pixel);
			quint32 alpha = qAlpha(pixel);
			quint32 objectID = red + (green << 8) + (blue << 16) + (alpha << 24);
			if(const ObjectPickingRecord* objRecord = lookupObjectPickingRecord(objectID)) {
				quint32 subObjectID = objectID - objRecord->baseObjectID;
				for(const auto& range : objRecord->indexedRanges) {
					if(subObjectID >= range.second && subObjectID < range.second + range.first->size()) {
						subObjectID = range.second + ConstDataBufferAccess<int>(range.first).get(subObjectID - range.second);
						break;
					}
				}
				return std::make_tuple(objRecord, subObjectID);
			}
		}
	}
	return std::tuple<const SceneRenderer::ObjectPickingRecord*, quint32>(nullptr, 0);
}

/******************************************************************************
* Returns the Z-value at the given window position.
******************************************************************************/
FloatType PickingOpenGLSceneRenderer::depthAtPixel(const QPoint& pos) const
{
	if(!framebufferImage().isNull() && _depthBuffer) {
		int w = framebufferImage().width();
		int h = framebufferImage().height();
		if(pos.x() >= 0 && pos.x() < w && pos.y() >= 0 && pos.y() < h) {
			QPoint mirroredPos(pos.x(), framebufferImage().height() - 1 - pos.y());
			if(framebufferImage().pixel(mirroredPos) != 0) {
				if(_depthBufferBits == 16) {
					GLushort bval = reinterpret_cast<const GLushort*>(_depthBuffer.get())[(mirroredPos.y()) * w + pos.x()];
					return (FloatType)bval / FloatType(65535.0);
				}
				else if(_depthBufferBits == 24) {
					GLuint bval = reinterpret_cast<const GLuint*>(_depthBuffer.get())[(mirroredPos.y()) * w + pos.x()];
					return (FloatType)((bval >> 8) & 0x00FFFFFF) / FloatType(16777215.0);
				}
				else if(_depthBufferBits == 32) {
					GLuint bval = reinterpret_cast<const GLuint*>(_depthBuffer.get())[(mirroredPos.y()) * w + pos.x()];
					return (FloatType)bval / FloatType(4294967295.0);
				}
				else if(_depthBufferBits == 0) {
					return reinterpret_cast<const GLfloat*>(_depthBuffer.get())[(mirroredPos.y()) * w + pos.x()];
				}
			}
		}
	}
	return 0;
}

/******************************************************************************
* Returns the world space position corresponding to the given screen position.
******************************************************************************/
Point3 PickingOpenGLSceneRenderer::worldPositionFromLocation(const QPoint& pos) const
{
	FloatType zvalue = depthAtPixel(pos);
	if(zvalue != 0) {
		OVITO_ASSERT(!framebufferImage().isNull());
		Point3 ndc(
				(FloatType)pos.x() / framebufferImage().width() * FloatType(2) - FloatType(1),
				1.0 - (FloatType)pos.y() / framebufferImage().height() * FloatType(2),
				zvalue * FloatType(2) - FloatType(1));
		Point3 worldPos = projParams().inverseViewMatrix * (projParams().inverseProjectionMatrix * ndc);
		return worldPos;
	}
	return Point3::Origin();
}

}	// End of namespace
