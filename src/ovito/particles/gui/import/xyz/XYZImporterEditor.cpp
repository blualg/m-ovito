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
#include <ovito/particles/import/xyz/XYZImporter.h>
#include <ovito/stdobj/gui/properties/InputColumnMappingDialog.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "XYZImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(XYZImporterEditor);
SET_OVITO_OBJECT_EDITOR(XYZImporter, XYZImporterEditor);

/******************************************************************************
* This method is called by the FileSource each time a new source
* file has been selected by the user.
******************************************************************************/
void XYZImporterEditor::inspectNewFile(FileImporter* importer, const QUrl& sourceFile, MainWindow& mainWindow)
{
    XYZImporter* xyzImporter = static_object_cast<XYZImporter>(importer);
    Future<ParticleInputColumnMapping> inspectFuture = xyzImporter->inspectFileHeader(FileSourceImporter::Frame(sourceFile));

    {
        // Block UI until reading is done.
        ProgressDialog progressDialog(mainWindow, &mainWindow, inspectFuture, tr("Inspecting file header"));
        inspectFuture.waitForFinished();
    }

    ParticleInputColumnMapping mapping = inspectFuture.result();

    // If column names were given in the XYZ file, use them rather than popping up a dialog.
    if(mapping.hasFileColumnNames())
        return;

    // If this is a newly created file importer, load old mapping from application settings store.
    if(xyzImporter->columnMapping().empty()) {
        QSettings settings;
        settings.beginGroup("viz/importer/xyz/");
        if(settings.contains("columnmapping")) {
            try {
                ParticleInputColumnMapping storedMapping;
                storedMapping.fromByteArray(settings.value("columnmapping").toByteArray());
                std::copy_n(storedMapping.begin(), std::min(storedMapping.size(), mapping.size()), mapping.begin());
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(tr("Failed to load last used column-to-property mapping from application settings store."));
                ex.logError();
            }
            for(auto& column : mapping)
                column.columnName.clear();
        }
    }
    else if(mapping.size() == xyzImporter->columnMapping().size()) {
        // If there was a mapping set up for a previously imported XYZ file,
        // and if the newly imported XYZ file has no column name information but the same number
        // of columns, adopt the existing column mapping from previously imported file.
        if(boost::algorithm::none_of(mapping, [](const auto& column) { return column.isMapped(); })) {
            boost::range::copy(xyzImporter->columnMapping(), mapping.begin());
        }
    }

    InputColumnMappingDialog dialog(mainWindow, mapping, &mainWindow);
    if(dialog.exec() == QDialog::Accepted) {
        xyzImporter->setColumnMapping(dialog.mapping());

        // Remember the user-defined mapping for the next time.
        QSettings settings;
        settings.beginGroup("viz/importer/xyz/");
        settings.setValue("columnmapping", dialog.mapping().toByteArray());
        settings.endGroup();
    }
    else {
        this_task::cancelAndThrow();
    }
}

/******************************************************************************
 * Displays a dialog box that allows the user to edit the custom file column to particle
 * property mapping.
 *****************************************************************************/
void XYZImporterEditor::showEditColumnMappingDialog(XYZImporter* importer, const FileSourceImporter::Frame& frame)
{
    Future<ParticleInputColumnMapping> inspectFuture = importer->inspectFileHeader(frame);

    {
        // Block UI until reading is done.
        ProgressDialog progressDialog(mainWindow(), parentWindow(),inspectFuture, tr("Inspecting file header"));
        inspectFuture.waitForFinished();
    }

    ParticleInputColumnMapping mapping = inspectFuture.result();

    if(!importer->columnMapping().empty()) {
        ParticleInputColumnMapping customMapping = importer->columnMapping();
        customMapping.resize(mapping.size());
        for(size_t i = 0; i < customMapping.size(); i++)
            customMapping[i].columnName = mapping[i].columnName;
        mapping = customMapping;
    }

    InputColumnMappingDialog dialog(mainWindow(), mapping, parentWindow());
    if(dialog.exec() == QDialog::Accepted) {
        importer->setColumnMapping(dialog.mapping());
        // Remember the user-defined mapping for the next time.
        QSettings settings;
        settings.beginGroup("viz/importer/xyz/");
        settings.setValue("columnmapping", dialog.mapping().toByteArray());
        settings.endGroup();
    }
    else {
        this_task::cancelAndThrow();
    }
}

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void XYZImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("XYZ reader"), rolloutParams, "manual:file_formats.input.xyz");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(optionsBox);

    // Multi-timestep file
    _multitimestepUI = new BooleanParameterUI(this, PROPERTY_FIELD(FileSourceImporter::isMultiTimestepFile));
    sublayout->addWidget(_multitimestepUI->checkBox());

    // Auto-rescale reduced coordinates.
    BooleanParameterUI* rescaleReducedUI = new BooleanParameterUI(this, PROPERTY_FIELD(XYZImporter::autoRescaleCoordinates));
    sublayout->addWidget(rescaleReducedUI->checkBox());

    // Sort particles
    BooleanParameterUI* sortParticlesUI = new BooleanParameterUI(this, PROPERTY_FIELD(ParticleImporter::sortParticles));
    sublayout->addWidget(sortParticlesUI->checkBox());

    QGroupBox* columnMappingBox = new QGroupBox(tr("File columns"), rollout);
    sublayout = new QVBoxLayout(columnMappingBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(columnMappingBox);

    QPushButton* editMappingButton = new QPushButton(tr("Edit column mapping..."));
    sublayout->addWidget(editMappingButton);
    connect(editMappingButton, &QPushButton::clicked, this, &XYZImporterEditor::onEditColumnMapping);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool XYZImporterEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == FileSourceImporter::MultiTimestepFileChanged) {
        _multitimestepUI->updateUI();
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Is called when the user pressed the "Edit column mapping" button.
******************************************************************************/
void XYZImporterEditor::onEditColumnMapping()
{
    if(XYZImporter* importer = static_object_cast<XYZImporter>(editObject())) {
        performTransaction(tr("Change file column mapping"), [this, importer]() {

            // Determine the currently loaded data file of the FileSource.
            FileSource* fileSource = importer->fileSource();
            if(!fileSource || fileSource->frames().empty()) return;
            int frameIndex = qBound(0, fileSource->dataCollectionFrame(), fileSource->frames().size()-1);

            // Show the dialog box, which lets the user modify the file column mapping.
            showEditColumnMappingDialog(importer, fileSource->frames()[frameIndex]);

            importer->requestReload();
        });
    }
}

}   // End of namespace
