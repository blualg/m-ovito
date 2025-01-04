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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertiesPanel.h>

namespace Ovito {

class UtilityListModel; // defined in UtilityListModel.h

/**
 * The command panel tab lets the user access utility functions.
 */
class OVITO_GUI_EXPORT UtilityCommandPage : public QWidget
{
    Q_OBJECT

public:

    /// Initializes the command panel page.
    UtilityCommandPage(MainWindow& mainWindow, QWidget* parent);

protected Q_SLOTS:

    /// Called when the user selected a utility from the drop-down list of available utilities.
    void onOpenUtility(int index);

private:

    /// Returns the selected viewport layer.
    ViewportOverlay* selectedLayer() const;

    /// The main window hosting this page.
    MainWindow& _mainWindow;

    /// Contains the list of available utilities.
    QComboBox* _utilitiesBox;

    /// This panel shows the GUI of the selected utility.
    PropertiesPanel* _propertiesPanel;

    /// The list model containing the available utilities.
    UtilityListModel* _utilityListModel;
};

}   // End of namespace
