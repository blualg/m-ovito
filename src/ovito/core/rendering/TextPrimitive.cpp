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
#include <ovito/core/app/Application.h>
#include "TextPrimitive.h"
#include "RenderingJob.h"

#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QAbstractTextDocumentLayout>

namespace Ovito {

static void ensureFontRenderingCapability()
{
    try {
        Application::instance()->createQtApplication(false);
    }
    catch(const Exception& ex) {
        RendererException rendererEx(ex.messages());
        throw rendererEx.prependGeneralMessage(QStringLiteral("Qt font rendering function is not available in this environment."));
    }
}

/******************************************************************************
* Determines whether the text primitive uses rich text formatting or not.
******************************************************************************/
Qt::TextFormat TextPrimitive::resolvedTextFormat() const
{
    Qt::TextFormat format = textFormat();
    if(format == Qt::AutoText)
        format = Qt::mightBeRichText(text()) ? Qt::RichText : Qt::PlainText;
    return format;
}

/******************************************************************************
* Computes the bounds of the text in local coordinates, i.e., in a
* coordinate system that is aligned with the text. The bounds are computed as if
* the text was drawn at (0,0).
* Does NOT take into account text alignment, offset position, rotation, or outline width.
******************************************************************************/
QRectF TextPrimitive::queryLocalBounds(qreal devicePixelRatio, Qt::TextFormat textFormatHint) const
{
    ensureFontRenderingCapability();

    QRectF textBounds;
    Qt::TextFormat resolvedTextFormat = textFormat();
    if(resolvedTextFormat == Qt::AutoText) {
        if(textFormatHint != Qt::AutoText) resolvedTextFormat = textFormatHint;
        else resolvedTextFormat = Qt::mightBeRichText(text()) ? Qt::RichText : Qt::PlainText;
    }
#ifndef Q_OS_MACOS
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth(devicePixelRatio) == 0) {
#else
    // Workaround for macOS: When rendering text using the regular QPainter::drawText() method, the font size changes between GUI/CLI mode (for unknown reasons).
    // To avoid this problem, always use the rich-text rendering method in console mode.
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth(devicePixelRatio) == 0 && Application::instance()->guiMode()) {
#endif
        if(!useTightBox()) {
            textBounds = QFontMetricsF(font()).boundingRect(text());
        }
        else {
            QPainterPath textPath;
            textPath.addText(0, 0, font(), text());
            textBounds = textPath.boundingRect();
        }
        textBounds.moveTo(devicePixelRatio * textBounds.x(), devicePixelRatio * textBounds.y());
        textBounds.setWidth(devicePixelRatio * textBounds.width());
        textBounds.setHeight(devicePixelRatio * textBounds.height());
        // Add 1 pixel of horizontal padding as text bounds to not account for anti aliasing
        // From testing the vertical text bounds seem to be sufficient
        textBounds.adjust(-1, 0, 0, 1);
    }
    else {
        QTextDocument doc;
        doc.setUndoRedoEnabled(false);
        if(resolvedTextFormat == Qt::RichText)
            doc.setHtml(text());
        else
            doc.setPlainText(text());
        doc.setDefaultFont(font());
        doc.setDocumentMargin(0);
        QTextOption opt = doc.defaultTextOption();
        opt.setAlignment(Qt::Alignment(alignment()));
        doc.setDefaultTextOption(opt);
        textBounds = QRectF(QPointF(0,0), devicePixelRatio * doc.size());
    }

    return textBounds;
}

/******************************************************************************
* Computes the axis-aligned bounding rectangle of the text in the canvas coordinate system.
* This method takes into account text alignment, offset position, rotation, and outline width.
* This overload uses the pre-computed size of the text in the local coordinate system.
******************************************************************************/
QRectF TextPrimitive::computeBounds(const QSizeF textSize, qreal devicePixelRatio) const
{
    QRectF boundingRect(QPointF(0,0), textSize);

    // Apply horizontal alignment.
    if(alignment() & Qt::AlignRight)
        boundingRect.moveLeft(-textSize.width());
    else if(alignment() & Qt::AlignHCenter)
        boundingRect.moveLeft(-textSize.width() / 2);

    // Apply vertical alignment.
    if(alignment() & Qt::AlignBottom)
        boundingRect.moveTop(-textSize.height());
    else if(alignment() & Qt::AlignVCenter)
        boundingRect.moveTop(-textSize.height() / 2);

    // Apply rotation.
    if(rotation() != 0.0) {
        boundingRect = QTransform().rotateRadians(rotation()).mapRect(boundingRect);
    }

    // Apply translation.
    boundingRect.translate(position().x(), position().y());

    // Apply outline margin.
    qreal effectiveOutlineWidth = this->effectiveOutlineWidth(devicePixelRatio);
    boundingRect.adjust(-effectiveOutlineWidth, -effectiveOutlineWidth, effectiveOutlineWidth, effectiveOutlineWidth);

    return boundingRect;
}

/******************************************************************************
* Draws the text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::draw(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const
{
    ensureFontRenderingCapability();

#ifndef Q_OS_MACOS
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth() == 0) {
#else
    // Workaround for macOS: When rendering text using the regular QPainter::drawText() method, the font size changes between GUI/CLI mode (for unknown reasons).
    // To avoid this problem, always use the rich-text rendering method in console mode.
    if(resolvedTextFormat != Qt::RichText && effectiveOutlineWidth() == 0 && Application::instance()->guiMode()) {
#endif
        drawPlainText(painter);
    }
    else {
        drawRichText(painter, resolvedTextFormat, textWidth);
    }
}

/******************************************************************************
* Draws the unformatted text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::drawPlainText(QPainter& painter) const
{
    OVITO_ASSERT_MSG(this->effectiveOutlineWidth() == 0, "TextPrimitive::drawPlainText()", "Outline rendering is only supported by the drawRichText routine.");

    painter.setFont(font());
    painter.setPen((QColor)color());
    painter.drawText(QPointF(0,0), text());
}

/******************************************************************************
* Draws the formatted text (and optional outline) using a QPainter.
******************************************************************************/
void TextPrimitive::drawRichText(QPainter& painter, Qt::TextFormat resolvedTextFormat, qreal textWidth) const
{
    QTextDocument doc;
    doc.setUndoRedoEnabled(false);
    doc.setDefaultFont(font());
    if(resolvedTextFormat == Qt::RichText)
        doc.setHtml(text());
    else
        doc.setPlainText(text());
    // Remove document margin.
    doc.setDocumentMargin(0);
    // Specify document alignment.
    QTextOption opt = doc.defaultTextOption();
    opt.setAlignment(Qt::Alignment(alignment()));
    doc.setDefaultTextOption(opt);
    doc.setTextWidth(textWidth);
    // When rendering outlined text is requested, apply the outlined text style to the entire document.
    qreal effectiveOutlineWidth = this->effectiveOutlineWidth();
    if(effectiveOutlineWidth != 0) {
        QTextCursor cursor(&doc);
        cursor.select(QTextCursor::Document);
        QTextCharFormat charFormat;
        charFormat.setTextOutline(QPen(QBrush(outlineColor()), 2 * effectiveOutlineWidth));
        doc.setUndoRedoEnabled(true);
        cursor.mergeCharFormat(charFormat);
    }
    QAbstractTextDocumentLayout::PaintContext ctx;
    // Specify default text color:
    ctx.palette.setColor(QPalette::Text, (QColor)color());
    doc.documentLayout()->draw(&painter, ctx);
    // When rendering outlined text, paint the text again on top without the outline
    // in order to make the outline only go outward, not inward into the letters.
    if(effectiveOutlineWidth != 0) {
        doc.undo();
        doc.documentLayout()->draw(&painter, ctx);
    }
}

}   // End of namespace
