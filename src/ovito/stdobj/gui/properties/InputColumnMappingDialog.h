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


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/InputColumnMapping.h>

namespace Ovito {

/**
 * \brief Dialog box that lets the user edit an InputColumnMapping.
 */
class OVITO_STDOBJGUI_EXPORT InputColumnMappingDialog : public QDialog, public UserInterfaceComponent<MainWindowUI>
{
    Q_OBJECT

public:

    /// Constructor.
    InputColumnMappingDialog(MainWindowUI& ui, const InputColumnMapping& mapping, QWidget* parent, const QString& fileName = {});

    /// Fills the editor with the given mapping.
    void setMapping(const InputColumnMapping& mapping);

    /// Returns the user-defined column mapping.
    InputColumnMapping mapping() const;

protected Q_SLOTS:

    /// This is called when the user has pressed the OK button.
    void onOk();

    /// Updates the list of vector components for the given file column.
    void updateVectorComponentList(int columnIndex);

    /// Saves the current mapping as a preset.
    void onSavePreset();

    /// Loads a preset mapping.
    void onLoadPreset();

    /// Called when the user clicks on an item in the table column for file columns.
    void fileColumnClicked(const QModelIndex& index);

    /// Called when the user has triggered the action to toggle the selected file columns.
    void toggleSelected();

protected:

    /// The property container type.
    PropertyContainerClassPtr _containerClass = nullptr;

    /// The main table widget that contains the entries for each data column of the input file.
    QTableWidget* _tableWidget;

    QVector<QComboBox*> _propertyBoxes;
    QVector<QComboBox*> _vectorComponentBoxes;
    QVector<int> _propertyDataTypes;
    QAction* _toggleSelectedAction;

    QSignalMapper* _vectorCmpntSignalMapper;

    QLabel* _fileExcerptLabel;
    QTextEdit* _fileExcerptField;
};

}   // End of namespace
