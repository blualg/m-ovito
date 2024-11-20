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
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "TemplatesPageBase.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(TemplatesPageBase);

/******************************************************************************
* Creates the widgets of the settings page
******************************************************************************/
void TemplatesPageBase::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, settingsPageTitle());
    QGridLayout* layout1 = new QGridLayout(page);
    layout1->setColumnStretch(0, 1);
    layout1->setRowStretch(3, 1);
    layout1->setSpacing(2);

    QLabel* label = new QLabel(settingsPageDescription());
    label->setWordWrap(true);
    layout1->addWidget(label, 0, 0, 1, 2);
    layout1->setRowMinimumHeight(1, 10);

    layout1->addWidget(new QLabel(tr("%1:").arg(settingsPageTitle())), 2, 0);
    _listWidget = new QListView(page);
    _listWidget->setUniformItemSizes(true);
    _listWidget->setModel(templateManager());
    layout1->addWidget(_listWidget, 3, 0);

    QVBoxLayout* layout2 = new QVBoxLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setSpacing(4);
    layout1->addLayout(layout2, 3, 1);
    QPushButton* createTemplateBtn = new QPushButton(tr("New..."), page);
    connect(createTemplateBtn, &QPushButton::clicked, this, &TemplatesPageBase::onCreateTemplate);
    layout2->addWidget(createTemplateBtn);
    QPushButton* deleteTemplateBtn = new QPushButton(tr("Delete"), page);
    connect(deleteTemplateBtn, &QPushButton::clicked, this, &TemplatesPageBase::onDeleteTemplate);
    deleteTemplateBtn->setEnabled(false);
    layout2->addWidget(deleteTemplateBtn);
    QPushButton* renameTemplateBtn = new QPushButton(tr("Rename..."), page);
    connect(renameTemplateBtn, &QPushButton::clicked, this, &TemplatesPageBase::onRenameTemplate);
    renameTemplateBtn->setEnabled(false);
    layout2->addWidget(renameTemplateBtn);
    layout2->addSpacing(10);
    QPushButton* exportTemplatesBtn = new QPushButton(tr("Export..."), page);
    connect(exportTemplatesBtn, &QPushButton::clicked, this, &TemplatesPageBase::onExportTemplates);
    layout2->addWidget(exportTemplatesBtn);
    QPushButton* importTemplatesBtn = new QPushButton(tr("Import..."), page);
    connect(importTemplatesBtn, &QPushButton::clicked, this, &TemplatesPageBase::onImportTemplates);
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
void TemplatesPageBase::onCreateTemplate()
{
    mainWindow().handleExceptions([&] {
        QDialog dlg(settingsDialog());
        dlg.setWindowTitle(tr("Create %1 template").arg(objectTypeNameLC()));
        QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);
        mainLayout->setSpacing(2);

        mainLayout->addWidget(new QLabel(tr("%1s to include in the new template:").arg(objectTypeName())));
        QTreeWidget* objectListWidget = new QTreeWidget(&dlg);
        objectListWidget->setUniformRowHeights(true);
        objectListWidget->setRootIsDecorated(false);
        objectListWidget->header()->hide();

        QComboBox* nameBox = new QComboBox(&dlg);
        nameBox->setEditable(true);
        nameBox->addItems(templateManager()->templateList());

        QVector<QTreeWidgetItem*> itemList = populateAvailableObjectsList(objectListWidget, nameBox);
        mainLayout->addWidget(objectListWidget, 1);

        mainLayout->addSpacing(8);
        mainLayout->addWidget(new QLabel(tr("Template name:")));
        mainLayout->addWidget(nameBox);

        mainLayout->addSpacing(12);
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel | QDialogButtonBox::Help);
        connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
            QString name = nameBox->currentText().trimmed();
            if(name.isEmpty()) {
                MessageDialog::critical(&dlg, tr("Create %1 template").arg(objectTypeNameLC()), tr("Please enter a name for the new %1 template.").arg(objectTypeNameLC()));
                return;
            }
            if(templateManager()->templateList().contains(name)) {
                if(MessageDialog::question(&dlg, tr("Create %1 template").arg(objectTypeNameLC()), tr("A %1 template with the same name '%2' already exists. Do you want to replace it?").arg(objectTypeNameLC()).arg(name), QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
                    return;
            }
            int selCount = boost::count_if(itemList, [](QTreeWidgetItem* item) { return item->checkState(0) == Qt::Checked; });
            if(!selCount) {
                MessageDialog::critical(&dlg, tr("Create %1 template").arg(objectTypeNameLC()), tr("Please check at least one %1 to include in the new template.").arg(objectTypeNameLC()));
                return;
            }
            dlg.accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        // Implement Help button.
        connect(buttonBox, &QDialogButtonBox::helpRequested, settingsDialog(), [&]() {
            mainWindow().actionManager()->openHelpTopic(helpTopicId());
        });

        mainLayout->addWidget(buttonBox);
        if(dlg.exec() == QDialog::Accepted) {
            QVector<OORef<RefTarget>> selectedObjectsList;
            for(QTreeWidgetItem* item : itemList) {
                if(item->checkState(0) == Qt::Checked) {
                    selectedObjectsList.push_back(static_object_cast<RefTarget>(item->data(0, Qt::UserRole).value<OORef<OvitoObject>>()));
                }
            }
            OVITO_ASSERT(!selectedObjectsList.empty());
            int idx = templateManager()->createTemplate(nameBox->currentText().trimmed(), selectedObjectsList);
            _listWidget->setCurrentIndex(_listWidget->model()->index(idx, 0));
            _dirtyFlag = true;
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Delete template" button.
******************************************************************************/
void TemplatesPageBase::onDeleteTemplate()
{
    mainWindow().handleExceptions([&] {
        QStringList selectedTemplates;
        for(const QModelIndex& index : _listWidget->selectionModel()->selectedRows())
            selectedTemplates.push_back(templateManager()->templateList()[index.row()]);
        for(const QString& templateName : selectedTemplates) {
            templateManager()->removeTemplate(templateName);
            _dirtyFlag = true;
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Rename template" button.
******************************************************************************/
void TemplatesPageBase::onRenameTemplate()
{
    mainWindow().handleExceptions([&] {
        for(const QModelIndex& index : _listWidget->selectionModel()->selectedRows()) {
            QString oldTemplateName = templateManager()->templateList()[index.row()];
            QString newTemplateName = oldTemplateName;
            for(;;) {
                newTemplateName = QInputDialog::getText(settingsDialog(), tr("Rename %1 template").arg(objectTypeNameLC()),
                    tr("Please enter a new name for the %1 template:").arg(objectTypeNameLC()),
                    QLineEdit::Normal, newTemplateName);
                if(newTemplateName.isEmpty() || newTemplateName == oldTemplateName) break;
                if(!templateManager()->templateList().contains(newTemplateName)) {
                    templateManager()->renameTemplate(oldTemplateName, newTemplateName);
                    _dirtyFlag = true;
                    break;
                }
                else {
                    MessageDialog::critical(settingsDialog(), tr("Rename %1 template").arg(objectTypeNameLC()), tr("A %1 template with the name '%2' already exists. Please choose a different name.").arg(objectTypeNameLC()).arg(newTemplateName));
                }
            }
        }
    });
}

/******************************************************************************
* Is invoked when the user presses the "Export templates" button.
******************************************************************************/
void TemplatesPageBase::onExportTemplates()
{
    mainWindow().handleExceptions([&] {
        if(templateManager()->templateList().empty())
            throw Exception(tr("There are no %1 templates to export.").arg(objectTypeNameLC()));

        TaskManager::setNativeDialogActive(true);
        QString filename = QFileDialog::getSaveFileName(settingsDialog(),
            tr("Export %1 templates").arg(objectTypeNameLC()), QString(), templateFileFilter());
        TaskManager::setNativeDialogActive(false);
        if(filename.isEmpty())
            return;

        QFile::remove(filename);
        QSettings settings(filename, QSettings::IniFormat);
        settings.clear();
        templateManager()->commit(settings);
        settings.sync();
        if(settings.status() != QSettings::NoError)
            throw Exception(tr("I/O error while writing template file."));
    });
}

/******************************************************************************
* Is invoked when the user presses the "Import templates" button.
******************************************************************************/
void TemplatesPageBase::onImportTemplates()
{
    mainWindow().handleExceptions([&] {
        TaskManager::setNativeDialogActive(true);
        QString filename = QFileDialog::getOpenFileName(settingsDialog(),
            tr("Import %1 templates").arg(objectTypeNameLC()), QString(), templateFileFilter());
        TaskManager::setNativeDialogActive(false);
        if(filename.isEmpty())
            return;

        QSettings settings(filename, QSettings::IniFormat);
        if(settings.status() != QSettings::NoError)
            throw Exception(tr("I/O error while reading template file."));
        if(templateManager()->load(settings) == 0)
            throw Exception(tr("The selected file does not contain any %1 templates.").arg(objectTypeNameLC()));

        _dirtyFlag = true;
    });
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void TemplatesPageBase::saveValues(QTabWidget* tabWidget)
{
    if(_dirtyFlag) {
        templateManager()->commit();
        _dirtyFlag = false;
    }
}

/******************************************************************************
* Lets the settings page restore the original values of changed settings when
* the user presses the Cancel button.
******************************************************************************/
void TemplatesPageBase::restoreValues(QTabWidget* tabWidget)
{
    mainWindow().handleExceptions([&] {
        if(_dirtyFlag) {
            templateManager()->restore();
            _dirtyFlag = false;
        }
    });
}

}   // End of namespace
