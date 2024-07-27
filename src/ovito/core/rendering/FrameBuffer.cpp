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
#include <ovito/core/rendering/ImagePrimitive.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#ifdef OVITO_VIDEO_OUTPUT_SUPPORT
    #include <ovito/core/utilities/io/video/VideoEncoder.h>
#endif

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBuffer::FrameBuffer(int width, int height, QObject* parent) : QObject(parent), _image(width, height, QImage::Format_ARGB32_Premultiplied)
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
    stream.beginChunk(0x01);
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
    stream.expectChunk(0x01);
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
void FrameBuffer::clear(const ColorA& color, const QRect& rect, bool delayed)
{
    commitChanges();
    if(!delayed) {
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
    else {
        _delayedClearRect = rect.isNull() ? _image.rect() : rect;
        _delayedClearColor = color;
    }
}

/******************************************************************************
* Performs a delayed clear buffer operation.
******************************************************************************/
void FrameBuffer::commitChanges()
{
    if(!_delayedClearRect.isNull()) {
        OVITO_ASSERT(!_image.isNull());
        QRect clearRect = std::exchange(_delayedClearRect, QRect()) & _image.rect();
        if(clearRect == _image.rect()) {
            _image.fill(_delayedClearColor);
        }
        else {
            OVITO_ASSERT(_image.format() == QImage::Format_ARGB32 || _image.format() == QImage::Format_ARGB32_Premultiplied);
            QRgb clearColor = _delayedClearColor.qrgb();
            if(_image.format() == QImage::Format_ARGB32_Premultiplied)
                clearColor = qPremultiply(clearColor);
            for(int y = clearRect.top(); y <= clearRect.bottom(); y++) {
                QRgb* dst = reinterpret_cast<QRgb*>(_image.scanLine(y)) + clearRect.left();
                std::fill(dst, dst + clearRect.width(), clearColor);
            }
        }
        _delayedUpdateRect |= clearRect;
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
    qreal dpr = image().devicePixelRatioF();
    QRectF rect(primitive.windowRect().minc.x() / dpr, primitive.windowRect().minc.y() / dpr, primitive.windowRect().width() / dpr, primitive.windowRect().height() / dpr);
    painter.drawImage(rect, primitive.image());
    painter.end();

    if(update)
        this->update(rect.toAlignedRect());
}

/******************************************************************************
 * Renders a text primitive directly into the framebuffer.
 ******************************************************************************/
void FrameBuffer::renderTextPrimitive(const TextPrimitive& primitive, const QRect& viewportRect, bool update)
{
    if(primitive.text().isEmpty())
        return;

    QPainter painter(&image());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if(!viewportRect.isNull() && viewportRect != image().rect())
        painter.setClipRect(viewportRect);

    Qt::TextFormat resolvedTextFormat = primitive.resolvedTextFormat();

    // Measure text size in local space (does NOT include alignment/offset/rotation/outline).
    // Bounds are calculated as if text was drawn at base coordinates (0,0).
    QRectF textBounds = primitive.queryLocalBounds(1.0, resolvedTextFormat);

    qreal dpr = image().devicePixelRatioF();
    painter.translate(primitive.position().x() / dpr, primitive.position().y() / dpr);
    if(primitive.rotation() != 0)
        painter.rotate(qRadiansToDegrees(primitive.rotation()));

    // Start with top-left alignment.
    QPointF textOffset(-textBounds.left(), -textBounds.top());

    // Apply horizontal alignment.
    if(primitive.alignment() & Qt::AlignRight)
        textOffset.rx() += -textBounds.width();
    else if(primitive.alignment() & Qt::AlignHCenter)
        textOffset.rx() += -textBounds.width() / 2;

    // Apply vertical alignment.
    if(primitive.alignment() & Qt::AlignBottom)
        textOffset.ry() += -textBounds.height();
    else if(primitive.alignment() & Qt::AlignVCenter)
        textOffset.ry() += -textBounds.height() / 2;

    painter.translate(textOffset);

    primitive.draw(painter, resolvedTextFormat, textBounds.width());
    painter.end();

    if(update) {
        QRectF boundingBox = primitive.computeBounds(textBounds.size(), 1.0);
        this->update(boundingBox.toAlignedRect());
    }
}

/******************************************************************************
* Renders a line primitive directly into the framebuffer (without depth-testing).
******************************************************************************/
void FrameBuffer::renderLinePrimitive(const LinePrimitive& primitive, const AffineTransformation& worldTransform, const ViewProjectionParameters& projectionParams, const QRect& viewportRect, bool update)
{
    if(!primitive.positions() || primitive.positions()->size() == 0)
        return;

    QPainter painter(&image());
    painter.setRenderHint(QPainter::Antialiasing);
    if(!viewportRect.isNull() && viewportRect != image().rect())
        painter.setClipRect(viewportRect);

    if(!primitive.colors())
        painter.setPen(primitive.uniformColor());
    RawBufferReadAccess colorAccess(primitive.colors());

    const qreal dpr = image().devicePixelRatioF();
    const bool preprojectedCoords = (worldTransform == AffineTransformation::Zero());

    primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>([&](auto _) {
        using T = decltype(_);
        using PointType = Point_3<T>;

        BufferReadAccess<PointType> posAccess(primitive.positions());
        size_t nlines = posAccess.size() / 2;

        const AffineTransformationT<T> modelView = (worldTransform * projectionParams.viewMatrix).toDataType<T>();
        const Matrix_4<T> projection = projectionParams.projectionMatrix.toDataType<T>();

        for(size_t i = 0; i < nlines; i++) {

            Vector_4<T> h1, h2;

            if(!preprojectedCoords) {
                // Transform to view space:
                PointType view_pos1 = modelView * posAccess[i*2+0];
                PointType view_pos2 = modelView * posAccess[i*2+1];

                // Project to homogeneous clip space:
                h1 = projection * Vector_4<T>(view_pos1.x(), view_pos1.y(), view_pos1.z(), 1);
                h2 = projection * Vector_4<T>(view_pos2.x(), view_pos2.y(), view_pos2.z(), 1);
            }
            else {
                // Us pre-projected coordinates directly:
                const PointType& pos1 = posAccess[i*2+0];
                const PointType& pos2 = posAccess[i*2+1];
                h1 = Vector_4<T>(pos1.x(), pos1.y(), pos1.z(), 1);
                h2 = Vector_4<T>(pos2.x(), pos2.y(), pos2.z(), 1);
            }

            // h1 and h2 describe the end points of a line segment in homogeneous coordinates.
            // Clip the line at the sides of the view frustum.

            // Check if both points are outside the view frustum
            if((h1.x() < -h1.w() && h2.x() < -h2.w()) || (h1.x() > h1.w() && h2.x() > h2.w()) ||
                (h1.y() < -h1.w() && h2.y() < -h2.w()) || (h1.y() > h1.w() && h2.y() > h2.w()) ||
                (h1.z() < -h1.w() && h2.z() < -h2.w()) || (h1.z() > h1.w() && h2.z() > h2.w()))
            {
                continue; // Line segment is completely outside the view frustum, skip rendering.
            }

            // Clip the line segment against the near plane of the view frustum
            if(h1.z() < -h1.w()) {
                Vector_4<T> intersection1 = h1 + (h2 - h1) * (-h1.w() / (h2.w() - h1.w()));
                h1 = intersection1;
            }
            if(h2.z() < -h2.w()) {
                Vector_4<T> intersection2 = h1 + (h2 - h1) * (-h1.w() / (h2.w() - h1.w()));
                h2 = intersection2;
            }

            // Clip the line segment against the left plane of the view frustum
            if(h1.x() < -h1.w()) {
                Vector_4<T> intersection1 = h1 + (h2 - h1) * ((-h1.w() - h1.x()) / ((h2.w() - h2.x()) - (h1.w() - h1.x())));
                h1 = intersection1;
            }
            if(h2.x() < -h2.w()) {
                Vector_4<T> intersection2 = h1 + (h2 - h1) * ((-h1.w() - h1.x()) / ((h2.w() - h2.x()) - (h1.w() - h1.x())));
                h2 = intersection2;
            }

            // Clip the line segment against the right plane of the view frustum
            if(h1.x() > h1.w()) {
                Vector_4<T> intersection1 = h1 + (h2 - h1) * ((h1.w() - h1.x()) / ((h2.w() - h2.x()) - (h1.w() - h1.x())));
                h1 = intersection1;
            }
            if(h2.x() > h2.w()) {
                Vector_4<T> intersection2 = h1 + (h2 - h1) * ((h1.w() - h1.x()) / ((h2.w() - h2.x()) - (h1.w() - h1.x())));
                h2 = intersection2;
            }

            // Clip the line segment against the bottom plane of the view frustum
            if(h1.y() < -h1.w()) {
                Vector_4<T> intersection1 = h1 + (h2 - h1) * ((-h1.w() - h1.y()) / ((h2.w() - h2.y()) - (h1.w() - h1.y())));
                h1 = intersection1;
            }
            if(h2.y() < -h2.w()) {
                Vector_4<T> intersection2 = h1 + (h2 - h1) * ((-h1.w() - h1.y()) / ((h2.w() - h2.y()) - (h1.w() - h1.y())));
                h2 = intersection2;
            }

            // Clip the line segment against the top plane of the view frustum
            if(h1.y() > h1.w()) {
                Vector_4<T> intersection1 = h1 + (h2 - h1) * ((h1.w() - h1.y()) / ((h2.w() - h2.y()) - (h1.w() - h1.y())));
                h1 = intersection1;
            }
            if(h2.y() > h2.w()) {
                Vector_4<T> intersection2 = h1 + (h2 - h1) * ((h1.w() - h1.y()) / ((h2.w() - h2.y()) - (h1.w() - h1.y())));
                h2 = intersection2;
            }

            // Project the clipped points to screen space (perspective division).
            Vector_4<T> screen_pos1 = h1 / h1.w();
            Vector_4<T> screen_pos2 = h2 / h2.w();

            QPointF p1, p2;
            p1.rx() = ( screen_pos1.x() + 1) * viewportRect.width()  / 2 + viewportRect.left();
            p1.ry() = (-screen_pos1.y() + 1) * viewportRect.height() / 2 + viewportRect.top();
            p2.rx() = ( screen_pos2.x() + 1) * viewportRect.width()  / 2 + viewportRect.left();
            p2.ry() = (-screen_pos2.y() + 1) * viewportRect.height() / 2 + viewportRect.top();
            p1.rx() /= dpr;
            p1.ry() /= dpr;
            p2.rx() /= dpr;
            p2.ry() /= dpr;

            if(colorAccess) {
                float r1 = colorAccess.get<float>(i*2+0, 0);
                float g1 = colorAccess.get<float>(i*2+0, 1);
                float b1 = colorAccess.get<float>(i*2+0, 2);
                float a1 = colorAccess.get<float>(i*2+0, 3);

                float r2 = colorAccess.get<float>(i*2+1, 0);
                float g2 = colorAccess.get<float>(i*2+1, 1);
                float b2 = colorAccess.get<float>(i*2+1, 2);
                float a2 = colorAccess.get<float>(i*2+1, 3);

                // Note: Only support for uniform line colors.
                if(r1 == r2 && g1 == g2 && b1 == b2 && a1 == a2) {
                    painter.setPen(QColor::fromRgbF(r1, g1, b1, a1));
                }
            }

            painter.drawLine(p1, p2);
        }
    });

    if(update)
        this->update(viewportRect);
}

/******************************************************************************
* Removes unnecessary pixels along the outer edges of the image.
******************************************************************************/
bool FrameBuffer::autoCrop()
{
    const QImage& image = this->image();
    OVITO_ASSERT(image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_ARGB32_Premultiplied);

    if(image.width() <= 0 || image.height() <= 0)
        return false;

    auto determineCropRect = [&](QRgb backgroundColor) -> QRect {
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
    r = determineCropRect(image.pixel(image.width()-1, 0));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;
    r = determineCropRect(image.pixel(image.width()-1, image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;
    r = determineCropRect(image.pixel(0, image.height()-1));
    if(r.width()*r.height() < cropRect.width()*cropRect.height())
        cropRect = r;

    if(cropRect != image.rect() && cropRect.width() > 0 && cropRect.height() > 0) {
        _image = _image.copy(cropRect);
        Q_EMIT bufferResized(_image.size());
        return true;
    }

    return false;
}

}   // End of namespace
