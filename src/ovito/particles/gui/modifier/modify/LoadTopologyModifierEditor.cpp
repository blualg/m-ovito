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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/modifier/modify/LoadTopologyModifier.h>
#include <ovito/particles/modifier/modify/LoadTrajectoryModifier.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/gui/desktop/dataset/io/FileImporterEditor.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/io/FileImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/ModifierEvaluationRequest.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include "LoadTopologyModifierEditor.h"

#include <QLabel>
#include <QPushButton>

namespace Ovito {

namespace {

bool pipelineContainsNode(Pipeline* pipeline, const ModificationNode* targetNode)
{
    if(!pipeline || !targetNode)
        return false;

    for(PipelineNode* node = pipeline->head(); node != nullptr; ) {
        if(node == static_cast<const PipelineNode*>(targetNode))
            return true;
        if(ModificationNode* modNode = dynamic_object_cast<ModificationNode>(node))
            node = modNode->input();
        else
            break;
    }

    return false;
}

Pipeline* findOwningPipeline(LoadTopologyModifierEditor* editor, ModificationNode* targetNode)
{
    if(!editor || !targetNode)
        return nullptr;

    if(Pipeline* pipeline = editor->selectedPipeline(); pipelineContainsNode(pipeline, targetNode))
        return pipeline;

    Pipeline* owner = nullptr;
    editor->visitScenePipelines([&](SceneNode* sceneNode) {
        if(Pipeline* pipeline = sceneNode->pipeline(); pipelineContainsNode(pipeline, targetNode)) {
            owner = pipeline;
            return false;
        }
        return true;
    });
    return owner;
}

ModificationNode* bottomModificationNode(Pipeline* pipeline)
{
    auto* modNode = dynamic_object_cast<ModificationNode>(pipeline ? pipeline->head() : nullptr);
    while(modNode) {
        if(auto* predecessor = dynamic_object_cast<ModificationNode>(modNode->input()))
            modNode = predecessor;
        else
            break;
    }
    return modNode;
}

OORef<FileImporter> createImporterForUrl(LoadTopologyModifierEditor* editor, const QUrl& url, const FileImporterClass* importerType, const QString& importerFormat)
{
    OVITO_ASSERT(editor);

    OORef<FileImporter> importer;
    if(!importerType) {
        importer = ProgressDialog::blockForFuture(FileImporter::autodetectFileFormat(url), editor->ui(), editor->parentWindow(), LoadTopologyModifierEditor::tr("Inspecting topology file"));
        if(!importer)
            throw Exception(LoadTopologyModifierEditor::tr("Could not auto-detect the format of the selected topology file."));
    }
    else {
        importer = static_object_cast<FileImporter>(importerType->createInstance());
        if(!importer)
            throw Exception(LoadTopologyModifierEditor::tr("Could not initialize the selected file importer."));
        importer->setSelectedFileFormat(importerFormat);
    }

    for(OvitoClassPtr clazz = &importer->getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
        if(OvitoClassPtr editorClass = PropertiesEditor::registry().getEditorClass(clazz)) {
            if(editorClass->isDerivedFrom(FileImporterEditor::OOClass())) {
                if(OORef<FileImporterEditor> importerEditor = dynamic_object_cast<FileImporterEditor>(editorClass->createInstance())) {
                    importerEditor->setUserInterface(editor->ui());
                    importerEditor->inspectNewFile(importer, url);
                }
            }
        }
    }

    return importer;
}

} // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(LoadTopologyModifierEditor);
SET_OVITO_OBJECT_EDITOR(LoadTopologyModifier, LoadTopologyModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LoadTopologyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Load topology"), rolloutParams, "manual:particles.modifiers.load_topology");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* loadTopologyButton = new QPushButton(tr("Load topology from file..."));
    connect(loadTopologyButton, &QPushButton::clicked, this, &LoadTopologyModifierEditor::onLoadTopologyFromFile);
    layout->addWidget(loadTopologyButton);

    auto* topologyInfoLabel = new QLabel(tr("Loads topology and static particle properties such as bonds, angles, dihedrals, charges, masses, and molecule identifiers. If the trajectory file provides a property too, the trajectory values take precedence."));
    topologyInfoLabel->setWordWrap(true);
    layout->addWidget(topologyInfoLabel);
}

/******************************************************************************
* Lets the user load topology from a separate file.
******************************************************************************/
void LoadTopologyModifierEditor::onLoadTopologyFromFile()
{
    handleExceptions([this]() {
        auto* loadTopologyModifier = static_object_cast<LoadTopologyModifier>(editObject());
        if(!loadTopologyModifier)
            throw Exception(tr("No Load topology modifier is currently being edited."));

        ModificationNode* targetNode = modificationNode();
        if(!targetNode)
            targetNode = loadTopologyModifier->someNode();
        if(!targetNode)
            throw Exception(tr("Could not determine the pipeline node associated with this Load topology modifier."));

        Pipeline* pipeline = findOwningPipeline(this, targetNode);
        if(!pipeline)
            throw Exception(tr("Could not determine the pipeline containing this Load topology modifier."));

        OORef<PipelineNode> trajectorySource;
        if(ModificationNode* firstModNode = bottomModificationNode(pipeline)) {
            if(auto* loadTrajectoryModifier = dynamic_object_cast<LoadTrajectoryModifier>(firstModNode->modifier()))
                trajectorySource = loadTrajectoryModifier->trajectorySource();
        }
        if(!trajectorySource)
            trajectorySource = pipeline->source();
        if(!trajectorySource)
            throw Exception(tr("The current pipeline has no source that can be reused as the trajectory input."));

        auto importerClasses = PluginManager::instance().metaclassMembers<FileImporter>(ParticleImporter::OOClass());
        ImportFileDialog dialog(ui(), importerClasses, parentWindow(), tr("Pick topology file"), false);
        if(FileSource* fileSource = dynamic_object_cast<FileSource>(trajectorySource.get())) {
            if(!fileSource->sourceUrls().empty() && fileSource->sourceUrls().front().isLocalFile()) {
#ifndef Q_OS_LINUX
                dialog.selectFile(fileSource->sourceUrls().front().toLocalFile());
#else
                dialog.setDirectory(QFileInfo(fileSource->sourceUrls().front().toLocalFile()).dir());
#endif
            }
        }
        if(dialog.exec() != QDialog::Accepted)
            return;

        const QUrl topologyUrl = dialog.urlToImport();
        const auto& [importerType, importerFormat] = dialog.selectedFileImporter();
        OORef<FileImporter> importer = createImporterForUrl(this, topologyUrl, importerType, importerFormat);

        auto* particleImporter = dynamic_object_cast<ParticleImporter>(importer.get());
        if(!particleImporter)
            throw Exception(tr("The selected file does not provide particle topology information."));
        if(particleImporter->isTrajectoryFormat())
            throw Exception(tr("Please choose a topology file, not a trajectory file."));

        auto* fileSourceImporter = dynamic_object_cast<FileSourceImporter>(importer.get());
        if(!fileSourceImporter)
            throw Exception(tr("The selected importer cannot be used as a pipeline file source."));

        performTransaction(tr("Load topology file"), [this, pipeline = OORef<Pipeline>(pipeline), topologyUrl, fileSourceImporter = OORef<FileSourceImporter>(fileSourceImporter), trajectorySource = OORef<PipelineNode>(trajectorySource)]() {
            OORef<FileSource> topologySource = OORef<FileSource>::create();
            topologySource->setSource({topologyUrl}, fileSourceImporter, true);

            ModificationNode* firstModNode = bottomModificationNode(pipeline);
            LoadTrajectoryModifier* existingLoadTrajectory = firstModNode ? dynamic_object_cast<LoadTrajectoryModifier>(firstModNode->modifier()) : nullptr;

            pipeline->setSource(topologySource);

            if(!existingLoadTrajectory) {
                std::vector<OORef<RefMaker>> dependentsList;
                topologySource->visitDependents([&](RefMaker* dependent) {
                    if(dynamic_object_cast<ModificationNode>(dependent) || dynamic_object_cast<Pipeline>(dependent))
                        dependentsList.push_back(dependent);
                });

                OORef<LoadTrajectoryModifier> loadTrajectoryModifier = OORef<LoadTrajectoryModifier>::create();
                OORef<ModificationNode> loadTrajectoryNode = loadTrajectoryModifier->createModificationNode();
                loadTrajectoryNode->setModifier(loadTrajectoryModifier);
                loadTrajectoryNode->setInput(topologySource);
                loadTrajectoryModifier->setTrajectorySource(trajectorySource);
                loadTrajectoryModifier->initializeModifier(ModifierInitializationRequest(currentAnimationTime(), false, true, loadTrajectoryNode));

                for(RefMaker* dependent : dependentsList) {
                    if(ModificationNode* predecessorModNode = dynamic_object_cast<ModificationNode>(dependent)) {
                        if(predecessorModNode->input() == topologySource)
                            predecessorModNode->setInput(loadTrajectoryNode);
                    }
                    else if(Pipeline* dependentPipeline = dynamic_object_cast<Pipeline>(dependent)) {
                        if(dependentPipeline->head() == topologySource)
                            dependentPipeline->setHead(loadTrajectoryNode);
                    }
                }
            }
        });
    });
}

}   // End of namespace
