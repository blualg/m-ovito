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
#include <ovito/core/rendering/FrameBuffer.h>
#include "RenderingJob.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(RenderingJob);
IMPLEMENT_ABSTRACT_OVITO_CLASS(AbstractRenderingFrameBuffer);

/******************************************************************************
 * Renders the 2d graphics of a frame graph render layer into the frame buffer.
 ******************************************************************************/
void RenderingJob::render2DPrimitives(FrameGraph::RenderLayer renderLayer, const FrameGraph& frameGraph, AbstractRenderingFrameBuffer& frameBuffer)
{
    if(!frameBuffer.outputFrameBuffer())
        return;

    for(const FrameGraph::RenderingCommand& command : frameGraph.commands()) {

        // Skip commands that are not relevant for the current rendering pass.
        if(command.skipInVisualPass() || command.renderLayer() != renderLayer)
            continue;

        if(const ImagePrimitive* primitive = dynamic_cast<const ImagePrimitive*>(command.primitive())) {
            frameBuffer.outputFrameBuffer()->renderImagePrimitive(*primitive, frameBuffer.outputViewportRect(), !frameGraph.isInteractive());
        }
        else if(const TextPrimitive* primitive = dynamic_cast<const TextPrimitive*>(command.primitive())) {
            frameBuffer.outputFrameBuffer()->renderTextPrimitive(*primitive, frameBuffer.outputViewportRect(), !frameGraph.isInteractive());
        }
        else if(const LinePrimitive* primitive = dynamic_cast<const LinePrimitive*>(command.primitive())) {
            frameBuffer.outputFrameBuffer()->renderLinePrimitive(*primitive, command.modelWorldTM(), frameGraph.projectionParams(), frameBuffer.outputViewportRect(), !frameGraph.isInteractive());
        }
    }
}

#ifdef OVITO_BUILD_BASIC
/******************************************************************************
 * Creates an image serving as watermark for demo versions of scene renderers.
 ******************************************************************************/
QImage RenderingJob::createWatermark(const QSize& size)
{
    static const QBrush watermarkBrush = []() {
        QFont font;
        font.setPointSize(24);
        QFontMetrics fm(font);
        QRect boundingRect = fm.boundingRect("OVITO Pro Demo");
        boundingRect.adjust(-10, -10, 10, 10);

        QImage watermark(boundingRect.size(), QImage::Format_ARGB32_Premultiplied);
        watermark.fill(QColor(0, 0, 0, 0));
        QPainter painter(&watermark);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(QColor(128, 128, 128, 128));
        painter.setFont(font);
        painter.drawText(watermark.rect(), Qt::AlignCenter, "OVITO Pro Demo");
        return QBrush(watermark);
    }();

    QImage watermark(size, QImage::Format_ARGB32_Premultiplied);
    watermark.fill(QColor(0, 0, 0, 0));
    QPainter painter(&watermark);
    painter.setBackground(watermarkBrush);
    painter.eraseRect(watermark.rect());

    return watermark;
}
#endif

}   // End of namespace
