////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/UtilityListModel.h>
#include "CommandPanel.h"
#include "UtilityCommandPage.h"

namespace Ovito {

/******************************************************************************
* Initializes the command panel page.
******************************************************************************/
UtilityCommandPage::UtilityCommandPage(MainWindow& mainWindow, QWidget* parent) : QWidget(parent), _mainWindow(mainWindow)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2,2,2,2);
    layout->setSpacing(4);

    // Create the dropdown box.
    _utilitiesBox = new QComboBox(this);
    layout->addWidget(_utilitiesBox);
    _utilityListModel = new UtilityListModel(this, mainWindow);
    _utilitiesBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _utilitiesBox->setModel(_utilityListModel);
    _utilitiesBox->setMaxVisibleItems(0xFFFF);
    connect(_utilitiesBox, qOverload<int>(&QComboBox::activated), this, &UtilityCommandPage::onOpenUtility);

    // Create the properties panel.
    _propertiesPanel = new PropertiesPanel(mainWindow);
    _propertiesPanel->setFrameStyle(QFrame::NoFrame | QFrame::Plain);
    layout->addWidget(_propertiesPanel, 1);

    // Connect the model to the properties panel.
    connect(_utilityListModel, &UtilityListModel::utilitySelected, this, [this](UtilityObject* utility) {
       _propertiesPanel->setEditObject(utility);
    });

    // Shut down the tab when the main window is closed.
    connect(&mainWindow, &MainWindow::closingWindow, _propertiesPanel, &PropertiesPanel::close);

    // If a state file has been loaded, close currently open utility, because it might have been replaced
    // with a deserialized version of the same utility class.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::dataSetChanged, this, [this]() {
        onOpenUtility(0);
    });
}

/******************************************************************************
* Called when the user selected a utility from the drop-down list of available utilities.
******************************************************************************/
void UtilityCommandPage::onOpenUtility(int index)
{
    if(index == 0) {
        _propertiesPanel->close();
    }
    else if(index == _utilityListModel->getMoreExtensionsItemIndex()) {
        QDesktopServices::openUrl(QStringLiteral("https://www.ovito.org/extensions/"));
    }
    else {
        if(QAction* action = _utilityListModel->actionFromIndex(index))
            action->trigger();
    }

    int currentIndex = _utilityListModel->indexFromUtilityObject(dynamic_object_cast<UtilityObject>(_propertiesPanel->editObject()));
    if(currentIndex < 0) currentIndex = 0;
    _utilitiesBox->setCurrentIndex(currentIndex);
}

}   // End of namespace
