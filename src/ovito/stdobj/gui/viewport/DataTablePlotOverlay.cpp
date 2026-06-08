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
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/oo/CloneHelper.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/ImagePrimitive.h>
#include "DataTablePlotOverlay.h"

#include <qwt/qwt_color_map.h>
#include <qwt/qwt_interval.h>
#include <qwt/qwt_matrix_raster_data.h>
#include <qwt/qwt_plot_grid.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_marker.h>
#include <qwt/qwt_plot_renderer.h>
#include <qwt/qwt_plot_shapeitem.h>
#include <qwt/qwt_plot_spectrogram.h>
#include <qwt/qwt_scale_map.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_text.h>

#include <QPainterPath>
#include <QVector>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Ovito {

namespace {

bool isFinite(double value)
{
    return std::isfinite(value);
}

QwtLinearColorMap* createHeatmapColorMap()
{
    auto* colorMap = new QwtLinearColorMap(QColor(74, 56, 177), QColor(255, 234, 67));
    colorMap->addColorStop(0.20, QColor(115, 84, 213));
    colorMap->addColorStop(0.45, QColor(67, 194, 245));
    colorMap->addColorStop(0.70, QColor(34, 190, 167));
    colorMap->addColorStop(0.88, QColor(167, 225, 73));
    return colorMap;
}

bool isScalarNumericProperty(const Property* property)
{
    if(!property || property->componentCount() != 1)
        return false;

    switch(property->dataType()) {
    case Property::Int8:
    case Property::Int32:
    case Property::Int64:
    case Property::Float32:
    case Property::Float64:
        return true;
    default:
        return false;
    }
}

bool propertyNameMatches(const Property* property, const QString& name)
{
    if(!property || name.trimmed().isEmpty())
        return false;
    const QString trimmedName = name.trimmed();
    return property->name().compare(trimmedName, Qt::CaseInsensitive) == 0
        || property->identifier().compare(trimmedName, Qt::CaseInsensitive) == 0;
}

const Property* findScalarNumericPropertyByName(const DataTable* table, const QString& name)
{
    if(!table || name.trimmed().isEmpty())
        return nullptr;

    for(const ConstPropertyPtr& property : table->properties()) {
        if(isScalarNumericProperty(property.get()) && propertyNameMatches(property.get(), name))
            return property.get();
    }
    return nullptr;
}

const Property* findFirstNamedScalarNumericProperty(const DataTable* table, const QStringList& names, const std::vector<const Property*>& excluded = {})
{
    if(!table)
        return nullptr;

    for(const QString& name : names) {
        for(const ConstPropertyPtr& property : table->properties()) {
            if(!isScalarNumericProperty(property.get()) || std::ranges::find(excluded, property.get()) != excluded.end())
                continue;
            if(propertyNameMatches(property.get(), name))
                return property.get();
        }
    }
    return nullptr;
}

const Property* findNextScalarNumericProperty(const DataTable* table, const std::vector<const Property*>& excluded)
{
    if(!table)
        return nullptr;

    for(const ConstPropertyPtr& property : table->properties()) {
        if(isScalarNumericProperty(property.get()) && std::ranges::find(excluded, property.get()) == excluded.end())
            return property.get();
    }
    return nullptr;
}

struct HeatmapColumnSelection {
    const Property* x = nullptr;
    const Property* y = nullptr;
    const Property* z = nullptr;
    const Property* boundary = nullptr;
};

HeatmapColumnSelection selectHeatmapColumns(const DataTable* table,
                                            const QString& requestedX,
                                            const QString& requestedY,
                                            const QString& requestedZ,
                                            const QString& requestedBoundary)
{
    HeatmapColumnSelection selection;
    if(!table)
        return selection;

    selection.x = findScalarNumericPropertyByName(table, requestedX);
    if(!selection.x)
        selection.x = findFirstNamedScalarNumericProperty(table, {
            QStringLiteral("Distance"),
            QStringLiteral("r"),
            QStringLiteral("Radius"),
            QStringLiteral("X"),
            QStringLiteral("x")
        });
    if(!selection.x)
        selection.x = findNextScalarNumericProperty(table, {});

    selection.y = findScalarNumericPropertyByName(table, requestedY);
    if(!selection.y)
        selection.y = findFirstNamedScalarNumericProperty(table, {
            QStringLiteral("Theta"),
            QStringLiteral("theta"),
            QStringLiteral("Angle"),
            QStringLiteral("Y"),
            QStringLiteral("y")
        }, { selection.x });
    if(!selection.y)
        selection.y = findNextScalarNumericProperty(table, { selection.x });

    selection.z = findScalarNumericPropertyByName(table, requestedZ);
    if(!selection.z)
        selection.z = findFirstNamedScalarNumericProperty(table, {
            QStringLiteral("Free energy"),
            QStringLiteral("PMF"),
            QStringLiteral("Value"),
            QStringLiteral("Density"),
            QStringLiteral("Count"),
            QStringLiteral("Z"),
            QStringLiteral("z")
        }, { selection.x, selection.y });
    if(!selection.z)
        selection.z = findNextScalarNumericProperty(table, { selection.x, selection.y });

    selection.boundary = findScalarNumericPropertyByName(table, requestedBoundary);
    if(!selection.boundary)
        selection.boundary = findFirstNamedScalarNumericProperty(table, {
            QStringLiteral("In HB basin"),
            QStringLiteral("In basin"),
            QStringLiteral("Basin"),
            QStringLiteral("Mask")
        }, { selection.x, selection.y, selection.z });

    return selection;
}

bool canPlotHeatmapTable(const DataTable* table,
                         const QString& requestedX,
                         const QString& requestedY,
                         const QString& requestedZ)
{
    const HeatmapColumnSelection selection = selectHeatmapColumns(table, requestedX, requestedY, requestedZ, {});
    return selection.x && selection.y && selection.z
        && selection.x != selection.y && selection.x != selection.z && selection.y != selection.z
        && selection.x->size() == selection.y->size() && selection.x->size() == selection.z->size()
        && selection.x->size() > 0;
}

QVector<double> readScalarPropertyValues(const Property* property)
{
    QVector<double> values;
    if(!isScalarNumericProperty(property))
        return values;
    values.resize(static_cast<int>(property->size()));
    property->copyComponentTo(values.begin(), 0);
    return values;
}

QVector<double> uniqueSortedFiniteValues(QVector<double> values)
{
    values.erase(std::remove_if(values.begin(), values.end(), [](double value) { return !isFinite(value); }), values.end());
    std::sort(values.begin(), values.end());

    QVector<double> uniqueValues;
    uniqueValues.reserve(values.size());
    for(double value : values) {
        const double tolerance = std::max(1e-12, std::abs(value) * 1e-10);
        if(uniqueValues.empty() || std::abs(value - uniqueValues.back()) > tolerance)
            uniqueValues.push_back(value);
    }
    return uniqueValues;
}

int coordinateIndex(const QVector<double>& uniqueValues, double value)
{
    if(uniqueValues.empty() || !isFinite(value))
        return -1;

    auto it = std::lower_bound(uniqueValues.begin(), uniqueValues.end(), value);
    const auto closeEnough = [&](double candidate) {
        const double tolerance = std::max(1e-12, std::max(std::abs(candidate), std::abs(value)) * 1e-10);
        return std::abs(candidate - value) <= tolerance;
    };

    if(it != uniqueValues.end() && closeEnough(*it))
        return static_cast<int>(it - uniqueValues.begin());
    if(it != uniqueValues.begin()) {
        --it;
        if(closeEnough(*it))
            return static_cast<int>(it - uniqueValues.begin());
    }
    return -1;
}

QVector<double> coordinateEdges(const QVector<double>& centers)
{
    QVector<double> edges;
    if(centers.empty())
        return edges;

    edges.resize(centers.size() + 1);
    if(centers.size() == 1) {
        edges[0] = centers.front() - 0.5;
        edges[1] = centers.front() + 0.5;
        return edges;
    }

    edges[0] = centers[0] - 0.5 * (centers[1] - centers[0]);
    for(int i = 1; i < centers.size(); ++i)
        edges[i] = 0.5 * (centers[i - 1] + centers[i]);
    edges[centers.size()] = centers.back() + 0.5 * (centers.back() - centers[centers.size() - 2]);
    return edges;
}

struct BoundaryOverlay {
    QPainterPath path;
    QPointF labelPoint = {};
    bool hasLabelPoint = false;
};

BoundaryOverlay buildBoundaryOverlay(const QVector<double>& xEdges,
                                      const QVector<double>& yEdges,
                                      int xBins,
                                      int yBins,
                                      const QVector<uint8_t>& mask)
{
    BoundaryOverlay overlay;
    if(xBins <= 0 || yBins <= 0 || xEdges.size() != xBins + 1 || yEdges.size() != yBins + 1
       || mask.size() != xBins * yBins)
        return overlay;

    double topmostLabelY = -std::numeric_limits<double>::infinity();
    double centeredLabelDistance = 0.0;
    const double xCenter = 0.5 * (xEdges.front() + xEdges.back());

    for(int yBin = 0; yBin < yBins; ++yBin) {
        for(int xBin = 0; xBin < xBins; ++xBin) {
            const int index = yBin * xBins + xBin;
            if(!mask[index])
                continue;

            const auto isOutside = [&](int nx, int ny) {
                return nx < 0 || nx >= xBins || ny < 0 || ny >= yBins || !mask[ny * xBins + nx];
            };
            const auto addSegment = [&](double x0, double y0, double x1, double y1) {
                overlay.path.moveTo(x0, y0);
                overlay.path.lineTo(x1, y1);
                const double segmentMidY = 0.5 * (y0 + y1);
                const double segmentMidX = 0.5 * (x0 + x1);
                const double distanceFromCenter = std::abs(segmentMidX - xCenter);
                if(!overlay.hasLabelPoint
                   || segmentMidY > topmostLabelY
                   || (std::abs(segmentMidY - topmostLabelY) < 1e-9 && distanceFromCenter < centeredLabelDistance)) {
                    overlay.labelPoint = QPointF(segmentMidX, segmentMidY);
                    overlay.hasLabelPoint = true;
                    topmostLabelY = segmentMidY;
                    centeredLabelDistance = distanceFromCenter;
                }
            };

            const double x0 = xEdges[xBin];
            const double x1 = xEdges[xBin + 1];
            const double y0 = yEdges[yBin];
            const double y1 = yEdges[yBin + 1];
            if(isOutside(xBin - 1, yBin))
                addSegment(x0, y0, x0, y1);
            if(isOutside(xBin + 1, yBin))
                addSegment(x1, y0, x1, y1);
            if(isOutside(xBin, yBin - 1))
                addSegment(x0, y0, x1, y0);
            if(isOutside(xBin, yBin + 1))
                addSegment(x0, y1, x1, y1);
        }
    }

    return overlay;
}

class HeatmapPlotWidget : public QwtPlot
{
public:

    explicit HeatmapPlotWidget(QWidget* parent = nullptr) : QwtPlot(parent)
    {
        setCanvasBackground(Qt::white);
        plotLayout()->setAlignCanvasToScales(true);

        auto* plotGrid = new QwtPlotGrid();
        plotGrid->setPen(Qt::gray, 0, Qt::DotLine);
        plotGrid->attach(this);
        plotGrid->setZ(0);

        QFont scaleFont(fontInfo().family(), 8);
        QFont titleFont(fontInfo().family(), 8, QFont::Bold);
        for(int axisId = 0; axisId < QwtPlot::axisCnt; ++axisId) {
            axisWidget(axisId)->setFont(scaleFont);
            QwtText axisText = axisWidget(axisId)->title();
            axisText.setFont(titleFont);
            axisWidget(axisId)->setTitle(axisText);
        }

        setAxisVisible(QwtAxis::XTop, false);
        setAxisVisible(QwtAxis::YRight, true);

        _spectrogram = new QwtPlotSpectrogram();
        _spectrogram->setRenderThreadCount(0);
        _spectrogram->setDisplayMode(QwtPlotSpectrogram::ImageMode, true);
        _spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
        _spectrogram->setDefaultContourPen(QPen(Qt::black, 1.5));
        _spectrogram->setColorMap(createHeatmapColorMap());
        _spectrogram->attach(this);

        _boundaryShape = new QwtPlotShapeItem();
        _boundaryShape->setBrush(Qt::NoBrush);
        _boundaryShape->setPen(QPen(Qt::black, 1.5));
        _boundaryShape->attach(this);

        _boundaryMarker = new QwtPlotMarker();
        _boundaryMarker->setLineStyle(QwtPlotMarker::NoLine);
        _boundaryMarker->setLabelAlignment(Qt::AlignLeft | Qt::AlignBottom);
        _boundaryMarker->attach(this);
        _boundaryMarker->setVisible(false);
    }

    bool setHeatmapTable(const DataTable* table,
                         const QString& requestedX,
                         const QString& requestedY,
                         const QString& requestedZ,
                         const QString& requestedBoundary,
                         const QString& titleOverride,
                         const QString& xAxisLabelOverride,
                         const QString& yAxisLabelOverride,
                         const QString& zAxisLabelOverride,
                         bool showBoundary,
                         bool fixedZRange,
                         double zRangeMin,
                         double zRangeMax,
                         double boundaryLabelValue)
    {
        HeatmapColumnSelection selection = selectHeatmapColumns(table, requestedX, requestedY, requestedZ, requestedBoundary);
        if(!selection.x || !selection.y || !selection.z
           || selection.x == selection.y || selection.x == selection.z || selection.y == selection.z
           || selection.x->size() != selection.y->size() || selection.x->size() != selection.z->size()
           || selection.x->size() == 0) {
            return false;
        }

        QVector<double> xValues = readScalarPropertyValues(selection.x);
        QVector<double> yValues = readScalarPropertyValues(selection.y);
        QVector<double> zValues = readScalarPropertyValues(selection.z);
        QVector<double> xCenters = uniqueSortedFiniteValues(xValues);
        QVector<double> yCenters = uniqueSortedFiniteValues(yValues);
        if(xCenters.empty() || yCenters.empty())
            return false;

        const int xBins = xCenters.size();
        const int yBins = yCenters.size();
        QVector<double> sums(xBins * yBins, 0.0);
        QVector<int> counts(xBins * yBins, 0);
        QVector<uint8_t> boundaryMask(xBins * yBins, 0);

        QVector<double> boundaryValues;
        if(showBoundary && selection.boundary && selection.boundary->size() == selection.x->size())
            boundaryValues = readScalarPropertyValues(selection.boundary);

        for(int row = 0; row < zValues.size(); ++row) {
            const int xIndex = coordinateIndex(xCenters, xValues[row]);
            const int yIndex = coordinateIndex(yCenters, yValues[row]);
            if(xIndex < 0 || yIndex < 0 || !isFinite(zValues[row]))
                continue;

            const int matrixIndex = yIndex * xBins + xIndex;
            sums[matrixIndex] += zValues[row];
            counts[matrixIndex] += 1;
            if(!boundaryValues.empty() && row < boundaryValues.size() && boundaryValues[row] > 0.5)
                boundaryMask[matrixIndex] = 1;
        }

        double zMin = std::numeric_limits<double>::infinity();
        double zMax = -std::numeric_limits<double>::infinity();
        for(int i = 0; i < sums.size(); ++i) {
            if(counts[i] <= 0)
                continue;
            sums[i] /= static_cast<double>(counts[i]);
            zMin = std::min(zMin, sums[i]);
            zMax = std::max(zMax, sums[i]);
        }
        if(!isFinite(zMin) || !isFinite(zMax))
            return false;

        if(fixedZRange) {
            if(!(zRangeMax > zRangeMin))
                return false;
            zMin = zRangeMin;
            zMax = zRangeMax;
        }
        else if(!(zMax > zMin)) {
            zMax = zMin + 1.0;
        }

        for(int i = 0; i < sums.size(); ++i) {
            if(counts[i] <= 0 || !isFinite(sums[i]))
                sums[i] = zMax;
        }

        QVector<double> xEdges = coordinateEdges(xCenters);
        QVector<double> yEdges = coordinateEdges(yCenters);
        if(xEdges.size() != xBins + 1 || yEdges.size() != yBins + 1)
            return false;

        auto* rasterData = new QwtMatrixRasterData();
        rasterData->setResampleMode(QwtMatrixRasterData::NearestNeighbour);
        rasterData->setInterval(Qt::XAxis, QwtInterval(xEdges.front(), xEdges.back()));
        rasterData->setInterval(Qt::YAxis, QwtInterval(yEdges.front(), yEdges.back()));
        rasterData->setInterval(Qt::ZAxis, QwtInterval(zMin, zMax));
        rasterData->setValueMatrix(sums, xBins);
        _spectrogram->setData(rasterData);

        axisWidget(QwtAxis::YRight)->setColorBarEnabled(true);
        axisWidget(QwtAxis::YRight)->setColorBarWidth(14);
        axisWidget(QwtAxis::YRight)->setColorMap(QwtInterval(zMin, zMax), createHeatmapColorMap());
        setAxisScale(QwtAxis::XBottom, xEdges.front(), xEdges.back());
        setAxisScale(QwtAxis::YLeft, yEdges.front(), yEdges.back());
        setAxisScale(QwtAxis::YRight, zMin, zMax);
        setAxisTitle(QwtAxis::XBottom, !xAxisLabelOverride.isEmpty() ? xAxisLabelOverride : selection.x->name());
        setAxisTitle(QwtAxis::YLeft, !yAxisLabelOverride.isEmpty() ? yAxisLabelOverride : selection.y->name());
        setAxisTitle(QwtAxis::YRight, !zAxisLabelOverride.isEmpty() ? zAxisLabelOverride : selection.z->name());
        setTitle(!titleOverride.isEmpty() ? titleOverride : table->objectTitle());

        _boundaryShape->setShape(QPainterPath());
        _boundaryMarker->setVisible(false);
        if(showBoundary && !boundaryValues.empty()) {
            const BoundaryOverlay overlay = buildBoundaryOverlay(xEdges, yEdges, xBins, yBins, boundaryMask);
            _boundaryShape->setShape(overlay.path);
            if(overlay.hasLabelPoint && isFinite(boundaryLabelValue)) {
                QwtText label(QString::number(boundaryLabelValue, 'g', 4));
                label.setColor(Qt::black);
                label.setBackgroundBrush(QBrush(QColor(255, 255, 255, 230)));
                label.setBorderPen(QPen(QColor(160, 160, 160)));
                label.setBorderRadius(4.0);
                label.setPaintAttribute(QwtText::PaintBackground, true);
                _boundaryMarker->setValue(overlay.labelPoint);
                _boundaryMarker->setLabel(label);
                _boundaryMarker->setVisible(true);
            }
        }

        replot();
        return true;
    }

private:

    QwtPlotSpectrogram* _spectrogram = nullptr;
    QwtPlotShapeItem* _boundaryShape = nullptr;
    QwtPlotMarker* _boundaryMarker = nullptr;
};

void renderPlotToImage(QwtPlot& plotWidget, QImage& image)
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QwtPlotRenderer plotRenderer;
    plotRenderer.setDiscardFlag(QwtPlotRenderer::DiscardCanvasFrame);
    plotRenderer.render(&plotWidget, &painter, QRectF(QPointF(0, 0), QSizeF(image.width(), image.height())));
}

}

IMPLEMENT_CREATABLE_OVITO_CLASS(DataTablePlotOverlay);
OVITO_CLASSINFO(DataTablePlotOverlay, "DisplayName", "Data table plot");
OVITO_CLASSINFO(DataTablePlotOverlay, "Description", "Plots a pipeline data table as a viewport layer.");
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, table);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, plotMode);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, alignment);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, offsetX);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, offsetY);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, plotWidth);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, plotHeight);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, opacity);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, title);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, xAxisLabel);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, yAxisLabel);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, zAxisLabel);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, heatmapXColumn);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, heatmapYColumn);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, heatmapZColumn);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, showHeatmapBoundary);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, heatmapBoundaryColumn);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, fixYAxisRange);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, yAxisRangeMin);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, yAxisRangeMax);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, fixZAxisRange);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, zAxisRangeMin);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, zAxisRangeMax);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, minorXTicks);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, minorYTicks);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, showTimeMarker);
DEFINE_PROPERTY_FIELD(DataTablePlotOverlay, timeMarkerColor);
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, table, "Data table");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, plotMode, "Plot type");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, alignment, "Position");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, offsetX, "Offset X");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, offsetY, "Offset Y");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, plotWidth, "Width");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, plotHeight, "Height");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, opacity, "Opacity");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, title, "Title");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, xAxisLabel, "X-axis label");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, yAxisLabel, "Y-axis label");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, zAxisLabel, "Color scale label");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, heatmapXColumn, "X column");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, heatmapYColumn, "Y column");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, heatmapZColumn, "Value column");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, showHeatmapBoundary, "Show boundary");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, heatmapBoundaryColumn, "Boundary column");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, fixYAxisRange, "Fix y-axis range");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, yAxisRangeMin, "Y min");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, yAxisRangeMax, "Y max");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, fixZAxisRange, "Fix color scale range");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, zAxisRangeMin, "Color min");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, zAxisRangeMax, "Color max");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, minorXTicks, "Minor x-ticks");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, minorYTicks, "Minor y-ticks");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, showTimeMarker, "Show time marker");
SET_PROPERTY_FIELD_LABEL(DataTablePlotOverlay, timeMarkerColor, "Marker color");
SET_PROPERTY_FIELD_UNITS(DataTablePlotOverlay, offsetX, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS(DataTablePlotOverlay, offsetY, PercentParameterUnit);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(DataTablePlotOverlay, plotWidth, PercentParameterUnit, 0.01, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(DataTablePlotOverlay, plotHeight, PercentParameterUnit, 0.01, 1);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(DataTablePlotOverlay, opacity, PercentParameterUnit, 0, 1);

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void DataTablePlotOverlay::initializeOverlay(Viewport* viewport)
{
    if(this_task::isInteractive() && !pipeline() && viewport && viewport->scene()) {
        viewport->scene()->visitPipelines([&](SceneNode* sceneNode) {
            if(!sceneNode->pipeline())
                return true;

            const PipelineFlowState& state = sceneNode->pipeline()->getCachedPipelineOutput(viewport->currentTime());
            std::vector<ConstDataObjectPath> tables = state.getObjectsRecursive(DataTable::OOClass());
            if(!tables.empty()) {
                setPipeline(sceneNode->pipeline());
                setTable(DataObjectReference(tables.front()));
                return false;
            }
            return true;
        });
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void DataTablePlotOverlay::propertyChanged(const PropertyFieldDescriptor* field)
{
    if(field == PROPERTY_FIELD(alignment) && !shouldIgnoreChanges() && !isUndoingOrRedoing() && this_task::isInteractive()) {
        setOffsetX(0);
        setOffsetY(0);
    }
    else if(field == PROPERTY_FIELD(table) && !shouldIgnoreChanges()) {
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    ViewportOverlay::propertyChanged(field);
}

/******************************************************************************
* Returns a short piece of information for the pipeline editor list.
******************************************************************************/
QVariant DataTablePlotOverlay::getPipelineEditorShortInfo(Scene* scene) const
{
    return table().dataTitleOrPath();
}

/******************************************************************************
* Lets the overlay paint its contents into the framebuffer.
******************************************************************************/
std::variant<PipelineStatus, Future<PipelineStatus>> DataTablePlotOverlay::render(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& logicalViewportRect, const QRect& physicalViewportRect, const ViewProjectionParameters& noninteractiveProjParams, const Scene* scene)
{
    if(!frameGraph.isInteractive())
        checkAlignmentParameterValue(alignment());

    Pipeline* sourcePipeline = pipeline();
    if(!sourcePipeline && scene) {
        scene->visitPipelines([&](SceneNode* sceneNode) {
            sourcePipeline = sceneNode->pipeline();
            return sourcePipeline == nullptr;
        });
    }

    if(!sourcePipeline)
        return PipelineStatus(PipelineStatus::Warning, tr("No source pipeline is available for the data table plot overlay."));

    PipelineEvaluationRequest request(frameGraph.time(), frameGraph.stopOnPipelineError(), frameGraph.isInteractive());
    return sourcePipeline->evaluatePipeline(request).then(ObjectExecutor(this), [this, frameGraph=OORef<FrameGraph>(&frameGraph), &commandGroup, physicalViewportRect](const PipelineFlowState& state) {
        return renderImplementation(*frameGraph, commandGroup, physicalViewportRect, state);
    });
}

/******************************************************************************
* Finds the selected data table.
******************************************************************************/
const DataTable* DataTablePlotOverlay::resolveTable(const PipelineFlowState& state) const
{
    const DataTable* selectedTable = nullptr;
    if(table())
        selectedTable = dynamic_object_cast<const DataTable>(state.getLeafObject(table()));
    if(selectedTable)
        return selectedTable;

    std::vector<ConstDataObjectPath> tables = state.getObjectsRecursive(DataTable::OOClass());
    if(tables.empty())
        return nullptr;
    return static_object_cast<const DataTable>(tables.front().back());
}

/******************************************************************************
* Renders the selected data table into the frame graph.
******************************************************************************/
PipelineStatus DataTablePlotOverlay::renderImplementation(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QRect& viewportRect, const PipelineFlowState& state) const
{
    const DataTable* sourceTable = resolveTable(state);
    if(!sourceTable)
        return PipelineStatus(PipelineStatus::Warning, tr("The selected pipeline output does not contain a data table."));

    int imageWidth = qMax(8, qRound(plotWidth() * viewportRect.width()));
    int imageHeight = qMax(8, qRound(plotHeight() * viewportRect.height()));
    if(imageWidth <= 0 || imageHeight <= 0)
        return {};

    QImage image(imageWidth, imageHeight, frameGraph.preferredImageFormat());
    image.fill(Qt::transparent);

    const bool forcedHeatmap = plotMode() == HeatmapPlot;
    const bool autoHeatmap = plotMode() == AutoPlot
        && !sourceTable->y()
        && canPlotHeatmapTable(sourceTable, heatmapXColumn(), heatmapYColumn(), heatmapZColumn());

    if(forcedHeatmap || autoHeatmap) {
        if(fixYAxisRange() && yAxisRangeMin() >= yAxisRangeMax())
            return PipelineStatus(PipelineStatus::Error, tr("The fixed y-axis range is invalid."));
        if(fixZAxisRange() && zAxisRangeMin() >= zAxisRangeMax())
            return PipelineStatus(PipelineStatus::Error, tr("The fixed color scale range is invalid."));

        double boundaryLabelValue = std::numeric_limits<double>::quiet_NaN();
        if(auto createdByNode = sourceTable->createdByNode().lock()) {
            if(const PipelineNode* pipelineNode = dynamic_object_cast<const PipelineNode>(createdByNode.get())) {
                const QVariant boundaryAttr =
                    state.getAttributeValue(pipelineNode, QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
                if(boundaryAttr.isValid())
                    boundaryLabelValue = boundaryAttr.toDouble();
            }
        }

        HeatmapPlotWidget plotWidget;
        plotWidget.resize(imageWidth, imageHeight);

        QPalette palette;
        palette.setCurrentColorGroup(QPalette::Active);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::Base, Qt::white);
        plotWidget.setPalette(std::move(palette));

        const bool heatmapReady = plotWidget.setHeatmapTable(
            sourceTable,
            heatmapXColumn(),
            heatmapYColumn(),
            heatmapZColumn(),
            heatmapBoundaryColumn(),
            title(),
            xAxisLabel(),
            yAxisLabel(),
            zAxisLabel(),
            showHeatmapBoundary(),
            fixZAxisRange(),
            zAxisRangeMin(),
            zAxisRangeMax(),
            boundaryLabelValue);
        if(!heatmapReady) {
            if(forcedHeatmap)
                return PipelineStatus(PipelineStatus::Error, tr("The selected data table does not contain compatible scalar X, Y, and value columns for a 2D heatmap."));
        }
        else {
            plotWidget.setAxisMaxMinor(QwtPlot::xBottom, minorXTicks() ? 5 : 0);
            plotWidget.setAxisMaxMinor(QwtPlot::yLeft, minorYTicks() ? 5 : 0);
            if(fixYAxisRange())
                plotWidget.setAxisScale(QwtPlot::yLeft, yAxisRangeMin(), yAxisRangeMax());
            plotWidget.replot();
            renderPlotToImage(plotWidget, image);
        }
    }

    if(image.isNull())
        return {};

    if(!forcedHeatmap && !autoHeatmap) {
        DataOORef<const DataTable> tableForPlot(sourceTable);
        DataOORef<DataTable> clonedTable;
        if(plotMode() != AutoPlot || !xAxisLabel().isEmpty() || !yAxisLabel().isEmpty()) {
            clonedTable = CloneHelper::cloneSingleObject(sourceTable, false);
            if(plotMode() != AutoPlot)
                clonedTable->setPlotMode(static_cast<DataTable::PlotMode>(plotMode()));
            if(!xAxisLabel().isEmpty())
                clonedTable->setAxisLabelX(xAxisLabel());
            if(!yAxisLabel().isEmpty())
                clonedTable->setAxisLabelY(yAxisLabel());
            tableForPlot = clonedTable;
        }

        DataTablePlotWidget plotWidget;
        plotWidget.setMouseNavigationEnabled(false);
        plotWidget.resize(imageWidth, imageHeight);

        QPalette palette;
        palette.setCurrentColorGroup(QPalette::Active);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::WindowText, Qt::black);
        palette.setColor(QPalette::ButtonText, Qt::black);
        palette.setColor(QPalette::Window, Qt::white);
        palette.setColor(QPalette::Base, Qt::white);
        plotWidget.setPalette(std::move(palette));
        plotWidget.setTable(tableForPlot, true);
        plotWidget.setTitle(!title().isEmpty() ? title() : sourceTable->objectTitle());
        plotWidget.setAxisMaxMinor(QwtPlot::xBottom, minorXTicks() ? 5 : 0);
        plotWidget.setAxisMaxMinor(QwtPlot::yLeft, minorYTicks() ? 5 : 0);
        if(fixYAxisRange()) {
            if(yAxisRangeMin() >= yAxisRangeMax())
                return PipelineStatus(PipelineStatus::Error, tr("The fixed y-axis range is invalid."));
            plotWidget.setAxisScale(QwtPlot::yLeft, yAxisRangeMin(), yAxisRangeMax());
        }
        plotWidget.replot();

        renderPlotToImage(plotWidget, image);

        if(showTimeMarker()) {
            const QRect canvasRect = plotWidget.canvas()->geometry();
            double x = plotWidget.canvasMap(QwtPlot::xBottom).transform(frameGraph.time().frame());
            if(std::isfinite(x) && x >= canvasRect.left() && x <= canvasRect.right()) {
                QColor markerColor(timeMarkerColor());
                markerColor.setAlphaF(qBound(0.0, static_cast<double>(opacity()), 1.0));
                QPen pen(markerColor, 2.0);
                QPainter markerPainter(&image);
                markerPainter.setPen(pen);
                markerPainter.drawLine(QPointF(x, canvasRect.top()), QPointF(x, canvasRect.bottom()));
            }
        }
    }

    if(opacity() < 1) {
        QImage translucent(image.size(), QImage::Format_ARGB32_Premultiplied);
        translucent.fill(Qt::transparent);
        QPainter painter(&translucent);
        painter.setOpacity(qBound(0.0, static_cast<double>(opacity()), 1.0));
        painter.drawImage(QPoint(0, 0), image);
        painter.end();
        image = std::move(translucent);
    }

    QPointF origin(offsetX() * viewportRect.width() + viewportRect.left(), -offsetY() * viewportRect.height() + viewportRect.top());
    const FloatType hmargin = FloatType(0.01) * viewportRect.width();
    const FloatType vmargin = FloatType(0.01) * viewportRect.height();

    if(alignment() & Qt::AlignLeft) origin.rx() += hmargin;
    else if(alignment() & Qt::AlignRight) origin.rx() += viewportRect.width() - hmargin - imageWidth;
    else if(alignment() & Qt::AlignHCenter) origin.rx() += FloatType(0.5) * viewportRect.width() - FloatType(0.5) * imageWidth;

    if(alignment() & Qt::AlignTop) origin.ry() += vmargin;
    else if(alignment() & Qt::AlignBottom) origin.ry() += viewportRect.height() - vmargin - imageHeight;
    else if(alignment() & Qt::AlignVCenter) origin.ry() += FloatType(0.5) * viewportRect.height() - FloatType(0.5) * imageHeight;

    QRectF targetRect(origin, QSizeF(imageWidth, imageHeight));
    commandGroup.addPrimitivePreprojected(std::make_unique<ImagePrimitive>(std::move(image), targetRect));

    return {};
}

}   // End of namespace
