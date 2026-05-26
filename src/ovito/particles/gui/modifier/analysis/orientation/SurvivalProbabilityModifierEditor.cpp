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
#include <ovito/particles/modifier/analysis/orientation/SurvivalProbabilityModifier.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include "SurvivalProbabilityModifierEditor.h"

namespace Ovito {

namespace {

bool survivalProbabilityIsIdle(const SurvivalProbabilityModifier* modifier, const ModificationNode* node)
{
    const auto* spNode = dynamic_object_cast<const SurvivalProbabilityModificationNode>(node);
    return modifier && spNode && !spNode->hasCachedResults() && modifier->runRequestId() <= spNode->completedRunRequestId();
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(SurvivalProbabilityModifierEditor);
SET_OVITO_OBJECT_EDITOR(SurvivalProbabilityModifier, SurvivalProbabilityModifierEditor);

SurvivalProbabilityModifier* SurvivalProbabilityModifierEditor::modifier() const
{
    return static_object_cast<SurvivalProbabilityModifier>(editObject());
}

void SurvivalProbabilityModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Survival probability"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    auto* referenceTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::referenceTypes));
    referenceTypesUI->lineEdit()->setPlaceholderText(tr("e.g. Na or Na,K"));
    auto* referenceExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::referenceExpression));
    gridLayout->addWidget(new QLabel(tr("Orient around atom type(s)")), 0, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        referenceTypesUI->textBox(),
        referenceExpressionUI,
        tr("Reference expression override"),
        tr("Use this expression instead of the reference atom types. Leave it empty to use the type field again.")), 0, 1);

    auto* anchorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::anchorTypes));
    anchorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or O,H"));
    auto* anchorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::anchorExpression));
    gridLayout->addWidget(new QLabel(tr("Molecule site atom type(s)")), 1, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        anchorTypesUI->textBox(),
        anchorExpressionUI,
        tr("Molecule site expression override"),
        tr("Use this expression instead of the molecule site atom types. Leave it empty to use the type field again.")), 1, 1);

    auto* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::cutoff));
    gridLayout->addWidget(new QLabel(tr("Distance cutoff")), 2, 0);
    gridLayout->addLayout(cutoffUI->createFieldLayout(), 2, 1);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::onlySelectedParticles));
    gridLayout->addWidget(onlySelectedUI->checkBox(), 3, 0, 1, 2);
    layout->addLayout(gridLayout);

    auto* optionsBox = new QGroupBox(tr("Options"), rollout);
    auto* optionsLayout = new QGridLayout(optionsBox);
    optionsLayout->setContentsMargins(4, 4, 4, 4);
    optionsLayout->setColumnStretch(1, 1);
    optionsLayout->setVerticalSpacing(4);

    IntegerParameterUI* intermittencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::intermittency));
    optionsLayout->addWidget(intermittencyUI->label(), 0, 0);
    optionsLayout->addLayout(intermittencyUI->createFieldLayout(), 0, 1);

    auto* noteLabel = new QLabel(tr("Treat short absences of up to N sampled frames as continuous residence when the molecule returns to the reference shell."), optionsBox);
    noteLabel->setWordWrap(true);
    optionsLayout->addWidget(noteLabel, 1, 0, 1, 2);
    layout->addWidget(optionsBox);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(SurvivalProbabilityModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maxLagUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SurvivalProbabilityModifier::maxLag));
    samplingLayout->addWidget(maxLagUI->label(), 1, 0);
    samplingLayout->addLayout(maxLagUI->createFieldLayout(), 1, 1);

    auto* samplingNote = new QLabel(
        tr("The survival probability is averaged over all valid time origins for each lag. These controls thin the sampled frames and limit the computed lag range."),
        samplingBox);
    samplingNote->setWordWrap(true);
    samplingLayout->addWidget(samplingNote, 2, 0, 1, 2);
    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);
    _runButton = new QPushButton(tr("Run survival probability analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &SurvivalProbabilityModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    auto* runNoteLabel = new QLabel(tr("The modifier traverses the selected trajectory interval only when you click Run."), runBox);
    runNoteLabel->setWordWrap(true);
    runLayout->addWidget(runNoteLabel);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(tr("Survival probability results are idle. Open the Run section and click 'Run survival probability analysis' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &SurvivalProbabilityModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &SurvivalProbabilityModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsChanged, this, &SurvivalProbabilityModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsReplaced, this, &SurvivalProbabilityModifierEditor::updateSummary);

    updatePlot();
    updateSummary();
}

void SurvivalProbabilityModifierEditor::runAnalysis()
{
    if(!_runButton || !modificationNode())
        return;

    handleExceptions([this]() {
        _runButton->setEnabled(false);
        auto restoreButton = qScopeGuard([this]() {
            if(_runButton)
                _runButton->setEnabled(true);
        });

        SurvivalProbabilityModifier* mod = modifier();
        if(!mod)
            return;

        mod->setRunRequestId(mod->runRequestId() + 1);

        const auto* spNode = dynamic_object_cast<const SurvivalProbabilityModificationNode>(modificationNode());
        const int startedGenerationId = spNode ? spNode->cacheGenerationId() : 0;
        if(_summaryLabel)
            _summaryLabel->setText(tr("Running survival probability analysis over the sampled trajectory..."));

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = modificationNode()->evaluate(request).asFuture();
        restoreButton.dismiss();

        future.finally(ObjectExecutor(this), [this, startedGenerationId]() noexcept {
            if(_runButton)
                _runButton->setEnabled(true);

            const auto* spNode = dynamic_object_cast<const SurvivalProbabilityModificationNode>(modificationNode());
            if(!spNode || spNode->cacheGenerationId() != startedGenerationId)
                return;

            updatePlot();
            updateSummary();
        });
    });
}

void SurvivalProbabilityModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), SurvivalProbabilityModifier::correlationTableId());
        _plot->setTable(std::move(table));
    });
}

void SurvivalProbabilityModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;
        if(survivalProbabilityIsIdle(modifier(), modificationNode())) {
            _summaryLabel->setText(tr("Survival probability results are idle. Open the Run section and click 'Run survival probability analysis' to compute the selected observable."));
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString warningText = state.status().type() == PipelineStatus::Warning
                                          ? tr("Warning: %1").arg(state.status().text())
                                          : QString{};

        const QVariant target = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.target"));
        const QVariant frameCount = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.sampled_frame_count"));
        const QVariant itemCount = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.sampled_item_count"));
        const QVariant maxLag = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.maximum_lag"));
        const QVariant intermittency = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.intermittency"));
        const QVariant zeroLag = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.zero_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("SurvivalProbability.final_value"));

        if(!target.isValid() || !frameCount.isValid()) {
            _summaryLabel->setText(warningText.isEmpty()
                ? tr("Survival probability results are being prepared...")
                : warningText);
            return;
        }

        QString summary = tr("Target: %1\nSampled frames: %2\nTracked molecules: %3\nMaximum lag: %4 sampled-frame steps\nIntermittency: %5")
                              .arg(target.toString())
                              .arg(frameCount.toInt())
                              .arg(itemCount.toInt())
                              .arg(maxLag.toDouble())
                              .arg(intermittency.toInt());
        if(zeroLag.isValid())
            summary += tr("\nSP(0): %1").arg(zeroLag.toDouble(), 0, 'g', 6);
        if(finalValue.isValid())
            summary += tr("\nFinal value: %1").arg(finalValue.toDouble(), 0, 'g', 6);
        if(!warningText.isEmpty())
            summary = warningText + QStringLiteral("\n\n") + summary;
        _summaryLabel->setText(summary);
    });
}

}  // namespace Ovito
