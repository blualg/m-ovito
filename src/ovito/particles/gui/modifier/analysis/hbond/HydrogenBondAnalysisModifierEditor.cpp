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
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <qwt/qwt_color_map.h>
#include <qwt/qwt_interval.h>
#include <qwt/qwt_matrix_raster_data.h>
#include <qwt/qwt_plot_grid.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_plot_marker.h>
#include <qwt/qwt_plot_shapeitem.h>
#include <qwt/qwt_plot_spectrogram.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_text.h>
#include <QPainterPath>
#include <QPointer>
#include <QVector>
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
                                               const BufferReadAccess<int64_t>& inBasin)
{
    BasinBoundaryOverlay overlay;
    if(distanceBins <= 0 || angleBins <= 0 || distanceMaximum <= distanceMinimum || thetaMaximum <= thetaMinimum)
        return overlay;

    const double distanceBinWidth = (distanceMaximum - distanceMinimum) / static_cast<double>(distanceBins);
    const double angleBinWidth = (thetaMaximum - thetaMinimum) / static_cast<double>(angleBins);

    double topmostLabelY = -std::numeric_limits<double>::infinity();
    double centeredLabelDistance = 0.0;
    const double distanceCenter = 0.5 * (distanceMinimum + distanceMaximum);

    for(int distanceBin = 0; distanceBin < distanceBins; ++distanceBin) {
        const double x0 = distanceMinimum + static_cast<double>(distanceBin) * distanceBinWidth;
        const double x1 = distanceMinimum + static_cast<double>(distanceBin + 1) * distanceBinWidth;
        for(int angleBin = 0; angleBin < angleBins; ++angleBin) {
            const size_t linearIndex = static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins)
                                     + static_cast<size_t>(angleBin);
            if(!inBasin[linearIndex])
                continue;

            const double y0 = thetaMinimum + static_cast<double>(angleBin) * angleBinWidth;
            const double y1 = thetaMinimum + static_cast<double>(angleBin + 1) * angleBinWidth;
            const auto addSegment = [&](double sx0, double sy0, double sx1, double sy1) {
                overlay.path.moveTo(sx0, sy0);
                overlay.path.lineTo(sx1, sy1);
                const double segmentMidY = 0.5 * (sy0 + sy1);
                const double segmentMidX = 0.5 * (sx0 + sx1);
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

            const bool leftOutside = (distanceBin == 0)
                || !inBasin[static_cast<size_t>(distanceBin - 1) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin)];
            const bool rightOutside = (distanceBin == distanceBins - 1)
                || !inBasin[static_cast<size_t>(distanceBin + 1) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin)];
            const bool bottomOutside = (angleBin == 0)
                || !inBasin[static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin - 1)];
            const bool topOutside = (angleBin == angleBins - 1)
                || !inBasin[static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins) + static_cast<size_t>(angleBin + 1)];

            if(leftOutside)
                addSegment(x0, y0, x0, y1);
            if(rightOutside)
                addSegment(x1, y0, x1, y1);
            if(bottomOutside)
                addSegment(x0, y0, x1, y0);
            if(topOutside)
                addSegment(x0, y1, x1, y1);
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

        setTitle(tr("PMF(r, theta)"));
        setAxisTitle(QwtAxis::XBottom, tr("r (A)"));
        setAxisTitle(QwtAxis::YLeft, tr("theta (degrees)"));
        setAxisTitle(QwtAxis::YRight, tr("kcal/mol"));
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
        rasterData->setResampleMode(QwtMatrixRasterData::NearestNeighbour);
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
                buildBasinBoundaryOverlay(distanceMinimum, distanceMaximum, thetaMinimum, thetaMaximum, distanceBins, angleBins, inBasin);
            _boundaryShape->setShape(overlay.path);
            if(overlay.hasLabelPoint) {
                QwtText label(QString::number(boundaryFreeEnergy, 'g', 4));
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

    criteriaLayout->addWidget(_pmfCriteriaWidget, 3, 0, 1, 2);

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

    _summaryLabel = new QLabel(tr("Hydrogen bond results are idle. Open the Run section and click 'Run hydrogen bond analysis' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    _pmfPlot = new HydrogenBondPmfPlotWidget(rollout);
    _pmfPlot->setMinimumHeight(320);
    _pmfPlot->setMaximumHeight(320);
    layout->addWidget(_pmfPlot);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondAnalysisModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondAnalysisModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsChanged, this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &HydrogenBondAnalysisModifierEditor::updateDefinitionControls);
    connect(definitionModeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
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

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* hbNode = dynamic_object_cast<const HydrogenBondAnalysisModificationNode>(node);
        const int startedGenerationId = hbNode ? hbNode->cacheGenerationId() : 0;

        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running hydrogen bond analysis over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<HydrogenBondAnalysisModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId](auto& task) noexcept {
            if(!task.isCanceled() && !task.exceptionStore())
                return;
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            HydrogenBondAnalysisModifier* mod = self->modifier();
            auto* hbNode = dynamic_object_cast<HydrogenBondAnalysisModificationNode>(self->modificationNode());
            if(!mod || !hbNode || mod->runRequestId() != startedRunRequestId || hbNode->cacheGenerationId() != startedGenerationId)
                return;

            hbNode->setCompletedRunRequestId(startedRunRequestId);
            self->updatePlot();
            self->updateSummary();
        });
        scheduleOperationAfter(std::move(future), [this, startedRunRequestId, startedGenerationId](const PipelineFlowState&) {
            HydrogenBondAnalysisModifier* mod = modifier();
            const auto* hbNode = dynamic_object_cast<const HydrogenBondAnalysisModificationNode>(modificationNode());
            if(!mod || !hbNode || mod->runRequestId() != startedRunRequestId || hbNode->cacheGenerationId() != startedGenerationId)
                return;
            updatePlot();
            updateSummary();
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
            if(_pmfPlot)
                _pmfPlot->clearPlot();
            return;
        }
        const PipelineFlowState& state = getPipelineOutput();
        _plot->setTable(state.getObjectBy<DataTable>(
            modificationNode(),
            HydrogenBondAnalysisModifier::countTableId()));

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
            _summaryLabel->setText(tr(
                "Hydrogen bond results are idle. Open the Run section and click 'Run hydrogen bond analysis' to compute the selected observable."));
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
        const QVariant pmfDistanceMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_minimum"));
        const QVariant pmfDistanceMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_maximum"));
        const QVariant pmfThetaMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_minimum"));
        const QVariant pmfThetaMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_theta_maximum"));
        const QVariant pmfBoundary = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
        const QVariant pmfVicinity = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"));
        const QVariant pmfBasinBins = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_basin_bin_count"));
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
        if(pmfDistanceMinimum.isValid())
            lines << tr("PMF distance minimum: %1").arg(pmfDistanceMinimum.toDouble(), 0, 'g', 6);
        if(pmfDistanceMaximum.isValid())
            lines << tr("PMF distance maximum: %1").arg(pmfDistanceMaximum.toDouble(), 0, 'g', 6);
        if(pmfThetaMinimum.isValid())
            lines << tr("PMF theta minimum: %1").arg(pmfThetaMinimum.toDouble(), 0, 'g', 6);
        if(pmfThetaMaximum.isValid())
            lines << tr("PMF theta maximum: %1").arg(pmfThetaMaximum.toDouble(), 0, 'g', 6);
        if(pmfBoundary.isValid())
            lines << tr("PMF basin boundary free energy: %1").arg(pmfBoundary.toDouble(), 0, 'f', 4);
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

        _summaryLabel->setText(lines.join(QStringLiteral("\n\n")));
        refreshSummaryGeometry();
    });
}

void HydrogenBondAnalysisModifierEditor::updateDefinitionControls()
{
    const HydrogenBondAnalysisModifier* mod = modifier();
    if(!mod)
        return;

    const bool fixedVisible = mod->definitionMode() == HydrogenBondAnalysisModifier::FixedGeometry;
    const bool pmfVisible = mod->definitionMode() == HydrogenBondAnalysisModifier::PMFDerived;

    if(_fixedCriteriaWidget)
        _fixedCriteriaWidget->setVisible(fixedVisible);
    if(_pmfCriteriaWidget)
        _pmfCriteriaWidget->setVisible(pmfVisible);
    if(_pmfPlot)
        _pmfPlot->setVisible(pmfVisible);

    const std::array<QWidget*, 3> widgets = {
        _fixedCriteriaWidget.data(),
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
