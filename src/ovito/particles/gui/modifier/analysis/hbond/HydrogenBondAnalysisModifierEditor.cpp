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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/modifier/analysis/hbond/HydrogenBondAnalysisModifier.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <qwt/qwt_color_map.h>
#include <qwt/qwt_interval.h>
#include <qwt/qwt_matrix_raster_data.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_grid.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_legenditem.h>
#include <qwt/qwt_plot_marker.h>
#include <qwt/qwt_plot_shapeitem.h>
#include <qwt/qwt_plot_spectrogram.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_symbol.h>
#include <qwt/qwt_text.h>
#include <QPainterPath>
#include <QPointer>
#include <QToolTip>
#include <QVector>
#include <algorithm>
#include <array>
#include <cmath>
#include "HydrogenBondAnalysisModifierEditor.h"

namespace Ovito {

namespace {

bool hydrogenBondAnalysisIsIdle(const HydrogenBondAnalysisModifier* modifier, const ModificationNode* node)
{
    const auto* hbNode = dynamic_object_cast<const HydrogenBondAnalysisModificationNode>(node);
    return modifier && hbNode && !hbNode->hasCachedResults() && modifier->runRequestId() <= hbNode->completedRunRequestId();
}

bool isFinite(double value)
{
    return std::isfinite(value);
}

QwtLinearColorMap* createPmfColorMap()
{
    auto* colorMap = new QwtLinearColorMap(QColor(74, 56, 177), QColor(255, 234, 67));
    colorMap->addColorStop(0.20, QColor(115, 84, 213));
    colorMap->addColorStop(0.45, QColor(67, 194, 245));
    colorMap->addColorStop(0.70, QColor(34, 190, 167));
    colorMap->addColorStop(0.88, QColor(167, 225, 73));
    return colorMap;
}

double invalidAwareMaximum(double a, double b)
{
    if(!isFinite(a))
        return b;
    if(!isFinite(b))
        return a;
    return std::max(a, b);
}

struct BasinBoundaryOverlay {
    QPainterPath path;
    QPointF labelPoint = {};
    bool hasLabelPoint = false;
};

BasinBoundaryOverlay buildBasinBoundaryOverlay(double distanceMinimum,
                                               double distanceMaximum,
                                               double thetaMinimum,
                                               double thetaMaximum,
                                               int distanceBins,
                                               int angleBins,
                                               const BufferReadAccess<FloatType>& freeEnergy,
                                               const BufferReadAccess<int64_t>& inBasin,
                                               double threshold)
{
    BasinBoundaryOverlay overlay;
    if(distanceBins < 2 || angleBins < 2 || distanceMaximum <= distanceMinimum || thetaMaximum <= thetaMinimum)
        return overlay;

    const double distanceBinWidth = (distanceMaximum - distanceMinimum) / static_cast<double>(distanceBins);
    const double angleBinWidth = (thetaMaximum - thetaMinimum) / static_cast<double>(angleBins);
    double topmostLabelY = -std::numeric_limits<double>::infinity();
    double centeredLabelDistance = 0.0;
    const double distanceCenter = 0.5 * (distanceMinimum + distanceMaximum);

    const auto pointFor = [&](int distanceBin, int angleBin) {
        return QPointF(distanceMinimum + (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth,
                       thetaMinimum + (static_cast<double>(angleBin) + 0.5) * angleBinWidth);
    };
    const auto indexFor = [&](int distanceBin, int angleBin) {
        return static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins)
             + static_cast<size_t>(angleBin);
    };
    const auto crossingPoint = [&](int firstDistanceBin,
                                   int firstAngleBin,
                                   int secondDistanceBin,
                                   int secondAngleBin) {
        const size_t firstIndex = indexFor(firstDistanceBin, firstAngleBin);
        const size_t secondIndex = indexFor(secondDistanceBin, secondAngleBin);
        const double firstValue = freeEnergy[firstIndex];
        const double secondValue = freeEnergy[secondIndex];
        double fraction = 0.5;
        if(isFinite(firstValue) && isFinite(secondValue)
           && std::abs(secondValue - firstValue) > 1e-12) {
            fraction = std::clamp((threshold - firstValue) / (secondValue - firstValue), 0.0, 1.0);
        }
        const QPointF firstPoint = pointFor(firstDistanceBin, firstAngleBin);
        const QPointF secondPoint = pointFor(secondDistanceBin, secondAngleBin);
        return firstPoint + fraction * (secondPoint - firstPoint);
    };
    const auto addSegment = [&](const QPointF& first, const QPointF& second) {
                overlay.path.moveTo(first);
                overlay.path.lineTo(second);
                const double segmentMidY = 0.5 * (first.y() + second.y());
                const double segmentMidX = 0.5 * (first.x() + second.x());
                const double distanceFromCenter = std::abs(segmentMidX - distanceCenter);
                if(!overlay.hasLabelPoint
                   || segmentMidY > topmostLabelY
                   || (std::abs(segmentMidY - topmostLabelY) < 1e-9 && distanceFromCenter < centeredLabelDistance)) {
                    overlay.labelPoint = QPointF(segmentMidX, segmentMidY);
                    overlay.hasLabelPoint = true;
                    topmostLabelY = segmentMidY;
                    centeredLabelDistance = distanceFromCenter;
                }
    };

    for(int distanceBin = 0; distanceBin < distanceBins - 1; ++distanceBin) {
        for(int angleBin = 0; angleBin < angleBins - 1; ++angleBin) {
            const std::array<std::pair<int, int>, 4> corners = {{
                {distanceBin, angleBin},
                {distanceBin + 1, angleBin},
                {distanceBin + 1, angleBin + 1},
                {distanceBin, angleBin + 1}
            }};
            std::array<bool, 4> inside = {};
            for(size_t corner = 0; corner < corners.size(); ++corner)
                inside[corner] = inBasin[indexFor(corners[corner].first, corners[corner].second)] != 0;

            std::vector<QPointF> crossings;
            crossings.reserve(4);
            for(size_t edge = 0; edge < corners.size(); ++edge) {
                const size_t next = (edge + 1) % corners.size();
                if(inside[edge] == inside[next])
                    continue;
                crossings.push_back(crossingPoint(corners[edge].first,
                                                  corners[edge].second,
                                                  corners[next].first,
                                                  corners[next].second));
            }
            if(crossings.size() == 2) {
                addSegment(crossings[0], crossings[1]);
            }
            else if(crossings.size() == 4) {
                addSegment(crossings[0], crossings[1]);
                addSegment(crossings[2], crossings[3]);
            }
        }
    }

    return overlay;
}

}

class HydrogenBondPmfPlotWidget : public QwtPlot
{
public:

    explicit HydrogenBondPmfPlotWidget(QWidget* parent = nullptr) : QwtPlot(parent)
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

        setTitle(tr("PMF W(r, theta) / kBT"));
        setAxisTitle(QwtAxis::XBottom, tr("r (A)"));
        setAxisTitle(QwtAxis::YLeft, tr("theta (degrees)"));
        setAxisTitle(QwtAxis::YRight, tr("W / kBT"));
        setAxisVisible(QwtAxis::XTop, false);
        setAxisVisible(QwtAxis::YRight, false);

        _spectrogram = new QwtPlotSpectrogram();
        _spectrogram->setRenderThreadCount(0);
        _spectrogram->setDisplayMode(QwtPlotSpectrogram::ImageMode, true);
        _spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
        _spectrogram->setDefaultContourPen(QPen(Qt::black, 1.5));
        _spectrogram->setColorMap(createPmfColorMap());
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

    void clearPlot()
    {
        _spectrogram->setData(nullptr);
        _spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
        _spectrogram->setContourLevels(QList<double>());
        _boundaryShape->setShape(QPainterPath());
        _boundaryMarker->setVisible(false);
        axisWidget(QwtAxis::YRight)->setColorBarEnabled(false);
        setAxisVisible(QwtAxis::YRight, false);
        replot();
    }

    void setPmfTable(DataOORef<const DataTable> table,
                     double distanceMinimum,
                     double distanceMaximum,
                     double thetaMinimum,
                     double thetaMaximum,
                     int distanceBins,
                     int angleBins,
                     double boundaryFreeEnergy)
    {
        if(!table || distanceBins <= 0 || angleBins <= 0 || distanceMaximum <= distanceMinimum || thetaMaximum <= thetaMinimum) {
            clearPlot();
            return;
        }

        BufferReadAccess<FloatType> freeEnergy(table->getProperty(QStringLiteral("Free energy")));
        BufferReadAccess<int64_t> inBasin(table->getProperty(QStringLiteral("In HB basin")));
        if(!freeEnergy || !inBasin || freeEnergy.size() != static_cast<size_t>(distanceBins * angleBins)
           || inBasin.size() != freeEnergy.size()) {
            clearPlot();
            return;
        }

        double zMin = std::numeric_limits<double>::infinity();
        double zMax = -std::numeric_limits<double>::infinity();
        int minDistanceBin = distanceBins;
        int maxDistanceBin = -1;
        int minAngleBin = angleBins;
        int maxAngleBin = -1;
        for(size_t index = 0; index < freeEnergy.size(); ++index) {
            const double value = freeEnergy[index];
            if(!isFinite(value))
                continue;
            zMin = std::min(zMin, value);
            zMax = std::max(zMax, value);
            const int distanceBin = static_cast<int>(index / static_cast<size_t>(angleBins));
            const int angleBin = static_cast<int>(index % static_cast<size_t>(angleBins));
            minDistanceBin = std::min(minDistanceBin, distanceBin);
            maxDistanceBin = std::max(maxDistanceBin, distanceBin);
            minAngleBin = std::min(minAngleBin, angleBin);
            maxAngleBin = std::max(maxAngleBin, angleBin);
        }

        if(!isFinite(zMin) || !isFinite(zMax) || maxDistanceBin < minDistanceBin || maxAngleBin < minAngleBin) {
            clearPlot();
            return;
        }

        zMax = invalidAwareMaximum(zMax, boundaryFreeEnergy);
        if(!(zMax > zMin))
            zMax = zMin + 1.0;

        const double distanceBinWidth = (distanceMaximum - distanceMinimum) / static_cast<double>(distanceBins);
        const double angleBinWidth = (thetaMaximum - thetaMinimum) / static_cast<double>(angleBins);
        QVector<double> values;
        values.resize(distanceBins * angleBins);
        for(int angleBin = 0; angleBin < angleBins; ++angleBin) {
            for(int distanceBin = 0; distanceBin < distanceBins; ++distanceBin) {
                const size_t linearIndex = static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins)
                                         + static_cast<size_t>(angleBin);
                const int matrixIndex = angleBin * distanceBins + distanceBin;
                values[matrixIndex] = isFinite(freeEnergy[linearIndex]) ? freeEnergy[linearIndex] : zMax;
            }
        }

        auto* rasterData = new QwtMatrixRasterData();
        rasterData->setResampleMode(QwtMatrixRasterData::BilinearInterpolation);
        rasterData->setInterval(Qt::XAxis, QwtInterval(distanceMinimum, distanceMaximum));
        rasterData->setInterval(Qt::YAxis, QwtInterval(thetaMinimum, thetaMaximum));
        rasterData->setInterval(Qt::ZAxis, QwtInterval(zMin, zMax));
        rasterData->setValueMatrix(values, distanceBins);
        _spectrogram->setData(rasterData);

        axisWidget(QwtAxis::YRight)->setColorBarEnabled(true);
        axisWidget(QwtAxis::YRight)->setColorBarWidth(14);
        axisWidget(QwtAxis::YRight)->setColorMap(QwtInterval(zMin, zMax), createPmfColorMap());
        setAxisVisible(QwtAxis::YRight, true);
        setAxisScale(QwtAxis::YRight, zMin, zMax);
        const double viewXMin = std::max(distanceMinimum, distanceMinimum + static_cast<double>(minDistanceBin) * distanceBinWidth - 0.5 * distanceBinWidth);
        const double viewXMax = std::min(distanceMaximum, distanceMinimum + static_cast<double>(maxDistanceBin + 1) * distanceBinWidth + 0.5 * distanceBinWidth);
        const double viewYMin = std::max(thetaMinimum, thetaMinimum + static_cast<double>(minAngleBin) * angleBinWidth - 0.5 * angleBinWidth);
        const double viewYMax = std::min(thetaMaximum, thetaMinimum + static_cast<double>(maxAngleBin + 1) * angleBinWidth + 0.5 * angleBinWidth);
        setAxisScale(QwtAxis::XBottom, viewXMin, viewXMax);
        setAxisScale(QwtAxis::YLeft, viewYMin, viewYMax);

        if(isFinite(boundaryFreeEnergy)) {
            _spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
            _spectrogram->setContourLevels(QList<double>());

            const BasinBoundaryOverlay overlay =
                buildBasinBoundaryOverlay(distanceMinimum,
                                          distanceMaximum,
                                          thetaMinimum,
                                          thetaMaximum,
                                          distanceBins,
                                          angleBins,
                                          freeEnergy,
                                          inBasin,
                                          boundaryFreeEnergy);
            _boundaryShape->setShape(overlay.path);
            if(overlay.hasLabelPoint) {
                QwtText label(tr("W = %1 kBT").arg(boundaryFreeEnergy, 0, 'g', 4));
                label.setColor(Qt::black);
                label.setBackgroundBrush(QBrush(QColor(255, 255, 255, 230)));
                label.setBorderPen(QPen(QColor(160, 160, 160)));
                label.setBorderRadius(4.0);
                label.setPaintAttribute(QwtText::PaintBackground, true);
                _boundaryMarker->setValue(overlay.labelPoint);
                _boundaryMarker->setLabel(label);
                _boundaryMarker->setVisible(true);
            }
            else {
                _boundaryMarker->setVisible(false);
                _boundaryShape->setShape(QPainterPath());
            }
        }
        else {
            _spectrogram->setDisplayMode(QwtPlotSpectrogram::ContourMode, false);
            _spectrogram->setContourLevels(QList<double>());
            _boundaryShape->setShape(QPainterPath());
            _boundaryMarker->setVisible(false);
        }

        replot();
    }

private:

    QwtPlotSpectrogram* _spectrogram = nullptr;
    QwtPlotShapeItem* _boundaryShape = nullptr;
    QwtPlotMarker* _boundaryMarker = nullptr;
};

class HydrogenBondGeometryPlotWidget : public QwtPlot
{
public:

    explicit HydrogenBondGeometryPlotWidget(QWidget* parent = nullptr) : QwtPlot(parent)
    {
        setCanvasBackground(Qt::white);
        plotLayout()->setAlignCanvasToScales(true);

        auto* plotGrid = new QwtPlotGrid();
        plotGrid->setPen(Qt::gray, 0, Qt::DotLine);
        plotGrid->setZ(0);
        plotGrid->attach(this);

        QFont scaleFont(fontInfo().family(), 8);
        QFont titleFont(fontInfo().family(), 8, QFont::Bold);
        for(int axisId = 0; axisId < QwtPlot::axisCnt; ++axisId) {
            axisWidget(axisId)->setFont(scaleFont);
            QwtText axisText = axisWidget(axisId)->title();
            axisText.setFont(titleFont);
            axisWidget(axisId)->setTitle(axisText);
        }
        setTitle(tr("D-A distance vs. D-H-A angle"));
        setAxisTitle(QwtAxis::XBottom, tr("D-A distance"));
        setAxisTitle(QwtAxis::YLeft, tr("D-H-A angle (degrees)"));
        setAxisVisible(QwtAxis::XTop, false);
        setAxisVisible(QwtAxis::YRight, false);
        setAxisScale(QwtAxis::YLeft, 0.0, 180.0);

        _otherCandidateCurve = new QwtPlotCurve(tr("Other candidates"));
        _otherCandidateCurve->setStyle(QwtPlotCurve::NoCurve);
        _otherCandidateCurve->setSymbol(new QwtSymbol(
            QwtSymbol::Ellipse,
            QBrush(QColor(120, 135, 150, 105)),
            QPen(Qt::NoPen),
            QSize(3, 3)));
        _otherCandidateCurve->setRenderHint(QwtPlotItem::RenderAntialiased, false);
        _otherCandidateCurve->setZ(1);
        _otherCandidateCurve->attach(this);

        _hydrogenBondCurve = new QwtPlotCurve(tr("Hydrogen bonds"));
        _hydrogenBondCurve->setStyle(QwtPlotCurve::NoCurve);
        _hydrogenBondCurve->setSymbol(new QwtSymbol(
            QwtSymbol::Ellipse,
            QBrush(QColor(211, 55, 43, 220)),
            QPen(QColor(145, 32, 25), 0.5),
            QSize(4, 4)));
        _hydrogenBondCurve->setRenderHint(QwtPlotItem::RenderAntialiased, false);
        _hydrogenBondCurve->setZ(2);
        _hydrogenBondCurve->attach(this);

        auto* legend = new QwtPlotLegendItem();
        legend->setAlignmentInCanvas(Qt::AlignRight | Qt::AlignTop);
        legend->attach(this);
    }

    void clearPlot()
    {
        _otherCandidateCurve->setSamples(QVector<QPointF>());
        _hydrogenBondCurve->setSamples(QVector<QPointF>());
        setAxisScale(QwtAxis::YLeft, 0.0, 180.0);
        replot();
    }

    void setTable(DataOORef<const DataTable> table)
    {
        if(!table || !table->x() || !table->y()) {
            clearPlot();
            return;
        }

        BufferReadAccess<FloatType> distances(table->x());
        BufferReadAccess<FloatType> angles(table->y());
        BufferReadAccess<int64_t> isHydrogenBond(table->getProperty(QStringLiteral("Is hydrogen bond")));
        if(!distances || !angles || !isHydrogenBond
           || distances.size() != angles.size() || isHydrogenBond.size() != angles.size()) {
            clearPlot();
            return;
        }

        QVector<QPointF> otherCandidates;
        QVector<QPointF> hydrogenBonds;
        otherCandidates.reserve(static_cast<qsizetype>(angles.size()));
        hydrogenBonds.reserve(static_cast<qsizetype>(angles.size()));
        for(size_t index = 0; index < angles.size(); ++index) {
            if(!isFinite(distances[index]) || !isFinite(angles[index]))
                continue;
            const QPointF point(distances[index], angles[index]);
            if(isHydrogenBond[index])
                hydrogenBonds.push_back(point);
            else
                otherCandidates.push_back(point);
        }

        _otherCandidateCurve->setSamples(otherCandidates);
        _hydrogenBondCurve->setSamples(hydrogenBonds);
        setAxisTitle(QwtAxis::XBottom, table->axisLabelX());
        setAxisTitle(QwtAxis::YLeft, table->axisLabelY());
        setAxisAutoScale(QwtAxis::XBottom);
        setAxisScale(QwtAxis::YLeft, 0.0, 180.0);
        replot();
    }

private:

    QwtPlotCurve* _otherCandidateCurve = nullptr;
    QwtPlotCurve* _hydrogenBondCurve = nullptr;
};

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(HydrogenBondAnalysisModifier, HydrogenBondAnalysisModifierEditor);

/******************************************************************************
* Returns the modifier being edited.
******************************************************************************/
HydrogenBondAnalysisModifier* HydrogenBondAnalysisModifierEditor::modifier() const
{
    return static_object_cast<HydrogenBondAnalysisModifier>(editObject());
}

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Hydrogen bond analysis"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* participantBox = new QGroupBox(tr("Participants"), rollout);
    auto* participantLayout = new QGridLayout(participantBox);
    participantLayout->setContentsMargins(4, 4, 4, 4);
    participantLayout->setColumnStretch(1, 1);
    participantLayout->setVerticalSpacing(4);

    StringParameterUI* donorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorTypes));
    donorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    StringParameterUI* donorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorExpression));
    participantLayout->addWidget(new QLabel(tr("Donor atom type(s)"), participantBox), 0, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        donorTypesUI->textBox(),
        donorExpressionUI,
        tr("Donor expression override"),
        tr("Use this expression instead of the donor atom types. Leave it empty to use the type field again.")), 0, 1);

    StringParameterUI* hydrogenTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::hydrogenTypes));
    hydrogenTypesUI->lineEdit()->setPlaceholderText(tr("e.g. H or 1"));
    StringParameterUI* hydrogenExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::hydrogenExpression));
    participantLayout->addWidget(new QLabel(tr("Hydrogen atom type(s)"), participantBox), 1, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        hydrogenTypesUI->textBox(),
        hydrogenExpressionUI,
        tr("Hydrogen expression override"),
        tr("Use this expression instead of the hydrogen atom types. Leave it empty to use the type field again.")), 1, 1);

    StringParameterUI* acceptorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::acceptorTypes));
    acceptorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    StringParameterUI* acceptorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::acceptorExpression));
    participantLayout->addWidget(new QLabel(tr("Acceptor atom type(s)"), participantBox), 2, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        acceptorTypesUI->textBox(),
        acceptorExpressionUI,
        tr("Acceptor expression override"),
        tr("Use this expression instead of the acceptor atom types. Leave it empty to use the type field again.")), 2, 1);

    layout->addWidget(participantBox);

    auto* criteriaBox = new QGroupBox(tr("Criteria"), rollout);
    auto* criteriaLayout = new QGridLayout(criteriaBox);
    criteriaLayout->setContentsMargins(4, 4, 4, 4);
    criteriaLayout->setColumnStretch(1, 1);
    criteriaLayout->setVerticalSpacing(4);

    FloatParameterUI* donorHydrogenCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorHydrogenCutoff));
    criteriaLayout->addWidget(donorHydrogenCutoffUI->label(), 0, 0);
    criteriaLayout->addLayout(donorHydrogenCutoffUI->createFieldLayout(), 0, 1);

    VariantComboBoxParameterUI* definitionModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::definitionMode));
    definitionModeUI->comboBox()->addItem(tr("Fixed geometry"),
                                          QVariant::fromValue((int)HydrogenBondAnalysisModifier::FixedGeometry));
    definitionModeUI->comboBox()->addItem(tr("D/H/A site interaction energy"),
                                          QVariant::fromValue((int)HydrogenBondAnalysisModifier::SiteInteractionEnergy));
    definitionModeUI->comboBox()->addItem(tr("PMF-derived"),
                                          QVariant::fromValue((int)HydrogenBondAnalysisModifier::PMFDerived));
    criteriaLayout->addWidget(new QLabel(tr("Hydrogen-bond definition"), criteriaBox), 1, 0);
    criteriaLayout->addWidget(definitionModeUI->comboBox(), 1, 1);

    _fixedCriteriaWidget = new QWidget(criteriaBox);
    auto* fixedLayout = new QGridLayout(_fixedCriteriaWidget);
    fixedLayout->setContentsMargins(0, 0, 0, 0);
    fixedLayout->setColumnStretch(1, 1);
    fixedLayout->setVerticalSpacing(4);

    FloatParameterUI* donorAcceptorCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorAcceptorCutoff));
    fixedLayout->addWidget(donorAcceptorCutoffUI->label(), 0, 0);
    fixedLayout->addLayout(donorAcceptorCutoffUI->createFieldLayout(), 0, 1);

    FloatParameterUI* angleCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::angleCutoff));
    fixedLayout->addWidget(angleCutoffUI->label(), 1, 0);
    fixedLayout->addLayout(angleCutoffUI->createFieldLayout(), 1, 1);

    criteriaLayout->addWidget(_fixedCriteriaWidget, 2, 0, 1, 2);

    _siteEnergyCriteriaWidget = new QWidget(criteriaBox);
    auto* siteEnergyLayout = new QGridLayout(_siteEnergyCriteriaWidget);
    siteEnergyLayout->setContentsMargins(0, 0, 0, 0);
    siteEnergyLayout->setColumnStretch(1, 1);
    siteEnergyLayout->setVerticalSpacing(4);

    FloatParameterUI* siteEnergyDistanceMaximumUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::siteEnergyDistanceMaximum));
    siteEnergyLayout->addWidget(siteEnergyDistanceMaximumUI->label(), 0, 0);
    siteEnergyLayout->addLayout(siteEnergyDistanceMaximumUI->createFieldLayout(), 0, 1);

    FloatParameterUI* siteEnergyThetaMaximumUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::siteEnergyThetaMaximum));
    siteEnergyLayout->addWidget(siteEnergyThetaMaximumUI->label(), 1, 0);
    siteEnergyLayout->addLayout(siteEnergyThetaMaximumUI->createFieldLayout(), 1, 1);

    VariantComboBoxParameterUI* siteEnergyCutoffModeUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::siteEnergyCutoffMode));
    siteEnergyCutoffModeUI->comboBox()->addItem(
        tr("Automatic distribution minimum"),
        QVariant::fromValue((int)HydrogenBondAnalysisModifier::AutomaticEnergyMinimum));
    siteEnergyCutoffModeUI->comboBox()->addItem(
        tr("Manual"),
        QVariant::fromValue((int)HydrogenBondAnalysisModifier::ManualEnergyCutoff));
    siteEnergyLayout->addWidget(new QLabel(tr("Energy cutoff"), _siteEnergyCriteriaWidget), 2, 0);
    siteEnergyLayout->addWidget(siteEnergyCutoffModeUI->comboBox(), 2, 1);

    _manualSiteEnergyCutoffWidget = new QWidget(_siteEnergyCriteriaWidget);
    auto* manualSiteEnergyCutoffLayout = new QGridLayout(_manualSiteEnergyCutoffWidget);
    manualSiteEnergyCutoffLayout->setContentsMargins(0, 0, 0, 0);
    manualSiteEnergyCutoffLayout->setColumnStretch(1, 1);
    FloatParameterUI* siteEnergyCutoffUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::siteEnergyCutoff));
    manualSiteEnergyCutoffLayout->addWidget(siteEnergyCutoffUI->label(), 0, 0);
    manualSiteEnergyCutoffLayout->addLayout(siteEnergyCutoffUI->createFieldLayout(), 0, 1);
    siteEnergyLayout->addWidget(_manualSiteEnergyCutoffWidget, 3, 0, 1, 2);

    VariantComboBoxParameterUI* siteEnergyUnitUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::siteEnergyUnit));
    siteEnergyUnitUI->comboBox()->addItem(
        tr("kcal/mol (distance in angstrom, charge in e)"),
        QVariant::fromValue((int)HydrogenBondAnalysisModifier::KcalPerMol));
    siteEnergyUnitUI->comboBox()->addItem(
        tr("eV (distance in angstrom, charge in e)"),
        QVariant::fromValue((int)HydrogenBondAnalysisModifier::ElectronVolt));
    siteEnergyLayout->addWidget(new QLabel(tr("Energy unit"), _siteEnergyCriteriaWidget), 4, 0);
    siteEnergyLayout->addWidget(siteEnergyUnitUI->comboBox(), 4, 1);

    FloatParameterUI* relativePermittivityUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::relativePermittivity));
    siteEnergyLayout->addWidget(relativePermittivityUI->label(), 5, 0);
    siteEnergyLayout->addLayout(relativePermittivityUI->createFieldLayout(), 5, 1);

    FloatParameterUI* donorLJEpsilonUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorLJEpsilon));
    siteEnergyLayout->addWidget(donorLJEpsilonUI->label(), 6, 0);
    siteEnergyLayout->addLayout(donorLJEpsilonUI->createFieldLayout(), 6, 1);

    FloatParameterUI* donorLJSigmaUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::donorLJSigma));
    siteEnergyLayout->addWidget(donorLJSigmaUI->label(), 7, 0);
    siteEnergyLayout->addLayout(donorLJSigmaUI->createFieldLayout(), 7, 1);

    FloatParameterUI* hydrogenLJEpsilonUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::hydrogenLJEpsilon));
    siteEnergyLayout->addWidget(hydrogenLJEpsilonUI->label(), 8, 0);
    siteEnergyLayout->addLayout(hydrogenLJEpsilonUI->createFieldLayout(), 8, 1);

    FloatParameterUI* hydrogenLJSigmaUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::hydrogenLJSigma));
    siteEnergyLayout->addWidget(hydrogenLJSigmaUI->label(), 9, 0);
    siteEnergyLayout->addLayout(hydrogenLJSigmaUI->createFieldLayout(), 9, 1);

    FloatParameterUI* acceptorLJEpsilonUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::acceptorLJEpsilon));
    siteEnergyLayout->addWidget(acceptorLJEpsilonUI->label(), 10, 0);
    siteEnergyLayout->addLayout(acceptorLJEpsilonUI->createFieldLayout(), 10, 1);

    FloatParameterUI* acceptorLJSigmaUI = createParamUI<FloatParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::acceptorLJSigma));
    siteEnergyLayout->addWidget(acceptorLJSigmaUI->label(), 11, 0);
    siteEnergyLayout->addLayout(acceptorLJSigmaUI->createFieldLayout(), 11, 1);

    criteriaLayout->addWidget(_siteEnergyCriteriaWidget, 3, 0, 1, 2);

    _pmfCriteriaWidget = new QWidget(criteriaBox);
    auto* pmfLayout = new QGridLayout(_pmfCriteriaWidget);
    pmfLayout->setContentsMargins(0, 0, 0, 0);
    pmfLayout->setColumnStretch(1, 1);
    pmfLayout->setVerticalSpacing(4);

    FloatParameterUI* pmfDistanceMinimumUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceMinimum));
    pmfLayout->addWidget(pmfDistanceMinimumUI->label(), 0, 0);
    pmfLayout->addLayout(pmfDistanceMinimumUI->createFieldLayout(), 0, 1);

    FloatParameterUI* pmfDistanceMaximumUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceMaximum));
    pmfLayout->addWidget(pmfDistanceMaximumUI->label(), 1, 0);
    pmfLayout->addLayout(pmfDistanceMaximumUI->createFieldLayout(), 1, 1);

    FloatParameterUI* pmfThetaMinimumUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfThetaMinimum));
    pmfLayout->addWidget(pmfThetaMinimumUI->label(), 2, 0);
    pmfLayout->addLayout(pmfThetaMinimumUI->createFieldLayout(), 2, 1);

    FloatParameterUI* pmfThetaMaximumUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfThetaMaximum));
    pmfLayout->addWidget(pmfThetaMaximumUI->label(), 3, 0);
    pmfLayout->addLayout(pmfThetaMaximumUI->createFieldLayout(), 3, 1);

    IntegerParameterUI* pmfDistanceBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceBins));
    pmfLayout->addWidget(pmfDistanceBinsUI->label(), 4, 0);
    pmfLayout->addLayout(pmfDistanceBinsUI->createFieldLayout(), 4, 1);

    IntegerParameterUI* pmfAngleBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfAngleBins));
    pmfLayout->addWidget(pmfAngleBinsUI->label(), 5, 0);
    pmfLayout->addLayout(pmfAngleBinsUI->createFieldLayout(), 5, 1);

    FloatParameterUI* pmfDistanceBandwidthUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceBandwidth));
    pmfLayout->addWidget(pmfDistanceBandwidthUI->label(), 6, 0);
    pmfLayout->addLayout(pmfDistanceBandwidthUI->createFieldLayout(), 6, 1);

    FloatParameterUI* pmfAngleBandwidthUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfAngleBandwidth));
    pmfLayout->addWidget(pmfAngleBandwidthUI->label(), 7, 0);
    pmfLayout->addLayout(pmfAngleBandwidthUI->createFieldLayout(), 7, 1);

    FloatParameterUI* pmfReferenceShellFractionUI =
        createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfReferenceShellFraction));
    pmfLayout->addWidget(pmfReferenceShellFractionUI->label(), 8, 0);
    pmfLayout->addLayout(pmfReferenceShellFractionUI->createFieldLayout(), 8, 1);

    criteriaLayout->addWidget(_pmfCriteriaWidget, 4, 0, 1, 2);

    layout->addWidget(criteriaBox);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(HydrogenBondAnalysisModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);

    _runButton = new QPushButton(tr("Run hydrogen bond analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &HydrogenBondAnalysisModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    _geometryPlot = new HydrogenBondGeometryPlotWidget(rollout);
    _geometryPlot->setMinimumHeight(260);
    _geometryPlot->setMaximumHeight(260);
    layout->addWidget(_geometryPlot);

    _pmfPlot = new HydrogenBondPmfPlotWidget(rollout);
    _pmfPlot->setMinimumHeight(320);
    _pmfPlot->setMaximumHeight(320);
    layout->addWidget(_pmfPlot);

    _dataInspectorButton = new QPushButton(tr("Show in data inspector"), rollout);
    connect(_dataInspectorButton, &QPushButton::clicked, this, &HydrogenBondAnalysisModifierEditor::openDataInspector);
    layout->addWidget(_dataInspectorButton);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondAnalysisModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondAnalysisModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsChanged, this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);
    connect(definitionModeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);
    connect(siteEnergyCutoffModeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);

    updateDefinitionControls();
    updatePlot();
    updateSummary();
}

/******************************************************************************
* Launches a non-interactive evaluation of the hydrogen-bond modifier.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        HydrogenBondAnalysisModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        if(_runButton)
            _runButton->setEnabled(false);

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* hbNode = dynamic_object_cast<const HydrogenBondAnalysisModificationNode>(node);
        const int startedGenerationId = hbNode ? hbNode->cacheGenerationId() : 0;

        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running hydrogen bond analysis over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        SharedFuture<PipelineFlowState> future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<HydrogenBondAnalysisModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId,
                                              future](auto& task) noexcept {
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            HydrogenBondAnalysisModifier* mod = self->modifier();
            auto* hbNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(self->modificationNode());
            if(!mod || !hbNode || mod->runRequestId() != startedRunRequestId || hbNode->cacheGenerationId() != startedGenerationId)
                return;

            if(task.isCanceled() || task.exceptionStore())
                hbNode->setCompletedRunRequestId(startedRunRequestId);

            self->handleExceptions([&]() {
                (void)future.result();
                hbNode->pipelineCache().invalidateInteractiveState();
                hbNode->notifyDependents(ReferenceEvent::InteractiveStateAvailable);
                Q_EMIT self->pipelineOutputChanged();
                self->updatePlot();
                self->updateSummary();
            });

            if(self->_runButton)
                self->_runButton->setEnabled(true);
        });
    });
}

/******************************************************************************
* Updates the plot widget from the modifier output table.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_plot)
            return;
        if(hydrogenBondAnalysisIsIdle(modifier(), modificationNode())) {
            _plot->setTable(nullptr);
            if(_geometryPlot)
                _geometryPlot->clearPlot();
            if(_pmfPlot)
                _pmfPlot->clearPlot();
            return;
        }
        const PipelineFlowState& state = getPipelineOutput();
        const bool siteEnergyMode =
            modifier() && modifier()->definitionMode() == HydrogenBondAnalysisModifier::SiteInteractionEnergy;
        const QString plotTableId =
            siteEnergyMode
                ? HydrogenBondAnalysisModifier::siteEnergyDistributionTableId()
                : HydrogenBondAnalysisModifier::countTableId();
        _plot->setTable(state.getObjectBy<DataTable>(modificationNode(), plotTableId));
        if(_geometryPlot) {
            _geometryPlot->setTable(
                state.getObjectBy<DataTable>(
                    modificationNode(),
                    HydrogenBondAnalysisModifier::geometryClassificationTableId()));
        }

        if(_pmfPlot) {
            if(modifier() && modifier()->definitionMode() == HydrogenBondAnalysisModifier::PMFDerived) {
                const QVariant distanceMaximumAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_maximum"));
                const QVariant distanceMinimumAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_minimum"));
                const QVariant distanceBinsAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_bins"));
                const QVariant thetaMinimumAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_minimum"));
                const QVariant thetaMaximumAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_maximum"));
                const QVariant angleBinsAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_angle_bins"));
                const QVariant boundaryAttr =
                    state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));

                _pmfPlot->setPmfTable(
                    state.getObjectBy<DataTable>(modificationNode(), HydrogenBondAnalysisModifier::pmfTableId()),
                    distanceMinimumAttr.isValid() ? distanceMinimumAttr.toDouble() : 0.0,
                    distanceMaximumAttr.isValid() ? distanceMaximumAttr.toDouble() : 0.0,
                    thetaMinimumAttr.isValid() ? thetaMinimumAttr.toDouble() : 0.0,
                    thetaMaximumAttr.isValid() ? thetaMaximumAttr.toDouble() : 180.0,
                    distanceBinsAttr.isValid() ? distanceBinsAttr.toInt() : 0,
                    angleBinsAttr.isValid() ? angleBinsAttr.toInt() : 0,
                    boundaryAttr.isValid() ? boundaryAttr.toDouble() : std::numeric_limits<double>::quiet_NaN());
            }
            else {
                _pmfPlot->clearPlot();
            }
        }
    });
}

/******************************************************************************
* Updates the summary label from the generated global attributes.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        if(hydrogenBondAnalysisIsIdle(modifier(), modificationNode())) {
            _summaryLabel->clear();
            refreshSummaryGeometry();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString donors = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_types")).toString()
                : expression;
        }();
        const QString hydrogens = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hydrogen_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hydrogen_types")).toString()
                : expression;
        }();
        const QString acceptors = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.acceptor_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.acceptor_types")).toString()
                : expression;
        }();
        const QString definitionMode = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.definition_mode")).toString();
        const QString pairingMode = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_hydrogen_pairing_mode")).toString();
        const QVariant donorHydrogenCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"));
        const QVariant donorAcceptorCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hb_donor_acceptor_cutoff"));
        const QVariant angleCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hb_theta_maximum"));
        const QVariant siteEnergyDistanceMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_distance_maximum"));
        const QVariant siteEnergyThetaMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_theta_maximum"));
        const QString siteEnergyCutoffMode = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_cutoff_mode")).toString();
        const QVariant siteEnergyCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_cutoff"));
        const QString siteEnergyUnit = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_unit")).toString();
        const QString siteEnergyModel = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_model")).toString();
        const QVariant siteEnergyCalibrationSampleCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_calibration_sample_count"));
        const QVariant siteEnergyCalibrationBandwidth = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_calibration_bandwidth"));
        const QVariant siteEnergyLowerPeak = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_lower_peak"));
        const QVariant siteEnergyUpperPeak = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_upper_peak"));
        const QVariant siteEnergyValleyRatio = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_valley_ratio"));
        const QVariant siteEnergyLowerFraction = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_lower_fraction"));
        const QVariant siteEnergyAverage = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_average"));
        const QVariant siteEnergyMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_minimum"));
        const QVariant siteEnergyMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.site_energy_maximum"));
        const QVariant pmfDistanceMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_minimum"));
        const QVariant pmfDistanceMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_maximum"));
        const QVariant pmfThetaMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_minimum"));
        const QVariant pmfThetaMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_maximum"));
        const QVariant pmfDistanceBandwidth = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_bandwidth"));
        const QVariant pmfAngleBandwidth = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_angle_bandwidth"));
        const QVariant pmfReferenceShellFraction = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_reference_shell_fraction"));
        const QVariant pmfReferenceDistanceMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_reference_distance_minimum"));
        const QVariant pmfBoundary = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
        const QVariant pmfVicinity = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"));
        const QVariant pmfBasinBins = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_basin_bin_count"));
        const QVariant pmfMinimumFreeEnergy = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_minimum_free_energy"));
        const QVariant pmfMinimumDistance = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_minimum_distance"));
        const QVariant pmfMinimumTheta = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_minimum_theta"));
        const QVariant pmfMinimumRequiredWellDepth = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_minimum_required_well_depth"));
        const QVariant sampledFrameCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.sampled_frame_count"));
        const QVariant totalObservations = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.total_observations"));
        const QVariant averageCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.average_count"));
        const QVariant maximumCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.maximum_count"));

        QStringList lines;
        if(!donors.isEmpty() || !hydrogens.isEmpty() || !acceptors.isEmpty())
            lines << tr("Donor selector: %1\nHydrogen selector: %2\nAcceptor selector: %3").arg(donors, hydrogens, acceptors);
        if(!definitionMode.isEmpty())
            lines << tr("Hydrogen-bond definition: %1").arg(definitionMode);
        if(!pairingMode.isEmpty())
            lines << tr("Donor-hydrogen pairing mode: %1").arg(pairingMode);
        if(donorHydrogenCutoff.isValid())
            lines << tr("Donor-hydrogen cutoff: %1").arg(donorHydrogenCutoff.toDouble(), 0, 'g', 6);
        if(donorAcceptorCutoff.isValid())
            lines << tr("Donor-acceptor cutoff: %1").arg(donorAcceptorCutoff.toDouble(), 0, 'g', 6);
        if(angleCutoff.isValid())
            lines << tr("HB theta maximum: %1").arg(angleCutoff.toDouble(), 0, 'g', 6);
        if(siteEnergyDistanceMaximum.isValid())
            lines << tr("Candidate D-A cutoff: %1").arg(siteEnergyDistanceMaximum.toDouble(), 0, 'g', 6);
        if(siteEnergyThetaMaximum.isValid())
            lines << tr("Candidate theta maximum: %1").arg(siteEnergyThetaMaximum.toDouble(), 0, 'g', 6);
        if(!siteEnergyCutoffMode.isEmpty())
            lines << tr("Energy cutoff mode: %1").arg(siteEnergyCutoffMode);
        if(!siteEnergyModel.isEmpty())
            lines << tr("Energy model: %1\nLocal D/H/A score; not the full molecular-pair interaction energy.")
                         .arg(siteEnergyModel);
        if(siteEnergyCutoff.isValid())
            lines << tr("Effective maximum D/H/A site energy: %1 %2")
                         .arg(siteEnergyCutoff.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyUnit);
        if(siteEnergyCalibrationSampleCount.isValid())
            lines << tr("Cutoff calibration candidates: %1").arg(siteEnergyCalibrationSampleCount.toLongLong());
        if(siteEnergyLowerPeak.isValid() && siteEnergyUpperPeak.isValid()) {
            lines << tr("Resolved energy peaks: %1 and %2 %3")
                         .arg(siteEnergyLowerPeak.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyUpperPeak.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyUnit);
        }
        if(siteEnergyCalibrationBandwidth.isValid())
            lines << tr("Energy-PDF bandwidth: %1 %2")
                         .arg(siteEnergyCalibrationBandwidth.toDouble(), 0, 'g', 6)
                         .arg(siteEnergyUnit);
        if(siteEnergyValleyRatio.isValid())
            lines << tr("Valley/peak density ratio: %1").arg(siteEnergyValleyRatio.toDouble(), 0, 'f', 3);
        if(siteEnergyLowerFraction.isValid())
            lines << tr("Lower-energy population: %1%").arg(100.0 * siteEnergyLowerFraction.toDouble(), 0, 'f', 1);
        if(siteEnergyAverage.isValid()) {
            lines << tr("Accepted site energy: mean %1, range %2 to %3 %4")
                         .arg(siteEnergyAverage.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyMinimum.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyMaximum.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyUnit);
        }
        if(pmfDistanceMinimum.isValid())
            lines << tr("PMF distance minimum: %1").arg(pmfDistanceMinimum.toDouble(), 0, 'g', 6);
        if(pmfDistanceMaximum.isValid())
            lines << tr("PMF distance maximum: %1").arg(pmfDistanceMaximum.toDouble(), 0, 'g', 6);
        if(pmfThetaMinimum.isValid())
            lines << tr("PMF theta minimum: %1").arg(pmfThetaMinimum.toDouble(), 0, 'g', 6);
        if(pmfThetaMaximum.isValid())
            lines << tr("PMF theta maximum: %1").arg(pmfThetaMaximum.toDouble(), 0, 'g', 6);
        if(pmfDistanceBandwidth.isValid() && pmfAngleBandwidth.isValid()) {
            lines << tr("PMF smoothing bandwidths: distance %1, theta %2 degrees")
                         .arg(pmfDistanceBandwidth.toDouble(), 0, 'g', 6)
                         .arg(pmfAngleBandwidth.toDouble(), 0, 'g', 6);
        }
        if(pmfReferenceDistanceMinimum.isValid() && pmfDistanceMaximum.isValid()) {
            lines << tr("Noninteracting reference shell: r >= %1 (outer %2%)")
                         .arg(pmfReferenceDistanceMinimum.toDouble(), 0, 'g', 6)
                         .arg(100.0 * pmfReferenceShellFraction.toDouble(), 0, 'f', 1);
        }
        if(pmfBoundary.isValid())
            lines << tr("PMF basin boundary: W = %1 kBT").arg(pmfBoundary.toDouble(), 0, 'f', 4);
        if(pmfMinimumFreeEnergy.isValid() && pmfMinimumDistance.isValid() && pmfMinimumTheta.isValid()) {
            lines << tr("PMF minimum: %1 kBT at r=%2, theta=%3 degrees")
                         .arg(pmfMinimumFreeEnergy.toDouble(), 0, 'f', 4)
                         .arg(pmfMinimumDistance.toDouble(), 0, 'g', 6)
                         .arg(pmfMinimumTheta.toDouble(), 0, 'g', 6);
        }
        if(pmfMinimumRequiredWellDepth.isValid())
            lines << tr("Minimum resolved PMF-well depth: %1 kBT").arg(pmfMinimumRequiredWellDepth.toDouble(), 0, 'f', 2);
        if(pmfVicinity.isValid())
            lines << tr("Derived vicinity cutoff: %1").arg(pmfVicinity.toDouble(), 0, 'f', 4);
        if(pmfBasinBins.isValid())
            lines << tr("PMF basin bins: %1").arg(pmfBasinBins.toLongLong());
        if(sampledFrameCount.isValid())
            lines << tr("Sampled frames: %1").arg(sampledFrameCount.toInt());
        if(totalObservations.isValid())
            lines << tr("Total hydrogen bonds: %1").arg(totalObservations.toLongLong());
        if(averageCount.isValid())
            lines << tr("Average hydrogen bonds per sampled frame: %1").arg(averageCount.toDouble(), 0, 'f', 3);
        if(maximumCount.isValid())
            lines << tr("Maximum hydrogen bonds in a sampled frame: %1").arg(maximumCount.toInt());

        _summaryLabel->setText(lines.join(QLatin1Char('\n')));
        refreshSummaryGeometry();
    });
}

void HydrogenBondAnalysisModifierEditor::updateDefinitionControls()
{
    const HydrogenBondAnalysisModifier* mod = modifier();
    if(!mod)
        return;

    const bool fixedVisible = mod->definitionMode() == HydrogenBondAnalysisModifier::FixedGeometry;
    const bool siteEnergyVisible = mod->definitionMode() == HydrogenBondAnalysisModifier::SiteInteractionEnergy;
    const bool manualSiteEnergyCutoffVisible =
        siteEnergyVisible && mod->siteEnergyCutoffMode() == HydrogenBondAnalysisModifier::ManualEnergyCutoff;
    const bool pmfVisible = mod->definitionMode() == HydrogenBondAnalysisModifier::PMFDerived;

    if(_fixedCriteriaWidget)
        _fixedCriteriaWidget->setVisible(fixedVisible);
    if(_siteEnergyCriteriaWidget)
        _siteEnergyCriteriaWidget->setVisible(siteEnergyVisible);
    if(_manualSiteEnergyCutoffWidget)
        _manualSiteEnergyCutoffWidget->setVisible(manualSiteEnergyCutoffVisible);
    if(_pmfCriteriaWidget)
        _pmfCriteriaWidget->setVisible(pmfVisible);
    if(_pmfPlot)
        _pmfPlot->setVisible(pmfVisible);

    const std::array<QWidget*, 5> widgets = {
        _fixedCriteriaWidget.data(),
        _siteEnergyCriteriaWidget.data(),
        _manualSiteEnergyCutoffWidget.data(),
        _pmfCriteriaWidget.data(),
        static_cast<QWidget*>(_pmfPlot.data())
    };
    for(QWidget* widget : widgets) {
        if(!widget)
            continue;
        widget->updateGeometry();
        for(QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
            if(QLayout* layout = parent->layout()) {
                layout->invalidate();
                layout->activate();
            }
            parent->updateGeometry();
        }
    }
    updatePlot();
}

/******************************************************************************
* Opens the primary output table for the active hydrogen-bond definition.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::openDataInspector()
{
    handleExceptions([&]() {
        HydrogenBondAnalysisModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node || !mod->isEnabled()) {
            QToolTip::showText(
                _dataInspectorButton->mapToGlobal(QPoint(0, _dataInspectorButton->height() / 2)),
                tr("No results available, because the modifier is turned off."),
                container(),
                container()->rect(),
                3000);
            return;
        }

        QString tableId;
        switch(mod->definitionMode()) {
        case HydrogenBondAnalysisModifier::FixedGeometry:
            tableId = HydrogenBondAnalysisModifier::countTableId();
            break;
        case HydrogenBondAnalysisModifier::PMFDerived:
            tableId = HydrogenBondAnalysisModifier::pmfTableId();
            break;
        case HydrogenBondAnalysisModifier::SiteInteractionEnergy:
            tableId = HydrogenBondAnalysisModifier::siteEnergyDistributionTableId();
            break;
        }

        if(!ui().mainWindow()->openDataInspector(node, tableId, 1)) {
            QToolTip::showText(
                _dataInspectorButton->mapToGlobal(QPoint(0, _dataInspectorButton->height() / 2)),
                tr("Results not available yet. Run the analysis first."),
                container(),
                container()->rect(),
                3000);
        }
        else {
            QToolTip::hideText();
        }
    });
}

/******************************************************************************
* Reflows the wrapped summary label after changing its contents.
******************************************************************************/
void HydrogenBondAnalysisModifierEditor::refreshSummaryGeometry()
{
    if(!_summaryLabel)
        return;

    _summaryLabel->updateGeometry();
    _summaryLabel->adjustSize();
    for(QWidget* widget = _summaryLabel->parentWidget(); widget; widget = widget->parentWidget()) {
        if(QLayout* layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        widget->updateGeometry();
    }
}

}  // namespace Ovito
