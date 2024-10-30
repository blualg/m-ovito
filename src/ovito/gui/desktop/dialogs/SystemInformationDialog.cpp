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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "SystemInformationDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
SystemInformationDialog::SystemInformationDialog(MainWindow& mainWindow, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("System Information"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    QTextEdit* textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(mainWindow.generateSystemReport());
    textEdit->setMinimumSize(QSize(600, 400));
    layout->addWidget(textEdit);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole), &QPushButton::clicked, [textEdit]() {
        QApplication::clipboard()->setText(textEdit->toPlainText());
    });
    layout->addWidget(buttonBox);
}

}   // End of namespace
