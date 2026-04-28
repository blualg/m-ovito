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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/AutocorrelationFunctionModifier.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include "AutocorrelationFunctionModifierEditor.h"
#include <QPointer>

namespace Ovito {

namespace {

bool autocorrelationAnalysisIsIdle(const AutocorrelationFunctionModifier* modifier, const ModificationNode* node)
{
    const auto* acfNode = dynamic_object_cast<const AutocorrelationFunctionModificationNode>(node);
    return modifier && acfNode && !acfNode->hasCachedResults() && modifier->runRequestId() <= acfNode->completedRunRequestId();
}

}

IMPLEMENT_CREATABLE_OVITO_CLASS(AutocorrelationFunctionModifierEditor);
SET_OVITO_OBJECT_EDITOR(AutocorrelationFunctionModifier, AutocorrelationFunctionModifierEditor);

/******************************************************************************
* Returns the modifier being edited.
******************************************************************************/
AutocorrelationFunctionModifier* AutocorrelationFunctionModifierEditor::modifier() const
{
    return static_object_cast<AutocorrelationFunctionModifier>(editObject());
}

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Autocorrelation function"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    VariantComboBoxParameterUI* analysisModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::analysisMode));
    analysisModeUI->comboBox()->addItem(tr("Signal autocorrelation"), QVariant::fromValue((int)AutocorrelationFunctionModifier::SignalAutocorrelation));
    analysisModeUI->comboBox()->addItem(tr("Vector reorientation"), QVariant::fromValue((int)AutocorrelationFunctionModifier::VectorReorientation));
    analysisModeUI->comboBox()->addItem(tr("Survival probability"), QVariant::fromValue((int)AutocorrelationFunctionModifier::SurvivalProbability));
    layout->addWidget(new QLabel(tr("Analysis mode:"), rollout));
    layout->addWidget(analysisModeUI->comboBox());

    VariantComboBoxParameterUI* targetTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::targetType));
    targetTypeUI->comboBox()->addItem(tr("Global attribute"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Attribute));
    targetTypeUI->comboBox()->addItem(tr("Data table"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Table));
    targetTypeUI->comboBox()->addItem(tr("Element property"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Property));
    targetTypeUI->comboBox()->addItem(tr("Simulation cell"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Cell));
    _targetTypeLabel = new QLabel(tr("Correlate:"), rollout);
    _targetTypeCombo = targetTypeUI->comboBox();
    layout->addWidget(_targetTypeLabel);
    layout->addWidget(_targetTypeCombo);

    _targetStack = new QStackedWidget(rollout);
    layout->addWidget(_targetStack);

    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(4);

        _attributeCombo = new QComboBox(page);
        _attributeCombo->setEditable(true);
        _attributeCombo->setInsertPolicy(QComboBox::NoInsert);

        _attributeUI = createParamUI<CustomParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::attributeName),
            _attributeCombo,
            [this](const QVariant& value) {
                populateAttributeList(value.toString());
            },
            [this]() -> QVariant {
                return _attributeCombo ? QVariant(_attributeCombo->currentText().trimmed()) : QVariant(QString{});
            },
            [this](RefTarget*) {
                if(AutocorrelationFunctionModifier* mod = modifier())
                    populateAttributeList(mod->attributeName());
                else
                    populateAttributeList({});
            });

        connect(_attributeCombo, &QComboBox::textActivated, _attributeUI, &CustomParameterUI::updatePropertyValue);
        if(_attributeCombo->lineEdit())
            connect(_attributeCombo->lineEdit(), &QLineEdit::editingFinished, _attributeUI, &CustomParameterUI::updatePropertyValue);

        pageLayout->addWidget(new QLabel(tr("Attribute:"), page));
        pageLayout->addWidget(_attributeCombo);
        _targetStack->addWidget(page);
    }

    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(4);

        DataObjectReferenceParameterUI* tableUI = createParamUI<DataObjectReferenceParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::table), DataTable::OOClass());
        pageLayout->addWidget(new QLabel(tr("Data table:"), page));
        pageLayout->addWidget(tableUI->comboBox());

        auto* noteLabel = new QLabel(tr("The modifier correlates the table's Y-values over time and averages over all rows for the overall curve."), page);
        noteLabel->setWordWrap(true);
        pageLayout->addWidget(noteLabel);
        _targetStack->addWidget(page);
    }

    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(4);

        DataObjectReferenceParameterUI* containerUI = createParamUI<DataObjectReferenceParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::propertyContainer), PropertyContainer::OOClass());
        containerUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
            return DataTable::OOClass().isMember(container) == false;
        });

        _propertyUI = createParamUI<PropertyReferenceParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::property), nullptr, PropertyReferenceParameterUI::ShowNoComponents, true);
        _propertyUI->setContainerField(PROPERTY_FIELD(AutocorrelationFunctionModifier::propertyContainer));
        _propertyUI->setPropertyFilter([this](const PropertyContainer*, const Property* property) {
            if(!property)
                return false;
            const bool numeric = property->dataType() == DataBuffer::Float32
                || property->dataType() == DataBuffer::Float64
                || property->dataType() == DataBuffer::Int8
                || property->dataType() == DataBuffer::Int32
                || property->dataType() == DataBuffer::Int64;
            if(!numeric || property->isTypedProperty() || property->typeId() == Property::GenericIdentifierProperty)
                return false;

            const AutocorrelationFunctionModifier* mod = modifier();
            if(!mod)
                return true;
            switch(mod->analysisMode()) {
            case AutocorrelationFunctionModifier::SignalAutocorrelation:
                return true;
            case AutocorrelationFunctionModifier::VectorReorientation:
                return property->componentCount() >= 2;
            case AutocorrelationFunctionModifier::SurvivalProbability:
                return property->componentCount() == 1;
            }
            return false;
        });

        pageLayout->addWidget(new QLabel(tr("Operate on:"), page));
        pageLayout->addWidget(containerUI->comboBox());
        pageLayout->addWidget(new QLabel(tr("Property:"), page));
        pageLayout->addWidget(_propertyUI->comboBox());

        _propertyNoteLabel = new QLabel(tr("Element-wise autocorrelation currently supports trajectories with a stable element count or stable element IDs."), page);
        _propertyNoteLabel->setWordWrap(true);
        pageLayout->addWidget(_propertyNoteLabel);
        _targetStack->addWidget(page);
    }

    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(4);

        auto* label = new QLabel(tr("Correlates the full simulation cell tensor over time, including the cell origin offset."), page);
        label->setWordWrap(true);
        pageLayout->addWidget(label);
        pageLayout->addStretch(1);
        _targetStack->addWidget(page);
    }

    _modeOptionsStack = new QStackedWidget(rollout);
    layout->addWidget(_modeOptionsStack);

    {
        auto* optionsBox = new QGroupBox(tr("Options"), _modeOptionsStack);
        auto* optionsLayout = new QVBoxLayout(optionsBox);
        optionsLayout->setContentsMargins(4, 4, 4, 4);
        optionsLayout->setSpacing(4);
        optionsLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::subtractMean))->checkBox());
        optionsLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::normalizeByZeroLag))->checkBox());
        _modeOptionsStack->addWidget(optionsBox);
    }

    {
        auto* optionsBox = new QGroupBox(tr("Options"), _modeOptionsStack);
        auto* optionsLayout = new QGridLayout(optionsBox);
        optionsLayout->setContentsMargins(4, 4, 4, 4);
        optionsLayout->setColumnStretch(1, 1);
        optionsLayout->setVerticalSpacing(4);

        IntegerParameterUI* legendreOrderUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::legendreOrder));
        optionsLayout->addWidget(legendreOrderUI->label(), 0, 0);
        optionsLayout->addLayout(legendreOrderUI->createFieldLayout(), 0, 1);

        VariantComboBoxParameterUI* selectionModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::selectionMode));
        selectionModeUI->comboBox()->addItem(tr("All elements"), QVariant::fromValue((int)AutocorrelationFunctionModifier::AllElements));
        selectionModeUI->comboBox()->addItem(tr("Selected at time origin"), QVariant::fromValue((int)AutocorrelationFunctionModifier::SelectedAtTimeOrigin));
        selectionModeUI->comboBox()->addItem(tr("Selected at both times"), QVariant::fromValue((int)AutocorrelationFunctionModifier::SelectedAtBothTimes));
        optionsLayout->addWidget(new QLabel(tr("Vector subset:"), optionsBox), 1, 0);
        optionsLayout->addWidget(selectionModeUI->comboBox(), 1, 1);

        auto* noteLabel = new QLabel(tr("Uses the chosen vector property on each element and evaluates P_l(u(0)·u(t)) over the sampled trajectory."), optionsBox);
        noteLabel->setWordWrap(true);
        optionsLayout->addWidget(noteLabel, 2, 0, 1, 2);
        _modeOptionsStack->addWidget(optionsBox);
    }

    {
        auto* optionsBox = new QGroupBox(tr("Options"), _modeOptionsStack);
        auto* optionsLayout = new QGridLayout(optionsBox);
        optionsLayout->setContentsMargins(4, 4, 4, 4);
        optionsLayout->setColumnStretch(1, 1);
        optionsLayout->setVerticalSpacing(4);

        IntegerParameterUI* intermittencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::intermittency));
        optionsLayout->addWidget(intermittencyUI->label(), 0, 0);
        optionsLayout->addLayout(intermittencyUI->createFieldLayout(), 0, 1);

        auto* noteLabel = new QLabel(tr("Treat short absences of up to N sampled frames as continuous residence when the membership property returns to nonzero."), optionsBox);
        noteLabel->setWordWrap(true);
        optionsLayout->addWidget(noteLabel, 1, 0, 1, 2);
        _modeOptionsStack->addWidget(optionsBox);
    }

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(AutocorrelationFunctionModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maxLagUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::maxLag));
    samplingLayout->addWidget(maxLagUI->label(), 1, 0);
    samplingLayout->addLayout(maxLagUI->createFieldLayout(), 1, 1);

    const QString samplingToolTip = tr("Analyze every Nth source frame. A value of 1 uses every frame, 2 uses every other frame, and so on.");
    samplingFrequencyUI->label()->setToolTip(samplingToolTip);
    maxLagUI->label()->setToolTip(tr("Largest lag to evaluate, measured in sampled-frame steps. A value of 0 uses the full sampled range."));

    auto* samplingNote = new QLabel(
        tr("The autocorrelation is averaged over all valid time origins for each lag. These controls only thin the sampled frames and limit the computed lag range."),
        samplingBox);
    samplingNote->setWordWrap(true);
    samplingLayout->addWidget(samplingNote, 2, 0, 1, 2);
    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);
    _runButton = new QPushButton(tr("Run autocorrelation analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &AutocorrelationFunctionModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    auto* runNoteLabel = new QLabel(tr("The modifier traverses the selected trajectory interval only when you click Run."), runBox);
    runNoteLabel->setWordWrap(true);
    runLayout->addWidget(runNoteLabel);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(tr("Correlation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    auto* noteLabel = new QLabel(tr("This modifier computes one trajectory correlation observable per instance and averages over all contributing elements or samples for the plotted curve."), rollout);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::contentsChanged, this, &AutocorrelationFunctionModifierEditor::updateTargetWidgets);
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &AutocorrelationFunctionModifierEditor::updateAttributeList);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &AutocorrelationFunctionModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &AutocorrelationFunctionModifierEditor::updateSummary);

    updateTargetWidgets();
    updateAttributeList();
    updatePlot();
    updateSummary();
}

/******************************************************************************
* Updates the currently visible target parameter page.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::updateTargetWidgets()
{
    if(!_targetStack)
        return;

    if(AutocorrelationFunctionModifier* mod = modifier()) {
        const bool signalMode = mod->analysisMode() == AutocorrelationFunctionModifier::SignalAutocorrelation;
        if(_targetTypeLabel)
            _targetTypeLabel->setVisible(signalMode);
        if(_targetTypeCombo)
            _targetTypeCombo->setVisible(signalMode);
        if(_modeOptionsStack)
            _modeOptionsStack->setCurrentIndex(std::clamp((int)mod->analysisMode(), 0, _modeOptionsStack->count() - 1));

        const int pageIndex = signalMode
            ? std::clamp((int)mod->targetType(), 0, _targetStack->count() - 1)
            : (int)AutocorrelationFunctionModifier::Property;
        _targetStack->setCurrentIndex(pageIndex);
        if(_propertyNoteLabel) {
            switch(mod->analysisMode()) {
            case AutocorrelationFunctionModifier::SignalAutocorrelation:
                _propertyNoteLabel->setText(tr("Element-wise autocorrelation currently supports trajectories with a stable element count or stable element IDs."));
                break;
            case AutocorrelationFunctionModifier::VectorReorientation:
                _propertyNoteLabel->setText(tr("Choose any vector property stored on a stable-ID container, such as the descriptor vectors generated upstream."));
                break;
            case AutocorrelationFunctionModifier::SurvivalProbability:
                _propertyNoteLabel->setText(tr("Choose any scalar membership property. Nonzero values are treated as present, and the analysis requires stable element IDs or a stable element count."));
                break;
            }
        }
        if(_propertyUI)
            _propertyUI->updateUI();
    }
    else {
        _targetStack->setCurrentIndex(0);
        if(_modeOptionsStack)
            _modeOptionsStack->setCurrentIndex(0);
    }
}

/******************************************************************************
* Refreshes the list of available attributes from the current pipeline input.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::updateAttributeList()
{
    handleExceptions([this]() {
        if(AutocorrelationFunctionModifier* mod = modifier())
            populateAttributeList(mod->attributeName());
        else
            populateAttributeList({});
    });
}

/******************************************************************************
* Repopulates the attribute combo box and preserves the current text.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::populateAttributeList(const QString& currentValue)
{
    if(!_attributeCombo)
        return;

    const QSignalBlocker signalBlocker(_attributeCombo);
    _attributeCombo->clear();

    QStringList attributeNames = getPipelineInput().buildAttributesMap().keys();
    attributeNames.sort(Qt::CaseInsensitive);
    for(const QString& attributeName : attributeNames)
        _attributeCombo->addItem(attributeName);

    if(!currentValue.isEmpty() && _attributeCombo->findText(currentValue) < 0)
        _attributeCombo->addItem(currentValue);

    _attributeCombo->setCurrentText(currentValue);
}

/******************************************************************************
* Launches a non-interactive evaluation of the autocorrelation modifier.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        AutocorrelationFunctionModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* acfNode = dynamic_object_cast<const AutocorrelationFunctionModificationNode>(node);
        const int startedGenerationId = acfNode ? acfNode->cacheGenerationId() : 0;
        if(_summaryLabel)
            _summaryLabel->setText(tr("Running correlation analysis over the sampled trajectory..."));

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        auto future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<AutocorrelationFunctionModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId](auto& task) noexcept {
            if(!task.isCanceled() && !task.exceptionStore())
                return;
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            AutocorrelationFunctionModifier* mod = self->modifier();
            auto* acfNode = dynamic_object_cast<AutocorrelationFunctionModificationNode>(self->modificationNode());
            if(!mod || !acfNode || mod->runRequestId() != startedRunRequestId || acfNode->cacheGenerationId() != startedGenerationId)
                return;

            acfNode->setCompletedRunRequestId(startedRunRequestId);
            self->updatePlot();
            self->updateSummary();
        });
        scheduleOperationAfter(std::move(future), [this, startedRunRequestId, startedGenerationId](const PipelineFlowState&) {
            AutocorrelationFunctionModifier* mod = modifier();
            const auto* acfNode = dynamic_object_cast<const AutocorrelationFunctionModificationNode>(modificationNode());
            if(!mod || !acfNode || mod->runRequestId() != startedRunRequestId || acfNode->cacheGenerationId() != startedGenerationId)
                return;
            updatePlot();
            updateSummary();
        });
    });
}

/******************************************************************************
* Updates the plot widget from the modifier output table.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_plot)
            return;
        if(autocorrelationAnalysisIsIdle(modifier(), modificationNode())) {
            _plot->setTable(nullptr);
            return;
        }
        const PipelineFlowState& state = getPipelineOutput();
        _plot->setTable(state.getObjectBy<DataTable>(modificationNode(), AutocorrelationFunctionModifier::correlationTableId()));
    });
}

/******************************************************************************
* Updates the summary label based on the generated global attributes.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;
        if(autocorrelationAnalysisIsIdle(modifier(), modificationNode())) {
            _summaryLabel->setText(tr("Correlation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."));
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString warningPrefix = (state.status().type() == PipelineStatus::Warning && !state.status().text().isEmpty())
                                          ? tr("Warning: %1").arg(state.status().text())
                                          : QString{};

        const QString mode = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.mode")).toString();
        const QVariant target = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.target"));
        const QVariant frameCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.sampled_frame_count"));
        const QVariant itemCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.sampled_item_count"));
        const QVariant maxLag = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.maximum_lag"));
        const QVariant zeroLag = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.zero_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.final_value"));

        if(!target.isValid()) {
            _summaryLabel->setText(tr("Correlation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."));
            return;
        }

        QString text;
        if(mode == QStringLiteral("vector_reorientation")) {
            const QVariant legendreOrder = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.legendre_order"));
            const QVariant selectionMode = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.selection_mode"));
            text = tr("Mode: Vector reorientation\nTarget: %1\nSampled frames: %2\nTracked elements: %3\nLegendre order: %4\nSubset: %5\nComputed maximum lag: %6 source frames\nZero-lag value: %7\nFinal lag value: %8")
                       .arg(target.toString(),
                            QString::number(frameCount.toInt()),
                            QString::number(itemCount.toInt()),
                            QString::number(legendreOrder.toInt()),
                            selectionMode.toString(),
                            QString::number(maxLag.toInt()),
                            zeroLag.toString(),
                            finalValue.toString());
        }
        else if(mode == QStringLiteral("survival_probability")) {
            const QVariant intermittency = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.intermittency"));
            text = tr("Mode: Survival probability\nTarget: %1\nSampled frames: %2\nTracked elements: %3\nIntermittency: %4 sampled frames\nComputed maximum lag: %5 source frames\nZero-lag value: %6\nFinal lag value: %7")
                       .arg(target.toString(),
                            QString::number(frameCount.toInt()),
                            QString::number(itemCount.toInt()),
                            QString::number(intermittency.toInt()),
                            QString::number(maxLag.toInt()),
                            zeroLag.toString(),
                            finalValue.toString());
        }
        else {
            const QVariant componentCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.component_count"));
            const QVariant subtractMean = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.subtract_mean"));
            const QVariant normalized = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.normalized"));
            text = tr("Mode: Signal autocorrelation\nTarget: %1\nSampled frames: %2\nAveraged items per frame: %3\nSignal components: %4\nComputed maximum lag: %5 source frames\nSubtract mean: %6\nNormalize by zero lag: %7\nZero-lag value: %8\nFinal lag value: %9")
                       .arg(target.toString(),
                            QString::number(frameCount.toInt()),
                            QString::number(itemCount.toInt()),
                            QString::number(componentCount.toInt()),
                            QString::number(maxLag.toInt()),
                            subtractMean.toDouble() != 0.0 ? tr("Yes") : tr("No"),
                            normalized.toDouble() != 0.0 ? tr("Yes") : tr("No"),
                            zeroLag.toString(),
                            finalValue.toString());
        }

        if(!warningPrefix.isEmpty())
            _summaryLabel->setText(warningPrefix + QStringLiteral("\n\n") + text);
        else
            _summaryLabel->setText(text);
    });
}

}   // End of namespace
