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

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/lammps/LAMMPSDataImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the LAMMPSDataImporter class.
 */
class LAMMPSDataImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(LAMMPSDataImporterEditor)
    Q_OBJECT

public:

    /// This is called by the system when the user has selected a new file to import.
    virtual void inspectNewFile(FileImporter* importer, const QUrl& sourceFile, MainWindow& mainWindow) override;

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
};

/**
 * This dialog box lets the user choose a LAMMPS atom style.
 */
class LAMMPSAtomStyleDialog : public QDialog
{
    Q_OBJECT

public:

    /// Constructor.
    LAMMPSAtomStyleDialog(MainWindow& mainWindow, LAMMPSDataImporter::LAMMPSAtomStyleHints& atomStyleHints, QWidget* parentWindow = nullptr);

private Q_SLOTS:

    /// Updates the displayed list of file data columns.
    void updateColumnList();

    /// Saves the values entered by the user and closes the dialog.
    void onOk();

private:

    LAMMPSDataImporter::LAMMPSAtomStyleHints& _atomStyleHints;
    QComboBox* _atomStyleList;
    QLabel* _subStylesLabel;
    std::array<QComboBox*,3> _subStyleLists;
    QLineEdit* _columnListField;
    QLabel* _columnMismatchLabel;
    QDialogButtonBox* _buttonBox;
};

}   // End of namespace
