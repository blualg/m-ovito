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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include "DataTablePlotExporter.h"

#include <qwt/qwt_plot_renderer.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTablePlotExporter);
DEFINE_PROPERTY_FIELD(DataTablePlotExporter, plotWidth);
DEFINE_PROPERTY_FIELD(DataTablePlotExporter, plotHeight);
DEFINE_PROPERTY_FIELD(DataTablePlotExporter, plotDPI);
SET_PROPERTY_FIELD_LABEL(DataTablePlotExporter, plotWidth, "Width (mm)");
SET_PROPERTY_FIELD_LABEL(DataTablePlotExporter, plotHeight, "Height (mm)");
SET_PROPERTY_FIELD_LABEL(DataTablePlotExporter, plotDPI, "Resolution (DPI)");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DataTablePlotExporter, plotWidth, FloatParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DataTablePlotExporter, plotHeight, FloatParameterUnit, 1);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(DataTablePlotExporter, plotDPI, IntegerParameterUnit, 1);

/******************************************************************************
* Creates a worker performing the actual data export.
*****************************************************************************/
OORef<FileExportJob> DataTablePlotExporter::createExportJob(const QString& filePath, int numberOfFrames)
{
    class Job : public FileExportJob
    {
    public:

        /// Writes the exportable data of a single trajectory frame to the output file.
        virtual SCFuture<void> exportFrameData(any_moveonly&& frameData, int frameNumber, const QString& filePath, TaskProgress& progress) override {
            // The exportable frame data.
            const PipelineFlowState state = any_cast<PipelineFlowState>(std::move(frameData));

            // Look up the DataTable to be exported in the pipeline state.
            DataObjectReference objectRef(&DataTable::OOClass(), dataObjectToExport().dataPath());
            const DataTable* table = static_object_cast<DataTable>(state.getLeafObject(objectRef));
            if(!table) {
                throw Exception(tr("The pipeline output does not contain the data table to be exported (animation frame: %1; object key: %2). Available data tables: (%3)")
                    .arg(frameNumber).arg(objectRef.dataPath()).arg(getAvailableDataObjectList(state, DataTable::OOClass())));
            }
            table->verifyIntegrity();

            QPalette palette;
            palette.setCurrentColorGroup(QPalette::Active);
            palette.setColor(QPalette::Text, Qt::black);
            palette.setColor(QPalette::WindowText, Qt::black);
            palette.setColor(QPalette::ButtonText, Qt::black);
            palette.setColor(QPalette::Window, Qt::white);
            palette.setColor(QPalette::Base, Qt::white);

            const auto plotWidth = static_cast<const DataTablePlotExporter*>(exporter())->plotWidth();
            const auto plotHeight = static_cast<const DataTablePlotExporter*>(exporter())->plotHeight();
            const auto plotDPI = static_cast<const DataTablePlotExporter*>(exporter())->plotDPI();

            DataTablePlotWidget plotWidget;
            plotWidget.setPalette(std::move(palette));
            plotWidget.setTable(table);
            plotWidget.axisScaleDraw(QwtPlot::yLeft)->setPenWidthF(1);
            plotWidget.axisScaleDraw(QwtPlot::xBottom)->setPenWidthF(1);
            QwtPlotRenderer plotRenderer;
            plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardBackground);
            plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardCanvasBackground);
            plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardCanvasFrame);
            plotRenderer.renderDocument(&plotWidget, filePath, QSizeF(plotWidth, plotHeight), plotDPI);

            co_return;
        }
    };

    return OORef<Job>::create(this, filePath, false);
}

}   // End of namespace
