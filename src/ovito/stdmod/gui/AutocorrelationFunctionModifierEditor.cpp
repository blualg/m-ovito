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
#include <ovito/stdobj/table/StretchedExponentialFit.h>
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
#include <QCheckBox>
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
        _onlySelectedCheckBox = createParamUI<BooleanParameterUI>(
            PROPERTY_FIELD(AutocorrelationFunctionModifier::useOnlySelectedParticles))->checkBox();
        pageLayout->addWidget(_onlySelectedCheckBox);
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
    optionsLayout->addWidget(createParamUI<BooleanParameterUI>(PROPERTY_FIELD(AutocorrelationFunctionModifier::useFullVectorDotProduct))->checkBox());
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

    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);
    _runButton = new QPushButton(tr("Run autocorrelation analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &AutocorrelationFunctionModifierEditor::runAnalysis);
    runLayout->addWidget(_runButton);
    layout->addWidget(runBox);

    _summaryLabel = new QLabel(rollout);
    _summaryLabel->setWordWrap(true);
    _summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(_summaryLabel);

    _plot = new DataTablePlotWidget();
    _plot->setMinimumHeight(220);
    _plot->setMaximumHeight(220);
    layout->addWidget(_plot);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show in data inspector"), AutocorrelationFunctionModifier::correlationTableId(), 1));
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
        if(_onlySelectedCheckBox)
            _onlySelectedCheckBox->setEnabled(mod->targetType() == AutocorrelationFunctionModifier::Property);
    }
    else {
        _targetStack->setCurrentIndex(0);
        if(_onlySelectedCheckBox)
            _onlySelectedCheckBox->setEnabled(false);
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

    QStringList attributeNames;
    const PipelineFlowState inputState = getPipelineInput();
    if(inputState.data())
        attributeNames = inputState.buildAttributesMap().keys();
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

        if(_runButton)
            _runButton->setEnabled(false);

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* acfNode = dynamic_object_cast<const AutocorrelationFunctionModificationNode>(node);
        const int startedGenerationId = acfNode ? acfNode->cacheGenerationId() : 0;
        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running autocorrelation analysis over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        SharedFuture<PipelineFlowState> future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<AutocorrelationFunctionModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId,
                                              future](auto& task) noexcept {
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            AutocorrelationFunctionModifier* mod = self->modifier();
            auto* acfNode = dynamic_object_cast<AutocorrelationFunctionModificationNode>(self->modificationNode());
            if(!mod || !acfNode || mod->runRequestId() != startedRunRequestId || acfNode->cacheGenerationId() != startedGenerationId)
                return;

            if(task.isCanceled() || task.exceptionStore())
                acfNode->setCompletedRunRequestId(startedRunRequestId);

            self->handleExceptions([&]() {
                (void)future.result();
                acfNode->pipelineCache().invalidateInteractiveState();
                acfNode->notifyDependents(ReferenceEvent::InteractiveStateAvailable);
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
            _summaryLabel->clear();
            refreshSummaryGeometry();
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
        const QVariant fullVectorDotProduct = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.full_vector_dot_product"));
        const QVariant onlySelectedParticles = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.only_selected_particles"));
        const QVariant zeroLag = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.zero_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("Autocorrelation.final_value"));
        const QString fitSummary = stretchedExponentialFitSummary(
            state, modificationNode(), QStringLiteral("Autocorrelation"), tr("frames"));

        if(!target.isValid()) {
            _summaryLabel->clear();
            refreshSummaryGeometry();
            return;
        }

        QStringList lines;
        lines << tr("Target: %1; frames: %2; items/frame: %3; components: %4; max lag: %5")
                                 .arg(target.toString())
                                 .arg(frameCount.toInt())
                                 .arg(itemCount.toInt())
                                 .arg(componentCount.toInt())
                                 .arg(maxLag.toInt());
        lines << tr("Subtract mean: %1; normalize: %2; dot product: %3; selected: %4")
                                 .arg(subtractMean.toDouble() != 0.0 ? tr("Yes") : tr("No"))
                                 .arg(normalized.toDouble() != 0.0 ? tr("Yes") : tr("No"))
                                 .arg(fullVectorDotProduct.toDouble() != 0.0 ? tr("Yes") : tr("No"))
                                 .arg(onlySelectedParticles.toDouble() != 0.0 ? tr("Yes") : tr("No"));
        lines << tr("C(0): %1; final: %2")
                                 .arg(zeroLag.toString())
                                 .arg(finalValue.toString());
        if(!fitSummary.isEmpty())
            lines << fitSummary;

        const QString text = lines.join(QStringLiteral("\n"));

        if(!warningPrefix.isEmpty())
            _summaryLabel->setText(warningPrefix + QStringLiteral("\n") + text);
        else
            _summaryLabel->setText(text);
        refreshSummaryGeometry();
    });
}

/******************************************************************************
* Reflows the wrapped summary label after changing its contents.
******************************************************************************/
void AutocorrelationFunctionModifierEditor::refreshSummaryGeometry()
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

}   // End of namespace
