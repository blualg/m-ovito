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
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <QPointer>
#include "HydrogenBondAnalysisModifierEditor.h"

namespace Ovito {

namespace {

bool hydrogenBondAnalysisIsIdle(const HydrogenBondAnalysisModifier* modifier, const ModificationNode* node)
{
    const auto* hbNode = dynamic_object_cast<const HydrogenBondAnalysisModificationNode>(node);
    return modifier && hbNode && !hbNode->hasCachedResults() && modifier->runRequestId() <= hbNode->completedRunRequestId();
}

}

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
    participantLayout->addWidget(new QLabel(tr("Donor atom type(s)"), participantBox), 0, 0);
    participantLayout->addWidget(donorTypesUI->textBox(), 0, 1);

    StringParameterUI* hydrogenTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::hydrogenTypes));
    hydrogenTypesUI->lineEdit()->setPlaceholderText(tr("e.g. H or 1"));
    participantLayout->addWidget(new QLabel(tr("Hydrogen atom type(s)"), participantBox), 1, 0);
    participantLayout->addWidget(hydrogenTypesUI->textBox(), 1, 1);

    StringParameterUI* acceptorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::acceptorTypes));
    acceptorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    participantLayout->addWidget(new QLabel(tr("Acceptor atom type(s)"), participantBox), 2, 0);
    participantLayout->addWidget(acceptorTypesUI->textBox(), 2, 1);

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

    FloatParameterUI* pmfDistanceMaximumUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceMaximum));
    pmfLayout->addWidget(pmfDistanceMaximumUI->label(), 0, 0);
    pmfLayout->addLayout(pmfDistanceMaximumUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* pmfDistanceBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfDistanceBins));
    pmfLayout->addWidget(pmfDistanceBinsUI->label(), 1, 0);
    pmfLayout->addLayout(pmfDistanceBinsUI->createFieldLayout(), 1, 1);

    IntegerParameterUI* pmfAngleBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondAnalysisModifier::pmfAngleBins));
    pmfLayout->addWidget(pmfAngleBinsUI->label(), 2, 0);
    pmfLayout->addLayout(pmfAngleBinsUI->createFieldLayout(), 2, 1);

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
            return;
        }
        _plot->setTable(getPipelineOutput().getObjectBy<DataTable>(
            modificationNode(),
            HydrogenBondAnalysisModifier::countTableId()));
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
        const QString donors = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_types")).toString();
        const QString hydrogens = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hydrogen_types")).toString();
        const QString acceptors = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.acceptor_types")).toString();
        const QString definitionMode = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.definition_mode")).toString();
        const QString pairingMode = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_hydrogen_pairing_mode")).toString();
        const QVariant donorHydrogenCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.donor_hydrogen_cutoff"));
        const QVariant donorAcceptorCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hb_donor_acceptor_cutoff"));
        const QVariant angleCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.hb_dha_minimum_angle"));
        const QVariant pmfDistanceMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_distance_maximum"));
        const QVariant pmfBoundary = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_boundary_free_energy"));
        const QVariant pmfVicinity = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_vicinity_cutoff"));
        const QVariant pmfBasinBins = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.pmf_basin_bin_count"));
        const QVariant sampledFrameCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.sampled_frame_count"));
        const QVariant totalObservations = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.total_observations"));
        const QVariant averageCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.average_count"));
        const QVariant maximumCount = state.getAttributeValue(modificationNode(), QStringLiteral("HydrogenBonds.maximum_count"));

        QStringList lines;
        if(!donors.isEmpty() || !hydrogens.isEmpty() || !acceptors.isEmpty())
            lines << tr("Donor atom type(s): %1\nHydrogen atom type(s): %2\nAcceptor atom type(s): %3").arg(donors, hydrogens, acceptors);
        if(!definitionMode.isEmpty())
            lines << tr("Hydrogen-bond definition: %1").arg(definitionMode);
        if(!pairingMode.isEmpty())
            lines << tr("Donor-hydrogen pairing mode: %1").arg(pairingMode);
        if(donorHydrogenCutoff.isValid())
            lines << tr("Donor-hydrogen cutoff: %1").arg(donorHydrogenCutoff.toDouble(), 0, 'g', 6);
        if(donorAcceptorCutoff.isValid())
            lines << tr("Donor-acceptor cutoff: %1").arg(donorAcceptorCutoff.toDouble(), 0, 'g', 6);
        if(angleCutoff.isValid())
            lines << tr("D-H-A angle cutoff: %1").arg(angleCutoff.toDouble(), 0, 'g', 6);
        if(pmfDistanceMaximum.isValid())
            lines << tr("PMF distance maximum: %1").arg(pmfDistanceMaximum.toDouble(), 0, 'g', 6);
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

    for(QWidget* widget : { _fixedCriteriaWidget.data(), _pmfCriteriaWidget.data() }) {
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
