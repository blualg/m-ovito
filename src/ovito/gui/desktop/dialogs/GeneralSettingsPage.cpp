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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/base/mainwin/ModifierListModel.h>
#include <ovito/core/app/Application.h>
#include "GeneralSettingsPage.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(GeneralSettingsPage);

/******************************************************************************
* Creates the widget that contains the plugin specific setting controls.
******************************************************************************/
void GeneralSettingsPage::insertSettingsDialogPage(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget)
{
	QWidget* page = new QWidget();
	tabWidget->addTab(page, tr("General"));
	QVBoxLayout* layout1 = new QVBoxLayout(page);

	QSettings settings;

	// Group "User interface options":
	QGroupBox* uiGroupBox = new QGroupBox(tr("User interface options"), page);
	layout1->addWidget(uiGroupBox);
	QGridLayout* layout2 = new QGridLayout(uiGroupBox);

	_keepDirHistory = new QCheckBox(tr("Seperate folders for data import/export and session states"));
	_keepDirHistory->setToolTip(tr(
			"<p>Maintain individual working directories for different types of file I/O operations.</p>"));
	layout2->addWidget(_keepDirHistory, 0, 0);
	_keepDirHistory->setChecked(HistoryFileDialog::keepWorkingDirectoryHistoryEnabled());

	_sortModifiersByCategory = new QCheckBox(tr("Sort list of available modifiers by category"));
	_sortModifiersByCategory->setToolTip(tr("<p>Show a categorized list of available modifiers in the command panel.</p>"));
	layout2->addWidget(_sortModifiersByCategory, 1, 0);
	_sortModifiersByCategory->setChecked(ModifierListModel::useCategoriesGlobal());

	// Group "Program updates":
#if !defined(OVITO_BUILD_APPSTORE_VERSION)
	QGroupBox* updateGroupBox = new QGroupBox(tr("Program updates"), page);
	layout1->addWidget(updateGroupBox);
	layout2 = new QGridLayout(updateGroupBox);

	_enableUpdateChecks = new QCheckBox(tr("Periodically check ovito.org website for program updates (and display notice when available)"), updateGroupBox);
	_enableUpdateChecks->setToolTip(tr(
			"<p>The news page is fetched from <i>www.ovito.org</i> on each program startup. "
			"It displays information about new program releases as soon as they become available.</p>"));
	layout2->addWidget(_enableUpdateChecks, 0, 0);

	_enableUpdateChecks->setChecked(settings.value("updates/check_for_updates", true).toBool());
#endif

	layout1->addStretch();
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void GeneralSettingsPage::saveValues(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget)
{
	HistoryFileDialog::setKeepWorkingDirectoryHistoryEnabled(_keepDirHistory->isChecked());
	ModifierListModel::setUseCategoriesGlobal(_sortModifiersByCategory->isChecked());

#if !defined(OVITO_BUILD_APPSTORE_VERSION)
	QSettings settings;
	settings.setValue("updates/check_for_updates", _enableUpdateChecks->isChecked());
#endif
}

}	// End of namespace
