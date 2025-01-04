////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/desktop/dialogs/ConfigureViewportGraphicsDialog.h>
#include <ovito/core/app/PluginManager.h>
#include "ViewportSettingsPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ViewportSettingsPage);

/******************************************************************************
* Creates the widgets of the settings page
******************************************************************************/
void ViewportSettingsPage::insertSettingsDialogPage(QTabWidget* tabWidget)
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

    QGroupBox* colorsGroupBox = new QGroupBox(tr("Viewport background"), page);
    layout1->addWidget(colorsGroupBox);
    layout2 = new QGridLayout(colorsGroupBox);

    _colorScheme = new QButtonGroup(page);
    QRadioButton* darkColorScheme = new QRadioButton(tr("Dark (default)"), colorsGroupBox);
    QRadioButton* lightColorScheme = new QRadioButton(tr("Light"), colorsGroupBox);
    layout2->addWidget(darkColorScheme, 0, 0, 1, 1);
    layout2->addWidget(lightColorScheme, 0, 1, 1, 1);
    _colorScheme->addButton(darkColorScheme, 0);
    _colorScheme->addButton(lightColorScheme, 1);
    if(_viewportSettings.viewportColor(ViewportSettings::COLOR_VIEWPORT_BKG) == Color(0,0,0))
        darkColorScheme->setChecked(true);
    else
        lightColorScheme->setChecked(true);

    QGroupBox* graphicsGroupBox = new QGroupBox(tr("Viewport 3D graphics"), page);
    layout1->addWidget(graphicsGroupBox);
    layout2 = new QGridLayout(graphicsGroupBox);
    layout2->setColumnStretch(1, 1);
    QPushButton* configureGraphicsBtn = new QPushButton(tr("Configure..."), graphicsGroupBox);
    layout2->addWidget(configureGraphicsBtn, 0, 0);
    connect(configureGraphicsBtn, &QPushButton::clicked, this, &ViewportSettingsPage::showConfigureViewportGraphicsDialog);

    layout1->addStretch();
}

/******************************************************************************
* Shows a sub-dialog that allows the user to configure the graphics system used by the viewport.
******************************************************************************/
void ViewportSettingsPage::showConfigureViewportGraphicsDialog()
{
    if(!_configureViewportGraphicsDialog)
        _configureViewportGraphicsDialog = new ConfigureViewportGraphicsDialog(mainWindow(), settingsDialog());
    _configureViewportGraphicsDialog->show();
    _configureViewportGraphicsDialog->raise();
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void ViewportSettingsPage::saveValues(QTabWidget* tabWidget)
{
    // Check if user has selected a different 3D graphics API than before.
    bool recreateViewportWindows = false;

    // Recreate all interactive viewport windows in all program windows after a different graphics API has been activated.
    // No restart of the software is required.
    if(recreateViewportWindows) {
        MainWindow::visitMainWindows([&](MainWindow* mainWindow) {
            mainWindow->viewportsPanel()->recreateViewportWindows();
        });
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

}   // End of namespace
