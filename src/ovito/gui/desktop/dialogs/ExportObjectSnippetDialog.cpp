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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include "ExportObjectSnippetDialog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ExportObjectSnippetDialog::ExportObjectSnippetDialog(const std::vector<OORef<RefTarget>>& objects, const QString& snippetDescription, const QString& usageNotice, MainWindowUI& ui, QWidget* parent) :
    QDialog(parent), UserInterfaceComponent<MainWindowUI>(ui)
{
    setWindowTitle(tr("Export as Snippet"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* label = new QLabel(usageNotice);
    label->setWordWrap(true);
    layout->addWidget(label);
    QTextEdit* textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setMinimumSize(QSize(600, 200));
    textEdit->setWordWrapMode(QTextOption::WrapAnywhere);
    textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(textEdit);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, [this]() {
        actionManager()->openHelpTopic("manual:object_snippets.export_snippet_dialog");
    });
    QPushButton* copyToClipboardBtn = buttonBox->addButton(tr("Copy to clipboard"), QDialogButtonBox::ActionRole);
    copyToClipboardBtn->setDefault(true);
    connect(copyToClipboardBtn, &QPushButton::clicked, [textEdit]() {
        QApplication::clipboard()->setText(textEdit->toPlainText());
    });
    layout->addWidget(buttonBox);

    textEdit->setPlainText(generateObjectSnippet(objects, snippetDescription));
}

/******************************************************************************
* Generates the text snippet representing the given list of objects.
******************************************************************************/
QString ExportObjectSnippetDialog::generateObjectSnippet(const std::vector<OORef<RefTarget>>& objects, const QString& snippetDescription)
{
    if(objects.empty())
        return {};

    try {
        QByteArray buffer;
        QDataStream dstream(&buffer, QIODevice::WriteOnly);
        ObjectSaveStream stream(dstream, true);
        stream.writeSizeT(objects.size());
        for(const auto& obj : objects) {
            stream.saveObject(obj, true);
        }
        stream.close();

        QByteArray compressed = qCompress(buffer, 9);
        QByteArray formatted = compressed.toBase64();

        QJsonObject dict;
        dict.insert(QStringLiteral("description"), QJsonValue(snippetDescription));
        dict.insert(QStringLiteral("payload"), QJsonValue(QString::fromLatin1(formatted)));
        QString snippet = QJsonDocument(dict).toJson(QJsonDocument::Compact);
        snippet.replace(QStringLiteral("\"description\":\""), QStringLiteral("\"description\": \"")); // insert spaces for better word-wrapping
        snippet.replace(QStringLiteral("\",\"payload\":"), QStringLiteral("\", \"payload\": ")); // insert spaces for better word-wrapping
        return snippet;
    }
    catch(const Exception& ex) {
        return tr("Warning - Could not generate object snippet:\n%1").arg(ex.messages().join(QStringLiteral("\n")));
    }
}

}   // End of namespace
