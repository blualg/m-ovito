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
#include <ovito/stdobj/table/StretchedExponentialFit.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <QPointer>
#include <cmath>
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
    StringParameterUI* donorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::donorExpression));
    participantLayout->addWidget(new QLabel(tr("Donor atom type(s)"), participantBox), 0, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        donorTypesUI->textBox(),
        donorExpressionUI,
        tr("Donor expression override"),
        tr("Use this expression instead of the donor atom types. Leave it empty to use the type field again.")), 0, 1);

    StringParameterUI* hydrogenTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::hydrogenTypes));
    hydrogenTypesUI->lineEdit()->setPlaceholderText(tr("e.g. H or 1"));
    StringParameterUI* hydrogenExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::hydrogenExpression));
    participantLayout->addWidget(new QLabel(tr("Hydrogen atom type(s)"), participantBox), 1, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        hydrogenTypesUI->textBox(),
        hydrogenExpressionUI,
        tr("Hydrogen expression override"),
        tr("Use this expression instead of the hydrogen atom types. Leave it empty to use the type field again.")), 1, 1);

    StringParameterUI* acceptorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::acceptorTypes));
    acceptorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    StringParameterUI* acceptorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::acceptorExpression));
    participantLayout->addWidget(new QLabel(tr("Acceptor atom type(s)"), participantBox), 2, 0);
    participantLayout->addWidget(createSelectorPopupRow(
        participantBox,
        acceptorTypesUI->textBox(),
        acceptorExpressionUI,
        tr("Acceptor expression override"),
        tr("Use this expression instead of the acceptor atom types. Leave it empty to use the type field again.")), 2, 1);

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
    definitionModeUI->comboBox()->addItem(tr("D/H/A site interaction energy (from Hydrogen bond analysis)"),
                                          QVariant::fromValue((int)HydrogenBondKineticsModifier::SiteInteractionEnergy));
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

    definitionLayout->addWidget(_fixedCriteriaWidget, 1, 0, 1, 2);

    _vicinityCriteriaWidget = new QWidget(definitionBox);
    auto* vicinityLayout = new QGridLayout(_vicinityCriteriaWidget);
    vicinityLayout->setContentsMargins(0, 0, 0, 0);
    vicinityLayout->setColumnStretch(1, 1);
    vicinityLayout->setVerticalSpacing(4);

    FloatParameterUI* vicinityCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::vicinityCutoff));
    vicinityLayout->addWidget(vicinityCutoffUI->label(), 0, 0);
    vicinityLayout->addLayout(vicinityCutoffUI->createFieldLayout(), 0, 1);

    definitionLayout->addWidget(_vicinityCriteriaWidget, 2, 0, 1, 2);

    _pmfCriteriaWidget = new QWidget(definitionBox);
    auto* pmfLayout = new QGridLayout(_pmfCriteriaWidget);
    pmfLayout->setContentsMargins(0, 0, 0, 0);
    pmfLayout->setColumnStretch(1, 1);
    pmfLayout->setVerticalSpacing(4);

    definitionLayout->addWidget(_pmfCriteriaWidget, 3, 0, 1, 2);

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

    auto* timeBox = new QGroupBox(tr("Time axis"), rollout);
    auto* timeLayout = new QGridLayout(timeBox);
    timeLayout->setContentsMargins(4, 4, 4, 4);
    timeLayout->setColumnStretch(1, 1);
    timeLayout->setVerticalSpacing(4);

    VariantComboBoxParameterUI* timeCoordinateUI =
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::timeCoordinateMode));
    timeCoordinateUI->comboBox()->addItem(
        tr("Automatic (prefer valid Timestep)"),
        QVariant::fromValue((int)HydrogenBondKineticsModifier::AutomaticTimeCoordinate));
    timeCoordinateUI->comboBox()->addItem(
        tr("Trajectory Timestep"),
        QVariant::fromValue((int)HydrogenBondKineticsModifier::TimestepAttributeTimeCoordinate));
    timeCoordinateUI->comboBox()->addItem(
        tr("Source frame index"),
        QVariant::fromValue((int)HydrogenBondKineticsModifier::SourceFrameTimeCoordinate));
    timeLayout->addWidget(new QLabel(tr("Time coordinate"), timeBox), 0, 0);
    timeLayout->addWidget(timeCoordinateUI->comboBox(), 0, 1);

    FloatParameterUI* timeStepUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::timeStep));
    timeLayout->addWidget(timeStepUI->label(), 1, 0);
    timeLayout->addLayout(timeStepUI->createFieldLayout(), 1, 1);

    VariantComboBoxParameterUI* timeUnitUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::timeUnit));
    timeUnitUI->comboBox()->addItem(tr("fs"), QVariant::fromValue((int)HydrogenBondKineticsModifier::Femtoseconds));
    timeUnitUI->comboBox()->addItem(tr("ps"), QVariant::fromValue((int)HydrogenBondKineticsModifier::Picoseconds));
    timeUnitUI->comboBox()->addItem(tr("ns"), QVariant::fromValue((int)HydrogenBondKineticsModifier::Nanoseconds));
    timeLayout->addWidget(new QLabel(tr("Time unit"), timeBox), 2, 0);
    timeLayout->addWidget(timeUnitUI->comboBox(), 2, 1);
    timeBox->setToolTip(tr(
        "Automatic mode uses the trajectory Timestep only when it is present and strictly increasing in every sampled frame; "
        "otherwise it uses source-frame indices. In source-frame mode, enter the physical time between consecutive saved frames "
        "as the time per coordinate unit."));
    layout->addWidget(timeBox);

    auto* kineticsBox = new QGroupBox(tr("Kinetic fit and uncertainty"), rollout);
    auto* kineticsLayout = new QGridLayout(kineticsBox);
    kineticsLayout->setContentsMargins(4, 4, 4, 4);
    kineticsLayout->setColumnStretch(1, 1);
    kineticsLayout->setVerticalSpacing(4);

    IntegerParameterUI* fitStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::kineticFitStartLag));
    kineticsLayout->addWidget(fitStartUI->label(), 0, 0);
    kineticsLayout->addLayout(fitStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* fitEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::kineticFitEndLag));
    kineticsLayout->addWidget(fitEndUI->label(), 1, 0);
    kineticsLayout->addLayout(fitEndUI->createFieldLayout(), 1, 1);

    IntegerParameterUI* fluxWindowUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::reactiveFluxSmoothingWindow));
    kineticsLayout->addWidget(fluxWindowUI->label(), 2, 0);
    kineticsLayout->addLayout(fluxWindowUI->createFieldLayout(), 2, 1);

    IntegerParameterUI* bootstrapReplicatesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::bootstrapReplicates));
    kineticsLayout->addWidget(bootstrapReplicatesUI->label(), 3, 0);
    kineticsLayout->addLayout(bootstrapReplicatesUI->createFieldLayout(), 3, 1);

    IntegerParameterUI* bootstrapBlockLengthUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::bootstrapBlockLength));
    kineticsLayout->addWidget(bootstrapBlockLengthUI->label(), 4, 0);
    kineticsLayout->addLayout(bootstrapBlockLengthUI->createFieldLayout(), 4, 1);

    IntegerParameterUI* bootstrapSeedUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::bootstrapSeed));
    kineticsLayout->addWidget(bootstrapSeedUI->label(), 5, 0);
    kineticsLayout->addLayout(bootstrapSeedUI->createFieldLayout(), 5, 1);

    IntegerParameterUI* lifetimeBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(HydrogenBondKineticsModifier::lifetimeHistogramBins));
    kineticsLayout->addWidget(lifetimeBinsUI->label(), 6, 0);
    kineticsLayout->addLayout(lifetimeBinsUI->createFieldLayout(), 6, 1);
    kineticsBox->setToolTip(tr(
        "The Luzar-Chandler rates are fitted to the integrated kinetic equation. "
        "The non-overlapping bootstrap blocks contain consecutive sampled frames; zero selects an automatic square-root block length."));
    layout->addWidget(kineticsBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);

    _runButton = new QPushButton(tr("Run hydrogen-bond kinetics"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &HydrogenBondKineticsModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    _dataInspectorButton = new QPushButton(tr("Show in data inspector"), rollout);
    connect(_dataInspectorButton, &QPushButton::clicked, this, &HydrogenBondKineticsModifierEditor::openDataInspector);
    layout->addWidget(_dataInspectorButton);
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

        if(_runButton)
            _runButton->setEnabled(false);

        node->cancelActiveEvaluations(false);
        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        auto* hbNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(node);
        if(hbNode)
            hbNode->pipelineCache().invalidate();
        const int startedGenerationId = hbNode ? hbNode->cacheGenerationId() : 0;

        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running hydrogen-bond kinetics over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        SharedFuture<PipelineFlowState> future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<HydrogenBondKineticsModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId,
                                              future](auto& task) noexcept {
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            HydrogenBondKineticsModifier* mod = self->modifier();
            auto* hbNode = dynamic_object_cast<HydrogenBondKineticsModificationNode>(self->modificationNode());
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

/******************************************************************************
* Opens the cached kinetics table without evaluating downstream modifiers.
******************************************************************************/
void HydrogenBondKineticsModifierEditor::openDataInspector()
{
    handleExceptions([&]() {
        HydrogenBondKineticsModifier* mod = modifier();
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

        if(!ui().mainWindow()->openDataInspectorForPipelineOutput(
               getPipelineOutput(), node, HydrogenBondKineticsModifier::kineticsTableId(), 1)) {
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

void HydrogenBondKineticsModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        if(hydrogenBondKineticsIsIdle(modifier(), modificationNode())) {
            _summaryLabel->clear();
            refreshSummaryGeometry();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString donors = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.donor_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.donor_types")).toString()
                : expression;
        }();
        const QString hydrogens = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.hydrogen_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.hydrogen_types")).toString()
                : expression;
        }();
        const QString acceptors = [&]() {
            const QString expression = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.acceptor_expression")).toString().trimmed();
            return expression.isEmpty()
                ? state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.acceptor_types")).toString()
                : expression;
        }();
        const QString definitionMode = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.definition_mode")).toString();
        const QString pairingMode = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.donor_hydrogen_pairing_mode")).toString();
        const QVariant sampledFrameCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.sampled_frame_count"));
        const QVariant totalCandidateTriplets = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.total_candidate_triplets"));
        const QVariant initialTripletSamples = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.initial_triplet_samples"));
        const QVariant maximumLag = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.maximum_lag"));
        const QVariant maximumLagSourceFrames = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.maximum_lag_source_frames"));
        const QString timeUnit = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.time_unit")).toString();
        const QString timeCoordinateSource = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.time_coordinate_source")).toString();
        const QVariant timeStep = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.time_step"));
        const QVariant finalS = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_S"));
        const QVariant finalC = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_C"));
        const QVariant finalN = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_n"));
        const QVariant finalCPlusN = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.final_C_plus_n"));
        const QVariant pmfDistanceMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_distance_minimum"));
        const QVariant pmfThetaMinimum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_theta_minimum"));
        const QVariant pmfThetaMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_theta_maximum"));
        const QVariant pmfBoundary = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_boundary_free_energy"));
        const QVariant pmfVicinity = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_vicinity_cutoff"));
        const QVariant pmfBasinBinCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.pmf_basin_bin_count"));
        const QVariant siteEnergyDistanceMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.site_energy_distance_maximum"));
        const QVariant siteEnergyThetaMaximum = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.site_energy_theta_maximum"));
        const QVariant siteEnergyCutoff = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.site_energy_cutoff"));
        const QString siteEnergyCutoffMode = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.site_energy_cutoff_mode")).toString();
        const QString siteEnergyUnit = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.site_energy_unit")).toString();
        const QVariant continuousLifetime = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.continuous_lifetime_observed"));
        const QVariant intermittentCorrelationTime = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.intermittent_correlation_time_observed"));
        const QVariant continuousIntegralTruncated = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.continuous_lifetime_integral_truncated"));
        const QVariant intermittentIntegralTruncated = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.intermittent_correlation_integral_truncated"));
        const QVariant completeEventCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.complete_event_count"));
        const QVariant leftCensoredEventCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.left_censored_event_count"));
        const QVariant rightCensoredEventCount = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.right_censored_event_count"));
        const QVariant completeEventMean = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.complete_event_mean_lifetime"));
        const QVariant completeEventMedian = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.complete_event_median_lifetime"));
        const QVariant lcFitValid = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.lc_fit_valid"));
        const QString lcFitStatus = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.lc_fit_status")).toString();
        const QVariant breakingRate = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.breaking_rate"));
        const QVariant reformationRate = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.reformation_rate"));
        const QVariant luzarChandlerLifetime = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.luzar_chandler_lifetime"));
        const QVariant reformationTime = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.reformation_time"));
        const QVariant lcFitRSquared = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.lc_fit_r_squared"));
        const QVariant lcFitStart = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.lc_fit_start"));
        const QVariant lcFitEnd = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.lc_fit_end"));
        const QVariant bootstrapRequested = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.bootstrap_requested_replicates"));
        const QVariant bootstrapSuccessfulRates = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.bootstrap_successful_rate_replicates"));
        const QVariant bootstrapBlockLength = state.getAttributeValue(modificationNode(), QStringLiteral("HBKinetics.bootstrap_block_length"));
        const QString fitSummary = stretchedExponentialFitSummary(
            state, modificationNode(), QStringLiteral("HBKinetics"), timeUnit.isEmpty() ? tr("time units") : timeUnit);
        const QString continuousFitSummary = stretchedExponentialFitSummary(
            state, modificationNode(), QStringLiteral("HBKineticsContinuous"), timeUnit.isEmpty() ? tr("time units") : timeUnit);

        const auto ci95 = [&](const QString& prefix) {
            const QString lowerKey = prefix + QStringLiteral("_ci95_lower");
            const QString upperKey = prefix + QStringLiteral("_ci95_upper");
            const QVariant lower = state.getAttributeValue(
                modificationNode(), QStringView(lowerKey));
            const QVariant upper = state.getAttributeValue(
                modificationNode(), QStringView(upperKey));
            return lower.isValid() && upper.isValid()
                ? tr(" [95% CI %1-%2]").arg(lower.toDouble(), 0, 'g', 6).arg(upper.toDouble(), 0, 'g', 6)
                : QString{};
        };

        QStringList lines;
        if(!donors.isEmpty() || !hydrogens.isEmpty() || !acceptors.isEmpty())
            lines << tr("Donor selector: %1\nHydrogen selector: %2\nAcceptor selector: %3").arg(donors, hydrogens, acceptors);
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
        if(maximumLag.isValid()) {
            QString maximumLagLine = tr("Maximum lag: %1 %2").arg(maximumLag.toDouble(), 0, 'g', 6).arg(timeUnit);
            if(maximumLagSourceFrames.isValid())
                maximumLagLine += tr(" (%1 source frames)").arg(maximumLagSourceFrames.toDouble(), 0, 'g', 6);
            lines << maximumLagLine;
        }
        if(!timeCoordinateSource.isEmpty() && timeStep.isValid())
            lines << tr("Time axis: %1 x %2 %3").arg(timeCoordinateSource).arg(timeStep.toDouble(), 0, 'g', 8).arg(timeUnit);
        if(pmfDistanceMinimum.isValid())
            lines << tr("PMF distance minimum: %1").arg(pmfDistanceMinimum.toDouble(), 0, 'g', 6);
        if(pmfThetaMinimum.isValid())
            lines << tr("PMF theta minimum: %1").arg(pmfThetaMinimum.toDouble(), 0, 'g', 6);
        if(pmfThetaMaximum.isValid())
            lines << tr("PMF theta maximum: %1").arg(pmfThetaMaximum.toDouble(), 0, 'g', 6);
        if(pmfBoundary.isValid())
            lines << tr("PMF basin boundary free energy: %1").arg(pmfBoundary.toDouble(), 0, 'f', 4);
        if(pmfVicinity.isValid())
            lines << tr("Derived vicinity cutoff: %1").arg(pmfVicinity.toDouble(), 0, 'f', 4);
        if(pmfBasinBinCount.isValid())
            lines << tr("PMF basin bins: %1").arg(pmfBasinBinCount.toLongLong());
        if(siteEnergyDistanceMaximum.isValid())
            lines << tr("Candidate D-A cutoff: %1").arg(siteEnergyDistanceMaximum.toDouble(), 0, 'g', 6);
        if(siteEnergyThetaMaximum.isValid())
            lines << tr("Candidate theta maximum: %1").arg(siteEnergyThetaMaximum.toDouble(), 0, 'g', 6);
        if(siteEnergyCutoff.isValid())
            lines << tr("Effective maximum D/H/A site energy: %1 %2")
                         .arg(siteEnergyCutoff.toDouble(), 0, 'g', 8)
                         .arg(siteEnergyUnit);
        if(!siteEnergyCutoffMode.isEmpty())
            lines << tr("Energy cutoff mode: %1").arg(siteEnergyCutoffMode);
        if(continuousLifetime.isValid()) {
            QString lifetimeLine = tr("Continuous first-break lifetime (observed integral): %1 %2%3")
                                       .arg(continuousLifetime.toDouble(), 0, 'g', 7)
                                       .arg(timeUnit)
                                       .arg(ci95(QStringLiteral("HBKinetics.continuous_lifetime")));
            if(continuousIntegralTruncated.toDouble() != 0.0)
                lifetimeLine += tr(" [truncated at maximum lag]");
            lines << lifetimeLine;
        }
        if(intermittentCorrelationTime.isValid()) {
            QString correlationLine = tr("Intermittent correlation time (observed integral): %1 %2%3")
                                          .arg(intermittentCorrelationTime.toDouble(), 0, 'g', 7)
                                          .arg(timeUnit)
                                          .arg(ci95(QStringLiteral("HBKinetics.intermittent_correlation_time")));
            if(intermittentIntegralTruncated.toDouble() != 0.0)
                correlationLine += tr(" [not converged at maximum lag]");
            lines << correlationLine;
        }
        if(completeEventCount.isValid()) {
            QString eventLine = tr("Complete uncensored bond events: %1").arg(completeEventCount.toLongLong());
            if(completeEventMean.isValid() && completeEventMedian.isValid()) {
                eventLine += tr("; mean %1 %2; median %3 %2")
                    .arg(completeEventMean.toDouble(), 0, 'g', 7)
                    .arg(timeUnit)
                    .arg(completeEventMedian.toDouble(), 0, 'g', 7);
            }
            lines << eventLine;
        }
        if(leftCensoredEventCount.isValid() && rightCensoredEventCount.isValid()) {
            lines << tr("Boundary-censored episodes: %1 left; %2 right")
                         .arg(leftCensoredEventCount.toLongLong())
                         .arg(rightCensoredEventCount.toLongLong());
        }
        if(lcFitValid.isValid() && lcFitValid.toDouble() != 0.0) {
            lines << tr("Luzar-Chandler breaking rate: %1 1/%2%3")
                         .arg(breakingRate.toDouble(), 0, 'g', 7)
                         .arg(timeUnit)
                         .arg(ci95(QStringLiteral("HBKinetics.breaking_rate")));
            lines << tr("Luzar-Chandler lifetime (1/k_break): %1 %2%3")
                         .arg(luzarChandlerLifetime.toDouble(), 0, 'g', 7)
                         .arg(timeUnit)
                         .arg(ci95(QStringLiteral("HBKinetics.luzar_chandler_lifetime")));
            lines << tr("Reformation rate: %1 1/%2%3")
                         .arg(reformationRate.toDouble(), 0, 'g', 7)
                         .arg(timeUnit)
                         .arg(ci95(QStringLiteral("HBKinetics.reformation_rate")));
            if(reformationTime.isValid() && std::isfinite(reformationTime.toDouble())) {
                lines << tr("Reformation time (1/k_form): %1 %2%3")
                             .arg(reformationTime.toDouble(), 0, 'g', 7)
                             .arg(timeUnit)
                             .arg(ci95(QStringLiteral("HBKinetics.reformation_time")));
            }
            if(lcFitRSquared.isValid() && lcFitStart.isValid() && lcFitEnd.isValid()) {
                lines << tr("Integrated Luzar-Chandler fit: R2=%1; range %2-%3 %4")
                             .arg(lcFitRSquared.toDouble(), 0, 'g', 5)
                             .arg(lcFitStart.toDouble(), 0, 'g', 6)
                             .arg(lcFitEnd.toDouble(), 0, 'g', 6)
                             .arg(timeUnit);
            }
        }
        else if(lcFitValid.isValid()) {
            lines << tr("Luzar-Chandler rate fit unavailable: %1").arg(lcFitStatus);
        }
        if(bootstrapRequested.isValid() && bootstrapRequested.toInt() > 0) {
            lines << tr("Block bootstrap: %1/%2 successful rate fits; block length %3 sampled frames")
                         .arg(bootstrapSuccessfulRates.toInt())
                         .arg(bootstrapRequested.toInt())
                         .arg(bootstrapBlockLength.toInt());
        }
        if(finalS.isValid())
            lines << tr("Final continuous S(t): %1").arg(finalS.toDouble(), 0, 'f', 6);
        if(finalC.isValid())
            lines << tr("Final intermittent C(t): %1").arg(finalC.toDouble(), 0, 'f', 6);
        if(finalN.isValid())
            lines << tr("Final n(t): %1").arg(finalN.toDouble(), 0, 'f', 6);
        if(finalCPlusN.isValid())
            lines << tr("Final C(t)+n(t): %1").arg(finalCPlusN.toDouble(), 0, 'f', 6);
        if(!fitSummary.isEmpty())
            lines << fitSummary;
        if(!continuousFitSummary.isEmpty())
            lines << continuousFitSummary;

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
    const bool vicinityVisible =
        fixedVisible || mod->definitionMode() == HydrogenBondKineticsModifier::SiteInteractionEnergy;

    if(_fixedCriteriaWidget)
        _fixedCriteriaWidget->setVisible(fixedVisible);
    if(_vicinityCriteriaWidget)
        _vicinityCriteriaWidget->setVisible(vicinityVisible);
    if(_pmfCriteriaWidget)
        _pmfCriteriaWidget->setVisible(pmfVisible);

    for(QWidget* widget : { _fixedCriteriaWidget.data(), _vicinityCriteriaWidget.data(), _pmfCriteriaWidget.data() }) {
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
