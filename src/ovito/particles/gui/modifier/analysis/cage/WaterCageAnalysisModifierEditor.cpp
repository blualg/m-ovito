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
#include <ovito/particles/modifier/analysis/cage/WaterCageAnalysisModifier.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include "WaterCageAnalysisModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WaterCageAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(WaterCageAnalysisModifier, WaterCageAnalysisModifierEditor);

void WaterCageAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Water cage analysis"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto* selectorBox = new QGroupBox(tr("Water network"));
    auto* selectorLayout = new QGridLayout(selectorBox);
    selectorLayout->setContentsMargins(6, 6, 6, 6);
    selectorLayout->setColumnStretch(1, 1);

    auto* oxygenTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::oxygenTypes));
    oxygenTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or 2"));
    auto* oxygenExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::oxygenExpression));
    selectorLayout->addWidget(new QLabel(tr("Water oxygen atom type(s)")), 0, 0);
    selectorLayout->addWidget(createSelectorPopupRow(
        rollout,
        oxygenTypesUI->textBox(),
        oxygenExpressionUI,
        tr("Water oxygen expression override"),
        tr("Use this expression instead of the water oxygen atom type list.")), 0, 1);

    auto* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::oxygenNeighborCutoff));
    selectorLayout->addWidget(new QLabel(tr("O-O neighbor cutoff")), 1, 0);
    selectorLayout->addLayout(cutoffUI->createFieldLayout(), 1, 1);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::onlySelectedParticles));
    selectorLayout->addWidget(onlySelectedUI->checkBox(), 2, 0, 1, 2);
    layout->addWidget(selectorBox);

    auto* cageBox = new QGroupBox(tr("Cage types"));
    auto* cageLayout = new QVBoxLayout(cageBox);
    cageLayout->setContentsMargins(6, 6, 6, 6);
    cageLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::find512Cages))->checkBox());
    cageLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::find51262Cages))->checkBox());
    cageLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::find51264Cages))->checkBox());
    cageLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::findGeneralCompleteCages))->checkBox());
    cageLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::findOpenPartialCageCandidates))->checkBox());
    layout->addWidget(cageBox);

    auto* searchBox = new QGroupBox(tr("Search"));
    auto* searchLayout = new QGridLayout(searchBox);
    searchLayout->setContentsMargins(6, 6, 6, 6);
    searchLayout->setColumnStretch(1, 1);
    auto* maxSearchUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::maximumSearchStates));
    searchLayout->addWidget(new QLabel(tr("Maximum ring/search states")), 0, 0);
    searchLayout->addLayout(maxSearchUI->createFieldLayout(), 0, 1);
    auto* minGeneralRingUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::minimumGeneralRingSize));
    searchLayout->addWidget(new QLabel(tr("Minimum general ring size")), 1, 0);
    searchLayout->addLayout(minGeneralRingUI->createFieldLayout(), 1, 1);
    auto* maxGeneralRingUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::maximumGeneralRingSize));
    searchLayout->addWidget(new QLabel(tr("Maximum general ring size")), 2, 0);
    searchLayout->addLayout(maxGeneralRingUI->createFieldLayout(), 2, 1);
    auto* maxGeneralFacesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::maximumGeneralCageFaces));
    searchLayout->addWidget(new QLabel(tr("Maximum general cage faces")), 3, 0);
    searchLayout->addLayout(maxGeneralFacesUI->createFieldLayout(), 3, 1);
    auto* maxMissingCandidateFacesUI =
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::maximumCandidateMissingFaces));
    searchLayout->addWidget(new QLabel(tr("Maximum missing candidate faces")), 4, 0);
    searchLayout->addLayout(maxMissingCandidateFacesUI->createFieldLayout(), 4, 1);
    auto* distortedThresholdUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::distortedCageThreshold));
    searchLayout->addWidget(new QLabel(tr("Distorted cage edge-CV threshold")), 5, 0);
    searchLayout->addLayout(distortedThresholdUI->createFieldLayout(), 5, 1);
    auto* maxCandidateFragmentsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::maximumCandidateFragments));
    searchLayout->addWidget(new QLabel(tr("Maximum candidate fragments")), 6, 0);
    searchLayout->addLayout(maxCandidateFragmentsUI->createFieldLayout(), 6, 1);
    searchLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WaterCageAnalysisModifier::createCageVisualization))->checkBox(), 7, 0, 1, 2);
    layout->addWidget(searchBox);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    _summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(_summaryLabel);

    layout->addWidget(new QLabel(tr("Cage counts:")));
    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(160);
    _plotWidget->setMaximumHeight(160);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show in data inspector"), WaterCageAnalysisModifier::CageCountTableIdentifier, 1));

    ObjectStatusDisplay* statusDisplay = createParamUI<ObjectStatusDisplay>();
    statusDisplay->statusWidget()->setMinimumHeight(64);
    statusDisplay->statusWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(statusDisplay->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &WaterCageAnalysisModifierEditor::plotCounts);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &WaterCageAnalysisModifierEditor::updateSummary);
}

void WaterCageAnalysisModifierEditor::plotCounts()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), WaterCageAnalysisModifier::CageCountTableIdentifier);
        _plotWidget->setTable(std::move(table));
    });
}

void WaterCageAnalysisModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        const PipelineFlowState& state = getPipelineOutput();
        const QVariant total = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.total_cages"));
        if(!total.isValid()) {
            _summaryLabel->setText(tr("No water cage results for the current frame."));
            return;
        }

        const int count512 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_5_12")).toInt();
        const int count51262 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_5_12_6_2")).toInt();
        const int count51264 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_5_12_6_4")).toInt();
        const int countGeneral = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_general")).toInt();
        const int countCandidates = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_candidates")).toInt();
        const int countPartialCandidates =
            state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_partial_candidates")).toInt();
        const int countDistortedCandidates =
            state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.count_distorted_candidates")).toInt();
        const int rejectedOverlapCandidates =
            state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.rejected_overlap_candidates")).toInt();
        const int rejectedPlanarCandidates =
            state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.rejected_planar_candidates")).toInt();
        const int ring4 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.ring_4_count")).toInt();
        const int ring5 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.ring_5_count")).toInt();
        const int ring6 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.ring_6_count")).toInt();
        const int ring7 = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.ring_7_count")).toInt();
        const int oxygenCount = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.oxygen_count")).toInt();
        const int edgeCount = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.oo_edges")).toInt();
        const double averageOxygenDegree =
            state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.average_oxygen_degree")).toDouble();
        const int visualizationParticles = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.visualization_particles")).toInt();
        const int visualizationBonds = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.visualization_bonds")).toInt();
        const bool truncated = state.getAttributeValue(modificationNode(), QStringLiteral("WaterCage.search_truncated")).toInt() != 0;

        QString summary = tr("Strict cages: %1 total (%2 5^12, %3 5^12 6^2, %4 5^12 6^4, %5 general/other). Candidate cages: %6 (%7 open/partial, %8 distorted complete). Network: %9 oxygens, %10 O-O edges, average degree %11, %12 four-rings, %13 five-rings, %14 six-rings, %15 seven-rings. Visualization: %16 particles, %17 cage bonds.")
                              .arg(total.toInt())
                              .arg(count512)
                              .arg(count51262)
                              .arg(count51264)
                              .arg(countGeneral)
                              .arg(countCandidates)
                              .arg(countPartialCandidates)
                              .arg(countDistortedCandidates)
                              .arg(oxygenCount)
                              .arg(edgeCount)
                              .arg(averageOxygenDegree, 0, 'f', 2)
                              .arg(ring4)
                              .arg(ring5)
                              .arg(ring6)
                              .arg(ring7)
                              .arg(visualizationParticles)
                              .arg(visualizationBonds);
        if(truncated)
            summary += tr(" Search was truncated by the state limit.");
        if(averageOxygenDegree > 6.0)
            summary += tr(" The O-O network is overconnected; reduce the cutoff to the first-neighbor shell.");
        if(rejectedOverlapCandidates > 0 || rejectedPlanarCandidates > 0)
            summary += tr(" Candidate filter rejected %1 strict-overlap fragment(s) and %2 near-planar sheet fragment(s).")
                           .arg(rejectedOverlapCandidates)
                           .arg(rejectedPlanarCandidates);
        _summaryLabel->setText(summary);
    });
}

}  // namespace Ovito
