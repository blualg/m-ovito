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
#include <ovito/particles/modifier/analysis/cacf/CoordinationEnvironmentAutocorrelationModifier.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <QPointer>
#include "CoordinationEnvironmentAutocorrelationModifierEditor.h"

namespace Ovito {

namespace {

bool cacfAnalysisIsIdle(const CoordinationEnvironmentAutocorrelationModifier* modifier, const ModificationNode* node)
{
    const auto* cacfNode = dynamic_object_cast<const CoordinationEnvironmentAutocorrelationModificationNode>(node);
    return modifier && cacfNode && !cacfNode->hasCachedResults() && modifier->runRequestId() <= cacfNode->completedRunRequestId();
}

}

IMPLEMENT_CREATABLE_OVITO_CLASS(CoordinationEnvironmentAutocorrelationModifierEditor);
SET_OVITO_OBJECT_EDITOR(CoordinationEnvironmentAutocorrelationModifier, CoordinationEnvironmentAutocorrelationModifierEditor);

/******************************************************************************
* Returns the modifier being edited.
******************************************************************************/
CoordinationEnvironmentAutocorrelationModifier* CoordinationEnvironmentAutocorrelationModifierEditor::modifier() const
{
    return static_object_cast<CoordinationEnvironmentAutocorrelationModifier>(editObject());
}

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Coordination environment autocorrelation function"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* shellBox = new QGroupBox(tr("Coordination shell"), rollout);
    auto* shellLayout = new QGridLayout(shellBox);
    shellLayout->setContentsMargins(4, 4, 4, 4);
    shellLayout->setColumnStretch(1, 1);
    shellLayout->setVerticalSpacing(4);

    StringParameterUI* centralTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::centralTypes));
    centralTypesUI->lineEdit()->setPlaceholderText(tr("e.g. Li,Na or 1,2"));
    shellLayout->addWidget(new QLabel(tr("Central atom type(s)"), shellBox), 0, 0);
    shellLayout->addWidget(centralTypesUI->textBox(), 0, 1);

    StringParameterUI* shellTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::shellTypes));
    shellTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,N or 5,8"));
    shellLayout->addWidget(new QLabel(tr("Shell atom type(s)"), shellBox), 1, 0);
    shellLayout->addWidget(shellTypesUI->textBox(), 1, 1);

    FloatParameterUI* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::cutoff));
    shellLayout->addWidget(cutoffUI->label(), 2, 0);
    shellLayout->addLayout(cutoffUI->createFieldLayout(), 2, 1);

    layout->addWidget(shellBox);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maxLagUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(CoordinationEnvironmentAutocorrelationModifier::maxLag));
    samplingLayout->addWidget(maxLagUI->label(), 1, 0);
    samplingLayout->addLayout(maxLagUI->createFieldLayout(), 1, 1);

    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);

    _runButton = new QPushButton(tr("Run CACF analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &CoordinationEnvironmentAutocorrelationModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(tr("CACF results are idle. Open the Run section and click 'Run CACF analysis' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &CoordinationEnvironmentAutocorrelationModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &CoordinationEnvironmentAutocorrelationModifierEditor::updateSummary);

    updatePlot();
    updateSummary();
}

/******************************************************************************
* Launches a non-interactive evaluation of the CACF modifier.
******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        CoordinationEnvironmentAutocorrelationModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* cacfNode = dynamic_object_cast<const CoordinationEnvironmentAutocorrelationModificationNode>(node);
        const int startedGenerationId = cacfNode ? cacfNode->cacheGenerationId() : 0;

        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running CACF analysis over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<CoordinationEnvironmentAutocorrelationModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId](auto& task) noexcept {
            if(!task.isCanceled() && !task.exceptionStore())
                return;
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            CoordinationEnvironmentAutocorrelationModifier* mod = self->modifier();
            auto* cacfNode = dynamic_object_cast<CoordinationEnvironmentAutocorrelationModificationNode>(self->modificationNode());
            if(!mod || !cacfNode || mod->runRequestId() != startedRunRequestId || cacfNode->cacheGenerationId() != startedGenerationId)
                return;

            cacfNode->setCompletedRunRequestId(startedRunRequestId);
            self->updatePlot();
            self->updateSummary();
        });
        scheduleOperationAfter(std::move(future), [this, startedRunRequestId, startedGenerationId](const PipelineFlowState&) {
            CoordinationEnvironmentAutocorrelationModifier* mod = modifier();
            const auto* cacfNode = dynamic_object_cast<const CoordinationEnvironmentAutocorrelationModificationNode>(modificationNode());
            if(!mod || !cacfNode || mod->runRequestId() != startedRunRequestId || cacfNode->cacheGenerationId() != startedGenerationId)
                return;
            updatePlot();
            updateSummary();
        });
    });
}

/******************************************************************************
* Updates the plot widget from the modifier output table.
******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_plot)
            return;
        if(cacfAnalysisIsIdle(modifier(), modificationNode())) {
            _plot->setTable(nullptr);
            return;
        }
        _plot->setTable(getPipelineOutput().getObjectBy<DataTable>(
            modificationNode(),
            CoordinationEnvironmentAutocorrelationModifier::correlationTableId()));
    });
}

/******************************************************************************
* Updates the summary label from the generated global attributes.
******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;

        if(cacfAnalysisIsIdle(modifier(), modificationNode())) {
            _summaryLabel->setText(tr(
                "CACF results are idle. Open the Run section and click 'Run CACF analysis' to compute the selected observable."));
            refreshSummaryGeometry();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString centralTypes = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.central_types")).toString();
        const QString shellTypes = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.shell_types")).toString();
        const QVariant cutoff = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.cutoff"));
        const QVariant frameCount = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.sampled_frame_count"));
        const QVariant centralCount = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.central_atom_count"));
        const QVariant averageShellSize = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.average_shell_size"));
        const QVariant maximumLag = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.maximum_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("CACF.final_value"));

        QStringList lines;
        if(!centralTypes.isEmpty() || !shellTypes.isEmpty())
            lines << tr("Central atom type(s): %1\nShell atom type(s): %2").arg(centralTypes, shellTypes);
        if(cutoff.isValid())
            lines << tr("Distance cutoff: %1").arg(cutoff.toDouble(), 0, 'g', 6);
        if(frameCount.isValid())
            lines << tr("Sampled frames: %1").arg(frameCount.toInt());
        if(centralCount.isValid())
            lines << tr("Average central atoms per frame: %1").arg(centralCount.toDouble(), 0, 'f', 2);
        if(averageShellSize.isValid())
            lines << tr("Average shell size: %1").arg(averageShellSize.toDouble(), 0, 'f', 3);
        if(maximumLag.isValid())
            lines << tr("Maximum lag: %1").arg(maximumLag.toInt());
        if(finalValue.isValid())
            lines << tr("Final CACF: %1").arg(finalValue.toDouble(), 0, 'f', 6);

        _summaryLabel->setText(lines.join(QStringLiteral("\n\n")));
        refreshSummaryGeometry();
    });
}

/******************************************************************************
* Reflows the wrapped summary label after changing its contents.
******************************************************************************/
void CoordinationEnvironmentAutocorrelationModifierEditor::refreshSummaryGeometry()
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
