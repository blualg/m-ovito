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

#include <iostream>
#include <QApplication>
#include <QInputDialog>
#include <QTimer>

int main(int argc, char* argv[])
{
    QTimer::singleShot(0, []() {
        QInputDialog inputDialog;
        inputDialog.setWindowTitle(QStringLiteral("OVITO - SSH Connection"));
        inputDialog.setTextEchoMode(QLineEdit::Password);

        QString labelText = QStringLiteral("<p style=\"margin-right: 200px\">A password is required.</p>");
        QStringList arguments = QCoreApplication::arguments();
        if(arguments.count() > 1)
            labelText.append(QStringLiteral("<p>%1</p>").arg(arguments[1].toHtmlEscaped()));
        inputDialog.setLabelText(labelText);

        if(inputDialog.exec() == QDialog::Accepted) {
            std::cout << qPrintable(inputDialog.textValue()) << std::endl;
        }
        else {
            QCoreApplication::exit(-1);
        }
    });

    return QApplication(argc, argv).exec();
}
