////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/app/undo/UndoStack.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/utilities/io/ObjectSaveStream.h>
#include <ovito/core/utilities/io/ObjectLoadStream.h>
#include <ovito/core/utilities/io/FileManager.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include "GuiDataSetContainer.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(GuiDataSetContainer);

/******************************************************************************
* Initializes the dataset manager.
******************************************************************************/
GuiDataSetContainer::GuiDataSetContainer(TaskManager& taskManager, MainWindow& mainWindow) : DataSetContainer(taskManager, mainWindow), _mainWindow(mainWindow)
{
}

/******************************************************************************
* Save the current dataset.
******************************************************************************/
bool GuiDataSetContainer::fileSave()
{
    if(currentSet() == nullptr)
        return false;

    // Ask the user for a filename if there is no one set.
    if(currentSet()->filePath().isEmpty())
        return fileSaveAs();

    // Save dataset to file.
    return userInterface().handleExceptions([&] {
        currentSet()->saveToFile(currentSet()->filePath());
        mainWindow().undoStack()->setClean();
    });
}

/******************************************************************************
* This is the implementation of the "Save As" action.
* Returns true, if the scene has been saved.
******************************************************************************/
bool GuiDataSetContainer::fileSaveAs(const QString& filename)
{
    if(currentSet() == nullptr)
        return false;

    if(filename.isEmpty()) {

        QFileDialog dialog(&mainWindow(), tr("Save Session State As"));
        dialog.setNameFilter(tr("OVITO State Files (*.ovito);;All Files (*)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setDefaultSuffix("ovito");

        QSettings settings;
        settings.beginGroup("file/scene");

        if(currentSet()->filePath().isEmpty()) {
            QString defaultPath = settings.value("last_directory").toString();
            if(!defaultPath.isEmpty())
                dialog.setDirectory(defaultPath);
        }
        else {
#ifndef Q_OS_LINUX
            dialog.selectFile(currentSet()->filePath());
#else
            // Workaround for bug in QFileDialog on Linux (Qt 6.2.4) crashing in exec() when selectFile() is called before (OVITO issue #216).
            dialog.setDirectory(QFileInfo(currentSet()->filePath()).dir());
#endif
        }

        if(!dialog.exec())
            return false;

        QStringList files = dialog.selectedFiles();
        if(files.isEmpty())
            return false;
        QString newFilename = files.front();

        // Remember directory for the next time...
        settings.setValue("last_directory", dialog.directory().absolutePath());

        currentSet()->setFilePath(newFilename);
    }
    else {
        currentSet()->setFilePath(filename);
    }
    return fileSave();
}

/******************************************************************************
* If the scene has been changed this will ask the user if he wants
* to save the changes.
* Returns false if the operation has been canceled by the user.
******************************************************************************/
bool GuiDataSetContainer::askForSaveChanges()
{
    if(!currentSet() || mainWindow().undoStack()->isClean() || currentSet()->filePath().isEmpty())
        return true;

    QString message;
    if(currentSet()->filePath().isEmpty() == false) {
        message = tr("The current session state has been modified. Do you want to save the changes?");
        message += QString("\n\nFile: %1").arg(currentSet()->filePath());
    }
    else {
        message = tr("The current program session has not been saved. Do you want to save it?");
    }

    QMessageBox::StandardButton result = QMessageBox::question(&mainWindow(), tr("Save changes"),
        message,
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);
    if(result == QMessageBox::Cancel)
        return false; // Operation canceled by user
    else if(result == QMessageBox::No)
        return true; // Continue without saving scene first.
    else {
        // Save scene first.
        return fileSave();
    }
}

/******************************************************************************
* Imports a given file into the scene.
******************************************************************************/
bool GuiDataSetContainer::importFiles(const std::vector<QUrl>& urls, const FileImporterClass* importerType, const QString& importerFormat)
{
    OVITO_ASSERT(ExecutionContext::current().isValid());
    OVITO_ASSERT(!urls.empty());

    // Create a reference to the active scene to keep it alive during this long-running operation.
    OORef<Scene> scene = activeScene();
    OVITO_ASSERT(scene);

    std::vector<std::pair<QUrl, OORef<FileImporter>>> urlImporters;
    for(const QUrl& url : urls) {
        if(!url.isValid())
            throw Exception(tr("Failed to import file. URL is not valid: %1").arg(url.toString()));

        OORef<FileImporter> importer;
        if(!importerType) {

            // Detect file format.
            Future<OORef<FileImporter>> importerFuture = FileImporter::autodetectFileFormat(url);
            if(!importerFuture.waitForFinished())
                return false;

            importer = importerFuture.result();
            if(!importer)
                throw Exception(tr("Could not auto-detect the format of the file %1. The file format might not be supported.").arg(url.fileName()));
        }
        else {
            importer = static_object_cast<FileImporter>(importerType->createInstance());
            if(!importer)
                throw Exception(tr("Failed to import file. Could not initialize import service."));
            importer->setSelectedFileFormat(importerFormat);
        }

        urlImporters.push_back(std::make_pair(url, std::move(importer)));
    }

    // Order URLs and their corresponding importers.
    std::stable_sort(urlImporters.begin(), urlImporters.end(), [](const auto& a, const auto& b) {
        int pa = a.second->importerPriority();
        int pb = b.second->importerPriority();
        if(pa > pb) return true;
        if(pa < pb) return false;
        return a.second->getOOClass().name() < b.second->getOOClass().name();
    });

    // Display the optional UI (which is provided by the corresponding FileImporterEditor class) for each importer.
    for(const auto& item : urlImporters) {
        const QUrl& url = item.first;
        const OORef<FileImporter>& importer = item.second;
        for(OvitoClassPtr clazz = &importer->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
            OvitoClassPtr editorClass = PropertiesEditor::registry().getEditorClass(clazz);
            if(editorClass && editorClass->isDerivedFrom(FileImporterEditor::OOClass())) {
                OORef<FileImporterEditor> editor = dynamic_object_cast<FileImporterEditor>(editorClass->createInstance());
                if(editor) {
                    if(!editor->inspectNewFile(importer, url, mainWindow()))
                        return false;
                }
            }
        }
    }

    // Determine how the file's data should be inserted into the current scene.
    FileImporter::ImportMode importMode = FileImporter::ResetScene;

    const QUrl& url = urlImporters.front().first;
    OORef<FileImporter> importer = urlImporters.front().second;
    if(importer->isReplaceExistingPossible(scene, urls)) {
        // Ask user if the existing pipeline should be preserved or reset.
        QMessageBox msgBox(QMessageBox::Question, tr("Import file"),
                tr("Do you want to reset the existing pipeline?"),
                QMessageBox::Yes | QMessageBox::Cancel, &mainWindow());
#ifdef OVITO_BUILD_PROFESSIONAL
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets.</p>"));
#else
        msgBox.setInformativeText(tr(
            "<p>Select <b>Yes</b> to start over and discard the existing pipeline before importing the new file.</p>"
            "<p>Select <b>No</b> to keep modifiers in the current pipeline and replace the input data with the selected file.</p>"
            "<p>Select <b>Add to scene</b> to create an additional pipeline and visualize multiple datasets (requires <a href=\"https://www.ovito.org/about/ovito-pro/\">OVITO Pro</a>).</p>"));
#endif
        QPushButton* noButton = msgBox.addButton(tr("No"), QMessageBox::NoRole);
        QPushButton* addToSceneButton = msgBox.addButton(tr("Add to scene"), QMessageBox::NoRole);
#ifndef OVITO_BUILD_PROFESSIONAL
        addToSceneButton->setEnabled(false);
#endif
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setEscapeButton(QMessageBox::Cancel);
        msgBox.exec();

        if(msgBox.clickedButton() == msgBox.button(QMessageBox::Cancel)) {
            return false; // Operation canceled by user.
        }
        else if(msgBox.clickedButton() == msgBox.button(QMessageBox::Yes)) {
            importMode = FileImporter::ResetScene;
            // Ask user if current scene should be saved before it is replaced by the imported data.
            if(!askForSaveChanges())
                return false;
        }
        else if(msgBox.clickedButton() == addToSceneButton) {
            importMode = FileImporter::AddToScene;
        }
        else {
            // No button
            importMode = FileImporter::ReplaceSelected;
        }
    }
    else if(scene->children().empty() == false) {
        // Ask user if the current scene should be completely replaced by the imported data.
        QMessageBox::StandardButton result = QMessageBox::question(&mainWindow(), tr("Import file"),
            tr("Do you want to keep the existing objects in the current scene?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);

        if(result == QMessageBox::Cancel) {
            return false; // Operation canceled by user.
        }
        else if(result == QMessageBox::No) {
            importMode = FileImporter::ResetScene;

            // Ask user if current scene should be saved before it is replaced by the imported data.
            if(!askForSaveChanges())
                return false;
        }
        else {
            importMode = FileImporter::AddToScene;
        }
    }

    // Do not create any animation keys during import.
    AnimationSuspender animSuspender(mainWindow());

    if(OORef<PipelineSceneNode> pipeline = importer->importFileSet(scene, std::move(urlImporters), importMode, true, ImportFileDialog::multiFileImportMode())) {
        if(importMode == FileImporter::ResetScene) {
            mainWindow().undoStack()->clear();
            currentSet()->setFilePath(QString());
        } 
        return true;
    }

    return false;
}

}   // End of namespace
