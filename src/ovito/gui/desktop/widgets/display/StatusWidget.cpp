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
#include "StatusWidget.h"

namespace Ovito {

namespace {
QColor alphaBlendColors(const QColor& foreground, const QColor& background)
{
    const qreal a = foreground.alphaF();
    const qreal invA = 1.0 - a;
    const int r = qRound(foreground.red() * a + background.red() * invA);
    const int g = qRound(foreground.green() * a + background.green() * invA);
    const int b = qRound(foreground.blue() * a + background.blue() * invA);
    return {r, g, b};
}
}  // namespace

/******************************************************************************
 * Constructor.
 ******************************************************************************/
StatusWidget::StatusWidget(QWidget* parent) : QLabel(parent)
{
    setWordWrap(true);
    setTextInteractionFlags(Qt::TextInteractionFlags(
        /*Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | */ Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Set margins
    setContentsMargins(fontMetrics().descent() + 1, 1, fontMetrics().descent() + 1, 1);

    // Set alignment
    setAlignment(Qt::AlignTop);

    // Indent for a AlignTop is applied to the top AND the left - we don't want additional space on the top
    setIndent(0);

    // Set colors
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);

    // Calculate the height of the widget based on the font and apply it
    setHeight();

    connect(this, &QLabel::linkActivated, this, &StatusWidget::linkActivated);

    // Tooltip timer: when it fires, show a popup containing the full status text.
    _tooltipTimer = new QTimer(this);
    _tooltipTimer->setSingleShot(true);
    connect(_tooltipTimer, &QTimer::timeout, this, &StatusWidget::showTooltipPopup);

    // Install event filter on the label to monitor enter/leave and start/stop timer
    installEventFilter(this);

    // Create overlay label
    _overlayLabel = new QLabel(tr("[show more]"), this);
    _overlayLabel->setBackgroundRole(QPalette::Window);
    _overlayLabel->setFrameStyle(QFrame::NoFrame);
    _overlayLabel->setLineWidth(1);
    _overlayLabel->setMargin(0);
    _overlayLabel->setIndent(0);
    _overlayLabel->setAlignment(Qt::AlignCenter);
    // Adjust size call is necessary to give correct positions
    _overlayLabel->adjustSize();
    toggleOverlayLabelVisibility();
}

/******************************************************************************
 * Calculate the height of the widget based on the font
 ******************************************************************************/
[[nodiscard]] int StatusWidget::calculateHeight() const
{
    // Number of lines shown in the widget
    constexpr int LineLabelHeight = 3;

    // Set height based on font
    int height = LineLabelHeight * fontMetrics().lineSpacing();
    height += contentsMargins().top() + contentsMargins().bottom();
    // Gap above the first row of text, needs to be manually added to make the bottom spacing look correct
    height += fontMetrics().descent();
    return height;
}

/******************************************************************************
 * Apply the widget height
 ******************************************************************************/
void StatusWidget::setHeight()
{
    const int height = calculateHeight();
    // Apply height to widget
    setFixedHeight(height);
    setMinimumHeight(height);
    setMaximumHeight(height);
}

[[nodiscard]] QSize StatusWidget::minimumSizeHint() const { return {QLabel::minimumSizeHint().width(), calculateHeight()}; }

[[nodiscard]] QSize StatusWidget::sizeHint() const { return {QLabel::sizeHint().width(), calculateHeight()}; }

/******************************************************************************
* Sets the status displayed by the widget.
******************************************************************************/
void StatusWidget::setStatus(const PipelineStatus& status)
{
    if(status.type() != _statusType) {
        _statusType = status.type();
        update();
    }

    // Widget text
    setText(status.text());
    toggleOverlayLabelVisibility();

   // Set tooltip text
   if(_tooltipPopupLabel) {
       _tooltipPopupLabel->setText(text());
    }
}

/******************************************************************************
 * Returns the default color indicating a success status.
 ******************************************************************************/
[[nodiscard]] QColor StatusWidget::defaultColor() const { return palette().color(QPalette::Mid); }

/******************************************************************************
* Returns the color indicating the current status.
******************************************************************************/
QColor StatusWidget::statusColor() const
{
    if(_statusType == PipelineStatus::Warning)
        return {251, 153, 0};
    else if(_statusType == PipelineStatus::Error)
        return {204, 2, 2};
    else
        return palette().color(QPalette::Mid);
}

/******************************************************************************
* Paints the widget's border.
******************************************************************************/
void StatusWidget::paintEvent(QPaintEvent* event)
{
    // Paint the main QLabel area.
    QPainter painter(this);
    const QColor& color = statusColor();
    const QColor& backgroundColor = palette().color(QPalette::Window);
    const QColor& highlightColor = alphaBlendColors(QColor(color.red(), color.green(), color.blue(), int(255 * 0.2)), backgroundColor);

    painter.setPen(QPen(color, 0));
    if(_statusType == PipelineStatus::Warning || _statusType == PipelineStatus::Error) {
        // Add colored background for warnings and errors
        painter.setBrush(highlightColor);
    }
    painter.drawRect(QRectF(this->rect()).adjusted(0.0, 0.0, -0.5, -0.5));

    // Paint the [show more] overlay area to match
    painter.setPen(Qt::NoPen);
    if(_overlayLabel && _overlayLabel->isVisible()) {
        QRect overlayRect = _overlayLabel->rect();
        // Shift overlay rectangle to the correct position
        const QPoint shift = calculateOverlayLabelPosition();
        overlayRect.adjust(shift.x(), shift.y(), shift.x(), shift.y());

        if(_statusType == PipelineStatus::Warning || _statusType == PipelineStatus::Error) {
            // Use the same colored background as the main qlabel background
            painter.setBrush(alphaBlendColors(highlightColor, backgroundColor));
        }
        else {
            // Use window background for normal status
            painter.setBrush(backgroundColor);
        }
        painter.drawRect(overlayRect);
    }

    QLabel::paintEvent(event);
}

/****************************************************************************
* Shows the tooltip popup.
****************************************************************************/
void StatusWidget::showTooltipPopup()
{
    if(text().isEmpty()) {
        return;
    }

    // Create popup widget lazily
    if(!_tooltipPopup) {
        _tooltipPopup = new QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
        _tooltipPopup->setAttribute(Qt::WA_ShowWithoutActivating);
        connect(this, &QObject::destroyed, _tooltipPopup, &QObject::deleteLater);
        _tooltipPopup->setBackgroundRole(QPalette::ToolTipBase);
        _tooltipPopup->setAutoFillBackground(true);
        QVBoxLayout* vlay = new QVBoxLayout(_tooltipPopup);
        vlay->setContentsMargins(0,0,0,0);
        vlay->setSpacing(0);
        _tooltipPopupLabel = new QLabel(_tooltipPopup);
        _tooltipPopupLabel->setWordWrap(wordWrap());
        _tooltipPopupLabel->setAlignment(alignment());
        _tooltipPopupLabel->setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
        _tooltipPopupLabel->setForegroundRole(QPalette::ToolTipText);
        _tooltipPopupLabel->setFont(font());
        connect(_tooltipPopupLabel, &QLabel::linkActivated, this, &StatusWidget::linkActivated);
        vlay->addWidget(_tooltipPopupLabel);

        // Add a small tool button at the bottom-right to copy the full
        // status text to the clipboard.
        QHBoxLayout* btnLay = new QHBoxLayout();
        btnLay->setContentsMargins(1, 1, 1, 1);
        btnLay->addStretch(1);
        QPushButton* copyBtn = new QPushButton(_tooltipPopup);
        QIcon copyIcon = QIcon::fromTheme("edit-copy", _tooltipPopup->style()->standardIcon(QStyle::SP_DialogOpenButton));
        copyBtn->setIcon(copyIcon);
        copyBtn->setFlat(false);
        copyBtn->setText(tr("Copy to clipboard"));
        copyBtn->setFont(font());
        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            if(!_tooltipPopupLabel)
                return;
            QClipboard* cb = qApp->clipboard();
            cb->setText(_tooltipPopupLabel->hasSelectedText() ? _tooltipPopupLabel->selectedText() : _tooltipPopupLabel->text());
        });
        btnLay->addWidget(copyBtn);
        btnLay->addStretch(1);
        vlay->addLayout(btnLay);

        // Install event filter so we can detect leave events on the popup
        _tooltipPopup->installEventFilter(this);
    }

    // Set contents and size. Fix the popup label width to the embedded
    // label's width so word-wrapping matches and we can compute the full
    // required height to display all lines.
    _tooltipPopupLabel->setText(text());

    QPoint globalPos = mapToGlobal(QPoint(0, 0));
    globalPos.rx() += contentsMargins().left();
    globalPos.ry() += contentsMargins().top();
    int labelWidth = width() - contentsMargins().left() - contentsMargins().right();

    // Ensure the popup label wraps at the same width as the embedded label.
    _tooltipPopupLabel->setFixedWidth(labelWidth);

    // Let the popup adjust to the new content size (height will expand
    // to fit all wrapped lines). Use sizeHint to include layout margins.
    _tooltipPopup->adjustSize();
    QSize popupSize = _tooltipPopup->sizeHint();

    // Position the popup at the label's top-left and set the computed size.
    QRect popupRect(globalPos, popupSize);
    // Ensure width is at least the label width.
    if(popupRect.width() < labelWidth)
        popupRect.setWidth(labelWidth);

    _tooltipPopup->setGeometry(popupRect);
    _tooltipPopup->show();
}

/******************************************************************************
* Event filter to watch the internal label and the tooltip popup for
* enter/leave events.
******************************************************************************/
bool StatusWidget::eventFilter(QObject* watched, QEvent* event)
{
    // If the event comes from the label, watch for enter/leave to start/stop timer
    if(watched == this) {
        if(event->type() == QEvent::Enter) {
            if(_tooltipTimer)
                _tooltipTimer->start(_tooltipDelayMs);
        }
        else if(event->type() == QEvent::Leave) {
            if(_tooltipTimer)
                _tooltipTimer->stop();
            // If the popup is visible and the cursor is not over it, close it
            if(_tooltipPopup && _tooltipPopup->isVisible()) {
                QPoint g = QCursor::pos();
                if(!_tooltipPopup->geometry().contains(g))
                    _tooltipPopup->hide();
            }
        }
    }

    // If the event comes from the popup or its label, close when the mouse leaves
    if(watched == _tooltipPopup && event->type() == QEvent::Leave) {
        if(_tooltipPopup)
            _tooltipPopup->hide();
    }

    return QLabel::eventFilter(watched, event);
}

/*****************************************************************************
* Mouse press event handler.
*****************************************************************************/
void StatusWidget::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        // show immediately
        if(_tooltipTimer && _tooltipTimer->isActive())
            _tooltipTimer->stop();
        showTooltipPopup();
    }
    QLabel::mousePressEvent(event);
}

/******************************************************************************
 * Sets the visibility of the overlay label based on the status text.
 ******************************************************************************/
bool StatusWidget::toggleOverlayLabelVisibility()
{
    const bool newVisibility = heightForWidth(width()) > calculateHeight();
    _overlayLabel->setVisible(newVisibility);
    return newVisibility;
}

/******************************************************************************
 * Calculate the position of the overlay label.
 ******************************************************************************/
QPoint StatusWidget::calculateOverlayLabelPosition() const
{
    const int x = width() - _overlayLabel->width() - contentsMargins().right();
    const int y = height() - _overlayLabel->height() - contentsMargins().bottom() - fontMetrics().descent();
    return {x, y};
}

/******************************************************************************
 * Handle widget resize to recalculate text elision.
 ******************************************************************************/
void StatusWidget::resizeEvent(QResizeEvent* event)
{
    setHeight();
    toggleOverlayLabelVisibility();

    // Position overlay label in bottom right corner
    if(_overlayLabel && _overlayLabel->isVisible()) {
        _overlayLabel->move(calculateOverlayLabelPosition());
    }

    QLabel::resizeEvent(event);
}

}  // namespace Ovito
