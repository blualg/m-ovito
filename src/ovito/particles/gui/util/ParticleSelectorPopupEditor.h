////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#pragma once

#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace Ovito {

inline QWidget* createSelectorPopupRow(QWidget* parent,
                                       QWidget* mainEditorWidget,
                                       StringParameterUI* expressionUI,
                                       const QString& popupTitle,
                                       const QString& helperText)
{
    OVITO_ASSERT(parent);
    OVITO_ASSERT(mainEditorWidget);
    OVITO_ASSERT(expressionUI);
    OVITO_ASSERT(expressionUI->lineEdit());

    auto* rowWidget = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);
    rowLayout->addWidget(mainEditorWidget);

    auto* button = new QToolButton(rowWidget);
    button->setText(QStringLiteral("fx"));
    button->setAutoRaise(true);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(QObject::tr("Edit expression override"));
    rowLayout->addWidget(button);

    auto updateButtonState = [button, lineEdit = expressionUI->lineEdit()]() {
        const bool active = !lineEdit->text().trimmed().isEmpty();
        button->setChecked(active);
        button->setText(active ? QStringLiteral("fx*") : QStringLiteral("fx"));
        button->setToolTip(active
            ? QObject::tr("Expression override active. Click to edit or clear it.")
            : QObject::tr("Edit expression override"));
    };
    updateButtonState();
    QObject::connect(expressionUI->lineEdit(), &QLineEdit::textChanged, button, updateButtonState);

    QObject::connect(button, &QToolButton::clicked, rowWidget,
                     [rowWidget, expressionUI, popupTitle, helperText, updateButtonState]() {
        QDialog dialog(rowWidget);
        dialog.setWindowTitle(popupTitle);
        dialog.setModal(true);

        auto* dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->setContentsMargins(10, 10, 10, 10);
        dialogLayout->setSpacing(6);

        auto* descriptionLabel = new QLabel(helperText, &dialog);
        descriptionLabel->setWordWrap(true);
        dialogLayout->addWidget(descriptionLabel);

        auto* expressionEdit = new QPlainTextEdit(&dialog);
        expressionEdit->setPlainText(expressionUI->lineEdit()->text());
        expressionEdit->setMinimumWidth(360);
        expressionEdit->setMinimumHeight(90);
        dialogLayout->addWidget(expressionEdit);

        auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QPushButton* clearButton = buttonBox->addButton(QObject::tr("Clear"), QDialogButtonBox::ResetRole);
        QObject::connect(clearButton, &QPushButton::clicked, expressionEdit, &QPlainTextEdit::clear);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialogLayout->addWidget(buttonBox);

        if(dialog.exec() == QDialog::Accepted)
            expressionUI->lineEdit()->setText(expressionEdit->toPlainText().trimmed());
        else
            updateButtonState();
    });

    return rowWidget;
}

}   // End of namespace Ovito
