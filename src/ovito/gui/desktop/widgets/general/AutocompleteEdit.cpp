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

namespace Ovito::AutocompleteEdit {

/******************************************************************************
 * Get the current token from the text string.
 * This will fail for strings with nested quotes.
 * Returns the starting index and the length of the token from the original string
 ******************************************************************************/
std::tuple<qsizetype, qsizetype> getToken(int curserPosition, const QString& expression, const QRegularExpression& splitExpression)
{
    OVITO_ASSERT(curserPosition <= expression.size());

    // '\0' used to delimit no (open) quote char
    QChar currentQuote = QChar::Null;

    // Find out what is the most recently opened quote char
    for(qsizetype i = 0; i < curserPosition; ++i) {
        if(expression[i] == '\'' || expression[i] == '\"') {
            currentQuote = (expression[i] == currentQuote) ? QChar::Null : expression[i];
        }
    }

    qsizetype start = 0;
    qsizetype end = expression.size();
    if(currentQuote != QChar::Null) {
        // Go through the expression from pos (exclusive) to 0
        // Determine the first currentQuote position
        for(qsizetype i = curserPosition - 1; i >= 0; --i) {
            if(expression[i] == currentQuote) {
                start = i;
                break;
            }
        }
        // Go through the expression from pos (inclusive) to the end
        // Determine the first currentQuote position
        for(qsizetype i = curserPosition; i < expression.size(); ++i) {
            if(expression[i] == currentQuote) {
                end = i;
                break;
            }
        }
    }
    else {
        OVITO_ASSERT(splitExpression.isValid());
        // Go through the expression from pos (exclusive) to 0
        // Determine the character that matches _wordSplitter
        for(qsizetype i = curserPosition - 1; i >= 0; --i) {
            if(!splitExpression.match(expression[i]).hasMatch()) {
                start = i + 1;
                break;
            }
        }
        // Go through the expression from pos (inclusive) to the end
        // Determine the character that matches _wordSplitter
        for(qsizetype i = curserPosition; i < expression.size(); ++i) {
            if(!splitExpression.match(expression[i]).hasMatch()) {
                end = i - 1;
                break;
            }
        }
    }
    OVITO_ASSERT(start >= 0 && end <= expression.size());
    // Return the text segment position
    return {start, end - start + 1};
}

/******************************************************************************
 * Calculates the new cursor position and the new string after successful insertion of completion into expression at the current position.
 ******************************************************************************/
std::tuple<qsizetype, QString> completeExpression(int curserPosition, const QString& expression, const QRegularExpression& splitExpression,
                                                  const QString& completion)
{
    // Get current token
    const auto [start, length] = getToken(curserPosition, expression, splitExpression);

    // Assemble new text
    QString newExpression = expression.first(start) + completion + expression.mid(start + length, expression.size() - (start + length));

    return {start + completion.size(), newExpression};
}

}  // namespace Ovito::AutocompleteEdit
