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
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/gui/desktop/dialogs/SystemInformationDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/ViewportsPanel.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "ConfigureViewportGraphicsDialog.h"

namespace Ovito {

/******************************************************************************
* Constructor.
******************************************************************************/
ConfigureViewportGraphicsDialog::ConfigureViewportGraphicsDialog(MainWindow& mainWindow, QWidget* parent) :
    QDockWidget(tr("Viewport Graphics Configuration"), parent),
    _mainWindow(mainWindow)
{
    setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::NoDockWidgetArea);
    setFloating(true);
    setAttribute(Qt::WA_DeleteOnClose); // Make sure this object gets deleted when the dialog window is closed by the user.

    QWidget* widget = new QWidget();
    setWidget(widget);

    QVBoxLayout* mainLayout = new QVBoxLayout(widget);

    QGroupBox* backendSelectionBox = new QGroupBox(tr("Real-time rendering method"), widget);
    mainLayout->addWidget(backendSelectionBox);

    QGridLayout* gridLayout = new QGridLayout(backendSelectionBox);
    gridLayout->setColumnStretch(0, 1);

    _backendSettingsStack = new QStackedWidget(this);
    mainLayout->addWidget(_backendSettingsStack, 1);

    // Create a radio button for each available rendering backend.
    _backendSelectionGroup = new QButtonGroup(this);
    int index = 0;
    for(const auto& [id, label, windowClass, rendererClass] : ViewportWindow::listInteractiveWindowImplementations()) {
        QRadioButton* option = new QRadioButton(label);
        option->setEnabled(windowClass);
        option->setProperty("graphics_api", id);
        gridLayout->addWidget(option, index, 0);
        _backendSelectionGroup->addButton(option, index);

        mainWindow.handleExceptions([&]() {
            // Create a settings panel for the rendering backend.
            if(OORef<SceneRenderer> rendererInstance = ViewportWindow::getInteractiveWindowRenderer(id)) {
                PropertiesPanel* propertiesPanel = new PropertiesPanel(mainWindow);
                propertiesPanel->setEditObject(rendererInstance);
                if(propertiesPanel->editor()) {
                    connect(&mainWindow, &MainWindow::closingWindow, propertiesPanel, &PropertiesPanel::close);
                    connect(propertiesPanel->editor(), &PropertiesEditor::contentsChanged, &mainWindow, [&mainWindow]() { mainWindow.updateViewports(); });
                    propertiesPanel->setFrameStyle(QFrame::NoFrame);
                    int stackIndex = _backendSettingsStack->addWidget(propertiesPanel);
                    _backendSettingsMap.emplace(id, stackIndex);
                }
                else {
                    delete propertiesPanel;
                }
            }
        });

        index++;
    }

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ConfigureViewportGraphicsDialog::close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ConfigureViewportGraphicsDialog::close);
    connect(buttonBox, &QDialogButtonBox::helpRequested, &mainWindow, [&mainWindow]() {
        mainWindow.actionManager()->openHelpTopic(QStringLiteral("manual:viewports.configure_graphics_dialog"));
    });
    connect(buttonBox->addButton(tr("System information..."), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this]() {
        _mainWindow.handleExceptions([&] {
            SystemInformationDialog(_mainWindow, this).exec();
        });
    });
    mainLayout->addWidget(buttonBox);

    updateGUI();

    connect(mainWindow.viewportsPanel(), &ViewportsPanel::interactiveWindowImplementationChanged, this, &ConfigureViewportGraphicsDialog::updateGUI);
    connect(_backendSelectionGroup, &QButtonGroup::buttonToggled, this, &ConfigureViewportGraphicsDialog::backendSelectionChanged);
    connect(&mainWindow, &MainWindow::closingWindow, this, &QWidget::close);
}

/******************************************************************************
* Updates the values displayed in the dialog.
******************************************************************************/
void ConfigureViewportGraphicsDialog::updateGUI()
{
    QString selectedGraphicsApi = ViewportWindow::getInteractiveWindowImplementationName();
    for(QAbstractButton* option : _backendSelectionGroup->buttons()) {
        if(option->isEnabled() && selectedGraphicsApi.compare(option->property("graphics_api").toString(), Qt::CaseInsensitive) == 0)
            option->setChecked(true);
    }

    // Automatically switch back to OpenGL if the currently selected renderer is not available anymore.
    if(_backendSelectionGroup->checkedId() == -1) {
        _backendSelectionGroup->button(0)->setChecked(true);
        selectedGraphicsApi = "opengl";
    }

    // Show the settings corresponding to the selected rendering backend.
    if(auto it = _backendSettingsMap.find(selectedGraphicsApi); it != _backendSettingsMap.end()) {
        _backendSettingsStack->setCurrentIndex(it->second);
    }
    else {
        _backendSettingsStack->setCurrentIndex(-1);
    }
}

/******************************************************************************
* Is called when the dialog window is being closed by the user.
******************************************************************************/
void ConfigureViewportGraphicsDialog::closeEvent(QCloseEvent* event)
{
    for(int i = 0; i < _backendSettingsStack->count(); i++) {
        if(PropertiesPanel* propertiesPanel = qobject_cast<PropertiesPanel*>(_backendSettingsStack->widget(i))) {
            propertiesPanel->close();
        }
    }
    _mainWindow.handleExceptions([&]() {
        ViewportWindow::saveInteractiveWindowRendererSettings();
    });

    QDockWidget::closeEvent(event);
}

/******************************************************************************
* Recreates all viewport windows in the application.
******************************************************************************/
void ConfigureViewportGraphicsDialog::recreateViewportWindows()
{
    MainWindow::visitMainWindows([&](MainWindow* mainWindow) {
        mainWindow->viewportsPanel()->recreateViewportWindows();
    });
}

/******************************************************************************
* Is called when the user selects a different rendering backend.
******************************************************************************/
void ConfigureViewportGraphicsDialog::backendSelectionChanged(QAbstractButton* option, bool checked)
{
    if(checked) {
        if(ViewportWindow::setInteractiveWindowImplementationName(option->property("graphics_api").toString())) {
            recreateViewportWindows();
        }
        updateGUI();
    }
}

}   // End of namespace
