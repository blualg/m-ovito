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

    VariantComboBoxParameterUI* targetTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::targetType));
    targetTypeUI->comboBox()->addItem(tr("Global attribute"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Attribute));
    targetTypeUI->comboBox()->addItem(tr("Data table"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Table));
    targetTypeUI->comboBox()->addItem(tr("Element property"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Property));
    targetTypeUI->comboBox()->addItem(tr("Simulation cell"), QVariant::fromValue((int)AutocorrelationFunctionModifier::Cell));
    layout->addWidget(new QLabel(tr("Correlate:"), rollout));
    layout->addWidget(targetTypeUI->comboBox());

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

        PropertyReferenceParameterUI* propertyUI = createParamUI<PropertyReferenceParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::property), nullptr, PropertyReferenceParameterUI::ShowNoComponents, true);
        propertyUI->setContainerField(PROPERTY_FIELD(AutocorrelationFunctionModifier::propertyContainer));
        propertyUI->setPropertyFilter([](const PropertyContainer*, const Property* property) {
            return property
                && (property->dataType() == DataBuffer::Float32
                    || property->dataType() == DataBuffer::Float64
                    || property->dataType() == DataBuffer::Int8
                    || property->dataType() == DataBuffer::Int32
                    || property->dataType() == DataBuffer::Int64)
                && !property->isTypedProperty()
                && property->typeId() != Property::GenericIdentifierProperty;
        });

        pageLayout->addWidget(new QLabel(tr("Operate on:"), page));
        pageLayout->addWidget(containerUI->comboBox());
        pageLayout->addWidget(new QLabel(tr("Property:"), page));
        pageLayout->addWidget(propertyUI->comboBox());

        auto* noteLabel = new QLabel(tr("Element-wise autocorrelation currently supports trajectories with a stable element count or stable element IDs."), page);
        noteLabel->setWordWrap(true);
        pageLayout->addWidget(noteLabel);
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

    auto* optionsBox = new QGroupBox(tr("Options"), rollout);
    auto* optionsLayout = new QVBoxLayout(optionsBox);
    optionsLayout->setContentsMargins(4, 4, 4, 4);
    optionsLayout->setSpacing(4);
    optionsLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::subtractMean))->checkBox());
    optionsLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::normalizeByZeroLag))->checkBox());
    layout->addWidget(optionsBox);

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

    _summaryLabel = new QLabel(tr("Autocorrelation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."), rollout);
    _summaryLabel->setWordWrap(true);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    auto* noteLabel = new QLabel(tr("This first open-source implementation computes one trajectory autocorrelation per modifier instance and averages over all elements or table rows for the overall curve."), rollout);
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
        const int pageIndex = std::clamp((int)mod->targetType(), 0, _targetStack->count() - 1);
        _targetStack->setCurrentIndex(pageIndex);
    }
    else {
        _targetStack->setCurrentIndex(0);
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
            _summaryLabel->setText(tr("Running autocorrelation analysis over the sampled trajectory..."));

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
            _summaryLabel->setText(tr("Autocorrelation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."));
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString warningPrefix = (state.status().type() == PipelineStatus::Warning && !state.status().text().isEmpty())
                                          ? tr("Warning: %1").arg(state.status().text())
                                          : QString{};

        const QVariant target = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.target"));
        const QVariant frameCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.sampled_frame_count"));
        const QVariant itemCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.sampled_item_count"));
        const QVariant componentCount = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.component_count"));
        const QVariant maxLag = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.maximum_lag"));
        const QVariant subtractMean = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.subtract_mean"));
        const QVariant normalized = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.normalized"));
        const QVariant zeroLag = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.zero_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.final_value"));

        if(!target.isValid()) {
            _summaryLabel->setText(tr("Autocorrelation results are idle. Open the Run section and click 'Run autocorrelation analysis' to compute the selected observable."));
            return;
        }

        const QString text = tr("Target: %1\nSampled frames: %2\nAveraged items per frame: %3\nSignal components: %4\nComputed maximum lag: %5 source frames\nSubtract mean: %6\nNormalize by zero lag: %7\nZero-lag value: %8\nFinal lag value: %9")
                                 .arg(target.toString(),
                                      QString::number(frameCount.toInt()),
                                      QString::number(itemCount.toInt()),
                                      QString::number(componentCount.toInt()),
                                      QString::number(maxLag.toInt()),
                                      subtractMean.toDouble() != 0.0 ? tr("Yes") : tr("No"),
                                      normalized.toDouble() != 0.0 ? tr("Yes") : tr("No"),
                                      zeroLag.toString(),
                                      finalValue.toString());

        if(!warningPrefix.isEmpty())
            _summaryLabel->setText(warningPrefix + QStringLiteral("\n\n") + text);
        else
            _summaryLabel->setText(text);
    });
}

}   // End of namespace
