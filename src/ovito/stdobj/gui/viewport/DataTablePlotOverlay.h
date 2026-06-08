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

#pragma once


#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

/**
 * \brief A viewport overlay that renders a selected DataTable plot.
 */
class OVITO_STDOBJGUI_EXPORT DataTablePlotOverlay : public ViewportOverlay
{
    OVITO_CLASS(DataTablePlotOverlay)

public:

    enum PlotModeOverride {
        HeatmapPlot = -2,
        AutoPlot = -1,
        LinePlot = DataTable::Line,
        HistogramPlot = DataTable::Histogram,
        BarChartPlot = DataTable::BarChart,
        ScatterPlot = DataTable::Scatter
    };
    Q_ENUM(PlotModeOverride);

    /// This virtual method gets called when the overlay is being newly attached to a viewport.
    virtual void initializeOverlay(Viewport* viewport) override;

    /// Lets the overlay paint its contents into the framebuffer.
    virtual std::variant<PipelineStatus, Future<PipelineStatus>> render(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene) override;

    /// Moves the position of the overlay in the viewport by the given amount.
    virtual void moveLayerInViewport(const Vector2& delta) override {
        auto roundPercent = [](FloatType f) { return std::round(f * 1e4) / 1e4; };
        setOffsetX(roundPercent(offsetX() + delta.x()));
        setOffsetY(roundPercent(offsetY() + delta.y()));
    }

    /// Returns a short piece of information to be displayed next to the object's title in the pipeline editor.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene) const override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Renders the selected data table into the frame graph.
    PipelineStatus renderImplementation(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& viewportRect, const PipelineFlowState& state) const;

    /// Finds the table selected by the user, or the first available data table if no table was selected yet.
    const DataTable* resolveTable(const PipelineFlowState& state) const;

    /// The data table to plot.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(DataObjectReference{}, table, setTable);

    /// Optional plot type override.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{AutoPlot}, plotMode, setPlotMode, PROPERTY_FIELD_MEMORIZE);

    /// The corner of the viewport where the plot is displayed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{Qt::AlignRight | Qt::AlignTop}, alignment, setAlignment, PROPERTY_FIELD_MEMORIZE);

    /// Controls the horizontal offset of plot position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetX, setOffsetX, PROPERTY_FIELD_MEMORIZE);

    /// Controls the vertical offset of plot position.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, offsetY, setOffsetY, PROPERTY_FIELD_MEMORIZE);

    /// Controls the relative plot width.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.42}, plotWidth, setPlotWidth, PROPERTY_FIELD_MEMORIZE);

    /// Controls the relative plot height.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.32}, plotHeight, setPlotHeight, PROPERTY_FIELD_MEMORIZE);

    /// Controls the overlay opacity.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, opacity, setOpacity, PROPERTY_FIELD_MEMORIZE);

    /// Optional plot title.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, title, setTitle);

    /// Optional x-axis label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, xAxisLabel, setXAxisLabel);

    /// Optional y-axis label.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, yAxisLabel, setYAxisLabel);

    /// Optional color scale label for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, zAxisLabel, setZAxisLabel);

    /// Optional name of the X coordinate column for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, heatmapXColumn, setHeatmapXColumn);

    /// Optional name of the Y coordinate column for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, heatmapYColumn, setHeatmapYColumn);

    /// Optional name of the value column for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, heatmapZColumn, setHeatmapZColumn);

    /// Toggles the boundary outline for 2D heatmap tables with a binary mask column.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, showHeatmapBoundary, setShowHeatmapBoundary, PROPERTY_FIELD_MEMORIZE);

    /// Optional name of the binary boundary mask column for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, heatmapBoundaryColumn, setHeatmapBoundaryColumn);

    /// Toggles use of a fixed y-axis range.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, fixYAxisRange, setFixYAxisRange, PROPERTY_FIELD_MEMORIZE);

    /// Lower y-axis limit.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, yAxisRangeMin, setYAxisRangeMin);

    /// Upper y-axis limit.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{1}, yAxisRangeMax, setYAxisRangeMax);

    /// Toggles use of a fixed color scale range for 2D heatmap tables.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, fixZAxisRange, setFixZAxisRange, PROPERTY_FIELD_MEMORIZE);

    /// Lower color scale limit.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, zAxisRangeMin, setZAxisRangeMin);

    /// Upper color scale limit.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{1}, zAxisRangeMax, setZAxisRangeMax);

    /// Toggles minor ticks on the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, minorXTicks, setMinorXTicks, PROPERTY_FIELD_MEMORIZE);

    /// Toggles minor ticks on the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, minorYTicks, setMinorYTicks, PROPERTY_FIELD_MEMORIZE);

    /// Draws a vertical marker at the current animation frame.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, showTimeMarker, setShowTimeMarker, PROPERTY_FIELD_MEMORIZE);

    /// Color of the current-frame marker.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.8, 0, 0}), timeMarkerColor, setTimeMarkerColor, PROPERTY_FIELD_MEMORIZE);

    friend class DataTablePlotOverlayEditor;
};

}   // End of namespace
