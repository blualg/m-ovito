////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 Alexander Stukowski
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
#include "StatusBar.h"

namespace Ovito {

/******************************************************************************
* Constructs a status bar widget.
******************************************************************************/
StatusBar::StatusBar(QWidget* parent) : QLabel(parent) 
{
    setMargin(2);
    setTextFormat(Qt::RichText);
    _overflowLabel = new QLabel(this);
    _overflowLabel->setMargin(margin());
    _overflowLabel->setAutoFillBackground(true);
    _overflowLabel->setAlignment(Qt::AlignLeading | Qt::AlignBottom);
    _overflowLabel->hide();
}

/******************************************************************************
* Displays the given message for the specified number of milli-seconds
******************************************************************************/
void StatusBar::showMessage(const QString& message, int timeout)
{
    if(timeout > 0) {
        if(!_timer) {
            _timer = new QTimer(this);
            connect(_timer, &QTimer::timeout, this, &StatusBar::clearMessage);
        }
        _timer->start(timeout);
    } 
    else if(_timer) {
        delete _timer;
        _timer = nullptr;
    }

    static const QString separatorMarker = QStringLiteral("<sep>");
    static const QString separatorText = QStringLiteral(" | ");
    static const QString separatorTextColored = QStringLiteral(" <font color=\"gray\">|</font> ");
    static const QString keyBeginMarker = QStringLiteral("<key>");
    const QString keyBeginText = QStringLiteral("<font color=\"%1\">").arg(palette().color(QPalette::Link).name());
    static const QString keyEndMarker = QStringLiteral("</key>");
    static const QString keyEndText = QStringLiteral("</font>");
    static const QString valueBeginMarker = QStringLiteral("<val>");
    static const QString valueBeginText = QStringLiteral("");
    static const QString valueEndMarker = QStringLiteral("</val>");
    static const QString valueEndText = QStringLiteral("");

    // Create a version of the message string that does not contain any markup.
    QString plainText = message;
    plainText.replace(separatorMarker, separatorText);
    plainText.remove(keyBeginMarker);
    plainText.remove(keyEndMarker);
    plainText.remove(valueBeginMarker);
    auto orgLength = plainText.length();
    plainText.remove(valueEndMarker);
    int nvalues = (orgLength - plainText.length()) / valueEndMarker.length();

    int availableSpace = contentsRect().width() - 2 * margin();
//    availableSpace -= 6 * nvalues;

    // Determine if the complete message fits into a single line of the status bar.
    QString elidedText = fontMetrics().elidedText(plainText, Qt::ElideRight, std::max(0, availableSpace));
    elidedText.remove(separatorText);
    plainText.remove(separatorText);
    auto iterPair = std::mismatch(plainText.cbegin(), plainText.cend(), elidedText.cbegin(), elidedText.cend());

    // If the elided text string and the original match completely, the text fits into a single line.
    if(iterPair.first == plainText.cend()) {
        QString richText = message;
        richText.replace(separatorMarker, separatorTextColored);
        richText.replace(keyBeginMarker, keyBeginText);
        richText.replace(keyEndMarker, keyEndText);
        richText.replace(valueBeginMarker, valueBeginText);
        richText.replace(valueEndMarker, valueEndText);
        setText(richText);
        _overflowLabel->hide();
        _overflowLabel->clear();
    }
    else {
        // Determine where to break the message string into two lines. 
        // Prefer breaking at a <sep> marker.
        QString firstLine;
        QString currentSpan;
        auto inputIter = message.cbegin();
        auto inputIterLast = inputIter;
        auto plainIterLast = plainText.cbegin();
        for(auto plainIter = plainText.cbegin(); plainIter != iterPair.first; ) {
            OVITO_ASSERT(inputIter < message.cend());
            auto result = std::mismatch(plainIter, iterPair.first, inputIter, message.cend());
            if(result.first == iterPair.first)
                break;
            currentSpan.append(&*plainIter, result.first - plainIter);
            plainIter = result.first;
            inputIter = result.second;
            if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(separatorMarker)) {
                firstLine.append(currentSpan);
                currentSpan = separatorText;
                inputIter += separatorMarker.length();
                inputIterLast = inputIter;
                plainIterLast = plainIter;
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(keyBeginMarker)) {
                currentSpan.append(keyBeginText);
                inputIter += keyBeginMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(keyEndMarker)) {
                currentSpan.append(keyEndText);
                inputIter += keyEndMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(valueBeginMarker)) {
                currentSpan.append(valueBeginText);
                inputIter += valueBeginMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(valueEndMarker)) {
                currentSpan.append(valueEndText);
                inputIter += valueEndMarker.length();
            }
            else break;
        }
        if(!firstLine.isEmpty()) {
            firstLine.replace(separatorText, separatorTextColored);
            _overflowLabel->setText(firstLine);
            _overflowLabel->show();
        }
        else {
            _overflowLabel->hide();
            _overflowLabel->clear();
        }

        QString secondLine;
        inputIter = inputIterLast;
        for(auto plainIter = plainIterLast; plainIter != plainText.cend(); ) {
            OVITO_ASSERT(inputIter < message.cend());
            auto result = std::mismatch(plainIter, plainText.cend(), inputIter, message.cend());
            secondLine.append(&*plainIter, result.first - plainIter);
            if(result.first == plainText.cend())
                break;
            plainIter = result.first;
            inputIter = result.second;
            if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(separatorMarker)) {
                secondLine.append(separatorText);
                inputIter += separatorMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(keyBeginMarker)) {
                secondLine.append(keyBeginText);
                inputIter += keyBeginMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(keyEndMarker)) {
                secondLine.append(keyEndText);
                inputIter += keyEndMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(valueBeginMarker)) {
                secondLine.append(valueBeginText);
                inputIter += valueBeginMarker.length();
            }
            else if(QStringView(&*inputIter, message.cend() - inputIter).startsWith(valueEndMarker)) {
                secondLine.append(valueEndText);
                inputIter += valueEndMarker.length();
            }
            else break;
        }
        secondLine.replace(separatorText, separatorTextColored);
        setText(secondLine);
    }
}

/******************************************************************************
* Removes any message being shown.
******************************************************************************/
void StatusBar::clearMessage()
{
    clear();
    _overflowLabel->hide();
    _overflowLabel->clear();
    if(_timer) {
        delete _timer;
        _timer = nullptr;
    }
}

/******************************************************************************
* Computes the preferred size of the status bar widget.
******************************************************************************/
QSize StatusBar::sizeHint() const
{
    if(_preferredHeight == 0)
        _preferredHeight = QLabel::sizeHint().height();
    return QSize(0, _preferredHeight);
}

/******************************************************************************
* Is called when the size of the status bar changes.
******************************************************************************/
void StatusBar::resizeEvent(QResizeEvent* event) 
{
    QWidget* parent = _overflowLabel->parentWidget();
    QPoint p = parent->mapFrom(window(), mapTo(window(), QPoint(0,0)));
    p.ry() += margin() * 2;
    QRect rect(p, QSize(event->size().width(), -event->size().height()));
    _overflowLabel->setGeometry(rect.normalized());

    QLabel::resizeEvent(event);
}

}	// End of namespace
