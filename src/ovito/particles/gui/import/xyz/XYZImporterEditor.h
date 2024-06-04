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
#include <ovito/particles/import/xyz/XYZImporter.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>

namespace Ovito {

/**
 * \brief A properties editor for the XYZImporter class.
 */
class XYZImporterEditor : public FileImporterEditor
{
    OVITO_CLASS(XYZImporterEditor)
    Q_OBJECT

public:

    /// This is called by the system when the user has selected a new file to import.
    virtual void inspectNewFile(FileImporter* importer, const QUrl& sourceFile, MainWindow& mainWindow) override;

    /// Displays a dialog box that allows the user to edit the custom file column to particle property mapping.
    void showEditColumnMappingDialog(XYZImporter* importer, const FileSourceImporter::Frame& frame);

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

protected Q_SLOTS:

    /// Is called when the user pressed the "Edit column mapping" button.
    void onEditColumnMapping();

private:

    BooleanParameterUI* _multitimestepUI;
};

}   // End of namespace
