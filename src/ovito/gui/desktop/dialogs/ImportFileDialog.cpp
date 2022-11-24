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
#include "ImportFileDialog.h"

namespace Ovito {

/******************************************************************************
* Constructs the dialog window.
******************************************************************************/
ImportFileDialog::ImportFileDialog(const QVector<const FileImporterClass*>& importerTypes, DataSet* dataset, QWidget* parent, const QString& caption, bool allowMultiSelection, const QString& dialogClass) :
	HistoryFileDialog(dialogClass, parent, caption)
{
	if(importerTypes.empty())
		throw Exception(tr("There are no importer plugins installed."));

	// Build list of file filter strings.
	QStringList fileFilterStrings;
	fileFilterStrings.push_back(tr("<Auto-detect file format> (*)"));
	_importerFormats.emplace_back(nullptr, QString());

	for(const auto& importerClass : importerTypes) {
		for(const FileImporterClass::SupportedFormat& format : importerClass->supportedFormats()) {
			OVITO_ASSERT(!format.description.isEmpty() && !format.fileFilter.isEmpty());
			fileFilterStrings << QString("%1 (%2)").arg(format.description, format.fileFilter);
			_importerFormats.emplace_back(importerClass, format.identifier);
		}
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// Sort file formats alphabetically (but leave leading <Auto-detect> entry in place).
	Ovito::sort_zipped(
		Ovito::make_span(fileFilterStrings).subspan(1), 
		Ovito::make_span( _importerFormats).subspan(1));
#endif

	setNameFilters(fileFilterStrings);
	selectNameFilter(fileFilterStrings.front());
	setAcceptMode(QFileDialog::AcceptOpen);
	setFileMode(allowMultiSelection ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
}

/******************************************************************************
* Returns the file to import after the dialog has been closed with "OK".
******************************************************************************/
QString ImportFileDialog::fileToImport() const
{
	QStringList filesToImport = selectedFiles();
	if(filesToImport.isEmpty()) return QString();
	return filesToImport.front();
}

/******************************************************************************
* Returns the file to import after the dialog has been closed with "OK".
******************************************************************************/
QUrl ImportFileDialog::urlToImport() const
{
	return FileManager::urlFromUserInput(fileToImport());
}

/******************************************************************************
* Returns the list of files to import after the dialog has been closed with "OK".
******************************************************************************/
std::vector<QUrl> ImportFileDialog::urlsToImport() const
{
	std::vector<QUrl> list;
	for(const QString& file : selectedFiles()) {
		list.push_back(FileManager::urlFromUserInput(file));
	}
	return list;
}

/******************************************************************************
* Returns the selected importer class and sub-format name.
******************************************************************************/
const std::pair<const FileImporterClass*, QString>& ImportFileDialog::selectedFileImporter() const
{
	int importFilterIndex = nameFilters().indexOf(selectedNameFilter());
	OVITO_ASSERT(importFilterIndex >= 0 && importFilterIndex < _importerFormats.size());
	return _importerFormats[importFilterIndex];
}

}	// End of namespace
