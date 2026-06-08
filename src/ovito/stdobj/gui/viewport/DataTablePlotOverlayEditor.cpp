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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/gui/viewport/DataTablePlotOverlay.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/PipelineSelectionParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/viewport/overlays/MoveOverlayInputMode.h>
#include <ovito/gui/desktop/widgets/general/ViewportModeButton.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include "DataTablePlotOverlayEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTablePlotOverlayEditor);
SET_OVITO_OBJECT_EDITOR(DataTablePlotOverlay, DataTablePlotOverlayEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void DataTablePlotOverlayEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Data table plot"), rolloutParams, "manual:viewport_layers.data_table_plot");

    QVBoxLayout* parentLayout = new QVBoxLayout(rollout);
    parentLayout->setContentsMargins(4,4,4,4);
    parentLayout->setSpacing(4);

    QGroupBox* sourceBox = new QGroupBox(tr("Source"));
    parentLayout->addWidget(sourceBox);
    QGridLayout* sourceLayout = new QGridLayout(sourceBox);
    sourceLayout->setContentsMargins(4,4,4,4);
    sourceLayout->setSpacing(4);
    sourceLayout->setColumnStretch(1, 1);

    PipelineSelectionParameterUI* pipelineUI = createParamUI<PipelineSelectionParameterUI>(PROPERTY_FIELD(ViewportOverlay::pipeline));
    sourceLayout->addWidget(new QLabel(tr("Pipeline:")), 0, 0);
    sourceLayout->addWidget(pipelineUI->comboBox(), 0, 1);

    DataObjectReferenceParameterUI* tableUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::table), DataTable::OOClass());
    sourceLayout->addWidget(new QLabel(tr("Data table:")), 1, 0);
    sourceLayout->addWidget(tableUI->comboBox(), 1, 1);

    VariantComboBoxParameterUI* plotModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::plotMode));
    plotModeUI->comboBox()->addItem(tr("Auto-detect"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::AutoPlot)));
    plotModeUI->comboBox()->addItem(tr("Heatmap / 2D table"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::HeatmapPlot)));
    plotModeUI->comboBox()->addItem(tr("Line"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::LinePlot)));
    plotModeUI->comboBox()->addItem(tr("Distribution histogram"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::HistogramPlot)));
    plotModeUI->comboBox()->addItem(tr("Category bar chart"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::BarChartPlot)));
    plotModeUI->comboBox()->addItem(tr("Scatter"), QVariant::fromValue(static_cast<int>(DataTablePlotOverlay::ScatterPlot)));
    sourceLayout->addWidget(new QLabel(tr("Plot type:")), 2, 0);
    sourceLayout->addWidget(plotModeUI->comboBox(), 2, 1);

    QGroupBox* heatmapBox = new QGroupBox(tr("2D heatmap"));
    parentLayout->addWidget(heatmapBox);
    QGridLayout* heatmapLayout = new QGridLayout(heatmapBox);
    heatmapLayout->setContentsMargins(4,4,4,4);
    heatmapLayout->setSpacing(4);
    heatmapLayout->setColumnStretch(1, 1);
    int heatmapRow = 0;

    StringParameterUI* heatmapXColumnUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::heatmapXColumn));
    heatmapLayout->addWidget(new QLabel(tr("X column:")), heatmapRow, 0);
    heatmapLayout->addWidget(heatmapXColumnUI->textBox(), heatmapRow++, 1);

    StringParameterUI* heatmapYColumnUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::heatmapYColumn));
    heatmapLayout->addWidget(new QLabel(tr("Y column:")), heatmapRow, 0);
    heatmapLayout->addWidget(heatmapYColumnUI->textBox(), heatmapRow++, 1);

    StringParameterUI* heatmapZColumnUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::heatmapZColumn));
    heatmapLayout->addWidget(new QLabel(tr("Value column:")), heatmapRow, 0);
    heatmapLayout->addWidget(heatmapZColumnUI->textBox(), heatmapRow++, 1);

    BooleanParameterUI* heatmapBoundaryUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::showHeatmapBoundary));
    heatmapLayout->addWidget(heatmapBoundaryUI->checkBox(), heatmapRow++, 0, 1, 2);

    StringParameterUI* heatmapBoundaryColumnUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::heatmapBoundaryColumn));
    heatmapLayout->addWidget(new QLabel(tr("Boundary column:")), heatmapRow, 0);
    heatmapLayout->addWidget(heatmapBoundaryColumnUI->textBox(), heatmapRow++, 1);

    QGroupBox* positionBox = new QGroupBox(tr("Positioning"));
    parentLayout->addWidget(positionBox);
    QGridLayout* positionLayout = new QGridLayout(positionBox);
    positionLayout->setContentsMargins(4,4,4,4);
    positionLayout->setSpacing(4);
    positionLayout->setColumnStretch(1, 1);
    positionLayout->setColumnStretch(2, 1);
    int row = 0;

    VariantComboBoxParameterUI* alignmentUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::alignment));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_left"), tr("Top left"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignLeft)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top"), tr("Top"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignHCenter)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_right"), tr("Top right"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignRight)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_right"), tr("Right"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignRight)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_right"), tr("Bottom right"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignRight)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom"), tr("Bottom"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignHCenter)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_left"), tr("Bottom left"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignLeft)));
    alignmentUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_left"), tr("Left"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignLeft)));
    positionLayout->addWidget(new QLabel(tr("Alignment:")), row, 0);
    positionLayout->addWidget(alignmentUI->comboBox(), row++, 1, 1, 2);

    FloatParameterUI* offsetXUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::offsetX));
    FloatParameterUI* offsetYUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::offsetY));
    positionLayout->addWidget(new QLabel(tr("XY offset:")), row, 0);
    positionLayout->addLayout(offsetXUI->createFieldLayout(), row, 1);
    positionLayout->addLayout(offsetYUI->createFieldLayout(), row++, 2);

    FloatParameterUI* widthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::plotWidth));
    FloatParameterUI* heightUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::plotHeight));
    positionLayout->addWidget(new QLabel(tr("Size:")), row, 0);
    positionLayout->addLayout(widthUI->createFieldLayout(), row, 1);
    positionLayout->addLayout(heightUI->createFieldLayout(), row++, 2);

    FloatParameterUI* opacityUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::opacity));
    positionLayout->addWidget(opacityUI->label(), row, 0);
    positionLayout->addLayout(opacityUI->createFieldLayout(), row++, 1, 1, 2);

    OORef<MoveOverlayInputMode> moveOverlayMode = OORef<MoveOverlayInputMode>::create(this);
    connect(this, &QObject::destroyed, moveOverlayMode, &ViewportInputMode::removeMode);
    ViewportModeAction* moveOverlayAction = new ViewportModeAction(ui(), tr("Move"), this, std::move(moveOverlayMode));
    moveOverlayAction->setIcon(QIcon::fromTheme("edit_mode_move"));
    moveOverlayAction->setToolTip(tr("Reposition the plot in the viewport using the mouse"));
    positionLayout->addWidget(new ViewportModeButton(moveOverlayAction), row++, 1, 1, 2, Qt::AlignRight | Qt::AlignTop);

    QGroupBox* styleBox = new QGroupBox(tr("Figure style"));
    parentLayout->addWidget(styleBox);
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    styleLayout->setContentsMargins(4,4,4,4);
    styleLayout->setSpacing(4);
    styleLayout->setColumnStretch(1, 1);
    row = 0;

    StringParameterUI* titleUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::title));
    styleLayout->addWidget(new QLabel(tr("Title:")), row, 0);
    styleLayout->addWidget(titleUI->textBox(), row++, 1);

    StringParameterUI* xAxisLabelUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::xAxisLabel));
    styleLayout->addWidget(new QLabel(tr("X-axis label:")), row, 0);
    styleLayout->addWidget(xAxisLabelUI->textBox(), row++, 1);

    StringParameterUI* yAxisLabelUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::yAxisLabel));
    styleLayout->addWidget(new QLabel(tr("Y-axis label:")), row, 0);
    styleLayout->addWidget(yAxisLabelUI->textBox(), row++, 1);

    StringParameterUI* zAxisLabelUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::zAxisLabel));
    styleLayout->addWidget(new QLabel(tr("Color scale label:")), row, 0);
    styleLayout->addWidget(zAxisLabelUI->textBox(), row++, 1);

    BooleanParameterUI* minorXUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::minorXTicks));
    styleLayout->addWidget(minorXUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* minorYUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::minorYTicks));
    styleLayout->addWidget(minorYUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* timeMarkerUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::showTimeMarker));
    styleLayout->addWidget(timeMarkerUI->checkBox(), row, 0);
    ColorParameterUI* timeMarkerColorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::timeMarkerColor));
    styleLayout->addWidget(timeMarkerColorUI->colorPicker(), row++, 1);

    BooleanGroupBoxParameterUI* yRangeUI = createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::fixYAxisRange));
    yRangeUI->groupBox()->setTitle(tr("Fixed y-axis range"));
    parentLayout->addWidget(yRangeUI->groupBox());

    QGridLayout* yRangeLayout = new QGridLayout(yRangeUI->childContainer());
    yRangeLayout->setContentsMargins(4,4,4,4);
    yRangeLayout->setSpacing(4);
    yRangeLayout->setColumnStretch(1, 1);

    FloatParameterUI* yMinUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::yAxisRangeMin));
    yRangeLayout->addWidget(yMinUI->label(), 0, 0);
    yRangeLayout->addLayout(yMinUI->createFieldLayout(), 0, 1);

    FloatParameterUI* yMaxUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::yAxisRangeMax));
    yRangeLayout->addWidget(yMaxUI->label(), 1, 0);
    yRangeLayout->addLayout(yMaxUI->createFieldLayout(), 1, 1);

    BooleanGroupBoxParameterUI* zRangeUI = createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::fixZAxisRange));
    zRangeUI->groupBox()->setTitle(tr("Fixed color scale range"));
    parentLayout->addWidget(zRangeUI->groupBox());

    QGridLayout* zRangeLayout = new QGridLayout(zRangeUI->childContainer());
    zRangeLayout->setContentsMargins(4,4,4,4);
    zRangeLayout->setSpacing(4);
    zRangeLayout->setColumnStretch(1, 1);

    FloatParameterUI* zMinUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::zAxisRangeMin));
    zRangeLayout->addWidget(zMinUI->label(), 0, 0);
    zRangeLayout->addLayout(zMinUI->createFieldLayout(), 0, 1);

    FloatParameterUI* zMaxUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(DataTablePlotOverlay::zAxisRangeMax));
    zRangeLayout->addWidget(zMaxUI->label(), 1, 0);
    zRangeLayout->addLayout(zMaxUI->createFieldLayout(), 1, 1);
}

}   // End of namespace
