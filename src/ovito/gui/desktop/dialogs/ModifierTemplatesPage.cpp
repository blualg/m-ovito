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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include "ModifierTemplatesPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ModifierTemplatesPage);

/******************************************************************************
* Creates the widget that contains the plugin specific setting controls.
******************************************************************************/
void ModifierTemplatesPage::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, tr("Modifier templates"));
    QGridLayout* layout1 = new QGridLayout(page);
    layout1->setColumnStretch(0, 1);
    layout1->setRowStretch(3, 1);
    layout1->setSpacing(2);

    QLabel* label = new QLabel(tr(
            "Modifier templates you define here will appear in the list of available modifiers, from where you can quickly insert them into a data pipeline. "
            "Templates may consist of several modifiers, making your life easier if you repeatedly need the same pipeline sequence."));
    label->setWordWrap(true);
    layout1->addWidget(label, 0, 0, 1, 2);
    layout1->setRowMinimumHeight(1, 10);

    layout1->addWidget(new QLabel(tr("Modifier templates:")), 2, 0);
    _listWidget = new QListView(page);
    _listWidget->setUniformItemSizes(true);
    _listWidget->setModel(ModifierTemplates::get());
    layout1->addWidget(_listWidget, 3, 0);

    QVBoxLayout* layout2 = new QVBoxLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setSpacing(4);
    layout1->addLayout(layout2, 3, 1);
    QPushButton* createTemplateBtn = new QPushButton(tr("New..."), page);
    connect(createTemplateBtn, &QPushButton::clicked, this, &ModifierTemplatesPage::onCreateTemplate);
    layout2->addWidget(createTemplateBtn);
    QPushButton* deleteTemplateBtn = new QPushButton(tr("Delete"), page);
    connect(deleteTemplateBtn, &QPushButton::clicked, this, &ModifierTemplatesPage::onDeleteTemplate);
    deleteTemplateBtn->setEnabled(false);
    layout2->addWidget(deleteTemplateBtn);
    QPushButton* renameTemplateBtn = new QPushButton(tr("Rename..."), page);
    connect(renameTemplateBtn, &QPushButton::clicked, this, &ModifierTemplatesPage::onRenameTemplate);
    renameTemplateBtn->setEnabled(false);
    layout2->addWidget(renameTemplateBtn);
    layout2->addSpacing(10);
    QPushButton* exportTemplatesBtn = new QPushButton(tr("Export..."), page);
    connect(exportTemplatesBtn, &QPushButton::clicked, this, &ModifierTemplatesPage::onExportTemplates);
    layout2->addWidget(exportTemplatesBtn);
    QPushButton* importTemplatesBtn = new QPushButton(tr("Import..."), page);
    connect(importTemplatesBtn, &QPushButton::clicked, this, &ModifierTemplatesPage::onImportTemplates);
    layout2->addWidget(importTemplatesBtn);
    layout2->addStretch(1);

    connect(_listWidget->selectionModel(), &QItemSelectionModel::selectionChanged, [this, deleteTemplateBtn, renameTemplateBtn]() {
        bool sel = !_listWidget->selectionModel()->selectedRows().empty();
        deleteTemplateBtn->setEnabled(sel);
        renameTemplateBtn->setEnabled(sel);
    });
}

/******************************************************************************
* Is invoked when the user presses the "Create template" button.
******************************************************************************/
void ModifierTemplatesPage::onCreateTemplate()
{
    mainWindow().handleExceptions([&] {
        QDialog dlg(settingsDialog());
        dlg.setWindowTitle(tr("Create Modifier Template"));
        QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
        mainLayout->setSpacing(2);

        mainLayout->addWidget(new QLabel(tr("Modifiers to include in the new template:")));
        QTreeWidget* modifierListWidget = new QTreeWidget(&dlg);
        modifierListWidget->setUniformRowHeights(true);
        modifierListWidget->setRootIsDecorated(false);
        modifierListWidget->header()->hide();
        PipelineListModel* pipelineModel = mainWindow().commandPanel()->modifyPage()->pipelineListModel();
        QVector<RefTarget*> selectedPipelineObjects = pipelineModel->selectedObjects();
        QVector<QTreeWidgetItem*> itemList;
        int rowCount = 0;

        // Iterate over the modifiers in the selected pipeline.
        if(Pipeline* pipeline = pipelineModel->selectedPipeline()) {
            ModifierGroup* currentGroup = nullptr;
            QTreeWidgetItem* currentGroupItem = nullptr;
            ModificationNode* modNode = dynamic_object_cast<ModificationNode>(pipeline->head());
            while(modNode) {
                if(modNode->modifierGroup() != currentGroup) {
                    if(modNode->modifierGroup()) {
                        currentGroupItem = new QTreeWidgetItem(modifierListWidget, {modNode->modifierGroup()->objectTitle()});
                        currentGroupItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate);
                        currentGroupItem->setExpanded(true);
                        rowCount++;
                    }
                    else currentGroupItem = nullptr;
                    currentGroup = modNode->modifierGroup();
                }
                if(modNode->modifier()) {
                    QTreeWidgetItem* listItem = currentGroupItem
                        ? new QTreeWidgetItem(currentGroupItem, {modNode->modifier()->objectTitle()})
                        : new QTreeWidgetItem(modifierListWidget, {modNode->modifier()->objectTitle()});
                    listItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                    if(selectedPipelineObjects.contains(modNode) || selectedPipelineObjects.contains(modNode->modifierGroup())) {
                        listItem->setCheckState(0, Qt::Checked);
                    }
                    else {
                        listItem->setCheckState(0, Qt::Unchecked);
                    }
                    listItem->setData(0, Qt::UserRole, QVariant::fromValue(OORef<OvitoObject>(modNode->modifier())));
                    itemList.push_back(listItem);
                    rowCount++;
                }
                modNode = dynamic_object_cast<ModificationNode>(modNode->input());
            }
        }
        if(itemList.empty())
            throw Exception(tr("A modifier template must always be created on the basis of existing modifiers, but the current data pipeline does not contain any modifiers. "
                                "Please close this dialog, insert some modifier into the pipeline first, configure its settings and then come back here to create a template from it."));
        modifierListWidget->setMaximumHeight(modifierListWidget->sizeHintForRow(0) * qBound(3, rowCount, 10) + 2 * modifierListWidget->frameWidth());
        mainLayout->addWidget(modifierListWidget, 1);

        mainLayout->addSpacing(8);
        mainLayout->addWidget(new QLabel(tr("Template name:")));
        QComboBox* nameBox = new QComboBox(&dlg);
        nameBox->setEditable(true);
        nameBox->addItems(ModifierTemplates::get()->templateList());

        ModificationNode* selectedModNode = (selectedPipelineObjects.size() == 1) ? dynamic_object_cast<ModificationNode>(selectedPipelineObjects.front()) : nullptr;
        if(selectedModNode && selectedModNode->modifier()) {
            if(selectedModNode->modifier()->title().isEmpty())
                nameBox->setCurrentText(tr("Custom %1").arg(selectedModNode->modifier()->objectTitle()));
            else
                nameBox->setCurrentText(selectedModNode->modifier()->title());
        }
        else if(ModifierGroup* selectedModGroup = (selectedPipelineObjects.size() == 1) ? dynamic_object_cast<ModifierGroup>(selectedPipelineObjects.front()) : nullptr) {
            if(selectedModGroup->title().isEmpty())
                nameBox->setCurrentText(tr("My %1").arg(selectedModGroup->objectTitle()));
            else
                nameBox->setCurrentText(selectedModGroup->title());
        }
        else {
            nameBox->setCurrentText(tr("Custom modifier template 1"));
        }

        mainLayout->addWidget(nameBox);

        mainLayout->addSpacing(12);
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::Help);
        connect(buttonBox, &QDialogButtonBox::accepted, [&dlg, nameBox, &itemList]() {
            QString name = nameBox->currentText().trimmed();
            if(name.isEmpty()) {
                MessageDialog::critical(&dlg, tr("Create modifier template"), tr("Please enter a name for the new modifier template."));
                return;
            }
            if(ModifierTemplates::get()->templateList().contains(name)) {
                if(MessageDialog::question(&dlg, tr("Create modifier template"), tr("A modifier template with the same name '%1' already exists. Do you want to replace it?").arg(name), QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
                    return;
            }
            int selCount = boost::count_if(itemList, [](QTreeWidgetItem* item) { return item->checkState(0) == Qt::Checked; });
            if(!selCount) {
                MessageDialog::critical(&dlg, tr("Create modifier template"), tr("Please check at least one modifier to include in the new template."));
                return;
            }
            dlg.accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        // Implement Help button.
        connect(buttonBox, &QDialogButtonBox::helpRequested, settingsDialog(), [&]() {
            mainWindow().actionManager()->openHelpTopic(QStringLiteral("manual:modifier_templates"));
        });

        mainLayout->addWidget(buttonBox);
        if(dlg.exec() == QDialog::Accepted) {
            QVector<OORef<Modifier>> selectedModifierList;
            for(QTreeWidgetItem* item : itemList) {
                if(item->checkState(0) == Qt::Checked) {
                    selectedModifierList.push_back(static_object_cast<Modifier>(item->data(0, Qt::UserRole).value<OORef<OvitoObject>>()));
                }
            }
            OVITO_ASSERT(!selectedModifierList.empty());
            int idx = ModifierTemplates::get()->createTemplate(nameBox->currentText().trimmed(), selectedModifierList);
            _listWidget->setCurrentIndex(_listWidget->model()->index(idx, 0));
            _dirtyFlag = true;
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Delete template" button.
******************************************************************************/
void ModifierTemplatesPage::onDeleteTemplate()
{
    mainWindow().handleExceptions([&] {
        QStringList selectedTemplates;
        for(const QModelIndex& index : _listWidget->selectionModel()->selectedRows())
            selectedTemplates.push_back(ModifierTemplates::get()->templateList()[index.row()]);
        for(const QString& templateName : selectedTemplates) {
            ModifierTemplates::get()->removeTemplate(templateName);
            _dirtyFlag = true;
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Rename template" button.
******************************************************************************/
void ModifierTemplatesPage::onRenameTemplate()
{
    mainWindow().handleExceptions([&] {
        for(const QModelIndex& index : _listWidget->selectionModel()->selectedRows()) {
            QString oldTemplateName = ModifierTemplates::get()->templateList()[index.row()];
            QString newTemplateName = oldTemplateName;
            for(;;) {
                newTemplateName = QInputDialog::getText(settingsDialog(), tr("Rename modifier template"),
                    tr("Please enter a new name for the modifier template:"),
                    QLineEdit::Normal, newTemplateName);
                if(newTemplateName.isEmpty() || newTemplateName == oldTemplateName) break;
                if(!ModifierTemplates::get()->templateList().contains(newTemplateName)) {
                    ModifierTemplates::get()->renameTemplate(oldTemplateName, newTemplateName);
                    _dirtyFlag = true;
                    break;
                }
                else {
                    MessageDialog::critical(settingsDialog(), tr("Rename modifier template"), tr("A modifier template with the name '%1' already exists. Please choose a different name.").arg(newTemplateName));
                }
            }
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Export templates" button.
******************************************************************************/
void ModifierTemplatesPage::onExportTemplates()
{
    mainWindow().handleExceptions([&] {
        if(ModifierTemplates::get()->templateList().empty())
            throw Exception(tr("There are no modifier templates to export."));

        QString filename = QFileDialog::getSaveFileName(settingsDialog(),
            tr("Export Modifier Templates"), QString(), tr("OVITO Modifier Templates (*.ovmod)"));
        if(filename.isEmpty())
            return;

        QFile::remove(filename);
        QSettings settings(filename, QSettings::IniFormat);
        settings.clear();
        ModifierTemplates::get()->commit(settings);
        settings.sync();
        if(settings.status() != QSettings::NoError)
            throw Exception(tr("I/O error while writing modifier template file."));
    });
}

/******************************************************************************
* Is invoked when the user presses the "Import templates" button.
******************************************************************************/
void ModifierTemplatesPage::onImportTemplates()
{
    mainWindow().handleExceptions([&] {
        QString filename = QFileDialog::getOpenFileName(settingsDialog(),
            tr("Import Modifier Templates"), QString(), tr("OVITO Modifier Templates (*.ovmod)"));
        if(filename.isEmpty())
            return;

        QSettings settings(filename, QSettings::IniFormat);
        if(settings.status() != QSettings::NoError)
            throw Exception(tr("I/O error while reading modifier template file."));
        if(ModifierTemplates::get()->load(settings) == 0)
            throw Exception(tr("The selected file does not contain any modifier templates."));

        _dirtyFlag = true;
    });
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void ModifierTemplatesPage::saveValues(QTabWidget* tabWidget)
{
    if(_dirtyFlag) {
        ModifierTemplates::get()->commit();
        _dirtyFlag = false;
    }
}

/******************************************************************************
* Lets the settings page restore the original values of changed settings when
* the user presses the Cancel button.
******************************************************************************/
void ModifierTemplatesPage::restoreValues(QTabWidget* tabWidget)
{
    mainWindow().handleExceptions([&] {
        if(_dirtyFlag) {
            ModifierTemplates::get()->restore();
            _dirtyFlag = false;
        }
    });
}

}   // End of namespace
