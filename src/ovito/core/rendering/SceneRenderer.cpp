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
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/dataset/scene/SceneNode.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/viewport/ViewportGizmo.h>
#include <ovito/core/app/Application.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(SceneRenderer);
IMPLEMENT_ABSTRACT_OVITO_CLASS(ObjectPickInfo);

#if 0 // TODO
/******************************************************************************
* Sets the view projection parameters, the animation frame to render,
* and the viewport being rendered.
******************************************************************************/
void SceneRenderer::endFrame(bool renderingSuccessful, const QRect& viewportRect)
{
    endPickObject();
    _scene.reset();
    if(frameBuffer()) {
        if(renderingSuccessful)
            frameBuffer()->commitChanges();
        else
            frameBuffer()->discardChanges();
    }
}
#endif

/******************************************************************************
* Registers a range of sub-IDs belonging to the current object being rendered.
******************************************************************************/
quint32 SceneRenderer::registerSubObjectIDs(quint32 subObjectCount, const ConstDataBufferPtr& indices)
{
#if 0 // TODO
    quint32 baseObjectID = _nextAvailablePickingID;
    if(indices)
        _currentObjectPickingRecord.indexedRanges.push_back(std::make_pair(indices, _nextAvailablePickingID - _currentObjectPickingRecord.baseObjectID));
    _nextAvailablePickingID += subObjectCount;
    return baseObjectID;
#else
    return 0;
#endif
}

#if 0 // TODO
/******************************************************************************
* When picking mode is active, this registers an object being rendered.
******************************************************************************/
quint32 SceneRenderer::beginPickObject(const Pipeline* pipeline, ObjectPickInfo* pickInfo)
{
    _currentObjectPickingRecord.pipeline = const_cast<Pipeline*>(pipeline);
    _currentObjectPickingRecord.pickInfo = pickInfo;
    _currentObjectPickingRecord.baseObjectID = _nextAvailablePickingID;
    return _currentObjectPickingRecord.baseObjectID;
}

/******************************************************************************
* Call this when rendering of a pickable object is finished.
******************************************************************************/
void SceneRenderer::endPickObject()
{
    if(_currentObjectPickingRecord.pipeline) {
        _objectPickingRecords.push_back(std::move(_currentObjectPickingRecord));
    }
    _currentObjectPickingRecord.baseObjectID = 0;
    _currentObjectPickingRecord.pipeline = nullptr;
    _currentObjectPickingRecord.pickInfo = nullptr;
    _currentObjectPickingRecord.indexedRanges.clear();
}

/******************************************************************************
* Resets the internal state of the picking renderer and clears the stored object records.
******************************************************************************/
void SceneRenderer::resetPickingBuffer()
{
    endPickObject();
    _objectPickingRecords.clear();
#if 1
    _nextAvailablePickingID = 1;
#else
    // This can be enabled during debugging to avoid alpha!=1 pixels in the picking render buffer.
    _nextAvailablePickingID = 0xEF000000;
#endif
}

/******************************************************************************
* Given an object picking ID, looks up the corresponding record.
******************************************************************************/
const SceneRenderer::ObjectPickingRecord* SceneRenderer::lookupObjectPickingRecord(quint32 objectID) const
{
    if(objectID == 0 || _objectPickingRecords.empty())
        return nullptr;

    for(auto iter = _objectPickingRecords.begin(); iter != _objectPickingRecords.end(); iter++) {
        if(iter->baseObjectID > objectID) {
            OVITO_ASSERT(iter != _objectPickingRecords.begin());
            OVITO_ASSERT(objectID >= (iter-1)->baseObjectID);
            return &*std::prev(iter);
        }
    }

    OVITO_ASSERT(objectID >= _objectPickingRecords.back().baseObjectID);
    return &_objectPickingRecords.back();
}
#endif

#ifdef OVITO_BUILD_BASIC
/******************************************************************************
 * Creates an image serving as watermark for demo versions of scene renderers.
 ******************************************************************************/
QImage SceneRenderer::createWatermark(const QSize& size)
{
    static const QBrush watermarkBrush = []() {
        QFont font;
        font.setPointSize(24);
        QFontMetrics fm(font);
        QRect boundingRect = fm.boundingRect("OVITO Pro Demo");
        boundingRect.adjust(-10, -10, 10, 10);

        QImage watermark(boundingRect.size(), QImage::Format_ARGB32);
        watermark.fill(QColor(0, 0, 0, 0));
        QPainter painter(&watermark);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(QColor(255, 255, 255, 128));
        painter.setFont(font);
        painter.drawText(watermark.rect(), Qt::AlignCenter, "OVITO Pro Demo");
        return QBrush(watermark);
    }();

    QImage watermark(size, QImage::Format_ARGB32);
    watermark.fill(QColor(0, 0, 0, 0));
    QPainter painter(&watermark);
    painter.setBackground(watermarkBrush);
    painter.eraseRect(watermark.rect());
    return watermark;
}
#endif

}   // End of namespace
