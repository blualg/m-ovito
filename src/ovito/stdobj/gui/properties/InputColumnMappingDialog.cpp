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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "InputColumnMappingDialog.h"

namespace Ovito {

enum {
    FILE_COLUMN_COLUMN = 0,
    PROPERTY_COLUMN,
    VECTOR_COMPNT_COLUMN
};

/******************************************************************************
* Constructor.
******************************************************************************/
InputColumnMappingDialog::InputColumnMappingDialog(MainWindowUI& ui, const InputColumnMapping& mapping, QWidget* parent, const QString& fileName) :
    QDialog(parent),
    UserInterfaceComponent<MainWindowUI>(ui),
    _containerClass(mapping.containerClass())
{
    OVITO_ASSERT(_containerClass);
    OVITO_CHECK_POINTER(parent);
    setWindowTitle(tr("File column mapping"));

    _vectorCmpntSignalMapper = new QSignalMapper(this);
    connect(_vectorCmpntSignalMapper, &QSignalMapper::mappedInt, this, &InputColumnMappingDialog::updateVectorComponentList);

    // Create the table sub-widget.
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(2);

    QLabel* captionLabel = new QLabel(
        !fileName.isEmpty() ?
            tr("Please specify how to map the data columns of the input file '%1' to properties in OVITO.").arg(fileName) :
            tr("Please specify how to map the file columns to properties in OVITO."));
    captionLabel->setWordWrap(true);
    layout->addWidget(captionLabel);
    layout->addSpacing(10);

    QGridLayout* tableWidgetLayout = new QGridLayout();
    _tableWidget = new QTableWidget(this);
    tableWidgetLayout->addWidget(_tableWidget, 0, 0);
    tableWidgetLayout->setRowMinimumHeight(0, 250);
    tableWidgetLayout->setRowStretch(0, 1);
    tableWidgetLayout->setColumnMinimumWidth(0, 450);
    tableWidgetLayout->setColumnStretch(0, 1);
    layout->addLayout(tableWidgetLayout, 4);

    _tableWidget->setColumnCount(3);
    QStringList horizontalHeaders;
    horizontalHeaders << tr("File column");
    horizontalHeaders << tr("Property");
    horizontalHeaders << tr("Component");
    _tableWidget->setHorizontalHeaderLabels(horizontalHeaders);
    _tableWidget->setEditTriggers(QAbstractItemView::AllEditTriggers);
    _tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    _tableWidget->horizontalHeader()->setSectionResizeMode(FILE_COLUMN_COLUMN, QHeaderView::ResizeToContents);
    _tableWidget->horizontalHeader()->setSectionResizeMode(PROPERTY_COLUMN, QHeaderView::Stretch);
    _tableWidget->horizontalHeader()->setSectionResizeMode(VECTOR_COMPNT_COLUMN, QHeaderView::ResizeToContents);

    _tableWidget->verticalHeader()->setVisible(false);
    _tableWidget->setShowGrid(false);

    connect(_tableWidget, &QTableWidget::clicked, this, &InputColumnMappingDialog::fileColumnClicked);
    _toggleSelectedAction = new QAction(tr("Toggle selected"), this);
    _toggleSelectedAction->setEnabled(false);
    connect(_toggleSelectedAction, &QAction::triggered, this, &InputColumnMappingDialog::toggleSelected);
    connect(_tableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        _toggleSelectedAction->setEnabled(_tableWidget->selectionModel()->hasSelection());
    });
    // Show action in a context menu:
    _tableWidget->addAction(_toggleSelectedAction);
    _tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
    _tableWidget->horizontalHeader()->addAction(_toggleSelectedAction);
    _tableWidget->horizontalHeader()->setContextMenuPolicy(Qt::ActionsContextMenu);

    layout->addSpacing(6);
    layout->addWidget(_fileExcerptLabel = new QLabel(tr("File excerpt:")));
    _fileExcerptLabel->setVisible(false);
    _fileExcerptField = new QTextEdit();
    _fileExcerptField->setLineWrapMode(QTextEdit::NoWrap);
    _fileExcerptField->setAcceptRichText(false);
    _fileExcerptField->setReadOnly(true);
    _fileExcerptField->setVisible(false);
    _fileExcerptField->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(_fileExcerptField, 1);
    layout->addSpacing(10);

    // Dialog buttons:
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
    buttonBox->button(QDialogButtonBox::Ok)->setFocus();
    connect(buttonBox->addButton(tr("Load preset..."), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, &InputColumnMappingDialog::onLoadPreset);
    connect(buttonBox->addButton(tr("Save preset..."), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, &InputColumnMappingDialog::onSavePreset);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InputColumnMappingDialog::onOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InputColumnMappingDialog::reject);
    layout->addWidget(buttonBox);

    setMapping(mapping);
}

/******************************************************************************
* This is called when the user has pressed the OK button.
******************************************************************************/
void InputColumnMappingDialog::onOk()
{
    setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.
    handleExceptions([&]() {
        // First, validate the current mapping.
        mapping().validate();

        // Close dialog box.
        accept();
    });
}

/******************************************************************************
 * Fills the editor with the given mapping.
 *****************************************************************************/
void InputColumnMappingDialog::setMapping(const InputColumnMapping& mapping)
{
    OVITO_ASSERT(_containerClass);
    OVITO_ASSERT(mapping.containerClass() == _containerClass);

    _tableWidget->clearContents();
    _propertyBoxes.clear();
    _vectorComponentBoxes.clear();
    _propertyDataTypes.clear();

    _tableWidget->setRowCount(mapping.size());
    for(int i = 0; i < (int)mapping.size(); i++) {
        QTableWidgetItem* fileColumnItem = new QTableWidgetItem();
        if(mapping[i].columnName.isEmpty())
            fileColumnItem->setText(tr("Column %1").arg(i+1));
        else
            fileColumnItem->setText(mapping[i].columnName);
        fileColumnItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        fileColumnItem->setCheckState(mapping[i].isMapped() ? Qt::Checked : Qt::Unchecked);
        _tableWidget->setItem(i, FILE_COLUMN_COLUMN, fileColumnItem);

        QComboBox* nameItem = new QComboBox();
        nameItem->setEditable(true);
        nameItem->setDuplicatesEnabled(false);
        for(const auto& item : _containerClass->standardPropertyIds()) {
            nameItem->addItem(item.first, item.second);
        }
        nameItem->setCurrentText(mapping[i].property.name().toString());
        nameItem->setEnabled(mapping[i].isMapped());
        _tableWidget->setCellWidget(i, PROPERTY_COLUMN, nameItem);
        _propertyBoxes.push_back(nameItem);

        // Make table items not selectable.
        QTableWidgetItem* nameColumnItem = new QTableWidgetItem();
        nameColumnItem->setFlags(Qt::ItemIsEnabled);
        _tableWidget->setItem(i, PROPERTY_COLUMN, nameColumnItem);

        QComboBox* vectorComponentItem = new QComboBox();
        _tableWidget->setCellWidget(i, VECTOR_COMPNT_COLUMN, vectorComponentItem);
        _vectorComponentBoxes.push_back(vectorComponentItem);
        updateVectorComponentList(i);
        if(vectorComponentItem->count() != 0)
            vectorComponentItem->setCurrentIndex(std::max(0, mapping[i].property.componentIndex(_containerClass)));

        // Make table items not selectable.
        QTableWidgetItem* cmpntColumnItem = new QTableWidgetItem();
        cmpntColumnItem->setFlags(Qt::ItemIsEnabled);
        _tableWidget->setItem(i, VECTOR_COMPNT_COLUMN, cmpntColumnItem);

        _vectorCmpntSignalMapper->setMapping(nameItem, i);
        connect(nameItem, &QComboBox::currentTextChanged, _vectorCmpntSignalMapper, qOverload<>(&QSignalMapper::map));

        _propertyDataTypes.push_back(mapping[i].dataType != QMetaType::Void ? mapping[i].dataType : Property::FloatDefault);
    }

    _tableWidget->resizeRowsToContents();

    if(!mapping.fileExcerpt().isEmpty()) {
        _fileExcerptField->setPlainText(mapping.fileExcerpt());
        _fileExcerptField->setVisible(true);
        _fileExcerptLabel->setVisible(true);
    }
    else {
        _fileExcerptField->setVisible(false);
        _fileExcerptLabel->setVisible(false);
    }
}

/******************************************************************************
 * Called when the user clicks on an item in the table column for file columns.
 *****************************************************************************/
void InputColumnMappingDialog::fileColumnClicked(const QModelIndex& index)
{
    if(index.column() == FILE_COLUMN_COLUMN) {
        if(QTableWidgetItem* item = _tableWidget->item(index.row(), index.column())) {
            QList<QTableWidgetItem*> selectedItems = _tableWidget->selectedItems();
            if(selectedItems.size() == 1 && selectedItems[0] == item) { // Only toggle the check state if the user is not trying to select multiple items.
                item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
                _propertyBoxes[index.row()]->setEnabled(item->checkState() == Qt::Checked);
                updateVectorComponentList(index.row());
            }
        }
    }
}

/******************************************************************************
 * Called when the user has triggered the action to toggle the selected file columns.
 *****************************************************************************/
void InputColumnMappingDialog::toggleSelected()
{
    for(QTableWidgetItem* item : _tableWidget->selectedItems()) {
        if(item->column() == FILE_COLUMN_COLUMN) {
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
            _propertyBoxes[item->row()]->setEnabled(item->checkState() == Qt::Checked);
            updateVectorComponentList(item->row());
        }
    }
}

/******************************************************************************
 * Updates the list of vector components for the given file column.
 *****************************************************************************/
void InputColumnMappingDialog::updateVectorComponentList(int columnIndex)
{
    OVITO_ASSERT(columnIndex < _vectorComponentBoxes.size());
    QComboBox* vecBox = _vectorComponentBoxes[columnIndex];

    QString propertyName = _propertyBoxes[columnIndex]->currentText();
    int standardProperty = _containerClass->standardPropertyTypeId(propertyName);
    if(!propertyName.isEmpty() && standardProperty != Property::GenericUserProperty) {
        int oldIndex = vecBox->currentIndex();
        _vectorComponentBoxes[columnIndex]->clear();
        for(const QString& name : _containerClass->standardPropertyComponentNames(standardProperty))
            vecBox->addItem(name);
        vecBox->setEnabled(_tableWidget->item(columnIndex, FILE_COLUMN_COLUMN)->checkState() == Qt::Checked && vecBox->count() != 0);
        if(oldIndex >= 0)
            vecBox->setCurrentIndex(std::min(oldIndex, vecBox->count()-1));
    }
    else {
        vecBox->clear();
        vecBox->setEnabled(false);
    }
}

/******************************************************************************
 * Returns the current contents of the editor.
 *****************************************************************************/
InputColumnMapping InputColumnMappingDialog::mapping() const
{
    InputColumnMapping mapping(_containerClass);
    mapping.resize(_tableWidget->rowCount());
    for(int index = 0; index < (int)mapping.size(); index++) {
        QTableWidgetItem* fileColumnItem = _tableWidget->item(index, FILE_COLUMN_COLUMN);
        mapping[index].columnName = fileColumnItem->text();
        if(fileColumnItem->checkState() == Qt::Checked) {
            QString propertyName = _propertyBoxes[index]->currentText().trimmed();
            int typeId = _containerClass->standardPropertyTypeId(propertyName);
            if(typeId != Property::GenericUserProperty) {
                int vectorCompnt = std::max(0, _vectorComponentBoxes[index]->currentIndex());
                mapping[index].mapToStandardProperty(_containerClass, typeId, vectorCompnt);
            }
            else if(!propertyName.isEmpty()) {
                mapping[index].mapToUserProperty(propertyName, _propertyDataTypes[index]);
            }
        }
    }
    if(!_fileExcerptField->isHidden()) {
        mapping.setFileExcerpt(_fileExcerptField->toPlainText());
    }
    return mapping;
}

/******************************************************************************
 * Saves the current mapping as a preset.
 *****************************************************************************/
void InputColumnMappingDialog::onSavePreset()
{
    handleExceptions([&]() {
        // Get current mapping.
        InputColumnMapping m = mapping();
        m.validate();

        // Load existing mappings.
        QSettings settings;
        settings.beginGroup("inputcolumnmapping");
        if(_containerClass->name() != QStringLiteral("Particles"))
            settings.beginGroup(_containerClass->name());
        int size = settings.beginReadArray("presets");
        QStringList presetNames;
        QList<QByteArray> presetData;
        for(int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);
            presetNames.push_back(settings.value("name").toString());
            presetData.push_back(settings.value("data").toByteArray());
        }
        settings.endArray();

        // Let the user give a name.
        bool ok;
        QString name = QInputDialog::getItem(this, tr("Save Column Mapping"),
            tr("Please enter a name for the column mapping preset:"), presetNames, -1, true, &ok);
        if(name.isEmpty() || !ok) return;

        // Serialize mapping and add it to the list.
        int index = presetNames.indexOf(name);
        if(index >= 0) {
            // Overwrite existing preset with the same name.
            presetData[index] = m.toByteArray();
        }
        else {
            // Add a new preset. Sort alphabetically.
            index = std::lower_bound(presetNames.begin(), presetNames.end(), name) - presetNames.begin();
            presetNames.insert(index, name);
            presetData.insert(index, m.toByteArray());
        }

        // Write mappings to settings store.
        settings.beginWriteArray("presets");
        for(int i = 0; i < presetNames.size(); ++i) {
            settings.setArrayIndex(i);
            settings.setValue("name", presetNames[i]);
            settings.setValue("data", presetData[i]);
        }
        settings.endArray();
    });
}

/******************************************************************************
 * Loads a preset mapping.
 *****************************************************************************/
void InputColumnMappingDialog::onLoadPreset()
{
    handleExceptions([&]() {
        // Load list of presets.
        QSettings settings;
        settings.beginGroup("inputcolumnmapping");
        if(_containerClass->name() != QStringLiteral("Particles"))
            settings.beginGroup(_containerClass->name());
        int size = settings.beginReadArray("presets");
        QStringList presetNames;
        QList<QByteArray> presetData;
        for(int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);
            presetNames.push_back(settings.value("name").toString());
            presetData.push_back(settings.value("data").toByteArray());
        }
        settings.endArray();

        if(size == 0)
            throw Exception(tr("You have not saved any presets so far which can be loaded."));

        // Let the user pick a preset.
        bool ok;
        QString name = QInputDialog::getItem(this, tr("Load Column Mapping"),
            tr("Select the column mapping to load:"), presetNames, 0, false, &ok);
        if(name.isEmpty() || !ok) return;

        // Load preset.
        InputColumnMapping mapping(_containerClass);
        mapping.fromByteArray(presetData[presetNames.indexOf(name)]);
        OVITO_ASSERT(mapping.containerClass() == _containerClass);

        for(int index = 0; index < (int)mapping.size() && index < _tableWidget->rowCount(); index++) {
            _tableWidget->item(index, FILE_COLUMN_COLUMN)->setCheckState(mapping[index].isMapped() ? Qt::Checked : Qt::Unchecked);
            _propertyBoxes[index]->setCurrentText(mapping[index].property.name().toString());
            _propertyBoxes[index]->setEnabled(mapping[index].isMapped());
            updateVectorComponentList(index);
            if(_vectorComponentBoxes[index]->count() != 0)
                _vectorComponentBoxes[index]->setCurrentIndex(std::max(0, mapping[index].property.componentIndex(_containerClass)));
        }
        for(int index = mapping.size(); index < _tableWidget->rowCount(); index++) {
            _tableWidget->item(index, FILE_COLUMN_COLUMN)->setCheckState(Qt::Unchecked);
            _propertyBoxes[index]->setCurrentText(QString());
            _propertyBoxes[index]->setEnabled(false);
            updateVectorComponentList(index);
        }
    });
}

}   // End of namespace
