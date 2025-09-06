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

/******************************************************************************
* Constructor.
******************************************************************************/
StatusWidget::StatusWidget(QWidget* parent) : QWidget(parent)
{
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    _statusIndicator = new QWidget(this);
    _statusIndicator->setVisible(false);
    _statusIndicator->setBackgroundRole(QPalette::Window);
    _statusIndicator->setAutoFillBackground(true);
    _statusIndicator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    _statusIndicator->setMinimumWidth(4);
    _statusIndicator->setMaximumWidth(4);
    layout->addWidget(_statusIndicator);

    static constexpr int LineLabelHeight = 3;
    // Custom label class that has a fixed height, showing exactly LineLabelHeight lines of text.
    class TwoLineLabel : public QLabel
    {
    public:
        TwoLineLabel(QWidget* parent = nullptr) : QLabel(parent) {
            setWordWrap(true);
            setTextInteractionFlags(Qt::TextInteractionFlags(/*Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | */Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            setMargin(1);
        }

        QSize minimumSizeHint() const override {
            QSize size = QLabel::minimumSizeHint();
            int lineHeight = fontMetrics().lineSpacing();
            return QSize(size.width(), lineHeight * LineLabelHeight + margin() * 2);
        }

        QSize sizeHint() const override {
            QSize size = QLabel::sizeHint();
            int lineHeight = fontMetrics().lineSpacing();
            return QSize(size.width(), lineHeight * LineLabelHeight + margin() * 2);
        }

        /// This label has a fixed height.
        bool hasHeightForWidth() const override { return false; }
    };

    _textLabel = new TwoLineLabel(this);
    _textLabel->setAlignment(Qt::AlignTop);
    QFont font = _textLabel->font();
    font.setPointSize(font.pointSize() - 2);
    _textLabel->setFont(font);
    connect(_textLabel, &QLabel::linkActivated, this, &StatusWidget::linkActivated);
    layout->addWidget(_textLabel, 1, Qt::AlignTop);

    // Tooltip timer: when it fires, show a popup containing the full status text.
    _tooltipTimer = new QTimer(this);
    _tooltipTimer->setSingleShot(true);
    connect(_tooltipTimer, &QTimer::timeout, this, &StatusWidget::showTooltipPopup);

    // Install event filter on the label to monitor enter/leave and start/stop timer
    _textLabel->installEventFilter(this);
}

/******************************************************************************
* Sets the status displayed by the widget.
******************************************************************************/
void StatusWidget::setStatus(const PipelineStatus& status)
{
    if(status.type() != _statusType) {
        _statusType = status.type();
        if(status.type() != PipelineStatus::Success) {
            QPalette palette = _statusIndicator->palette();
            palette.setColor(QPalette::Window, statusColor());
            _statusIndicator->setPalette(palette);
            _statusIndicator->setVisible(true);
        }
        else {
            _statusIndicator->setVisible(false);
        }
        update();
    }
    _textLabel->setText(status.text());
    if(_tooltipPopupLabel)
        _tooltipPopupLabel->setText(status.text());
}

/******************************************************************************
* Returns the color indicating the current status.
******************************************************************************/
QColor StatusWidget::statusColor() const
{
    if(_statusType == PipelineStatus::Warning)
        return QColor(255, 130, 0);
    else if(_statusType == PipelineStatus::Error)
        return QColor(255, 0, 0);
    else
        return _statusIndicator->palette().color(QPalette::Mid);
}

/******************************************************************************
* Paints the widget's border.
******************************************************************************/
void StatusWidget::paintEvent(QPaintEvent* event)
{
    // Paint a border with a cosmetic pen.
    QPainter painter(this);
    painter.setPen(QPen(statusColor(), 0));
    painter.drawRect(QRectF(this->rect()).adjusted(0.0, 0.0, -0.5, -0.5));

    QWidget::paintEvent(event);
}

/****************************************************************************
* Shows the tooltip popup.
****************************************************************************/
void StatusWidget::showTooltipPopup()
{
    if(!_textLabel || _textLabel->text().isEmpty())
        return;

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
        _tooltipPopupLabel->setWordWrap(_textLabel->wordWrap());
        _tooltipPopupLabel->setAlignment(_textLabel->alignment());
        _tooltipPopupLabel->setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
        _tooltipPopupLabel->setForegroundRole(QPalette::ToolTipText);
        _tooltipPopupLabel->setFont(_textLabel->font());
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
        copyBtn->setFlat(true);
        copyBtn->setText(tr("Copy to clipboard"));
        copyBtn->setFont(_textLabel->font());
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
    _tooltipPopupLabel->setText(_textLabel->text());

    QPoint globalPos = _textLabel->mapToGlobal(QPoint(0,0));
    globalPos.rx() += _textLabel->margin();
    globalPos.ry() += _textLabel->margin();
    int labelWidth = _textLabel->width() - 2 * _textLabel->margin();

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
    if(watched == _textLabel) {
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

    return QWidget::eventFilter(watched, event);
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
    QWidget::mousePressEvent(event);
}

}   // End of namespace
