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

#include <ovito/core/Core.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/ImagePrimitive.h>
#include <ovito/core/rendering/TextPrimitive.h>
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    #include <ovito/core/utilities/io/video/VideoEncoder.h>
#endif

#include <QTextDocument>
#include <QTextFrame> 
#include <QTextFrameFormat> 
#include <QAbstractTextDocumentLayout> 

namespace Ovito {

#define IMAGE_FORMAT_FILE_FORMAT_VERSION        1

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBuffer::FrameBuffer(int width, int height, QObject* parent) : QObject(parent), _image(width, height, QImage::Format_ARGB32)
{
    _info.setImageWidth(width);
    _info.setImageHeight(height);
    clear();
}

/******************************************************************************
* Detects the file format based on the filename suffix.
******************************************************************************/
bool ImageInfo::guessFormatFromFilename()
{
    if(filename().endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
        setFormat("png");
        return true;
    }
    else if(filename().endsWith(QStringLiteral(".jpg"), Qt::CaseInsensitive) || filename().endsWith(QStringLiteral(".jpeg"), Qt::CaseInsensitive)) {
        setFormat("jpg");
        return true;
    }
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    for(const auto& videoFormat : VideoEncoder::supportedFormats()) {
        for(const QString& extension : videoFormat.extensions) {
            if(filename().endsWith(QStringLiteral(".") + extension, Qt::CaseInsensitive)) {
                setFormat(videoFormat.name);
                return true;
            }
        }
    }
#endif

    return false;
}

/******************************************************************************
* Returns whether the selected file format is a video format.
******************************************************************************/
bool ImageInfo::isMovie() const
{
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    for(const auto& videoFormat : VideoEncoder::supportedFormats()) {
        if(format() == videoFormat.name)
            return true;
    }
#endif

    return false;
}

/******************************************************************************
* Writes an ImageInfo to an output stream.
******************************************************************************/
SaveStream& operator<<(SaveStream& stream, const ImageInfo& i)
{
    stream.beginChunk(IMAGE_FORMAT_FILE_FORMAT_VERSION);
    stream << i._imageWidth;
    stream << i._imageHeight;
    stream << i._filename;
    stream << i._format;
    stream.endChunk();
    return stream;
}

/******************************************************************************
* Reads an ImageInfo from an input stream.
******************************************************************************/
LoadStream& operator>>(LoadStream& stream, ImageInfo& i)
{
    stream.expectChunk(IMAGE_FORMAT_FILE_FORMAT_VERSION);
    stream >> i._imageWidth;
    stream >> i._imageHeight;
    stream >> i._filename;
    stream >> i._format;
    stream.closeChunk();
    return stream;
}

/******************************************************************************
* Clears the framebuffer with a uniform color.
******************************************************************************/
void FrameBuffer::clear(const ColorA& color, const QRect& rect)
{
    QRect bufferRect = _image.rect();
    if(rect.isNull() || rect == bufferRect) {
        _image.fill(color);
        update(bufferRect);
    }
    else {
        QPainter painter(&_image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, color);
        update(rect);
    }
}

/******************************************************************************
* Renders an image primitive directly into the framebuffer.
******************************************************************************/
void FrameBuffer::renderImagePrimitive(const ImagePrimitive& primitive, const QRect& viewportRect, bool update)
{
    if(primitive.image().isNull())
        return;

    QPainter painter(&image());
    if(!viewportRect.isNull() && viewportRect != image().rect())
        painter.setClipRect(viewportRect);
    QRect rect(primitive.windowRect().minc.x(), primitive.windowRect().minc.y(), primitive.windowRect().width(), primitive.windowRect().height());
    painter.drawImage(rect, primitive.image());

    if(update)
        this->update(rect);
}

/******************************************************************************
* Renders a text primitive directly into the framebuffer.
******************************************************************************/
void FrameBuffer::renderTextPrimitive(const TextPrimitive& primitive, const QRect& viewportRect, bool update)
{
    if(primitive.text().isEmpty())
        return;

    // Determine whether the text primitive uses rich text formatting or not.
    Qt::TextFormat resolvedTextFormat = primitive.textFormat();
    if(resolvedTextFormat == Qt::AutoText)
        resolvedTextFormat = Qt::mightBeRichText(primitive.text()) ? Qt::RichText : Qt::PlainText;

    QPainter painter(&image());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(primitive.font());
    if(!viewportRect.isNull() && viewportRect != image().rect())
        painter.setClipRect(viewportRect);

    QRectF textBounds;
    if(resolvedTextFormat != Qt::RichText) {
        if(!primitive.useTightBox()) {
            textBounds = QFontMetricsF(primitive.font()).boundingRect(primitive.text());
        }
        else {
            QPainterPath textPath;
            textPath.addText(0, 0, primitive.font(), primitive.text());
            textBounds = textPath.boundingRect();
        }
    }
    else {
        QTextDocument doc;
        doc.setUndoRedoEnabled(false);
        doc.setHtml(primitive.text());
        doc.setDefaultFont(primitive.font());
        doc.setDocumentMargin(0);
        QTextOption opt = doc.defaultTextOption();
        opt.setAlignment(Qt::Alignment(primitive.alignment()));
        doc.setDefaultTextOption(opt);
        textBounds = QRectF(QPointF(0,0), doc.size());      
    }

    QPointF offset(-textBounds.left(), -textBounds.top());
    if(primitive.alignment() & Qt::AlignLeft) offset.rx() += primitive.position().x();
    else if(primitive.alignment() & Qt::AlignRight) offset.rx() += primitive.position().x() - textBounds.width();
    else if(primitive.alignment() & Qt::AlignHCenter) offset.rx() += primitive.position().x() - textBounds.width() / 2.0;
    
    if(primitive.alignment() & Qt::AlignTop) offset.ry() += primitive.position().y();
    else if(primitive.alignment() & Qt::AlignBottom) offset.ry() += primitive.position().y() - textBounds.height();
    else if(primitive.alignment() & Qt::AlignVCenter) offset.ry() += primitive.position().y() - textBounds.height() / 2.0;

    qreal outlineWidth = std::max(0.0, (primitive.outlineColor().a() > 0.0) ? (qreal)primitive.outlineWidth() : 0.0);

    QRectF updateRect(textBounds.left() + offset.x(), textBounds.top() + offset.y(), textBounds.width(), textBounds.height());
    if(outlineWidth != 0) {
        updateRect.adjust(-outlineWidth, -outlineWidth, outlineWidth, outlineWidth);
    }
    if(primitive.backgroundColor().a() > 0) {
        painter.fillRect(updateRect, (QColor)primitive.backgroundColor());
    }

    if(resolvedTextFormat != Qt::RichText) {
        if(outlineWidth != 0) {
            QPainterPath textPath;
            textPath.addText(offset, primitive.font(), primitive.text());
            painter.setPen(QPen(QBrush(primitive.outlineColor()), outlineWidth));
            painter.drawPath(textPath);
        }

        painter.setPen((QColor)primitive.color());
        painter.drawText(offset, primitive.text());
    }
    else {
        QTextDocument doc;
        doc.setUndoRedoEnabled(false);
        doc.setDefaultFont(primitive.font());
        doc.setHtml(primitive.text());
        // Remove document margin.
        doc.setDocumentMargin(0);
        // Specify document alignment.
        QTextOption opt = doc.defaultTextOption();
        opt.setAlignment(Qt::Alignment(primitive.alignment()));
        doc.setDefaultTextOption(opt);
        doc.setTextWidth(textBounds.width());
        // When rendering outlined text is requested, apply the outlined text style to the entire document.
        if(outlineWidth != 0) {
            QTextCursor cursor(&doc);
            cursor.select(QTextCursor::Document);
            QTextCharFormat charFormat;
            charFormat.setTextOutline(QPen(QBrush(primitive.outlineColor()), primitive.outlineWidth()));
            doc.setUndoRedoEnabled(true);
            cursor.mergeCharFormat(charFormat);
        }
        QAbstractTextDocumentLayout::PaintContext ctx;
        // Specify default text color:
        ctx.palette.setColor(QPalette::Text, (QColor)primitive.color());
        painter.translate(offset);
        doc.documentLayout()->draw(&painter, ctx);
        // When rendering outlined text, paint the text again on top without the outline 
        // in order to make the outline only go outward, not inward into the letters.
        if(outlineWidth != 0) {
            doc.undo();
            doc.documentLayout()->draw(&painter, ctx);
        }       
    }

    if(update)
        this->update(updateRect.toAlignedRect());
}

/******************************************************************************
* Removes unnecessary pixels along the outer edges of the image.
******************************************************************************/
bool FrameBuffer::autoCrop()
{
    QImage image = this->image().convertToFormat(QImage::Format_ARGB32);
    if(image.width() <= 0 || image.height() <= 0) 
        return false;

    auto determineCropRect = [&image](QRgb backgroundColor) -> QRect {
        int x1 = 0, y1 = 0;
        int x2 = image.width() - 1, y2 = image.height() - 1;
        bool significant;
        for(;; x1++) {
            significant = false;
            for(int y = y1; y <= y2; y++) {
                if(reinterpret_cast<const QRgb*>(image.constScanLine(y))[x1] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || x1 > x2)
                break;
        }
        for(; x2 >= x1; x2--) {
            significant = false;
            for(int y = y1; y <= y2; y++) {
                if(reinterpret_cast<const QRgb*>(image.constScanLine(y))[x2] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || x1 > x2)
                break;
        }
        for(;; y1++) {
            significant = false;
            const QRgb* s = reinterpret_cast<const QRgb*>(image.constScanLine(y1));
            for(int x = x1; x <= x2; x++) {
                if(s[x] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || y1 >= y2)
                break;
        }
        for(; y2 >= y1; y2--) {
            significant = false;
            const QRgb* s = reinterpret_cast<const QRgb*>(image.constScanLine(y2));
            for(int x = x1; x <= x2; x++) {
                if(s[x] != backgroundColor) {
                    significant = true;
                    break;
                }
            }
            if(significant || y1 > y2)
                break;
        }
        return QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    };

    // Use the pixel colors in the four corners of the images as candidate background colors.
    // Compute the crop rect for each candidate color and use the one that leads
    // to the smallest crop rect.
    QRect cropRect = determineCropRect(image.pixel(0,0));
    QRect r;
    r = determineCropRect(image.pixel(image.width()-1,0));
    if(r.width()*r.height() < cropRect.width()*cropRect.height()) cropRect = r;
    r = determineCropRect(image.pixel(image.width()-1,image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height()) cropRect = r;
    r = determineCropRect(image.pixel(0,image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height()) cropRect = r;

    if(cropRect != image.rect() && cropRect.width() > 0 && cropRect.height() > 0) {
        _image = _image.copy(cropRect);
        Q_EMIT bufferResized(_image.size());
        return true;
    }

    return false;
}

}   // End of namespace
