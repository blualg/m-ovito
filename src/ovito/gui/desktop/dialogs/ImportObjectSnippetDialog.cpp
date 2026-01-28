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
#include <ovito/gui/desktop/mainwin/MainWindowUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "ImportObjectSnippetDialog.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ImportObjectSnippetDialog::ImportObjectSnippetDialog(const QString& objectType, MainWindowUI& ui, QWidget* parent) :
    QDialog(parent), UserInterfaceComponent<MainWindowUI>(ui), _objectType(objectType)
{
    setWindowTitle(tr("Import from Snippet"));
    resize(700, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Instructions label.
    QLabel* instructionLabel = new QLabel(tr("Paste a text snippet below to import OVITO %1(s):").arg(objectType));
    mainLayout->addWidget(instructionLabel);

    // Snippet text input.
    _snippetInput = new QPlainTextEdit(this);
    _snippetInput->setPlaceholderText(tr("Paste snippet here..."));
    _snippetInput->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    _snippetInput->setMaximumHeight(100);
    mainLayout->addWidget(_snippetInput);

    // Status widget for displaying errors.
    _statusWidget = new StatusWidget(this);
    mainLayout->addWidget(_statusWidget);

    // Splitter for list and properties panel (horizontal layout).
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    mainLayout->addWidget(splitter, 1);

    // List widget for displaying deserialized objects (on the left).
    _objectListWidget = new QListWidget(this);
    _objectListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    splitter->addWidget(_objectListWidget);

    // Properties panel for displaying selected object's parameters (on the right).
    _propertiesPanel = new PropertiesPanel(ui, this);
    _propertiesPanel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    splitter->addWidget(_propertiesPanel);
    splitter->setStretchFactor(1, 1);

    // Button box.
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
    _importButton = buttonBox->addButton(tr("Import"), QDialogButtonBox::AcceptRole);
    _importButton->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ImportObjectSnippetDialog::onImport);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::helpRequested, this, [this]() {
        actionManager()->openHelpTopic("manual:object_snippets.import_snippet_dialog");
    });
    mainLayout->addWidget(buttonBox);

    // Connect signals.
    connect(_snippetInput, &QPlainTextEdit::textChanged, this, &ImportObjectSnippetDialog::onSnippetTextChanged);
    connect(_objectListWidget, &QListWidget::currentRowChanged, this, &ImportObjectSnippetDialog::onObjectSelectionChanged);

    // Try to parse clipboard content on startup.
    QString clipboardText = QApplication::clipboard()->text();
    if(!clipboardText.isEmpty() && clipboardText.contains(QStringLiteral("\"payload\""))) {
        _snippetInput->setPlainText(clipboardText);
    }
}

/******************************************************************************
* Is called when the snippet text changes.
******************************************************************************/
void ImportObjectSnippetDialog::onSnippetTextChanged()
{
    QString snippetText = _snippetInput->toPlainText().trimmed();
    parseSnippet(snippetText);
}

/******************************************************************************
* Parses the snippet text and deserializes the objects.
******************************************************************************/
void ImportObjectSnippetDialog::parseSnippet(const QString& snippetText)
{
    // Clear previous state.
    _objects.clear();
    _objectListWidget->clear();
    _propertiesPanel->setEditObject(nullptr);
    _importButton->setEnabled(false);

    if(snippetText.isEmpty()) {
        _statusWidget->clearStatus();
        return;
    }

    try {
        // Parse JSON.
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(snippetText.toUtf8(), &parseError);
        if(parseError.error != QJsonParseError::NoError) {
            throw Exception(tr("This does not appear to be a valid OVITO snippet. Invalid JSON format: %1").arg(parseError.errorString()));
        }

        if(!doc.isObject()) {
            throw Exception(tr("Expected a JSON object."));
        }

        QJsonObject obj = doc.object();

        // Check for required fields.
        if(!obj.contains(QStringLiteral("payload"))) {
            throw Exception(tr("Missing 'payload' field in snippet."));
        }

        // Get description (optional).
        QString description = obj.value(QStringLiteral("description")).toString();

        // Decode payload.
        QString payloadBase64 = obj.value(QStringLiteral("payload")).toString();
        QByteArray compressed = QByteArray::fromBase64(payloadBase64.toLatin1());
        if(compressed.isEmpty()) {
            throw Exception(tr("Failed to decode base64 payload."));
        }

        // Decompress.
        QByteArray buffer = qUncompress(compressed);
        if(buffer.isEmpty()) {
            throw Exception(tr("Failed to decompress payload data."));
        }

        // Deserialize objects.
        QDataStream dstream(&buffer, QIODevice::ReadOnly);
        ObjectLoadStream stream(dstream, true); // Open stream in secure mode to prevent execution of untrusted code.

        size_t objectCount;
        stream.readSizeT(objectCount);

        for(size_t i = 0; i < objectCount; i++) {
            OORef<RefTarget> object = stream.loadObject<RefTarget>();
            if(object) {
                _objects.push_back(std::move(object));
            }
        }

        stream.close();

        if(_objects.empty()) {
            throw Exception(tr("No objects found in snippet."));
        }

        // Populate the list widget.
        static QIcon defaultIcon = QIcon::fromTheme("modify_modifier_action_icon");
        for(const auto& object : _objects) {
            QListWidgetItem* item = new QListWidgetItem(_objectListWidget);
            item->setText(object->objectTitle());
            item->setIcon(defaultIcon);
        }

        // Show success status.
        _statusWidget->setStatus(PipelineStatus(PipelineStatus::Success,
            tr("Successfully parsed %n %2(s) from snippet.", "", static_cast<int>(_objects.size())).arg(_objectType)));

        // Enable import button.
        _importButton->setEnabled(true);

        // Select the first item.
        if(_objectListWidget->count() > 0) {
            _objectListWidget->setCurrentRow(0);
        }
    }
    catch(const Exception& ex) {
        _statusWidget->setStatus(PipelineStatus(PipelineStatus::Error, ex.messages().join(QStringLiteral("\n"))));
    }
}

/******************************************************************************
* Is called when the user selects an object in the list.
******************************************************************************/
void ImportObjectSnippetDialog::onObjectSelectionChanged()
{
    int currentRow = _objectListWidget->currentRow();
    if(currentRow >= 0 && currentRow < static_cast<int>(_objects.size())) {
        _propertiesPanel->setEditObject(_objects[currentRow]);
    }
    else {
        _propertiesPanel->setEditObject(nullptr);
    }
}

/******************************************************************************
* Is called when the user presses the 'Import' button.
******************************************************************************/
void ImportObjectSnippetDialog::onImport()
{
    if(_objects.empty())
        return;

    accept();
}

}   // End of namespace
