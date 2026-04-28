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
#include <ovito/particles/modifier/analysis/hbond/HydrogenBondKineticsModifier.h>
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
#include "HydrogenBondKineticsModifierEditor.h"

namespace Ovito {

namespace {

bool hydrogenBondKineticsIsIdle(const HydrogenBondKineticsModifier* modifier, const ModificationNode* node)
{
    const auto* hbNode = dynamic_object_cast<const HydrogenBondKineticsModificationNode>(node);
    return modifier && hbNode && !hbNode->hasCachedResults() && modifier->runRequestId() <= hbNode->completedRunRequestId();
}

}

IMPLEMENT_CREATABLE_OVITO_CLASS(HydrogenBondKineticsModifierEditor);
SET_OVITO_OBJECT_EDITOR(HydrogenBondKineticsModifier, HydrogenBondKineticsModifierEditor);

HydrogenBondKineticsModifier* HydrogenBondKineticsModifierEditor::modifier() const
{
    return static_object_cast<HydrogenBondKineticsModifier>(editObject());
}

void HydrogenBondKineticsModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Hydrogen bond kinetics"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* participantBox = new QGroupBox(tr("Participants"), rollout);
    auto* participantLayout = new QGridLayout(participantBox);
    participantLayout->setContentsMargins(4, 4, 4, 4);
    participantLayout->setColumnStretch(1, 1);
    participantLayout->setVerticalSpacing(4);

    StringParameterUI* donorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::donorTypes));
    donorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    participantLayout->addWidget(new QLabel(tr("Donor atom type(s)"), participantBox), 0, 0);
    participantLayout->addWidget(donorTypesUI->textBox(), 0, 1);

    StringParameterUI* hydrogenTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::hydrogenTypes));
    hydrogenTypesUI->lineEdit()->setPlaceholderText(tr("e.g. H or 1"));
    participantLayout->addWidget(new QLabel(tr("Hydrogen atom type(s)"), participantBox), 1, 0);
    participantLayout->addWidget(hydrogenTypesUI->textBox(), 1, 1);

    StringParameterUI* acceptorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::acceptorTypes));
    acceptorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    participantLayout->addWidget(new QLabel(tr("Acceptor atom type(s)"), participantBox), 2, 0);
    participantLayout->addWidget(acceptorTypesUI->textBox(), 2, 1);

    FloatParameterUI* donorHydrogenCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::donorHydrogenCutoff));
    participantLayout->addWidget(donorHydrogenCutoffUI->label(), 3, 0);
    participantLayout->addLayout(donorHydrogenCutoffUI->createFieldLayout(), 3, 1);

    layout->addWidget(participantBox);

    auto* definitionBox = new QGroupBox(tr("Definition"), rollout);
    auto* definitionLayout = new QGridLayout(definitionBox);
    definitionLayout->setContentsMargins(4, 4, 4, 4);
    definitionLayout->setColumnStretch(1, 1);
    definitionLayout->setVerticalSpacing(4);

    VariantComboBoxParameterUI* definitionModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::definitionMode));
    definitionModeUI->comboBox()->addItem(tr("Fixed geometry"),
                                          QVariant::fromValue((int)HydrogenBondKineticsModifier::FixedGeometry));
    definitionModeUI->comboBox()->addItem(tr("PMF-derived (from Hydrogen bond analysis)"),
                                          QVariant::fromValue((int)HydrogenBondKineticsModifier::PMFDerived));
    definitionLayout->addWidget(new QLabel(tr("Hydrogen-bond definition"), definitionBox), 0, 0);
    definitionLayout->addWidget(definitionModeUI->comboBox(), 0, 1);

    _fixedCriteriaWidget = new QWidget(definitionBox);
    auto* fixedLayout = new QGridLayout(_fixedCriteriaWidget);
    fixedLayout->setContentsMargins(0, 0, 0, 0);
    fixedLayout->setColumnStretch(1, 1);
    fixedLayout->setVerticalSpacing(4);

    FloatParameterUI* donorAcceptorCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::donorAcceptorCutoff));
    fixedLayout->addWidget(donorAcceptorCutoffUI->label(), 0, 0);
    fixedLayout->addLayout(donorAcceptorCutoffUI->createFieldLayout(), 0, 1);

    FloatParameterUI* angleCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::angleCutoff));
    fixedLayout->addWidget(angleCutoffUI->label(), 1, 0);
    fixedLayout->addLayout(angleCutoffUI->createFieldLayout(), 1, 1);

    FloatParameterUI* vicinityCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::vicinityCutoff));
    fixedLayout->addWidget(vicinityCutoffUI->label(), 2, 0);
    fixedLayout->addLayout(vicinityCutoffUI->createFieldLayout(), 2, 1);

    definitionLayout->addWidget(_fixedCriteriaWidget, 1, 0, 1, 2);

    _pmfCriteriaWidget = new QWidget(definitionBox);
    auto* pmfLayout = new QGridLayout(_pmfCriteriaWidget);
    pmfLayout->setContentsMargins(0, 0, 0, 0);
    pmfLayout->setColumnStretch(1, 1);
    pmfLayout->setVerticalSpacing(4);

    auto* pmfInfoLabel = new QLabel(tr(
        "Uses the PMF basin and vicinity boundary from an upstream 'Hydrogen bond analysis' modifier in PMF-derived mode."), _pmfCriteriaWidget);
    pmfInfoLabel->setWordWrap(true);
    pmfLayout->addWidget(pmfInfoLabel, 0, 0, 1, 2);

    definitionLayout->addWidget(_pmfCriteriaWidget, 2, 0, 1, 2);

    layout->addWidget(definitionBox);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(HydrogenBondKineticsModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maxLagUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::maxLag));
    samplingLayout->addWidget(maxLagUI->label(), 1, 0);
    samplingLayout->addLayout(maxLagUI->createFieldLayout(), 1, 1);

    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);

    _runButton = new QPushButton(tr("Run hydrogen-bond kinetics"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &HydrogenBondKineticsModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(tr("Hydrogen-bond kinetics results are idle. Open the Run section and click 'Run hydrogen-bond kinetics' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondKineticsModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &HydrogenBondKineticsModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsChanged, this, &HydrogenBondKineticsModifierEditor::updateDefinitionControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &HydrogenBondKineticsModifierEditor::updateDefinitionControls);
    connect(definitionModeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged),
            this, &HydrogenBondKineticsModifierEditor::updateDefinitionControls);

    updateDefinitionControls();
    updatePlot();
    updateSummary();
}

void HydrogenBondKineticsModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        HydrogenBondKineticsModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* hbNode = dynamic_object_cast<const HydrogenBondKineticsModificationNode>(node);
        const int startedGenerationId = hbNode ? hbNode->cacheGenerationId() : 0;

        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running hydrogen-bond kinetics over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<HydrogenBondKineticsModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId](auto& task) noexcept {
            if(!task.isCanceled() && !task.exceptionStore())
                return;
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            HydrogenBondKineticsModifier* mod = self->modifier();
            auto* hbNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(self->modificationNode());
            if(!mod || !hbNode || mod->runRequestId() != startedRunRequestId || hbNode->cacheGenerationId() != startedGenerationId)
                return;

            hbNode->setCompletedRunRequestId(startedRunRequestId);
            self->updatePlot();
            self->updateSummary();
        });
        scheduleOperationAfter(std::move(future), [this, startedRunRequestId, startedGenerationId](const PipelineFlowState&) {
            HydrogenBondKineticsModifier* mod = modifier();
            const auto* hbNode = dynamic_object_cast<const HydrogenBondKineticsModificationNode>(modificationNode());
            if(!mod || !hbNode || mod->runRequestId() != startedRunRequestId || hbNode->cacheGenerationId() != startedGenerationId)
                return;
            updatePlot();
            updateSummary();
        });
    });
}

void HydrogenBondKineticsModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_plot)
            return;
        if(hydrogenBondKineticsIsIdle(modifier(), modificationNode())) {
            _plot->setTable(nullptr);
            return;
        }
        _plot->setTable(getPipelineOutput().getObjectBy<DataTable>(
            modificationNode(),
            HydrogenBondKineticsModifier::kineticsTableId()));
    });
}

void HydrogenBondKineticsModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        if(hydrogenBondKineticsIsIdle(modifier(), modificationNode())) {
            _summaryLabel->setText(tr(
                "Hydrogen-bond kinetics results are idle. Open the Run section and click 'Run hydrogen-bond kinetics' to compute the selected observable."));
            refreshSummaryGeometry();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString donors = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.donor_types")).toString();
        const QString hydrogens = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.hydrogen_types")).toString();
        const QString acceptors = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.acceptor_types")).toString();
        const QString definitionMode = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.definition_mode")).toString();
        const QString pairingMode = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.donor_hydrogen_pairing_mode")).toString();
        const QVariant sampledFrameCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.sampled_frame_count"));
        const QVariant totalCandidateTriplets = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.total_candidate_triplets"));
        const QVariant initialTripletSamples = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.initial_triplet_samples"));
        const QVariant maximumLag = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.maximum_lag"));
        const QVariant finalC = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_C"));
        const QVariant finalN = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_n"));
        const QVariant finalCPlusN = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_C_plus_n"));
        const QVariant pmfDistanceMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_distance_minimum"));
        const QVariant pmfBoundary = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_boundary_free_energy"));
        const QVariant pmfVicinity = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_vicinity_cutoff"));
        const QVariant pmfBasinBinCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_basin_bin_count"));

        QStringList lines;
        if(!donors.isEmpty() || !hydrogens.isEmpty() || !acceptors.isEmpty())
            lines << tr("Donor atom type(s): %1\nHydrogen atom type(s): %2\nAcceptor atom type(s): %3").arg(donors, hydrogens, acceptors);
        if(!definitionMode.isEmpty())
            lines << tr("Hydrogen-bond definition: %1").arg(definitionMode);
        if(!pairingMode.isEmpty())
            lines << tr("Donor-hydrogen pairing mode: %1").arg(pairingMode);
        const QVariant donorAcceptorCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.hb_donor_acceptor_cutoff"));
        const QVariant angleCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.hb_theta_maximum"));
        const QVariant vicinityCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.vicinity_cutoff"));
        if(donorAcceptorCutoff.isValid())
            lines << tr("Donor-acceptor cutoff: %1").arg(donorAcceptorCutoff.toDouble(), 0, 'g', 6);
        if(angleCutoff.isValid())
            lines << tr("HB theta maximum: %1").arg(angleCutoff.toDouble(), 0, 'g', 6);
        if(vicinityCutoff.isValid())
            lines << tr("Vicinity donor-acceptor cutoff: %1").arg(vicinityCutoff.toDouble(), 0, 'g', 6);
        if(sampledFrameCount.isValid())
            lines << tr("Sampled frames: %1").arg(sampledFrameCount.toInt());
        if(totalCandidateTriplets.isValid())
            lines << tr("Candidate triplets sampled: %1").arg(totalCandidateTriplets.toLongLong());
        if(initialTripletSamples.isValid())
            lines << tr("Initial hydrogen-bond triplet samples: %1").arg(initialTripletSamples.toLongLong());
        if(maximumLag.isValid())
            lines << tr("Maximum lag: %1").arg(maximumLag.toInt());
        if(pmfDistanceMinimum.isValid())
            lines << tr("PMF distance minimum: %1").arg(pmfDistanceMinimum.toDouble(), 0, 'g', 6);
        if(pmfBoundary.isValid())
            lines << tr("PMF basin boundary free energy: %1").arg(pmfBoundary.toDouble(), 0, 'f', 4);
        if(pmfVicinity.isValid())
            lines << tr("Derived vicinity cutoff: %1").arg(pmfVicinity.toDouble(), 0, 'f', 4);
        if(pmfBasinBinCount.isValid())
            lines << tr("PMF basin bins: %1").arg(pmfBasinBinCount.toLongLong());
        if(finalC.isValid())
            lines << tr("Final C(t): %1").arg(finalC.toDouble(), 0, 'f', 6);
        if(finalN.isValid())
            lines << tr("Final n(t): %1").arg(finalN.toDouble(), 0, 'f', 6);
        if(finalCPlusN.isValid())
            lines << tr("Final C(t)+n(t): %1").arg(finalCPlusN.toDouble(), 0, 'f', 6);

        _summaryLabel->setText(lines.join(QStringLiteral("\n\n")));
        refreshSummaryGeometry();
    });
}

void HydrogenBondKineticsModifierEditor::updateDefinitionControls()
{
    const HydrogenBondKineticsModifier* mod = modifier();
    if(!mod)
        return;

    const bool fixedVisible = mod->definitionMode() == HydrogenBondKineticsModifier::FixedGeometry;
    const bool pmfVisible = mod->definitionMode() == HydrogenBondKineticsModifier::PMFDerived;

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

void HydrogenBondKineticsModifierEditor::refreshSummaryGeometry()
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
