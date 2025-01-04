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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include "FrameBufferWidget.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
FrameBufferWidget::FrameBufferWidget(QWidget* parent) : QAbstractScrollArea(parent),
    _zoomAnimation(this, "zoomFactor"),
    _horizontalScrollAnimation(horizontalScrollBar(), "value"),
    _verticalScrollAnimation(verticalScrollBar(), "value")
{
    _zoomAnimation.setDuration(150);
    _zoomAnimation.setEasingCurve(QEasingCurve::OutQuad);
    _horizontalScrollAnimation.setDuration(_zoomAnimation.duration());
    _horizontalScrollAnimation.setEasingCurve(_zoomAnimation.easingCurve());
    _verticalScrollAnimation.setDuration(_zoomAnimation.duration());
    _verticalScrollAnimation.setEasingCurve(_zoomAnimation.easingCurve());

    // Pick dark gray as background color.
    QPalette pal = viewport()->palette();
    pal.setColor(QPalette::Window, QColor(38,38,38));
    viewport()->setPalette(std::move(pal));
    viewport()->setAutoFillBackground(false); // We fill the background in paintEvent().
    viewport()->setBackgroundRole(QPalette::Window);

    // Background for transparent framebuffer images.
    QImage img(32, 32, QImage::Format_RGB32);
    QPainter painter(&img);
    QColor c1(136, 136, 136);
    QColor c2(120, 120, 120);
    painter.fillRect(0, 0, 16, 16, c1);
    painter.fillRect(16, 16, 16, 16, c1);
    painter.fillRect(16, 0, 16, 16, c2);
    painter.fillRect(0, 16, 16, 16, c2);
    _backgroundBrush.setTextureImage(std::move(img));

    // Create the label that indicates the current zoom factor.
    _zoomFactorDisplay = new QLabel("Hello", this);
    _zoomFactorDisplay->hide();
    _zoomFactorDisplay->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    _zoomFactorDisplay->setIndent(6);
    QFont labelFont;
    labelFont.setBold(true);
    labelFont.setPointSizeF(1.5 * labelFont.pointSizeF());
    _zoomFactorDisplay->setFont(std::move(labelFont));

    _zoomLabelAnimation.setStartValue(1.0);
    _zoomLabelAnimation.setKeyValueAt(0.9, 1.0);
    _zoomLabelAnimation.setEndValue(0.0);
    _zoomLabelAnimation.setDuration(2000);
    connect(&_zoomLabelAnimation, &QAbstractAnimation::stateChanged, this, [this](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
        _zoomFactorDisplay->setVisible(newState == QAbstractAnimation::Running);
    });
    connect(&_zoomLabelAnimation, &QVariantAnimation::valueChanged, this, &FrameBufferWidget::zoomLabelAnimationChanged);
    zoomLabelAnimationChanged(_zoomLabelAnimation.startValue());
}

/******************************************************************************
* Sets the FrameBuffer that is currently shown in the widget.
******************************************************************************/
void FrameBufferWidget::setFrameBuffer(const std::shared_ptr<FrameBuffer>& newFrameBuffer)
{
    if(newFrameBuffer == frameBuffer()) {
        onFrameBufferResize();
        return;
    }

    if(frameBuffer()) {
        disconnect(_frameBuffer.get(), &FrameBuffer::contentChanged, this, &FrameBufferWidget::onFrameBufferContentChanged);
        disconnect(_frameBuffer.get(), &FrameBuffer::bufferResized, this, &FrameBufferWidget::onFrameBufferResize);
    }

    _frameBuffer = newFrameBuffer;

    connect(_frameBuffer.get(), &FrameBuffer::contentChanged, this, &FrameBufferWidget::onFrameBufferContentChanged);
    connect(_frameBuffer.get(), &FrameBuffer::bufferResized, this, &FrameBufferWidget::onFrameBufferResize);

    onFrameBufferResize();
}

/******************************************************************************
* Computes the preferred size of the viewport widget.
******************************************************************************/
QSize FrameBufferWidget::viewportSizeHint() const
{
    if(frameBuffer()) {
        return frameBuffer()->size() * zoomFactor();
    }
    return QAbstractScrollArea::viewportSizeHint();
}

/******************************************************************************
* Computes the preferred size of the scroll area widget.
******************************************************************************/
QSize FrameBufferWidget::sizeHint() const
{
    int f = 2 * frameWidth();
    return QSize(f, f) + viewportSizeHint();
}

/******************************************************************************
* Updates the scrollbars of the widget.
******************************************************************************/
void FrameBufferWidget::updateScrollBarRange()
{
    QSize areaSize = viewport()->size();
    QSize imageSize = frameBuffer() ? frameBuffer()->size() * zoomFactor() : QSize(0,0);
    verticalScrollBar()->setPageStep(areaSize.height() * ScrollBarScale);
    horizontalScrollBar()->setPageStep(areaSize.width() * ScrollBarScale);
    horizontalScrollBar()->setSingleStep(zoomFactor() * 8 * ScrollBarScale);
    verticalScrollBar()->setSingleStep(zoomFactor() * 8 * ScrollBarScale);
    verticalScrollBar()->setRange(0, (imageSize.height() - areaSize.height()) * ScrollBarScale);
    horizontalScrollBar()->setRange(0, (imageSize.width() - areaSize.width()) * ScrollBarScale);
}

/******************************************************************************
* Handles viewport resize events.
******************************************************************************/
void FrameBufferWidget::resizeEvent(QResizeEvent* event)
{
    updateScrollBarRange();
}

/******************************************************************************
* Calculates the drawing rectangle for the framebuffer image within the viewport.
******************************************************************************/
QRect FrameBufferWidget::calculateViewportRect() const
{
    QSize areaSize = viewport()->size();
    QSize imageSize = frameBuffer()->size() * zoomFactor();
    QPoint origin(-horizontalScrollBar()->value() / ScrollBarScale, -verticalScrollBar()->value() / ScrollBarScale);
    if(imageSize.width() < areaSize.width()) origin.rx() = (areaSize.width() - imageSize.width()) / 2;
    if(imageSize.height() < areaSize.height()) origin.ry() = (areaSize.height() - imageSize.height()) / 2;
    return QRect(origin, imageSize);
}

/******************************************************************************
* This is called by the system to paint the widgets area.
******************************************************************************/
void FrameBufferWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    if(frameBuffer()) {
        QRect imageRect = calculateViewportRect();
        if(!imageRect.contains(event->rect()))
            painter.eraseRect(event->rect());
        painter.setBrushOrigin(imageRect.topLeft());
        painter.fillRect(imageRect, _backgroundBrush);
        if(imageRect.width() < frameBuffer()->width() || imageRect.height() < frameBuffer()->height())
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(imageRect, frameBuffer()->displayImage());
    }
    else {
        painter.eraseRect(event->rect());
    }
}

/******************************************************************************
* Zooms in or out of the image.
******************************************************************************/
void FrameBufferWidget::setZoomFactor(qreal zoom)
{
    if(_zoomFactor != zoom) {
        _zoomFactor = zoom;
        _zoomFactorDisplay->setText(QStringLiteral("%1%").arg(int(std::round(zoomFactor() * 100))));
        _zoomFactorDisplay->resize(_zoomFactorDisplay->sizeHint());
        _zoomLabelAnimation.stop();
        _zoomLabelAnimation.start();
    }
    updateScrollBarRange();
    viewport()->update();
}

/******************************************************************************
* Smoothly adjusts the zoom factor.
******************************************************************************/
void FrameBufferWidget::zoomTo(qreal newZoomFactor)
{
    if(_zoomAnimation.state() != QAbstractAnimation::Stopped)
        return;
    qreal factor = newZoomFactor / zoomFactor();
    _zoomAnimation.setStartValue(zoomFactor());
    _zoomAnimation.setEndValue(newZoomFactor);
    _horizontalScrollAnimation.setStartValue((qreal)horizontalScrollBar()->value());
    _horizontalScrollAnimation.setEndValue(factor * horizontalScrollBar()->value() + ((factor - 1) * horizontalScrollBar()->pageStep() / 2));
    _verticalScrollAnimation.setStartValue((qreal)verticalScrollBar()->value());
    _verticalScrollAnimation.setEndValue(factor * verticalScrollBar()->value() + ((factor - 1) * verticalScrollBar()->pageStep() / 2));
    _zoomAnimation.start();
    _horizontalScrollAnimation.start();
    _verticalScrollAnimation.start();
}

/******************************************************************************
* Scales the image up.
******************************************************************************/
void FrameBufferWidget::zoomIn()
{
    zoomTo(std::min(ZoomFactorMax, zoomFactor() * ZoomIncrement));
}

/******************************************************************************
* Scales the image down.
******************************************************************************/
void FrameBufferWidget::zoomOut()
{
    zoomTo(std::max(ZoomFactorMin, zoomFactor() / ZoomIncrement));
}

/******************************************************************************
* Handles mouse wheel events.
******************************************************************************/
void FrameBufferWidget::wheelEvent(QWheelEvent* event)
{
    if(QPoint pixelDelta = event->pixelDelta(); !pixelDelta.isNull()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pixelDelta.x() * ScrollBarScale);
        verticalScrollBar()->setValue(verticalScrollBar()->value() - pixelDelta.y() * ScrollBarScale);
    }
    else if(QPoint degreeDelta = event->angleDelta() / 8; !degreeDelta.isNull()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - degreeDelta.x() * ScrollBarScale);
        verticalScrollBar()->setValue(verticalScrollBar()->value() - degreeDelta.y() * ScrollBarScale);
    }
    event->accept();
}

/******************************************************************************
* Handles mouse press events.
******************************************************************************/
void FrameBufferWidget::mousePressEvent(QMouseEvent* event)
{
    _mouseLastPosition = ViewportInputMode::getMousePosition(event);
    event->accept();
}

/******************************************************************************
* Handles mouse move events.
******************************************************************************/
void FrameBufferWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPointF mousePosition = ViewportInputMode::getMousePosition(event);
    QPoint pixelDelta = (mousePosition - _mouseLastPosition).toPoint();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - pixelDelta.x() * ScrollBarScale);
    verticalScrollBar()->setValue(verticalScrollBar()->value() - pixelDelta.y() * ScrollBarScale);
    _mouseLastPosition = mousePosition;
    event->accept();
}

/******************************************************************************
* Handles events of the viewport.
******************************************************************************/
bool FrameBufferWidget::viewportEvent(QEvent* event)
{
    if(event->type() == QEvent::NativeGesture) {
        QNativeGestureEvent* gesture = static_cast<QNativeGestureEvent*>(event);
        if(gesture->gestureType() == Qt::ZoomNativeGesture) {
            QPointF mousePos = ViewportInputMode::getMousePosition(gesture);
            qreal centerx = (mousePos.x() + horizontalScrollBar()->value() / ScrollBarScale) / zoomFactor();
            qreal centery = (mousePos.y() + verticalScrollBar()->value() / ScrollBarScale) / zoomFactor();
            qreal newZoomFactor = qBound(ZoomFactorMin, zoomFactor() * (1.0 + gesture->value()), ZoomFactorMax);
            setZoomFactor(newZoomFactor);
            horizontalScrollBar()->setValue((centerx * zoomFactor() - mousePos.x()) * ScrollBarScale);
            verticalScrollBar()->setValue((centery * zoomFactor() - mousePos.y()) * ScrollBarScale);
            return true;
        }
        else if(gesture->gestureType() == Qt::EndNativeGesture) {
            qreal roundedExponent = std::round(std::log(zoomFactor()) / std::log(ZoomIncrement));
            qreal roundedZoomFactor = std::pow(ZoomIncrement, roundedExponent);
            zoomTo(roundedZoomFactor);
        }
    }
    return QAbstractScrollArea::viewportEvent(event);
}

/******************************************************************************
* Handles bufferResized() signals from the frame buffer.
******************************************************************************/
void FrameBufferWidget::onFrameBufferResize()
{
    // Reset zoom factor.
    _zoomFactor = 1.0; // Reset value here to prevent the zoom indicator label from showing.
    qreal newZoomFactor = _zoomFactor;

    // Automatically reduce zoom factor to <100% to fit frame buffer window onto the user's screen.
    if(frameBuffer()) {
        if(QScreen* screen = this->screen()) {
            QSize availableSize = screen->availableSize();
            availableSize.setWidth(availableSize.width() * 2 / 3);
            availableSize.setHeight(availableSize.height() * 2 / 3);
            availableSize.rheight() -= 50; // Take into account toolbar and window title bar height.
            QSize zoomedSize = frameBuffer()->size();
            while(zoomedSize.width() > availableSize.width() || zoomedSize.height() > availableSize.height()) {
                if(newZoomFactor - 1e-9 <= ZoomFactorMin)
                    break;
                newZoomFactor /= ZoomIncrement * ZoomIncrement;
                zoomedSize = frameBuffer()->size() * newZoomFactor;
            }
        }
    }

    // Note: Setting the zoom factor automatically repaints the widget.
    setZoomFactor(newZoomFactor);
    updateGeometry();
}

/******************************************************************************
* This handles contentChanged() signals from the frame buffer.
******************************************************************************/
void FrameBufferWidget::onFrameBufferContentChanged(const QRect& changedRegion)
{
    // Repaint only a portion of the image.
    QRect vprect = calculateViewportRect();
    QSize imageSize = frameBuffer()->size();
    QRectF updateRect(
        (qreal)changedRegion.x() / imageSize.width()  * vprect.width()  + vprect.x(),
        (qreal)changedRegion.y() / imageSize.height() * vprect.height() + vprect.y(),
        (qreal)changedRegion.width() / imageSize.width()  * vprect.width(),
        (qreal)changedRegion.height() / imageSize.height()  * vprect.height());
    viewport()->update(updateRect.toAlignedRect());
}

/******************************************************************************
* Updates the transparency of the zoom value indicator.
******************************************************************************/
void FrameBufferWidget::zoomLabelAnimationChanged(const QVariant& value)
{
    QPalette palette = _zoomFactorDisplay->palette();
    QColor color(70, 70, 255);
    color.setAlphaF(value.value<qreal>());
    palette.setColor(_zoomFactorDisplay->foregroundRole(), color);
    _zoomFactorDisplay->setPalette(std::move(palette));
}

}   // End of namespace
