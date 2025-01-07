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

#include <ovito/gui/desktop/GUI.h>
#include "AutocompleteLineEdit.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AutocompleteLineEdit::AutocompleteLineEdit(QWidget* parent) : EnterLineEdit(parent), _wordSplitter(R"([0-9a-zA-Z\.@\[\]])")
{
    _wordListModel = new QStringListModel(this);
    _completer = new QCompleter(this);
    _completer->setCompletionMode(QCompleter::PopupCompletion);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setModel(_wordListModel);
    _completer->setWidget(this);
    connect(_completer, qOverload<const QString&>(&QCompleter::activated), this, &AutocompleteLineEdit::onComplete);
}

/******************************************************************************
* Inserts a complete word into the text field.
******************************************************************************/
void AutocompleteLineEdit::onComplete(const QString& completion)
{
    const auto [start, length] = getToken();

    // Assemble and set new text
    setText(text().first(start) + completion + text().mid(start + length, text().size() - (start + length)));

    // Set new position
    setCursorPosition((int)(start + completion.size()));
}

/******************************************************************************
 * Get the current token from the text string.
 * This will fail for strings with nested quotes.
 * Returns the starting index and the length of the token from the original string
 ******************************************************************************/
std::tuple<qsizetype, qsizetype> AutocompleteLineEdit::getToken() const
{
    OVITO_ASSERT(cursorPosition() <= text().size());

    // '\0' used to delimit no (open) quote char
    QChar currentQuote = QChar::Null;

    // Find out what is the most recent opened quote char
    for(qsizetype i = 0; i < cursorPosition(); ++i) {
        if(text()[i] == '\'' || text()[i] == '\"') {
            currentQuote = (text()[i] == currentQuote) ? QChar::Null : text()[i];
        }
    }

    qsizetype start = 0;
    qsizetype end = text().size();
    if(currentQuote != QChar::Null) {
        // Go through the text() from pos (exclusive) to 0
        // Determine the first currentQuote position
        for(qsizetype i = cursorPosition() - 1; i >= 0; --i) {
            if(text()[i] == currentQuote) {
                start = i;
                break;
            }
        }
        // Go through the text() from pos (inclusive) to the end
        // Determine the first currentQuote position
        for(qsizetype i = cursorPosition(); i < text().size(); ++i) {
            if(text()[i] == currentQuote) {
                end = i;
                break;
            }
        }
    }
    else {
        OVITO_ASSERT(_wordSplitter.isValid());
        // Go through the text() from pos (exclusive) to 0
        // Determine the character that matches _wordSplitter
        for(qsizetype i = cursorPosition() - 1; i >= 0; --i) {
            if(!_wordSplitter.match(text()[i]).hasMatch()) {
                start = i + 1;
                break;
            }
        }
        // Go through the text() from pos (inclusive) to the end
        // Determine the character that matches _wordSplitter
        for(qsizetype i = cursorPosition(); i < text().size(); ++i) {
            if(!_wordSplitter.match(text()[i]).hasMatch()) {
                end = i - 1;
                break;
            }
        }
    }
    OVITO_ASSERT(start >= 0 && end <= text().size());
    // Return the text segment position
    return {start, end - start + 1};
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void AutocompleteLineEdit::keyPressEvent(QKeyEvent* event)
{
    if(_completer->popup()->isVisible()) {
        if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
                event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab) {
                    event->ignore();
                    return;
        }
    }

    QLineEdit::keyPressEvent(event);

    const auto [start, length] = getToken();
    QString completionPrefix = text().mid(start, length);

    if(completionPrefix != _completer->completionPrefix()) {
        _completer->setCompletionPrefix(completionPrefix);
        _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0,0));
    }
    if(!completionPrefix.isEmpty() && !_wordListModel->stringList().contains(completionPrefix))
        _completer->complete();
    else
        _completer->popup()->hide();
}

}   // End of namespace
