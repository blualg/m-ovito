////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <ovito/gui/base/mainwin/AvailableOverlaysModel.h>

namespace Ovito {

class OverlayListModel;  // Defined in OverlayListModel.h

/**
 * A combo box widget that displays the list of available viewport layers and allows the user
 * to insert a layer into the current viewport.
 */
class OVITO_GUI_EXPORT AvailableOverlaysSelectorWidget : public QComboBox, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableOverlaysSelectorWidget(QWidget* parent, MainWindowUI& ui, OverlayListModel* overlayListModel);

    /// Returns the tree model that organizes all available overlays by category.
    AvailableOverlaysModel* availableOverlaysModel() const;

    /// Returns the list model that presents the available overlays in flat list format.
    AvailableOverlaysListModel* availableOverlaysListModel() const;

private Q_SLOTS:

    /// Handles selection of an overlay from the drop-down list.
    void onOverlaySelected(int index);

    /// Updates the enabled state of this widget based on the current viewport.
    void onActiveViewportChanged(Viewport* activeViewport);

private:

    /// The overlay list model used to determine the enabled state.
    OverlayListModel* _overlayListModel;
};

}   // End of namespace
