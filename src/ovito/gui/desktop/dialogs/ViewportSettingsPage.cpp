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
#include <ovito/gui/desktop/mainwin/ViewportsPanel.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/app/PluginManager.h>
#include "ViewportSettingsPage.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ViewportSettingsPage);

/******************************************************************************
* Creates the widget that contains the plugin specific setting controls.
******************************************************************************/
void ViewportSettingsPage::insertSettingsDialogPage(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget)
{
	// Retrieve current settings.
	_viewportSettings.assign(ViewportSettings::getSettings());
	QSettings settings;

	QWidget* page = new QWidget();
	tabWidget->addTab(page, tr("Viewports"));
	QVBoxLayout* layout1 = new QVBoxLayout(page);

	QGroupBox* upDirectionGroupBox = new QGroupBox(tr("Camera"), page);
	layout1->addWidget(upDirectionGroupBox);
	QGridLayout* layout2 = new QGridLayout(upDirectionGroupBox);

	QLabel* label1 = new QLabel(tr("<html><p>Coordinate system orientation:</p></html>"));
	label1->setWordWrap(true);
	layout2->addWidget(label1, 0, 0, 1, 4);

	_upDirectionGroup = new QButtonGroup(page);
	QRadioButton* verticalAxisX = new QRadioButton(QString(), upDirectionGroupBox);
	QRadioButton* verticalAxisY = new QRadioButton(QString(), upDirectionGroupBox);
	QRadioButton* verticalAxisZ = new QRadioButton(tr("(default)"), upDirectionGroupBox);
	_upDirectionGroup->addButton(verticalAxisX, ViewportSettings::X_AXIS);
	_upDirectionGroup->addButton(verticalAxisY, ViewportSettings::Y_AXIS);
	_upDirectionGroup->addButton(verticalAxisZ, ViewportSettings::Z_AXIS);
	verticalAxisX->setIcon(QIcon(":/gui/mainwin/settings/vertical_axis_x.png"));
	verticalAxisX->setIconSize(verticalAxisX->icon().availableSizes().front());
	verticalAxisX->setToolTip(tr("X-axis"));
	verticalAxisY->setIcon(QIcon(":/gui/mainwin/settings/vertical_axis_y.png"));
	verticalAxisY->setIconSize(verticalAxisY->icon().availableSizes().front());
	verticalAxisY->setToolTip(tr("Y-axis"));
	verticalAxisZ->setIcon(QIcon(":/gui/mainwin/settings/vertical_axis_z.png"));
	verticalAxisZ->setIconSize(verticalAxisZ->icon().availableSizes().front());
	verticalAxisZ->setToolTip(tr("Z-axis"));
	layout2->addWidget(verticalAxisX, 1, 0, 1, 1);
	layout2->addWidget(verticalAxisY, 1, 1, 1, 1);
	layout2->addWidget(verticalAxisZ, 1, 2, 1, 1);
	_upDirectionGroup->button(_viewportSettings.upDirection())->setChecked(true);
	layout2->setColumnStretch(3, 1);

	_constrainCameraRotationBox = new QCheckBox(tr("Restrict camera rotation to keep major axis pointing upward"));
	_constrainCameraRotationBox->setChecked(_viewportSettings.constrainCameraRotation());
	layout2->addWidget(_constrainCameraRotationBox, 2, 0, 1, 3);

	QGroupBox* colorsGroupBox = new QGroupBox(tr("Color scheme"), page);
	layout1->addWidget(colorsGroupBox);
	layout2 = new QGridLayout(colorsGroupBox);

	_colorScheme = new QButtonGroup(page);
	QRadioButton* darkColorScheme = new QRadioButton(tr("Dark"), colorsGroupBox);
	QRadioButton* lightColorScheme = new QRadioButton(tr("Light"), colorsGroupBox);
	layout2->addWidget(darkColorScheme, 0, 0, 1, 1);
	layout2->addWidget(lightColorScheme, 0, 1, 1, 1);
	_colorScheme->addButton(darkColorScheme, 0);
	_colorScheme->addButton(lightColorScheme, 1);
	if(_viewportSettings.viewportColor(ViewportSettings::COLOR_VIEWPORT_BKG) == Color(0,0,0))
		darkColorScheme->setChecked(true);
	else
		lightColorScheme->setChecked(true);

	// Group "3D graphics system":
	QGroupBox* graphicsGroupBox = new QGroupBox(tr("3D graphics"), page);
	layout1->addWidget(graphicsGroupBox);
	layout2 = new QGridLayout(graphicsGroupBox);
	layout2->setColumnStretch(2, 1);

	layout2->addWidget(new QLabel(tr("Graphics hardware interface:")), 0, 0);
	_graphicsSystem = new QButtonGroup(page);
	QRadioButton* openglOption = new QRadioButton(tr("OpenGL"), graphicsGroupBox);
	QRadioButton* vulkanOption = new QRadioButton(tr("Vulkan"), graphicsGroupBox);
	layout2->addWidget(openglOption, 0, 1);
	layout2->addWidget(vulkanOption, 1, 1);
	_graphicsSystem->addButton(openglOption, 0);
	_graphicsSystem->addButton(vulkanOption, 1);
	_vulkanDevices = new QComboBox();
	layout2->addWidget(_vulkanDevices, 1, 2);

	if(settings.value("rendering/selected_graphics_api").toString() == "Vulkan")
		vulkanOption->setChecked(true);
	else
		openglOption->setChecked(true);

	if(OvitoClassPtr rendererClass = PluginManager::instance().findClass("VulkanRenderer", "VulkanSceneRenderer")) {
		// Call the VulkanSceneRenderer::OOMetaClass::querySystemInformation() function to let the Vulkan plugin write the
		// list of available devices to the application settings store, from where we can read them.
		QString dummyBuffer;
		QTextStream dummyStream(&dummyBuffer);
		rendererClass->querySystemInformation(dummyStream, settingsDialog->mainWindow());

		settings.beginGroup("rendering/vulkan");
		int numDevices = settings.beginReadArray("available_devices");
		if(numDevices != 0) {
			for(int deviceIndex = 0; deviceIndex < numDevices; deviceIndex++) {
				settings.setArrayIndex(deviceIndex);
				QString title = settings.value("name").toString();
				switch(settings.value("deviceType").toInt()) {
				case 1: // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
					title += tr(" (integrated GPU)");
					break;
				case 2: // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
					title += tr(" (discrete GPU)");
					break;
				case 3: // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU  
					title += tr(" (virtual GPU)");
					break;
				}
				_vulkanDevices->addItem(std::move(title));
			}
		}
		else {
			_vulkanDevices->addItem(tr("<No devices found>"));
			vulkanOption->setEnabled(false);
			openglOption->setChecked(true);
			_vulkanDevices->setEnabled(false);
		}
		settings.endArray();
		_vulkanDevices->setCurrentIndex(settings.value("selected_device", 0).toInt());
		settings.endGroup();
	}
	else {
		vulkanOption->setEnabled(false);
		_vulkanDevices->setEnabled(false);
		_vulkanDevices->addItem(tr("Not available on this platform"));
	}

	// Automatically switch back to OpenGL if the currently selected renderer is not available anymore.
	if(!vulkanOption->isEnabled() && vulkanOption->isChecked())
		openglOption->setChecked(true);
	_vulkanDevices->setEnabled(vulkanOption->isChecked());
	connect(vulkanOption, &QAbstractButton::toggled, _vulkanDevices, &QComboBox::setEnabled);

	// Transparency rendering method.
	_transparencyRenderingMethod = new QComboBox();
	_transparencyRenderingMethod->addItem(tr("Back-to-Front Ordered"), QVariant::fromValue(1));
	_transparencyRenderingMethod->addItem(tr("Weighted Blended Order-Independent"), QVariant::fromValue(2));
	_transparencyRenderingMethod->setCurrentIndex(
		_transparencyRenderingMethod->findData(settings.value("rendering/transparency_method", 1)));
	layout2->addWidget(new QLabel(tr("Transparency rendering method:")), 3, 0);
	layout2->addWidget(_transparencyRenderingMethod, 3, 1, 1, 2);
	_transparencyRenderingMethod->setEnabled(openglOption->isChecked());
	connect(openglOption, &QAbstractButton::toggled, _transparencyRenderingMethod, &QComboBox::setEnabled);

	layout1->addStretch();
}

/******************************************************************************
* Lets the settings page validate the values entered by the user before saving them.
******************************************************************************/
bool ViewportSettingsPage::validateValues(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget)
{
	QSettings settings;

	// Check if user has selected a different 3D graphics API than before.
	bool recreateViewportWindows = false;
	bool wasVulkanSelected = (settings.value("rendering/selected_graphics_api").toString() == "Vulkan");
	bool isVulkanSelected = (_graphicsSystem->checkedId() == 1);
	if(isVulkanSelected != wasVulkanSelected && isVulkanSelected) {
		// Warn the user that some Vulkan implementations may be incompatible with Ovito and can 
		// render the application unusable.
		QMessageBox msgBox(settingsDialog);
		msgBox.setIcon(QMessageBox::Question);
		msgBox.setText("Are you sure you want to enable the Vulkan-based viewport renderer?");
		msgBox.setInformativeText(tr(
					"In rare cases, Vulkan graphics drivers can be incompatible with OVITO. This concerns especially very old graphics chip models. "
					"In such a case, OVITO may only display a black window and become entirely unusable.\n\n"
					"It may then be necessary to deactivate the Vulkan renderer of OVITO again. If OVITO is no longer usable, this must be done manually "
					"by resetting the program settings to factory defaults. Please refer to the user manual to see where OVITO stores its program settings and how to reset them.\n\n"
					"Click OK to continue and activate the Vulkan renderer now."));
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel | QMessageBox::Help);
		msgBox.setDefaultButton(QMessageBox::Ok);
		int ret = msgBox.exec();
		if(ret != QMessageBox::Ok) {
			if(ret == QMessageBox::Help) {
				settingsDialog->onHelp();
			}
			return false;
		}
	}

	return true;
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void ViewportSettingsPage::saveValues(ApplicationSettingsDialog* settingsDialog, QTabWidget* tabWidget)
{
	QSettings settings;

	// Check if user has selected a different 3D graphics API than before.
	bool recreateViewportWindows = false;
	QString oldGraphicsApi = settings.value("rendering/selected_graphics_api").toString();
	QString newGraphicsApi;
	if(_graphicsSystem->checkedId() == 1) newGraphicsApi = "Vulkan";
	if(newGraphicsApi != oldGraphicsApi) {
		// Save new API selection in the application settings store.
		if(!newGraphicsApi.isEmpty())
			settings.setValue("rendering/selected_graphics_api", newGraphicsApi);
		else
			settings.remove("rendering/selected_graphics_api");
		recreateViewportWindows = true;
	}

	// Check if a different Vulkan device was selected by the user.
	if(settings.value("rendering/vulkan/selected_device", 0).toInt() != _vulkanDevices->currentIndex()) {
		settings.setValue("rendering/vulkan/selected_device", _vulkanDevices->currentIndex());
		recreateViewportWindows = true;
	}

	// Check if a different transparency rendering method was selected by the user.
	if(settings.value("rendering/transparency_method", 1).toInt() != _transparencyRenderingMethod->currentData().toInt()) {
		settings.setValue("rendering/transparency_method", _transparencyRenderingMethod->currentData().toInt());
		recreateViewportWindows = true;
	}

	// Recreate all interactive viewport windows in all program windows after a different graphics API has been activated.
	// No restart of the software is required.
	if(recreateViewportWindows) {
		for(QWidget* widget : QApplication::topLevelWidgets()) {
			if(MainWindow* mainWindow = qobject_cast<MainWindow*>(widget)) {
				mainWindow->viewportsPanel()->recreateViewportWindows();
			}
		}
	}

	// Update settings.
	_viewportSettings.setUpDirection((ViewportSettings::UpDirection)_upDirectionGroup->checkedId());
	_viewportSettings.setConstrainCameraRotation(_constrainCameraRotationBox->isChecked());
	if(_colorScheme->checkedId() == 1) {
		// Light color scheme.
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_VIEWPORT_BKG, Color(1.0f, 1.0f, 1.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_GRID, Color(0.6f, 0.6f, 0.6f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_GRID_INTENS, Color(0.5f, 0.5f, 0.5f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_GRID_AXIS, Color(0.4f, 0.4f, 0.4f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_VIEWPORT_CAPTION, Color(0.0f, 0.0f, 0.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_SELECTION, Color(0.0f, 0.0f, 0.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_UNSELECTED, Color(0.5f, 0.5f, 1.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_ACTIVE_VIEWPORT_BORDER, Color(1.0f, 1.0f, 0.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_ANIMATION_MODE, Color(1.0f, 0.0f, 0.0f));
		_viewportSettings.setViewportColor(ViewportSettings::COLOR_CAMERAS, Color(0.5f, 0.5f, 1.0f));
	}
	else {
		// Dark color scheme.
		_viewportSettings.restoreDefaultViewportColors();
	}

	// Store current settings.
	ViewportSettings::setSettings(_viewportSettings);
}

}	// End of namespace
