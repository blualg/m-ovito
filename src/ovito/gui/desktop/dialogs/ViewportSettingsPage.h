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
#include <ovito/gui/desktop/dialogs/ApplicationSettingsDialog.h>
#include <ovito/core/viewport/ViewportSettings.h>

namespace Ovito {

/**
 * Page of the application settings dialog, which hosts viewport-related program options.
 */
class OVITO_GUI_EXPORT ViewportSettingsPage : public ApplicationSettingsDialogPage
{
	OVITO_CLASS(ViewportSettingsPage)

public:

	/// Default constructor.
	Q_INVOKABLE ViewportSettingsPage() = default;

	/// \brief Creates the widget.
	virtual void insertSettingsDialogPage(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget) override;

	/// \brief Lets the settings page validate the values entered by the user before saving them.
	virtual bool validateValues(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget) override;

	/// \brief Lets the settings page to save all values entered by the user.
	/// \param settingsDialog The settings dialog box.
	virtual void saveValues(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget) override;

	/// \brief Returns an integer value that is used to sort the dialog pages in ascending order.
	virtual int pageSortingKey() const override { return 2; }

private:

	/// The settings object being modified.
	ViewportSettings _viewportSettings;

	QButtonGroup* _upDirectionGroup;
	QCheckBox* _constrainCameraRotationBox;
	QButtonGroup* _colorScheme;
	QButtonGroup* _graphicsSystem;
	QComboBox* _vulkanDevices;
	QComboBox* _transparencyRenderingMethod;
};

}	// End of namespace
