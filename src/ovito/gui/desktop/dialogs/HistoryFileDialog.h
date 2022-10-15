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

namespace Ovito {

/**
 * \brief The file chooser dialog that saves a history of recently visited directories.
 */
class OVITO_GUI_EXPORT HistoryFileDialog : public QFileDialog
{
	Q_OBJECT

public:

	/// \brief Constructs the dialog window.
	HistoryFileDialog(const QString& dialogClass, QWidget* parent = nullptr, const QString& caption = QString(), const QString& directory = QString(), const QString& filter = QString());

	/// Returns whether the user has activated the program option to maintain separate
	/// working directories for different file I/O operations.
	static bool keepWorkingDirectoryHistoryEnabled() {
#ifdef Q_OS_LINUX
		return QSettings().value("file/keep_dir_history", false).toBool();
#else
		return QSettings().value("file/keep_dir_history", true).toBool();
#endif
	}

	/// Sets whether to maintain separate working directories for different file I/O operations.
	static void setKeepWorkingDirectoryHistoryEnabled(bool on) {
		QSettings settings;
#ifdef Q_OS_LINUX
		if(on) settings.setValue("file/keep_dir_history", true);
		else settings.remove("file/keep_dir_history");
#else
		if(!on) settings.setValue("file/keep_dir_history", false);
		else settings.remove("file/keep_dir_history");
#endif
	}

private Q_SLOTS:

	/// This is called when the user has pressed the OK button of the dialog box.
	void onFileSelected(const QString& file);

protected:

	/// Loads the list of most recently visited directories from the settings store.
	QStringList loadDirHistory() const;

	/// Saves the list of most recently visited directories to the settings store.
	void saveDirHistory(const QStringList& list) const;

private:

	/// The type of file dialog: "import", "export" etc.
	QString _dialogClass;
};

}	// End of namespace
