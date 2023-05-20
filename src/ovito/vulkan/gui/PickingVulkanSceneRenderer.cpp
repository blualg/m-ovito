////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "PickingVulkanSceneRenderer.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(PickingVulkanSceneRenderer);

/******************************************************************************
* Constructor.
******************************************************************************/
PickingVulkanSceneRenderer::PickingVulkanSceneRenderer(ObjectInitializationFlags flags, std::shared_ptr<VulkanContext> vulkanDevice, ViewportWindowInterface* window)
    : OffscreenVulkanSceneRenderer(flags, std::move(vulkanDevice), true), _window(window)
{
    setPicking(true);
    setInteractive(true);
}

/******************************************************************************
* This method is called just before renderFrame() is called.
******************************************************************************/
void PickingVulkanSceneRenderer::beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    // Caller should never provide an external frame buffer.
    OVITO_ASSERT(!frameBuffer);

    // Use our internal frame buffer instead.
    frameBuffer = &_frameBuffer;

    OffscreenVulkanSceneRenderer::beginFrame(time, scene, params, vp, viewportRect, frameBuffer);
}

/******************************************************************************
* Renders the current animation frame.
******************************************************************************/
bool PickingVulkanSceneRenderer::renderFrame(const QRect& viewportRect, MainThreadOperation& operation)
{
    // Caller should never provide an external frame buffer.
    OVITO_ASSERT(frameBuffer() == &_frameBuffer);

    // Clear previous object records.
    resetPickingBuffer();

    // Let the base class do the main rendering work.
    if(!OffscreenVulkanSceneRenderer::renderFrame(viewportRect, operation))
        return false;

    return true;
}

/******************************************************************************
* This method is called after renderFrame() has been called.
******************************************************************************/
void PickingVulkanSceneRenderer::endFrame(bool renderingSuccessful, const QRect& viewportRect)
{
    // Caller should never provide an external frame buffer.
    OVITO_ASSERT(frameBuffer() == &_frameBuffer);

    // Make sure old framebuffer content has been discarded, because we don't want OffscreenVulkanSceneRenderer::endFrame() to blend images.
    OVITO_ASSERT(_frameBuffer.image().isNull());

    // Let the base implementation fetch the Vulkan framebuffer contents.
    OffscreenVulkanSceneRenderer::endFrame(renderingSuccessful, viewportRect);
}

/******************************************************************************
* Resets the internal state of the picking renderer and clears the stored object records.
******************************************************************************/
void PickingVulkanSceneRenderer::resetPickingBuffer()
{
    _frameBuffer.image() = QImage();

    OffscreenVulkanSceneRenderer::resetPickingBuffer();
}

/******************************************************************************
* Returns the object record and the sub-object ID for the object at the given pixel coordinates.
******************************************************************************/
std::tuple<const SceneRenderer::ObjectPickingRecord*, quint32> PickingVulkanSceneRenderer::objectAtLocation(const QPoint& pos) const
{
    if(!_frameBuffer.image().isNull()) {
        if(pos.x() >= 0 && pos.x() < _frameBuffer.image().width() && pos.y() >= 0 && pos.y() < _frameBuffer.image().height()) {
            QRgb pixel = _frameBuffer.image().pixel(pos);
            quint32 red = qRed(pixel);
            quint32 green = qGreen(pixel);
            quint32 blue = qBlue(pixel);
            quint32 alpha = qAlpha(pixel);
            quint32 objectID = red + (green << 8) + (blue << 16) + (alpha << 24);
            if(const ObjectPickingRecord* objRecord = lookupObjectPickingRecord(objectID)) {
                quint32 subObjectID = objectID - objRecord->baseObjectID;
                for(const auto& range : objRecord->indexedRanges) {
                    if(subObjectID >= range.second && subObjectID < range.second + range.first->size()) {
                        subObjectID = range.second + BufferAccess<const int>(range.first).get(subObjectID - range.second);
                        break;
                    }
                }
                return std::make_tuple(objRecord, subObjectID);
            }
        }
    }
    return std::tuple<const ObjectPickingRecord*, quint32>(nullptr, 0);
}

/******************************************************************************
* Returns the world space position corresponding to the given screen position.
******************************************************************************/
Point3 PickingVulkanSceneRenderer::worldPositionFromLocation(const QPoint& pos) const
{
    FloatType zvalue = depthAtPixel(pos);
    if(zvalue != 0) {
        Point3 ndc(
                (FloatType)pos.x() / _frameBuffer.image().width() * FloatType(2) - FloatType(1),
                (FloatType)pos.y() / _frameBuffer.image().height() * FloatType(2) - FloatType(1),
                zvalue);
        Point3 worldPos = projParams().inverseViewMatrix * (projParams().inverseProjectionMatrix * clipCorrection().inverse() * ndc);
        return worldPos;
    }
    return Point3::Origin();
}

}   // End of namespace
