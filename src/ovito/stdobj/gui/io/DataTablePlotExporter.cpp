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
 * This is called once for every output file to be written.
 *****************************************************************************/
void DataTablePlotExporter::openOutputFile(const QString& filePath, int numberOfFrames)
{
    OVITO_ASSERT(!_outputFile.isOpen());
    _outputFile.setFileName(filePath);
}

/******************************************************************************
 * This is called once for every output file written .
 *****************************************************************************/
void DataTablePlotExporter::closeOutputFile(bool exportCompleted)
{
    if(!exportCompleted)
        _outputFile.remove();
}

/******************************************************************************
 * Exports a single animation frame to the current output file.
 *****************************************************************************/
void DataTablePlotExporter::exportFrame(int frameNumber, const QString& filePath)
{
    // Evaluate pipeline.
    const PipelineFlowState& state = getPipelineDataToBeExported(frameNumber);

    // Look up the DataTable to be exported in the pipeline state.
    DataObjectReference objectRef(&DataTable::OOClass(), dataObjectToExport().dataPath());
    const DataTable* table = static_object_cast<DataTable>(state.getLeafObject(objectRef));
    if(!table) {
        throw Exception(tr("The pipeline output does not contain the data table to be exported (animation frame: %1; object key: %2). Available data tables: (%3)")
            .arg(frameNumber).arg(objectRef.dataPath()).arg(getAvailableDataObjectList(state, DataTable::OOClass())));
    }
    table->verifyIntegrity();

    DataTablePlotWidget plotWidget;
    plotWidget.setTable(table);
    plotWidget.axisScaleDraw(QwtPlot::yLeft)->setPenWidthF(1);
    plotWidget.axisScaleDraw(QwtPlot::xBottom)->setPenWidthF(1);
    QwtPlotRenderer plotRenderer;
    plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardBackground);
    plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardCanvasBackground);
    plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardFlag::DiscardCanvasFrame);
    plotRenderer.renderDocument(&plotWidget, outputFile().fileName(), QSizeF(plotWidth(), plotHeight()), plotDPI());
}

}   // End of namespace
