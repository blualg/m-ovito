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
constexpr int LineLabelHeight = 3;

/// Elides multi-line text to fit within a specified width and maximum number of lines.
/// If the last line is truncated and a " [show more]" suffix is appended.
[[nodiscard]] QString elideTextToRowCount(QString text, const int width, const int maxLines, const QFont& font)
{
    // QTextLayout requires QChar::LineSeparator instead of the common line break characters
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\r"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);

    // New layout
    QTextLayout layout(text, font);
    layout.beginLayout();

    // Output
    QStringList lines;

    // Split the text into lines
    QTextLine line;
    int lineCount = 0;

    // Suffix
    const QFontMetrics fm(font);
    static const QString suffix = QStringLiteral(" [show more]");
    const int suffixLengthPx = fm.horizontalAdvance(suffix);

    // Typset
    while((line = layout.createLine()).isValid()) {
        lineCount++;
        line.setLineWidth((qreal)width);
        const int start = line.textStart();
        const int length = line.textLength();
        QString lineText = text.mid(start, length);

        // Left and right padding (based on the non-adjustable top padding)
        const int leftRightPad = 2 * fm.descent();

        if(lineCount < maxLines) {
            // Full lines that fit
            lines << lineText;
        }
        else {
            // Line that needs to be ellided
            lineText = lineText.trimmed();

            // Space character width
            const int spacePx = fm.horizontalAdvance(QStringLiteral(" "));

            // Estimate and add bulk padding
            const int padding = (width - (fm.horizontalAdvance(lineText) + suffixLengthPx + leftRightPad)) / spacePx;
            if(padding > 0) {
                lineText.append(QStringLiteral(" ").repeated(padding));
            }

            // Incrementally finalize padding
            while(fm.horizontalAdvance(lineText) + suffixLengthPx + leftRightPad < width) {
                lineText.append(" ");
            }

            // Ensure that the total line length is not too long
            while(!lineText.isEmpty() && fm.horizontalAdvance(lineText) + suffixLengthPx + leftRightPad >= (width - 0.5)) {
                lineText.chop(1);
            }

            // Add suffix to line
            lines << lineText + suffix;
            break;
        }
    }
    layout.endLayout();

    return lines.join(QStringLiteral(""));
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
    setMargin(1);
    setContentsMargins(fontMetrics().descent(), 0, 0, 0);
    setAlignment(Qt::AlignTop);
    // Indent for a AlignTop is applied to the top AND the left - we don't want additional space on the top
    setIndent(0);

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
}

/******************************************************************************
 * Calculate the height of the widget based on the font and apply it
 ******************************************************************************/
void StatusWidget::setHeight()
{
    // Set height based on font
    QSize size = QLabel::minimumSizeHint();
    _height = LineLabelHeight * fontMetrics().lineSpacing();
    _height += margin() * 2;
    // Gap above the first row of text, needs to be manually added to make the bottom spacing look correct
    _height += fontMetrics().descent();

    // Apply height to widget
    setFixedHeight(_height);
    setMinimumHeight(_height);
    setMaximumHeight(_height);
}

[[nodiscard]] QSize StatusWidget::minimumSizeHint() const { return {QLabel::minimumSizeHint().width(), _height}; }

[[nodiscard]] QSize StatusWidget::sizeHint() const { return {QLabel::sizeHint().width(), _height}; }

/******************************************************************************
* Sets the status displayed by the widget.
******************************************************************************/
void StatusWidget::setStatus(const PipelineStatus& status)
{
    if(status.type() != _statusType) {
        _statusType = status.type();
        update();
    }
    // Prepend a single space to give the text some space on the left
    _statusText = status.text();
    // Shorten and set ellided text in widget
    updateAndElideWidgetText();

    // Set tooltip text to full message
    if(_tooltipPopupLabel) {
        _tooltipPopupLabel->setText(_statusText);
    }
}

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
        return QPalette::Mid;
}

/******************************************************************************
* Paints the widget's border.
******************************************************************************/
void StatusWidget::paintEvent(QPaintEvent* event)
{
    // Paint a border with a cosmetic pen.
    QPainter painter(this);
    const QColor& color = statusColor();
    painter.setPen(QPen(color, 0));
    if(_statusType == PipelineStatus::Warning || _statusType == PipelineStatus::Error) {
        // Add colored background for warnings and errors
        painter.setBrush(QBrush(QColor(color.red(), color.green(), color.blue(), int(255 * 0.2))));
    }
    painter.drawRect(QRectF(this->rect()).adjusted(0.0, 0.0, -0.5, -0.5));
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
    _tooltipPopupLabel->setText(_statusText);

    QPoint globalPos = mapToGlobal(QPoint(0, 0));
    globalPos.rx() += margin();
    globalPos.ry() += margin();
    int labelWidth = width() - 2 * margin();

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
 * Updates text elision based on current label width and applies it to the widget.
 ******************************************************************************/
void StatusWidget::updateAndElideWidgetText()
{
    if(_statusText.isEmpty()) {
        return;
    }

    setText(_statusText);
    if(heightForWidth(width()) > _height) {
        _elidedText = elideTextToRowCount(_statusText, width(), LineLabelHeight, font());
        setText(_statusText);
    }
}

/******************************************************************************
 * Handle widget resize to recalculate text elision.
 ******************************************************************************/
void StatusWidget::resizeEvent(QResizeEvent* event)
{
    updateAndElideWidgetText();
    setHeight();
    QLabel::resizeEvent(event);
}

}  // namespace Ovito
