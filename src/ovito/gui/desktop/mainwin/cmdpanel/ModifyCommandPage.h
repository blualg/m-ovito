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
#include <ovito/gui/desktop/widgets/general/RolloutContainer.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>
#include <ovito/core/oo/RefTargetListener.h>

namespace Ovito {

class PipelineListModel;    // defined in PipelineListModel.h
class PipelineListItem;     // defined in PipelineListItem.h

/**
 * The command panel tab lets the user modify the selected object.
 */
class OVITO_GUI_EXPORT ModifyCommandPage : public QWidget
{
    Q_OBJECT

public:

    /// Initializes the command panel page.
    ModifyCommandPage(MainWindow& mainWindow, QWidget* parent);

    /// Returns the object that is currently being edited in the properties panel.
    RefTarget* editObject() const { return _propertiesPanel->editObject(); }

    /// Returns the list model that encapsulates the modification pipeline of the selected node(s).
    PipelineListModel* pipelineListModel() const { return _pipelineListModel; }

    /// Returns the list model that lists the available modifiers.
    AvailableModifiersModel* availableModifiersModel() const { return static_cast<AvailableModifiersModel*>(_modifierSelector->model()); }

    /// Loads the layout of the widgets from the settings store.
    void restoreLayout();

    /// Saves the layout of the widgets to the settings store.
    void saveLayout();

    /// Displays the given HTML page content in the About pane.
    void showProgramNotice(const QString& htmlPage);

protected Q_SLOTS:

    /// Is called when the user has selected a modifier from drop-down list of available modifiers.
    void onInsertNewModifier(int index);

    /// Is called when a new modification list item has been selected, or if the currently
    /// selected item has changed.
    void onSelectedItemChanged();

    /// This called when the user double clicks on an item in the modifier stack.
    void onModifierStackDoubleClicked(const QModelIndex& index);

private:

    /// Creates the rollout panel that shows information about the application whenever no object is selected.
    void createAboutPanel();

private:

    /// The main window hosting this page.
    MainWindow& _mainWindow;

    /// This list box shows the modifier stack of the selected scene node(s).
    QListView* _pipelineWidget;

    /// The Qt model for the data pipeline of the selected node(s).
    PipelineListModel* _pipelineListModel;

    /// This widget displays the list of available modifiers and allows the user to insert a modifier into the pipeline.
    QComboBox* _modifierSelector;

    /// This panel shows the properties of the selected modifier stack entry
    PropertiesPanel* _propertiesPanel;

    /// The panel displaying information about the application when no object is selected.
    Rollout* _aboutRollout;

    /// The splitter widget separating the pipeline editor and the properties panel.
    QSplitter* _splitter;
};

}   // End of namespace
