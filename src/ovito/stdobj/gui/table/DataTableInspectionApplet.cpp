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
#include <ovito/stdobj/gui/io/DataTablePlotExporter.h>
#include <ovito/stdobj/io/DataTableExporter.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/FileExporterSettingsDialog.h>
#include <ovito/core/app/Application.h>
#include "DataTableInspectionApplet.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTableInspectionApplet);
OVITO_CLASSINFO(DataTableInspectionApplet, "DisplayName", "Data Tables");

/******************************************************************************
* Lets the applet create the UI widget that is to be placed into the data
* inspector panel.
******************************************************************************/
QWidget* DataTableInspectionApplet::createWidget()
{
    createBaseWidgets();

    QSplitter* splitter = new QSplitter();
    splitter->addWidget(objectSelectionWidget());

    QWidget* rightContainer = new QWidget();
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    QHBoxLayout* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(0);

    QToolBar* toolbar = new QToolBar();
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconSize(QSize(22,22));

    QActionGroup* plotTypeActionGroup = new QActionGroup(this);
    _switchToPlotAction = plotTypeActionGroup->addAction(QIcon::fromTheme("inspector_view_chart"), tr("Chart view"));
    _switchToTableAction = plotTypeActionGroup->addAction(QIcon::fromTheme("inspector_view_table"), tr("Table view"));
    toolbar->addAction(_switchToPlotAction);
    toolbar->addAction(_switchToTableAction);
    _switchToPlotAction->setCheckable(true);
    _switchToTableAction->setCheckable(true);
    _switchToPlotAction->setChecked(true);
    toolbar->addSeparator();

    _exportTableToFileAction = new QAction(QIcon::fromTheme("file_save_as"), tr("Export data plot"), this);
    connect(_exportTableToFileAction, &QAction::triggered, this, [this]() {
        const DataTable* table = plotWidget()->table();
        if(!table) {
            return;
        }
        // Generate filter string for file dialog.
        const QString filterString = (_stackedWidget->currentIndex() == 0)
                                         ? QStringLiteral("%1 (%2)").arg(DataTablePlotExporter::OOClass().fileFilterDescription(),
                                                                         DataTablePlotExporter::OOClass().fileFilter())
                                         : QStringLiteral("%1 (%2)").arg(DataTableExporter::OOClass().fileFilterDescription(),
                                                                         DataTableExporter::OOClass().fileFilter());
        // Create exporter service.
        handleExceptions([&] {
            if(_stackedWidget->currentIndex() == 0) {
                exportDataToFile(DataObjectReference(&DataTable::OOClass(), table->identifier(), table->title()),
                                 OORef<DataTablePlotExporter>::create(), filterString);
            }
            else if(_stackedWidget->currentIndex() == 1) {
                exportDataToFile(DataObjectReference(&DataTable::OOClass(), table->identifier(), table->title()),
                                 OORef<DataTableExporter>::create(), filterString);
            }
            else {
                OVITO_ASSERT_MSG(false, "DataTableInspectionApplet::_exportTableToFileAction", "Stacked widget index out of range.");
            }
        });
    });
    toolbar->addAction(_exportTableToFileAction);

    _stackedWidget = new QStackedWidget();
    rightLayout->addWidget(_stackedWidget, 1);
    rightLayout->addWidget(toolbar, 0);

    connect(_switchToPlotAction, &QAction::triggered, this, [this]() {
        _stackedWidget->setCurrentIndex(0);
        _exportTableToFileAction->setToolTip(tr("Export data plot"));
    });
    connect(_switchToTableAction, &QAction::triggered, this, [this]() {
        _stackedWidget->setCurrentIndex(1);
        _exportTableToFileAction->setToolTip(tr("Export data to text file"));
    });

    _plotWidget = new DataTablePlotWidget();
    _stackedWidget->addWidget(_plotWidget);

    QWidget* panel = new QWidget();
    QGridLayout* layout = new QGridLayout(panel);
    layout->setContentsMargins(0,0,0,0);
    layout->setHorizontalSpacing(0);
    layout->setVerticalSpacing(4);

    filterExpressionEdit()->setPlaceholderText(tr("Filter table rows..."));
    layout->addWidget(filterExpressionEdit(), 0, 0);
    layout->addWidget(countDisplayLabel(), 0, 1);
    layout->addWidget(tableView(), 1, 0, 1, 2);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    _stackedWidget->addWidget(panel);

    connect(this, &DataInspectionApplet::currentObjectChanged, this, &DataTableInspectionApplet::onCurrentContainerChanged);

    return splitter;
}

/******************************************************************************
* Creates an optional ad-hoc property that serves as header column for the table.
******************************************************************************/
ConstPropertyPtr DataTableInspectionApplet::createHeaderColumnProperty(const PropertyContainer* container)
{
    OVITO_ASSERT(this_task::get());
    const DataTable* table = static_object_cast<DataTable>(container);
    if(!table->x())
        return table->getXValues();
    return {};
}

/******************************************************************************
* Is called when the user selects a different container object from the list.
******************************************************************************/
void DataTableInspectionApplet::onCurrentContainerChanged(const DataObject* dataObject)
{
    handleExceptions([&]() {
        // Update the displayed plot.
        plotWidget()->setTable(static_object_cast<DataTable>(dataObject));

        // Switch to table view if plot mode is none
        if(const DataTable* table = static_object_cast<DataTable>(dataObject)) {
            if(table->plotMode() == DataTable::None) {
                _switchToTableAction->trigger();
            }
        }

        // Update actions.
        _exportTableToFileAction->setEnabled(plotWidget()->table() != nullptr);
    });
}

/******************************************************************************
* Selects a specific data object in this applet.
******************************************************************************/
bool DataTableInspectionApplet::selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint)
{
    // Let the base class switch to the right data table object.
    bool result = PropertyInspectionApplet::selectDataObject(createdByNode, objectIdentifierHint, modeHint);

    if(result) {
        // The mode hint is used to switch between plot and table view.
        int mode = modeHint.toInt();
        if(mode == 0) {
            _switchToPlotAction->trigger(); // Plot view
        }
        else {
            _switchToTableAction->trigger(); // Table view
        }
    }

    return result;
}

}   // End of namespace
