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
#include "AutocompleteTextEdit.h"
#include "AutocompleteEdit.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
AutocompleteTextEdit::AutocompleteTextEdit(QWidget* parent)
    : QPlainTextEdit(parent), _wordSplitter(AutocompleteEdit::wordSplitterExpression)
{
    _wordListModel = new QStringListModel(this);
    _completer = new QCompleter(this);
    _completer->setCompletionMode(QCompleter::PopupCompletion);
    _completer->setCaseSensitivity(Qt::CaseInsensitive);
    _completer->setModel(_wordListModel);
    _completer->setWidget(this);
    connect(_completer, qOverload<const QString&>(&QCompleter::activated), this, &AutocompleteTextEdit::onComplete);
}

/******************************************************************************
* Inserts a complete word into the text field.
******************************************************************************/
void AutocompleteTextEdit::onComplete(const QString& completion)
{
    const auto& [newCursorPos, newText] =
        AutocompleteEdit::completeExpression(textCursor().position(), toPlainText(), _wordSplitter, completion);

    // Set new text
    setPlainText(newText);

    // Set new position
    QTextCursor cursor = textCursor();
    cursor.setPosition((int)newCursorPos);
    setTextCursor(cursor);
}

/******************************************************************************
* Handles key-press events.
******************************************************************************/
void AutocompleteTextEdit::keyPressEvent(QKeyEvent* event)
{
    if(_completer->popup()->isVisible()) {
        if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
                event->key() == Qt::Key_Escape || event->key() == Qt::Key_Tab) {
                    event->ignore();
                    return;
        }
    }
    else if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if(commitOnReturn()) {
            Q_EMIT editingFinished();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    const auto [start, length] = AutocompleteEdit::getToken(textCursor().position(), toPlainText(), _wordSplitter);
    QString completionPrefix = toPlainText().mid(start, length);

    if(completionPrefix != _completer->completionPrefix()) {
        _completer->setCompletionPrefix(completionPrefix);
        _completer->popup()->setCurrentIndex(_completer->completionModel()->index(0,0));
    }
    if(!completionPrefix.isEmpty() && !_wordListModel->stringList().contains(completionPrefix)) {
        QRect cr = cursorRect();
        cr.setWidth(_completer->popup()->sizeHintForColumn(0)
                + _completer->popup()->verticalScrollBar()->sizeHint().width());
        _completer->complete(cr);
    }
    else
        _completer->popup()->hide();
}

/******************************************************************************
* Handles keyboard focus lost events.
******************************************************************************/
void AutocompleteTextEdit::focusOutEvent(QFocusEvent* event)
{
    if(event->reason() != Qt::PopupFocusReason || !(QApplication::activePopupWidget() && QApplication::activePopupWidget()->parentWidget() == this)) {
        Q_EMIT editingFinished();
    }
    QPlainTextEdit::focusOutEvent(event);
}

/******************************************************************************
* Returns the preferred size of the widget.
******************************************************************************/
QSize AutocompleteTextEdit::sizeHint() const
{
    QFontMetrics m(font());
    int lineHeight = m.lineSpacing();
    int numLines = 3;
    return QSize(QPlainTextEdit::sizeHint().width(), numLines * lineHeight);
}


}   // End of namespace
