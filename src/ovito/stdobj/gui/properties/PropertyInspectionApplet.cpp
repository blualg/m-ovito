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
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyExpressionEvaluator.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include <ovito/gui/desktop/widgets/general/CopyableTableView.h>
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectorPanel.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/HistoryFileDialog.h>
#include <ovito/gui/desktop/dialogs/FileExporterSettingsDialog.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include "PropertyInspectionApplet.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PropertyInspectionApplet);

/******************************************************************************
* Lets the applet create the UI widgets that are to be placed into the data
* inspector panel.
******************************************************************************/
void PropertyInspectionApplet::createBaseWidgets()
{
    _filterExpressionEdit = new AutocompleteLineEdit();
    _filterExpressionEdit->setPlaceholderText(tr("Filter..."));
    _filterExpressionEdit->setClearButtonEnabled(true);
    _cleanupHandler.add(_filterExpressionEdit);
    connect(_filterExpressionEdit, &AutocompleteLineEdit::editingFinished, this, &PropertyInspectionApplet::onFilterExpressionEntered);
    connect(_filterExpressionEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        if(text.isEmpty())
            onFilterExpressionEntered();
    });

    _filterStatusAction = _filterExpressionEdit->addAction(QIcon(":/guibase/mainwin/status/status_error.png"), QLineEdit::LeadingPosition);
    _filterStatusAction->setVisible(false);

    _tableView = new CopyableTableView();
    _tableModel = new PropertyTableModel(this, _tableView);
    _filterModel = new PropertyFilterModel(this, _tableView);
    _filterModel->setSourceModel(_tableModel);
    _tableView->setModel(_filterModel);
    _tableView->horizontalHeader()->setResizeContentsPrecision(64); // Limit the number of rows taken into account when auto-resizing columns.
    _tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _cleanupHandler.add(_tableView);

    // Custom QLineEdit subclass that automatically adjusts its width to fit the text.
    class AutoSizeLineEdit : public QLineEdit
    {
    public:
        AutoSizeLineEdit(QWidget* parent = nullptr) : QLineEdit(parent) {
            // Update size hint whenever the text changes.
            connect(this, &QLineEdit::textChanged, this, [this]() { updateGeometry(); });
        }
    protected:
        QSize sizeHint() const override {
            // Note: This is based on QLineEdit::sizeHint() implementation.
            ensurePolished();
            QFontMetrics fm(font());
            const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
            const QMargins tm = textMargins();
            int h = qMax(fm.height(), qMax(14, iconSize - 2)) + 2 * 1
                    + tm.top() + tm.bottom();
            int w = fm.horizontalAdvance(text()) + 2 * 2
                    + tm.left() + tm.right() + 8;
            QStyleOptionFrame opt;
            initStyleOption(&opt);
            return style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h), this);
        }
    };

    _countDisplayLabel = new AutoSizeLineEdit();
    _countDisplayLabel->setReadOnly(true);
    _countDisplayLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _countDisplayLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _cleanupHandler.add(_countDisplayLabel);

    // Clear filter expression whenever a different scene pipeline or data object is selected by the user.
    connect(this, &DataInspectionApplet::currentObjectPathChanged, _filterExpressionEdit, &QLineEdit::clear);
    connect(this, &DataInspectionApplet::currentObjectPathChanged, _filterExpressionEdit, &AutocompleteLineEdit::editingFinished);
    connect(inspectorPanel(), &DataInspectorPanel::selectedPipelineChanged, _filterExpressionEdit, &QLineEdit::clear);
    connect(inspectorPanel(), &DataInspectorPanel::selectedPipelineChanged, _filterExpressionEdit, &AutocompleteLineEdit::editingFinished);
    // Update tabular display whenever the user selects a different property container in the list.
    connect(this, &DataInspectionApplet::currentObjectChanged, this, &PropertyInspectionApplet::onCurrentContainerChanged);
    // Update the displayed element count when the filter expression changes.
    connect(this, &PropertyInspectionApplet::filterChanged, this, &PropertyInspectionApplet::updateCountDisplay);
}

/******************************************************************************
* Is called when the user selects a different container object from the list.
******************************************************************************/
void PropertyInspectionApplet::onCurrentContainerChanged()
{
    handleExceptions([&]() {
        const PropertyContainer* container = selectedContainerObject();
        int oldColumnCount = _tableModel->setContents(container);
        _filterModel->setContentsBegin();
        _filterModel->setContentsEnd();
        updateCountDisplay();

        // Adjust the widths of the newly added columns.
        for(int i = oldColumnCount; i < _tableModel->columnCount(); i++) {
            _tableView->resizeColumnToContents(i);
        }

        // Update the list of variables that can be referenced in the filter expression.
        if(selectedContainerObject() && currentState()) {
            try {
                auto evaluator = createExpressionEvaluator();
                evaluator->initialize(QStringList(), currentState(), selectedDataObjectPath());
                _filterExpressionEdit->setWordList(evaluator->inputVariableNames());
            }
            catch(const Exception&) {}
        }
        else {
            _filterExpressionEdit->setWordList({});
        }
    });
}

/******************************************************************************
* Updates the element count display label.
******************************************************************************/
void PropertyInspectionApplet::updateCountDisplay()
{
    const PropertyContainer* container = selectedContainerObject();
    if(!container || _countDisplayLabel->parentWidget() == nullptr) {
        _countDisplayLabel->setText({});
        _countDisplayLabel->setVisible(false);
        return;
    }

    if(_filterExpressionEdit->text().isEmpty()) {
        _countDisplayLabel->setText(QStringLiteral("%1 %2").arg(container->elementCount()).arg(elementDescriptionName()));
    }
    else {
        _countDisplayLabel->setText(QStringLiteral("%1 of %2 %3").arg(visibleElementCount()).arg(container->elementCount()).arg(elementDescriptionName()));
    }
    _countDisplayLabel->setVisible(true);
}

/******************************************************************************
* Selects a specific data object in this applet.
******************************************************************************/
bool PropertyInspectionApplet::selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint)
{
    // Check the property container list in case the requested data object is a PropertyContainer.
    if(DataInspectionApplet::selectDataObject(createdByNode, objectIdentifierHint, modeHint))
        return true;

    // Check the property columns in case the requested data object is a property object.
    const auto& properties = _tableModel->properties();
    if(auto iter = std::ranges::find_if(properties, [&](const Property* property) {
        return property->createdByNode().lock().get() == createdByNode &&
            (objectIdentifierHint.isEmpty() || property->identifier().startsWith(objectIdentifierHint));
    }); iter != properties.end()) {
        _tableView->selectColumn(iter - properties.begin());
        return true;
    }
    return false;
}

/******************************************************************************
 * Exports the current data table to a text file.
 ******************************************************************************/
void PropertyInspectionApplet::exportDataToFile(const DataObjectReference& dataObjectRef, OORef<FileExporter>&& exporter,
                                                const QString& filterString) const
{
    OVITO_ASSERT_MSG(this_task::get(), "PropertyInspectionApplet::exportDataToFile",
                     "This method must be called from a handleExceptions() context.");

    // Let the user select a destination file.
    HistoryFileDialog dialog("export", ui().mainWindow(), tr("Export Table"));
    dialog.setNameFilter(filterString);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);

    // Go to the last directory used.
    QSettings settings;
    settings.beginGroup("file/export");
    QString lastExportDirectory = settings.value("last_export_dir").toString();
    if(!lastExportDirectory.isEmpty()) dialog.setDirectory(lastExportDirectory);

    if(!dialog.exec() || dialog.selectedFiles().empty()) return;
    QString exportFile = dialog.selectedFiles().front();

    // Remember directory for the next time...
    settings.setValue("last_export_dir", dialog.directory().absolutePath());

    // Export to selected file.
    // Pass output filename to exporter.
    exporter->setOutputFilename(exportFile);

    // Set pipeline to be exported.
    exporter->setSceneToExport(currentSceneNode()->scene());
    exporter->setPipelineToExport(currentPipeline());

    // If the exporter supports it, automatically choose the data object(s) to be exported.
    exporter->selectDefaultExportableData(dataset(), currentSceneNode()->scene());

    // Set data table to be exported.
    exporter->setDataObjectToExport(dataObjectRef);

    // Let the user adjust the export settings.
    FileExporterSettingsDialog settingsDialog(ui(), *exporter->sceneToExport(), exporter, ui().mainWindow());
    if(settingsDialog.exec() != QDialog::Accepted) return;

    // Let the exporter do its job.
    Future<void> future = exporter->performExport();

    // Show a progress dialog while the operation is in progress. The dialog will self-destruct when the operation is done.
    ProgressDialog::showForFuture(std::move(future), ui(), tr("File export"));
}

/******************************************************************************
* Replaces the contents of this data model.
******************************************************************************/
int PropertyInspectionApplet::PropertyTableModel::setContents(const PropertyContainer* container)
{
    OVITO_ASSERT(this_task::get());

    // Generate the new list of properties.
    std::vector<ConstPropertyPtr> newProperties;
    if(container) {
        // Let the sub-class insert an extra ad-hoc column.
        // This option is used for DataTables, for example, which compute the x-axis dynamically.
        if(ConstPropertyPtr headerColumn = _applet->createHeaderColumnProperty(container))
            newProperties.push_back(std::move(headerColumn));
        // Insert regular properties of the container.
        newProperties.insert(newProperties.end(), container->properties().begin(), container->properties().end());
    }
    int oldRowCount = rowCount();
    int newRowCount = container ? container->elementCount() : 0;
    if(!newProperties.empty())
        newRowCount = (int)std::min(newProperties.front()->size(), (size_t)std::numeric_limits<int>::max());

    // Try to preserve the existing columns of the model as far as possible.
    auto iter_pair = std::mismatch(_properties.begin(), _properties.end(), newProperties.begin(), newProperties.end(),
        [](const Property* prop1, const Property* prop2) {
            return prop1->typeId() == prop2->typeId() && prop1->name() == prop2->name() && prop1->componentNames() == prop2->componentNames();
        });

    // Remove columns from the model that are no longer present in the new list.
    if(iter_pair.first != _properties.end()) {
        beginRemoveColumns(QModelIndex(), iter_pair.first - _properties.begin(), _properties.size()-1);
        _properties.erase(iter_pair.first, _properties.end());
        endRemoveColumns();
    }

    OVITO_ASSERT(_properties.size() <= newProperties.size());
    int oldColumnCount = _properties.size();
    if(!_properties.empty()) {
        if(oldRowCount > newRowCount) {
            beginRemoveRows(QModelIndex(), newRowCount, oldRowCount-1);
            std::move(newProperties.begin(), newProperties.begin() + _properties.size(), _properties.begin());
            _container = container;
            endRemoveRows();
        }
        else if(newRowCount > oldRowCount) {
            beginInsertRows(QModelIndex(), oldRowCount, newRowCount-1);
            std::move(newProperties.begin(), newProperties.begin() + _properties.size(), _properties.begin());
            _container = container;
            endInsertRows();
        }
        else {
            std::move(newProperties.begin(), newProperties.begin() + _properties.size(), _properties.begin());
            _container = container;
        }
        int changedRows = std::min(oldRowCount, newRowCount);
        if(changedRows) {
            dataChanged(index(0, 0), index(changedRows-1, _properties.size()-1));
        }

        // Insert new columns that are present in the new list but not in the existing model.
        if(newProperties.size() > _properties.size()) {
            beginInsertColumns(QModelIndex(), _properties.size(), newProperties.size() - 1);
            _properties.insert(_properties.end(), std::make_move_iterator(newProperties.begin() + _properties.size()), std::make_move_iterator(newProperties.end()));
            endInsertColumns();
        }
    }
    else {
        beginResetModel();
        _properties = std::move(newProperties);
        _container = container;
        endResetModel();
    }

    OVITO_ASSERT(rowCount() == newRowCount);
    return oldColumnCount;
}

/******************************************************************************
* Replaces the contents of this data model.
******************************************************************************/
void PropertyInspectionApplet::PropertyFilterModel::setContentsBegin()
{
    if(_filterExpression.isEmpty() == false)
        beginResetModel();
    setupEvaluator();
}

/******************************************************************************
* Initializes the expression evaluator.
******************************************************************************/
void PropertyInspectionApplet::PropertyFilterModel::setupEvaluator()
{
    _evaluatorWorker.reset();
    _evaluator.reset();
    if(_filterExpression.isEmpty() == false && _applet->currentState()) {
        if(const PropertyContainer* container = _applet->selectedContainerObject()) {
            try {
                // Check if expression contains a variable assignment ('=' operator).
                // This should be considered an error, because the user is probably referring to the comparison operator '=='.
                if(_filterExpression.contains(QRegularExpression(QStringLiteral("[^=!><]=(?!=)"))))
                    throw Exception(tr("The entered expression contains the assignment operator '='. Please use the correct comparison operator '==' instead."));

                int animationFrame = _applet->currentAnimationTime().frame();
                _evaluator = _applet->createExpressionEvaluator();
                _evaluator->initialize(QStringList(_filterExpression), _applet->currentState(), _applet->selectedDataObjectPath(), animationFrame);
                _evaluatorWorker = std::make_unique<PropertyExpressionEvaluator::Worker>(*_evaluator);
            }
            catch(const Exception& ex) {
                _applet->onFilterStatusChanged(ex.messages().join("\n"));
                _evaluatorWorker.reset();
                _evaluator.reset();
                return;
            }
        }
    }
    _applet->onFilterStatusChanged(QString());
}

/******************************************************************************
* Returns the data stored under the given 'role' for the item referred to by
* the 'index'.
******************************************************************************/
QVariant PropertyInspectionApplet::PropertyTableModel::data(const QModelIndex& index, int role) const
{
    if(role == Qt::DisplayRole) {
        OVITO_ASSERT(index.column() >= 0 && index.column() < _properties.size());
        size_t elementIndex = index.row();
        const auto& property = _properties[index.column()];
        if(elementIndex < property->size()) {
            QString str;
            for(size_t component = 0; component < property->componentCount(); component++) {
                if(component != 0) str += QStringLiteral(" ");
                if(property->dataType() == Property::Int32) {
                    BufferReadAccess<int32_t*> data(property);
                    str += QString::number(data.get(elementIndex, component));
                    if(!property->elementTypes().empty()) {
                        if(const ElementType* ptype = property->elementType(data.get(elementIndex, component))) {
                            if(!ptype->name().isEmpty())
                                str += QStringLiteral(" (%1)").arg(ptype->name());
                        }
                    }
                }
                else {
                    property->forAnyType([&](auto type) {
                        BufferReadAccess<decltype(type)*> accessor(property);
                        str += QString::number(accessor.get(elementIndex, component));
                    });
                }

            }
            return str;
        }
    }
    else if(role == Qt::DecorationRole) {
        OVITO_ASSERT(index.column() >= 0 && index.column() < _properties.size());
        const auto& property = _properties[index.column()];
        size_t elementIndex = index.row();
        if(elementIndex < property->size()) {
            if(_applet->isColorProperty(property)) {
                if(property->dataType() == DataBuffer::Float32)
                    return static_cast<QColor>(BufferReadAccess<ColorT<float>>(property)[elementIndex]);
                else if(property->dataType() == DataBuffer::Float64)
                    return static_cast<QColor>(BufferReadAccess<ColorT<double>>(property)[elementIndex]);
            }
            else if(property->dataType() == Property::Int32 && property->componentCount() == 1 && property->elementTypes().empty() == false) {
                BufferReadAccess<int32_t> data(property);
                if(const ElementType* ptype = property->elementType(data[elementIndex]))
                    return static_cast<QColor>(ptype->color());
            }
        }
    }
    return {};
}

/******************************************************************************
* Returns the data for the given role and section in the header with the specified orientation.
******************************************************************************/
QVariant PropertyInspectionApplet::PropertyTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        OVITO_ASSERT(section >= 0 && section < _properties.size());
        const Property* property = _properties[section];
        QString name = property->name();
        if(property->componentNames().empty() == false) {
            name += QStringLiteral(" [");
            for(qsizetype i = 0; i < property->componentNames().size(); i++) {
                if(i != 0) name += QStringLiteral(" ");
                name += property->componentNames()[i];
            }
            name += QStringLiteral("]");
        }
        return name;
    }
    else if(orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return _applet->headerColumnText(section);
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

/******************************************************************************
* Is called when the uer has changed the filter expression.
******************************************************************************/
void PropertyInspectionApplet::onFilterExpressionEntered()
{
    _filterModel->setFilterExpression(_filterExpressionEdit->text());
    Q_EMIT filterChanged();
}

/******************************************************************************
* Sets the filter expression.
******************************************************************************/
void PropertyInspectionApplet::setFilterExpression(const QString& expression)
{
    _filterExpressionEdit->setText(expression);
    _filterModel->setFilterExpression(expression);
    Q_EMIT filterChanged();
}

/******************************************************************************
* Is called when an error during filter evaluation occurred.
******************************************************************************/
void PropertyInspectionApplet::onFilterStatusChanged(const QString& msgText)
{
    if(msgText.isEmpty() == false) {
        _filterStatusString = msgText;
        _filterStatusAction->setVisible(true);
        _filterStatusAction->setToolTip(_filterStatusString);
        _filterExpressionEdit->update();
        // Show tooltip only when the filter input field has the focus.
        // Otherwise it would be distracting to the user.
        if(_filterExpressionEdit->hasFocus()) {
            // Note: Deferring the showText() call to a slightly later time, because otherwise the tooltip might immediately disappear again.
            // This seems to be an issue on macOS, where the tooltip is hidden by KeyPress/KeyRelease events.
#ifdef Q_OS_MAC
            QTimer::singleShot(150, this, [this]() {
#endif
            if(!_filterStatusString.isEmpty()) {
                QToolTip::showText(_filterExpressionEdit->mapToGlobal(_filterExpressionEdit->rect().bottomLeft()), _filterStatusString,
                    _filterExpressionEdit, QRect());
            }
#ifdef Q_OS_MAC
            });
#endif
        }
    }
    else if(!_filterStatusString.isEmpty()) {
        QToolTip::hideText();
        _filterStatusString.clear();
        _filterStatusAction->setVisible(false);
        _filterStatusAction->setToolTip({});
        _filterExpressionEdit->update();
    }
}

/******************************************************************************
* Performs the filtering of data rows.
******************************************************************************/
bool PropertyInspectionApplet::PropertyFilterModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if(_evaluatorWorker && (size_t)source_row < _evaluator->elementCount()) {
        try {
            return _evaluatorWorker->evaluate(source_row, 0);
        }
        catch(const Exception& ex) {
            _applet->onFilterStatusChanged(ex.messages().join("\n"));
            _evaluatorWorker.reset();
            _evaluator.reset();
        }
    }
    return true;
}

}   // End of namespace
