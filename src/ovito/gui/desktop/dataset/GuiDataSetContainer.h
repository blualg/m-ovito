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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief Manages the DataSet being edited.
 */
class OVITO_GUI_EXPORT GuiDataSetContainer : public DataSetContainer
{
	OVITO_CLASS(GuiDataSetContainer)

public:

	/// \brief Constructor.
	GuiDataSetContainer(TaskManager& taskManager, MainWindow& mainWindow);

	/// \brief Returns the window this dataset container is linked to.
	MainWindow& mainWindow() const { return _mainWindow; }

	/// \brief Imports a set of files into the current dataset.
	/// \param urls The locations of the files to import.
	/// \param operation The process in which this operation is being performed.
	/// \param importerType The FileImporter type selected by the user. If null, the file's format will be auto-detected.
	/// \param importerFormat The sub-format name selected by the user, which is supported by the selected importer class.
	/// \return true if the file(s) were successfully imported; false if operation has been canceled by the user.
	/// \throw Exception on error.
	bool importFiles(const std::vector<QUrl>& urls, MainThreadOperation operation, const FileImporterClass* importerType = nullptr, const QString& importerFormat = {});

	/// \brief Save the current dataset.
	/// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
	/// \throw Exception on error.
	///
	/// If the current dataset has not been assigned a file path, then this method
	/// displays a file selector dialog by calling fileSaveAs() to let the user select a file path.
	bool fileSave();

	/// \brief Lets the user select a new destination filename for the current dataset. Then saves the dataset by calling fileSave().
	/// \param filename If \a filename is an empty string that this method asks the user for a filename. Otherwise
	///                 the provided filename is used.
	/// \return \c true, if the dataset has been saved; \c false if the operation has been canceled by the user.
	/// \throw Exception on error.
	bool fileSaveAs(const QString& filename = QString());

	/// \brief Asks the user if changes made to the dataset should be saved.
	/// \return \c false if the operation has been canceled by the user; \c true on success.
	/// \throw Exception on error.
	///
	/// If the current dataset has been changed, this method asks the user if changes should be saved.
	/// If yes, then the dataset is saved by calling fileSave().
	bool askForSaveChanges();

private:

	/// The window this dataset container is linked to (may be null).
	MainWindow& _mainWindow;
};

}	// End of namespace
