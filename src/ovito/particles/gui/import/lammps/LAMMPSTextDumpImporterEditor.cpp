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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/lammps/LAMMPSTextDumpImporter.h>
#include <ovito/stdobj/gui/properties/InputColumnMappingDialog.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "LAMMPSTextDumpImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSTextDumpImporterEditor);
SET_OVITO_OBJECT_EDITOR(LAMMPSTextDumpImporter, LAMMPSTextDumpImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LAMMPSTextDumpImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("LAMMPS dump reader"), rolloutParams, "manual:file_formats.input.lammps_dump");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(optionsBox);

    // Multi-timestep file
    _multitimestepUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FileSourceImporter::isMultiTimestepFile));
    sublayout->addWidget(_multitimestepUI->checkBox());

    // Sort particles
    BooleanParameterUI* sortParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::sortParticles));
    sublayout->addWidget(sortParticlesUI->checkBox());

    QGroupBox* columnMappingBox = new QGroupBox(tr("File columns"), rollout);
    sublayout = new QVBoxLayout(columnMappingBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(columnMappingBox);

    BooleanRadioButtonParameterUI* useCustomMappingUI = createParamUI<BooleanRadioButtonParameterUI>(PROPERTY_FIELD(LAMMPSTextDumpImporter::useCustomColumnMapping));
    useCustomMappingUI->buttonFalse()->setText(tr("Automatic mapping"));
    sublayout->addWidget(useCustomMappingUI->buttonFalse());
    useCustomMappingUI->buttonTrue()->setText(tr("User-defined mapping to particle properties"));
    sublayout->addWidget(useCustomMappingUI->buttonTrue());

    QPushButton* editMappingButton = new QPushButton(tr("Edit column mapping..."));
    sublayout->addWidget(editMappingButton);
    connect(editMappingButton, &QPushButton::clicked, this, &LAMMPSTextDumpImporterEditor::onEditColumnMapping);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool LAMMPSTextDumpImporterEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == FileSourceImporter::MultiTimestepFileChanged) {
        _multitimestepUI->updateUI();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the user pressed the "Edit column mapping" button.
******************************************************************************/
void LAMMPSTextDumpImporterEditor::onEditColumnMapping()
{
    OORef<LAMMPSTextDumpImporter> importer = static_object_cast<LAMMPSTextDumpImporter>(editObject());
    if(!importer)
        return;

    handleExceptions([&]() {

        // Determine the currently loaded data file of the FileSource.
        FileSource* fileSource = importer->fileSource();
        if(!fileSource || fileSource->frames().empty())
            return;
        int frameIndex = qBound(0, fileSource->dataCollectionFrame(), fileSource->frames().size() - 1);

        // Read the list of columns from the file's header.
        Future<ParticleInputColumnMapping> future = importer->inspectFileHeader(fileSource->frames()[frameIndex]);

        // Show a progress dialog while performing the whole operation.
        scheduleOperationAfter(std::move(future), [this, importer=std::move(importer)](ParticleInputColumnMapping&& mapping) {

            if(!importer->customColumnMapping().empty()) {
                ParticleInputColumnMapping customMapping = importer->customColumnMapping();
                customMapping.resize(mapping.size());
                for(size_t i = 0; i < customMapping.size(); i++)
                    customMapping[i].columnName = mapping[i].columnName;
                mapping = std::move(customMapping);
            }

            InputColumnMappingDialog dialog(mainWindow(), mapping, parentWindow());
            if(dialog.exec() == QDialog::Accepted) {
                performTransaction(tr("Change file column mapping"), [&]() {
                    importer->setCustomColumnMapping(dialog.mapping());
                    importer->setUseCustomColumnMapping(true);
                });
            }
        });
    });
}

}   // End of namespace
