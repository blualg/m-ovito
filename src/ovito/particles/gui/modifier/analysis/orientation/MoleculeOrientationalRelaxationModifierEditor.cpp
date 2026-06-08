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
#include <ovito/particles/modifier/analysis/orientation/MoleculeOrientationalRelaxationModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/table/StretchedExponentialFit.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <QSignalBlocker>
#include "MoleculeOrientationalRelaxationModifierEditor.h"

namespace Ovito {

namespace {

bool orientationalRelaxationIsIdle(const MoleculeOrientationalRelaxationModifier* modifier, const ModificationNode* node)
{
    const auto* morNode = dynamic_object_cast<const MoleculeOrientationalRelaxationModificationNode>(node);
    return modifier && morNode && !morNode->hasCachedResults() && modifier->runRequestId() <= morNode->completedRunRequestId();
}

}  // namespace

IMPLEMENT_CREATABLE_OVITO_CLASS(MoleculeOrientationalRelaxationModifierEditor);
SET_OVITO_OBJECT_EDITOR(MoleculeOrientationalRelaxationModifier, MoleculeOrientationalRelaxationModifierEditor);

MoleculeOrientationalRelaxationModifier* MoleculeOrientationalRelaxationModifierEditor::modifier() const
{
    return static_object_cast<MoleculeOrientationalRelaxationModifier>(editObject());
}

void MoleculeOrientationalRelaxationModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Molecule orientational relaxation"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    auto* descriptorModeUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::descriptorMode));
    descriptorModeUI->comboBox()->addItem(tr("Molecular dipole vector"),
                                          QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::DipoleVector));
    descriptorModeUI->comboBox()->addItem(tr("Centroid-to-centroid vector"),
                                          QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::AtomTypeCentroidVector));
    descriptorModeUI->comboBox()->addItem(tr("Atom-pair vectors"),
                                          QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::MatchingPairVector));
    gridLayout->addWidget(new QLabel(tr("Orientation vector")), 0, 0);
    gridLayout->addWidget(descriptorModeUI->comboBox(), 0, 1);

    _manualDirectionWidget = new QWidget();
    auto* manualLayout = new QGridLayout(_manualDirectionWidget);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    manualLayout->setColumnStretch(1, 1);

    _fromTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::fromTypeId));
    auto* fromExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::fromExpression));
    manualLayout->addWidget(new QLabel(tr("Vector start atom type")), 0, 0);
    manualLayout->addWidget(createSelectorPopupRow(
        _manualDirectionWidget,
        _fromTypeUI->comboBox(),
        fromExpressionUI,
        tr("Vector start expression override"),
        tr("Use this expression instead of the vector start atom type. Leave it empty to use the type field again.")), 0, 1);

    _toTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::toTypeId));
    auto* toExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::toExpression));
    manualLayout->addWidget(new QLabel(tr("Vector end atom type")), 1, 0);
    manualLayout->addWidget(createSelectorPopupRow(
        _manualDirectionWidget,
        _toTypeUI->comboBox(),
        toExpressionUI,
        tr("Vector end expression override"),
        tr("Use this expression instead of the vector end atom type. Leave it empty to use the type field again.")), 1, 1);

    gridLayout->addWidget(_manualDirectionWidget, 1, 0, 1, 2);

    auto* referenceTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::referenceTypes));
    referenceTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or O,H"));
    auto* referenceExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::referenceExpression));
    gridLayout->addWidget(new QLabel(tr("Reference atom type(s)")), 2, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        referenceTypesUI->textBox(),
        referenceExpressionUI,
        tr("Reference expression override"),
        tr("Use this expression instead of the reference atom types. Leave it empty to use the type field again.")), 2, 1);

    auto* anchorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::anchorTypes));
    anchorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or O,H"));
    auto* anchorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::anchorExpression));
    gridLayout->addWidget(new QLabel(tr("Tracked site atom type(s)")), 3, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        anchorTypesUI->textBox(),
        anchorExpressionUI,
        tr("Tracked site expression override"),
        tr("Use this expression instead of the tracked site atom types. Leave it empty to use the type field again.")), 3, 1);

    auto* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::cutoff));
    gridLayout->addWidget(new QLabel(tr("Distance cutoff")), 4, 0);
    gridLayout->addLayout(cutoffUI->createFieldLayout(), 4, 1);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::onlySelectedParticles));
    gridLayout->addWidget(onlySelectedUI->checkBox(), 5, 0, 1, 2);

    layout->addLayout(gridLayout);

    auto* optionsBox = new QGroupBox(tr("Correlation options"), rollout);
    auto* optionsLayout = new QGridLayout(optionsBox);
    optionsLayout->setContentsMargins(4, 4, 4, 4);
    optionsLayout->setColumnStretch(1, 1);
    optionsLayout->setVerticalSpacing(4);

    IntegerParameterUI* legendreOrderUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::legendreOrder));
    optionsLayout->addWidget(legendreOrderUI->label(), 0, 0);
    optionsLayout->addLayout(legendreOrderUI->createFieldLayout(), 0, 1);

    VariantComboBoxParameterUI* selectionModeUI = createParamUI<VariantComboBoxParameterUI>(
        PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::selectionMode));
    selectionModeUI->comboBox()->addItem(tr("All descriptors"),
                                         QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::AllElements));
    selectionModeUI->comboBox()->addItem(tr("Inside shell at time origin"),
                                         QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::SelectedAtTimeOrigin));
    selectionModeUI->comboBox()->addItem(tr("Inside shell at both times"),
                                         QVariant::fromValue((int)MoleculeOrientationalRelaxationModifier::SelectedAtBothTimes));
    optionsLayout->addWidget(new QLabel(tr("Vector subset:")), 1, 0);
    optionsLayout->addWidget(selectionModeUI->comboBox(), 1, 1);

    layout->addWidget(optionsBox);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0, 0, 0, 0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingBox = new QGroupBox(tr("Sampling"), rollout);
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(4, 4, 4, 4);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* maxLagUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MoleculeOrientationalRelaxationModifier::maxLag));
    samplingLayout->addWidget(maxLagUI->label(), 1, 0);
    samplingLayout->addLayout(maxLagUI->createFieldLayout(), 1, 1);

    layout->addWidget(samplingBox);

    auto* runBox = new QGroupBox(tr("Run"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);
    _runButton = new QPushButton(tr("Run orientational relaxation analysis"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &MoleculeOrientationalRelaxationModifierEditor::runAnalysis);
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
        this, tr("Show in data inspector"), MoleculeOrientationalRelaxationModifier::correlationTableId(), 1));
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineInputChanged, this, &MoleculeOrientationalRelaxationModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsChanged, this, &MoleculeOrientationalRelaxationModifierEditor::updateDescriptorControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &MoleculeOrientationalRelaxationModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsReplaced, this, &MoleculeOrientationalRelaxationModifierEditor::updateDescriptorControls);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &MoleculeOrientationalRelaxationModifierEditor::updatePlot);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &MoleculeOrientationalRelaxationModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsChanged, this, &MoleculeOrientationalRelaxationModifierEditor::updateSummary);
    connect(this, &PropertiesEditor::contentsReplaced, this, &MoleculeOrientationalRelaxationModifierEditor::updateSummary);

    updateTypeCombos();
    updateDescriptorControls();
    updatePlot();
    updateSummary();
}

void MoleculeOrientationalRelaxationModifierEditor::updateTypeCombos()
{
    if(!_fromTypeUI || !_toTypeUI)
        return;

    const Particles* particles = getPipelineInput().getObject<Particles>();
    const Property* typeProperty = particles ? particles->getProperty(Particles::TypeProperty) : nullptr;

    const MoleculeOrientationalRelaxationModifier* modifier = static_object_cast<MoleculeOrientationalRelaxationModifier>(editObject());
    const int currentFromTypeId = modifier ? modifier->fromTypeId() : 0;
    const int currentToTypeId = modifier ? modifier->toTypeId() : 0;

    auto refillCombo = [typeProperty](VariantComboBoxParameterUI* comboUI) {
        QSignalBlocker blocker(comboUI->comboBox());
        comboUI->comboBox()->clear();
        if(!typeProperty)
            return;
        for(const ElementType* type : typeProperty->elementTypes())
            comboUI->comboBox()->addItem(type->nameOrNumericId(), QVariant::fromValue(type->numericId()));
    };
    refillCombo(_fromTypeUI);
    refillCombo(_toTypeUI);

    auto findTypeIndex = [](QComboBox* comboBox, int typeId) {
        for(int index = 0; index < comboBox->count(); ++index) {
            if(comboBox->itemData(index).toInt() == typeId)
                return index;
        }
        return -1;
    };

    int fromIndex = findTypeIndex(_fromTypeUI->comboBox(), currentFromTypeId);
    int toIndex = findTypeIndex(_toTypeUI->comboBox(), currentToTypeId);

    if(fromIndex < 0 && _fromTypeUI->comboBox()->count() > 0)
        fromIndex = 0;
    if(toIndex < 0 && _toTypeUI->comboBox()->count() > 1)
        toIndex = (fromIndex == 0 ? 1 : 0);
    else if(toIndex < 0 && _toTypeUI->comboBox()->count() > 0)
        toIndex = 0;

    if(fromIndex >= 0)
        _fromTypeUI->comboBox()->setCurrentIndex(fromIndex);
    if(toIndex >= 0)
        _toTypeUI->comboBox()->setCurrentIndex(toIndex);

    MoleculeOrientationalRelaxationModifier* mutableModifier = static_object_cast<MoleculeOrientationalRelaxationModifier>(editObject());
    if(mutableModifier) {
        if(fromIndex >= 0) {
            const int typeId = _fromTypeUI->comboBox()->itemData(fromIndex).toInt();
            if(mutableModifier->fromTypeId() != typeId)
                mutableModifier->setFromTypeId(typeId);
        }
        if(toIndex >= 0) {
            const int typeId = _toTypeUI->comboBox()->itemData(toIndex).toInt();
            if(mutableModifier->toTypeId() != typeId)
                mutableModifier->setToTypeId(typeId);
        }
        if(mutableModifier->referenceTypes().trimmed().isEmpty() && typeProperty && !typeProperty->elementTypes().empty())
            mutableModifier->setReferenceTypes(typeProperty->elementTypes().front()->nameOrNumericId());
    }
}

void MoleculeOrientationalRelaxationModifierEditor::updateDescriptorControls()
{
    if(!_manualDirectionWidget)
        return;

    const MoleculeOrientationalRelaxationModifier* modifier = static_object_cast<MoleculeOrientationalRelaxationModifier>(editObject());
    const bool visible = modifier && modifier->descriptorMode() != MoleculeOrientationalRelaxationModifier::DipoleVector;
    if(_manualDirectionWidget->isVisible() == visible)
        return;

    _manualDirectionWidget->setVisible(visible);
    _manualDirectionWidget->updateGeometry();

    for(QWidget* widget = _manualDirectionWidget->parentWidget(); widget; widget = widget->parentWidget()) {
        if(QLayout* parentLayout = widget->layout()) {
            parentLayout->invalidate();
            parentLayout->activate();
        }
        widget->updateGeometry();
    }
}

void MoleculeOrientationalRelaxationModifierEditor::runAnalysis()
{
    handleExceptions([&]() {
        MoleculeOrientationalRelaxationModifier* mod = modifier();
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        if(_runButton)
            _runButton->setEnabled(false);

        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();

        const auto* morNode = dynamic_object_cast<const MoleculeOrientationalRelaxationModificationNode>(node);
        const int startedGenerationId = morNode ? morNode->cacheGenerationId() : 0;
        if(_summaryLabel) {
            _summaryLabel->setText(tr("Running orientational relaxation analysis over the sampled trajectory..."));
            refreshSummaryGeometry();
        }

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        SharedFuture<PipelineFlowState> future = node->evaluate(request).asFuture();

        future.finally(ObjectExecutor(this), [self = QPointer<MoleculeOrientationalRelaxationModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId,
                                              future](auto& task) noexcept {
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;

            MoleculeOrientationalRelaxationModifier* mod = self->modifier();
            auto* morNode = dynamic_object_cast<MoleculeOrientationalRelaxationModificationNode>(self->modificationNode());
            if(!mod || !morNode || mod->runRequestId() != startedRunRequestId || morNode->cacheGenerationId() != startedGenerationId)
                return;

            if(task.isCanceled() || task.exceptionStore())
                morNode->setCompletedRunRequestId(startedRunRequestId);

            self->handleExceptions([&]() {
                (void)future.result();
                morNode->pipelineCache().invalidateInteractiveState();
                morNode->notifyDependents(ReferenceEvent::InteractiveStateAvailable);
                Q_EMIT self->pipelineOutputChanged();
                self->updatePlot();
                self->updateSummary();
            });

            if(self->_runButton)
                self->_runButton->setEnabled(true);
        });
    });
}

void MoleculeOrientationalRelaxationModifierEditor::updatePlot()
{
    handleExceptions([&]() {
        if(!_plot)
            return;
        if(orientationalRelaxationIsIdle(modifier(), modificationNode())) {
            _plot->setTable(nullptr);
            return;
        }
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), MoleculeOrientationalRelaxationModifier::correlationTableId());
        _plot->setTable(std::move(table));
    });
}

void MoleculeOrientationalRelaxationModifierEditor::updateSummary()
{
    handleExceptions([&]() {
        if(!_summaryLabel)
            return;
        if(orientationalRelaxationIsIdle(modifier(), modificationNode())) {
            _summaryLabel->clear();
            refreshSummaryGeometry();
            return;
        }

        const PipelineFlowState& state = getPipelineOutput();
        const QString warningText = state.status().type() == PipelineStatus::Warning
                                          ? tr("Warning: %1").arg(state.status().text())
                                          : QString{};

        const QVariant target = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.target"));
        const QVariant frameCount = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.sampled_frame_count"));
        const QVariant itemCount = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.sampled_item_count"));
        const QVariant maxLag = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.maximum_lag"));
        const QVariant legendreOrder = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.legendre_order"));
        const QVariant selectionMode = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.selection_mode"));
        const QVariant zeroLag = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.zero_lag"));
        const QVariant finalValue = state.getAttributeValue(modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation.final_value"));
        const QString fitSummary = stretchedExponentialFitSummary(
            state, modificationNode(), QStringLiteral("MoleculeOrientationalRelaxation"), tr("source frames"));

        if(!target.isValid() || !frameCount.isValid()) {
            _summaryLabel->setText(warningText.isEmpty()
                ? tr("Orientational relaxation results are being prepared...")
                : warningText);
            refreshSummaryGeometry();
            return;
        }

        QString summary = tr("Target: %1; frames: %2; descriptors: %3; max lag: %4 sampled-frame steps\nP%5; subset: %6")
                              .arg(target.toString())
                              .arg(frameCount.toInt())
                              .arg(itemCount.toInt())
                              .arg(maxLag.toDouble())
                              .arg(legendreOrder.toInt())
                              .arg(selectionMode.toString());
        QStringList valueParts;
        if(zeroLag.isValid())
            valueParts << tr("C(0): %1").arg(zeroLag.toDouble(), 0, 'g', 6);
        if(finalValue.isValid())
            valueParts << tr("final: %1").arg(finalValue.toDouble(), 0, 'g', 6);
        if(!valueParts.empty())
            summary += QStringLiteral("\n") + valueParts.join(QStringLiteral("; "));
        if(!fitSummary.isEmpty())
            summary += QStringLiteral("\n") + fitSummary;
        if(!warningText.isEmpty())
            summary = warningText + QStringLiteral("\n\n") + summary;
        _summaryLabel->setText(summary);
        refreshSummaryGeometry();
    });
}

/******************************************************************************
* Reflows the wrapped summary label after changing its contents.
******************************************************************************/
void MoleculeOrientationalRelaxationModifierEditor::refreshSummaryGeometry()
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
