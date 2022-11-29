////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/core/utilities/SortZipped.h>
#include "ImportRemoteFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
ImportRemoteFileDialog::ImportRemoteFileDialog(const QVector<const FileImporterClass*>& importerTypes, QWidget* parent, const QString& caption) : QDialog(parent)
{
	setWindowTitle(caption);

	QVBoxLayout* layout1 = new QVBoxLayout(this);
	layout1->setSpacing(2);

	layout1->addWidget(new QLabel(tr("Remote URL:")));

	QHBoxLayout* layout2 = new QHBoxLayout();
	layout2->setContentsMargins(0,0,0,0);
	layout2->setSpacing(4);

	_urlEdit = new QComboBox(this);
	_urlEdit->setEditable(true);
	_urlEdit->setInsertPolicy(QComboBox::NoInsert);
	_urlEdit->setMinimumContentsLength(40);
	if(_urlEdit->lineEdit())
		_urlEdit->lineEdit()->setPlaceholderText(tr("sftp://username@hostname/path/file"));

	// Load list of recently accessed URLs.
	QSettings settings;
	settings.beginGroup("file/import_remote_file");
	QStringList list = settings.value("history").toStringList();
	for(QString entry : list)
		_urlEdit->addItem(entry);

	layout2->addWidget(_urlEdit);
	QToolButton* clearURLHistoryButton = new QToolButton();
	clearURLHistoryButton->setIcon(QIcon::fromTheme("edit_clear"));
	clearURLHistoryButton->setToolTip(tr("Clear history"));
	connect(clearURLHistoryButton, &QToolButton::clicked, [this]() {
		if(QMessageBox::question(this, tr("Clear history"),
		                               tr("Do you really want to clear the history of remote URLs? This cannot be undone."),
		                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
			QString text = _urlEdit->currentText();
			_urlEdit->clear();
			_urlEdit->setCurrentText(text);
		}
	});
	layout2->addWidget(clearURLHistoryButton);

	layout1->addLayout(layout2);
	layout1->addSpacing(10);

	layout1->addWidget(new QLabel(tr("File type:")));

	// Build list of file filter strings.
	QStringList fileFilterStrings;
	fileFilterStrings.push_back(tr("<Auto-detect file format>"));
	_importerFormats.emplace_back(nullptr, QString());

	for(const auto& importerClass : importerTypes) {
		for(const FileImporterClass::SupportedFormat& format : importerClass->supportedFormats()) {
			fileFilterStrings << format.description;
			_importerFormats.emplace_back(importerClass, format.identifier);
		}
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// Sort file formats alphabetically (but leave leading <Auto-detect> item in place).
	Ovito::sort_zipped(
		Ovito::make_span(fileFilterStrings).subspan(1), 
		Ovito::make_span( _importerFormats).subspan(1));
#endif

	_formatSelector = new QComboBox(this);
	_formatSelector->addItems(fileFilterStrings);
	layout1->addWidget(_formatSelector);
	layout1->addSpacing(10);

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, Qt::Horizontal, this);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &ImportRemoteFileDialog::onOk);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &ImportRemoteFileDialog::reject);
	layout1->addWidget(buttonBox);
}

/******************************************************************************
* Sets the current URL in the dialog.
******************************************************************************/
void ImportRemoteFileDialog::selectFile(const QUrl& url)
{
	_urlEdit->setCurrentText(url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded));
}

/******************************************************************************
* This is called when the user has pressed the OK button of the dialog.
* Validates and saves all input made by the user and closes the dialog box.
******************************************************************************/
void ImportRemoteFileDialog::onOk()
{
	try {
		QUrl url = QUrl::fromUserInput(_urlEdit->currentText());
		if(!url.isValid())
			throw Exception(tr("The entered URL is invalid."));

		// Save list of recently accessed URLs.
		QStringList list;
		for(int index = 0; index < _urlEdit->count(); index++) {
			list << _urlEdit->itemText(index);
		}
		QString newEntry = url.toString(QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::PrettyDecoded);
		list.removeAll(newEntry);
		list.prepend(newEntry);
		while(list.size() > 40)
			list.removeLast();
		QSettings settings;
		settings.beginGroup("file/import_remote_file");
		settings.setValue("history", list);

		// Close dialog box.
		accept();
	}
	catch(const Exception& ex) {
		ex.reportError();
	}
}

/******************************************************************************
* Returns the file to import after the dialog has been closed with "OK".
******************************************************************************/
QUrl ImportRemoteFileDialog::urlToImport() const
{
	return QUrl::fromUserInput(_urlEdit->currentText());
}

/******************************************************************************
* Returns the selected importer type or NULL if auto-detection is requested.
******************************************************************************/
const std::pair<const FileImporterClass*, QString>& ImportRemoteFileDialog::selectedFileImporter() const
{
	int importFilterIndex = _formatSelector->currentIndex();
	OVITO_ASSERT(importFilterIndex >= -1 && importFilterIndex < _importerFormats.size());
	return _importerFormats[importFilterIndex];
}

}	// End of namespace
