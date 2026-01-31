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
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>

namespace Ovito {

class PipelineListModel;  // Defined in PipelineListModel.h

/**
 * A combo box widget that displays the list of available modifiers and allows the user
 * to insert a modifier into the current data pipeline.
 */
class OVITO_GUI_EXPORT AvailableModifiersSelectorWidget : public QComboBox, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    AvailableModifiersSelectorWidget(QWidget* parent, MainWindowUI& ui, PipelineListModel* pipelineListModel);

    /// Returns the tree model that organizes all available modifiers by category.
    AvailableModifiersModel* availableModifiersModel() const;

    /// Returns the list model that presents the available modifiers in flat list format.
    AvailableModifiersListModel* availableModifiersListModel() const;

protected:

    /// Called when the popup menu is about to be shown.
    virtual void showPopup() override;

private Q_SLOTS:

    /// Handles selection of a modifier from the drop-down list.
    void onModifierSelected(int index);

    /// Updates the enabled state of this widget based on the current pipeline selection.
    void onPipelineSelectionChanged();

private:

    /// The pipeline list model used to determine the enabled state.
    PipelineListModel* _pipelineListModel;
};

}   // End of namespace
