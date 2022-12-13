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
#include <ovito/core/oo/OvitoObject.h>

namespace Ovito {

class ApplicationSettingsDialog;		// defined below.

/**
 * \brief Abstract base class for tab providers for the application's settings dialog.
 */
class OVITO_GUI_EXPORT ApplicationSettingsDialogPage : public OvitoObject
{
	OVITO_CLASS(ApplicationSettingsDialogPage)

protected:

	/// Base class constructor.
	ApplicationSettingsDialogPage() = default;

public:

	/// \brief Creates the tab that is inserted into the settings dialog.
	/// \param tabWidget The QTabWidget into which the method should insert the settings page.
	virtual void insertSettingsDialogPage(QTabWidget* tabWidget) = 0;

	/// \brief Lets the settings page validate the values entered by the user before saving them.
	/// \return true if the settings are valid; false if settings need to be corrected by the user and the dialog should not be closed.
	virtual bool validateValues(QTabWidget* tabWidget) { return true; }

	/// \brief Lets the settings page to save all values entered by the user.
	virtual void saveValues(QTabWidget* tabWidget) {}

	/// \brief Lets the settings page restore the original values of changed settings.
	virtual void restoreValues(QTabWidget* tabWidget) {}

	/// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
	virtual int pageSortingKey() const { return 1000; }

	/// Returns the parent dialog hosting this settings page.
	ApplicationSettingsDialog* settingsDialog() const { OVITO_ASSERT(_settingsDialog); return _settingsDialog; }

	/// Returns the main window hosting this settings page.
	MainWindow& mainWindow() const;

private:

	ApplicationSettingsDialog* _settingsDialog = nullptr;

	friend class ApplicationSettingsDialog;
};

/**
 * \brief The dialog window that lets the user change the global application settings.
 *
 * Plugins can add additional pages to this dialog by deriving new classes from
 * the ApplicationSettingsDialogPage class.
 */
class OVITO_GUI_EXPORT ApplicationSettingsDialog : public QDialog
{
	Q_OBJECT

public:

	/// \brief Constructs the dialog window.
	/// \param mainWindow The parent window of the settings dialog.
	/// \param startPage An optional pointer to the ApplicationSettingsDialogPage derived class whose
	///                  settings page should be activated initially.
	ApplicationSettingsDialog(MainWindow& mainWindow, OvitoClassPtr startPage = nullptr);

	/// Returns the main window the application settings dialog was opened from.
	MainWindow& mainWindow() const { return _mainWindow; }

public Q_SLOTS:

	/// This is called when the user has pressed the OK button of the settings dialog.
	/// Validates and saves all settings made by the user and closes the dialog box.
	void onOk();

	/// This is called when the user closes the dialog box using the Cancel button.
	void onCancel();

	/// This is called when the user has pressed the help button of the settings dialog.
	void onHelp();

private:

	MainWindow& _mainWindow;
	QVector<OORef<ApplicationSettingsDialogPage>> _pages;
	QTabWidget* _tabWidget;
};

}	// End of namespace
