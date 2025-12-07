////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include "RenderingJob.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(RenderingJob);

#ifdef OVITO_BUILD_BASIC
/******************************************************************************
 * Creates an image serving as watermark for demo versions of scene renderers.
 ******************************************************************************/
QImage RenderingJob::createWatermark(const QSize& size)
{
    static const QBrush watermarkBrush = []() {
        QFont font;
        font.setPointSize(36);
        font.setBold(true);
        QFontMetrics fm(font);
        QRect boundingRect = fm.boundingRect("OVITO Pro Demo");
        boundingRect.adjust(-20, -20, 20, 20);

        QImage watermark(boundingRect.size(), QImage::Format_ARGB32_Premultiplied);
        watermark.fill(QColor(0, 0, 0, 0));
        QPainter painter(&watermark);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(QColor(128, 128, 128, 255));
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
