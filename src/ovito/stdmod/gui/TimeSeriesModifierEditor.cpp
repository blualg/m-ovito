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
#include <ovito/stdmod/modifiers/TimeSeriesModifier.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "TimeSeriesModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeSeriesModifierEditor);
SET_OVITO_OBJECT_EDITOR(TimeSeriesModifier, TimeSeriesModifierEditor);

void TimeSeriesModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Time series"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    VariantComboBoxParameterUI* targetTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::targetType));
    targetTypeUI->comboBox()->addItem(tr("Global attribute"), QVariant::fromValue((int)TimeSeriesModifier::Attribute));
    targetTypeUI->comboBox()->addItem(tr("Data table"), QVariant::fromValue((int)TimeSeriesModifier::Table));
    targetTypeUI->comboBox()->addItem(tr("Element property"), QVariant::fromValue((int)TimeSeriesModifier::Property));
    targetTypeUI->comboBox()->addItem(tr("Simulation cell"), QVariant::fromValue((int)TimeSeriesModifier::Cell));
    layout->addWidget(new QLabel(tr("Plot:")));
    layout->addWidget(targetTypeUI->comboBox());

    _targetStack = new QStackedWidget(rollout);
    layout->addWidget(_targetStack);

    // Attribute target page.
    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0,0,0,0);
        pageLayout->setSpacing(4);

        _attributeCombo = new QComboBox(page);
        _attributeCombo->setEditable(true);
        _attributeCombo->setInsertPolicy(QComboBox::NoInsert);

        _attributeUI = createParamUI<CustomParameterUI>(
            PROPERTY_FIELD(TimeSeriesModifier::attributeName),
            _attributeCombo,
            [this](const QVariant& value) {
                populateAttributeList(value.toString());
            },
            [this]() -> QVariant {
                return _attributeCombo ? QVariant(_attributeCombo->currentText().trimmed()) : QVariant(QString{});
            },
            [this](RefTarget*) {
                if(TimeSeriesModifier* modifier = static_object_cast<TimeSeriesModifier>(editObject()))
                    populateAttributeList(modifier->attributeName());
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

    // Table target page.
    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0,0,0,0);
        pageLayout->setSpacing(4);

        DataObjectReferenceParameterUI* tableUI = createParamUI<DataObjectReferenceParameterUI>(
            PROPERTY_FIELD(TimeSeriesModifier::table), DataTable::OOClass());
        pageLayout->addWidget(new QLabel(tr("Data table:"), page));
        pageLayout->addWidget(tableUI->comboBox());

        auto* noteLabel = new QLabel(tr("The selected table is reduced to one value per frame using the chosen reduction mode."), page);
        noteLabel->setWordWrap(true);
        pageLayout->addWidget(noteLabel);
        _targetStack->addWidget(page);
    }

    // Property target page.
    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0,0,0,0);
        pageLayout->setSpacing(4);

        DataObjectReferenceParameterUI* containerUI = createParamUI<DataObjectReferenceParameterUI>(
            PROPERTY_FIELD(TimeSeriesModifier::propertyContainer), PropertyContainer::OOClass());
        containerUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
            return DataTable::OOClass().isMember(container) == false;
        });

        PropertyReferenceParameterUI* propertyUI = createParamUI<PropertyReferenceParameterUI>(
            PROPERTY_FIELD(TimeSeriesModifier::property), nullptr, PropertyReferenceParameterUI::ShowComponentsAndVectorProperties, true);
        propertyUI->setContainerField(PROPERTY_FIELD(TimeSeriesModifier::propertyContainer));
        propertyUI->setPropertyFilter([](const PropertyContainer*, const Property* property) {
            return property
                && (property->dataType() == DataBuffer::Float32
                    || property->dataType() == DataBuffer::Float64
                    || property->dataType() == DataBuffer::Int8
                    || property->dataType() == DataBuffer::Int32
                    || property->dataType() == DataBuffer::Int64)
                && property->typeId() != Property::GenericIdentifierProperty;
        });

        pageLayout->addWidget(new QLabel(tr("Operate on:"), page));
        pageLayout->addWidget(containerUI->comboBox());
        pageLayout->addWidget(new QLabel(tr("Property or component:"), page));
        pageLayout->addWidget(propertyUI->comboBox());

        auto* noteLabel = new QLabel(tr("Element properties are reduced to one value per frame using the selected reduction mode. Vector properties can be sampled per component."), page);
        noteLabel->setWordWrap(true);
        pageLayout->addWidget(noteLabel);
        _targetStack->addWidget(page);
    }

    // Cell target page.
    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0,0,0,0);
        pageLayout->setSpacing(4);

        auto* label = new QLabel(tr("Samples the full simulation cell tensor over time and plots each cell component as a separate curve."), page);
        label->setWordWrap(true);
        pageLayout->addWidget(label);
        pageLayout->addStretch(1);
        _targetStack->addWidget(page);
    }

    _reductionWidget = new QWidget(rollout);
    auto* reductionLayout = new QGridLayout(_reductionWidget);
    reductionLayout->setContentsMargins(0,0,0,0);
    reductionLayout->setColumnStretch(1, 1);
    reductionLayout->setVerticalSpacing(4);

    VariantComboBoxParameterUI* reductionModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::reductionMode));
    reductionModeUI->comboBox()->addItem(tr("Mean"), QVariant::fromValue((int)TimeSeriesModifier::Mean));
    reductionModeUI->comboBox()->addItem(tr("Sum"), QVariant::fromValue((int)TimeSeriesModifier::Sum));
    reductionModeUI->comboBox()->addItem(tr("Minimum"), QVariant::fromValue((int)TimeSeriesModifier::Minimum));
    reductionModeUI->comboBox()->addItem(tr("Maximum"), QVariant::fromValue((int)TimeSeriesModifier::Maximum));
    reductionLayout->addWidget(new QLabel(tr("Reduction:"), _reductionWidget), 0, 0);
    reductionLayout->addWidget(reductionModeUI->comboBox(), 0, 1);
    layout->addWidget(_reductionWidget);

    auto* xAxisLayout = new QGridLayout();
    xAxisLayout->setContentsMargins(0,0,0,0);
    xAxisLayout->setColumnStretch(1, 1);
    xAxisLayout->setVerticalSpacing(4);

    VariantComboBoxParameterUI* xAxisModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::xAxisMode));
    xAxisModeUI->comboBox()->addItem(tr("Frame number"), QVariant::fromValue((int)TimeSeriesModifier::Frame));
    xAxisModeUI->comboBox()->addItem(tr("Animation time"), QVariant::fromValue((int)TimeSeriesModifier::AnimationTime));
    xAxisLayout->addWidget(new QLabel(tr("Horizontal axis:"), rollout), 0, 0);
    xAxisLayout->addWidget(xAxisModeUI->comboBox(), 0, 1);
    layout->addLayout(xAxisLayout);

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(TimeSeriesModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0,0,0,0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingLayout = new QGridLayout();
    samplingLayout->setContentsMargins(0,0,0,0);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeSeriesModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);
    layout->addLayout(samplingLayout);

    auto* runBox = new QGroupBox(tr("Start"), rollout);
    auto* runLayout = new QVBoxLayout(runBox);
    runLayout->setContentsMargins(4, 4, 4, 4);
    runLayout->setSpacing(4);
    _runButton = new QPushButton(tr("Start series"), runBox);
    connect(_runButton, &QPushButton::clicked, this, &TimeSeriesModifierEditor::runSeries);
    runLayout->addWidget(_runButton);
    auto* runNoteLabel = new QLabel(tr("The modifier stays idle after insertion and traverses the selected trajectory interval only when you click Start series."), runBox);
    runNoteLabel->setWordWrap(true);
    runLayout->addWidget(runNoteLabel);
    layout->addWidget(runBox);

    auto* generalNoteLabel = new QLabel(tr("This modifier samples one trajectory quantity per modifier instance and writes the result as a line-plot data table."), rollout);
    generalNoteLabel->setWordWrap(true);
    layout->addWidget(generalNoteLabel);

    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::contentsChanged, this, &TimeSeriesModifierEditor::updateTargetWidgets);
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &TimeSeriesModifierEditor::updateAttributeList);

    updateTargetWidgets();
    updateAttributeList();
}

void TimeSeriesModifierEditor::updateTargetWidgets()
{
    if(!_targetStack)
        return;

    if(TimeSeriesModifier* modifier = static_object_cast<TimeSeriesModifier>(editObject())) {
        const int pageIndex = std::clamp((int)modifier->targetType(), 0, _targetStack->count() - 1);
        _targetStack->setCurrentIndex(pageIndex);
        if(_reductionWidget)
            _reductionWidget->setVisible(modifier->targetType() == TimeSeriesModifier::Table || modifier->targetType() == TimeSeriesModifier::Property);
    }
    else {
        _targetStack->setCurrentIndex(0);
        if(_reductionWidget)
            _reductionWidget->setVisible(true);
    }
}

void TimeSeriesModifierEditor::updateAttributeList()
{
    handleExceptions([this]() {
        if(TimeSeriesModifier* modifier = static_object_cast<TimeSeriesModifier>(editObject()))
            populateAttributeList(modifier->attributeName());
        else
            populateAttributeList({});
    });
}

void TimeSeriesModifierEditor::populateAttributeList(const QString& currentValue)
{
    if(!_attributeCombo)
        return;

    const QSignalBlocker signalBlocker(_attributeCombo);
    _attributeCombo->clear();

    QStringList attributeNames;
    const PipelineFlowState inputState = getPipelineInput();
    if(inputState.data()) {
        const QVariantMap attributes = inputState.buildAttributesMap();
        for(auto iter = attributes.constBegin(); iter != attributes.constEnd(); ++iter) {
            bool ok = false;
            iter.value().toDouble(&ok);
            if(ok)
                attributeNames.push_back(iter.key());
        }
    }
    attributeNames.sort(Qt::CaseInsensitive);
    for(const QString& attributeName : attributeNames)
        _attributeCombo->addItem(attributeName);

    if(!currentValue.isEmpty() && _attributeCombo->findText(currentValue) < 0)
        _attributeCombo->addItem(currentValue);

    _attributeCombo->setCurrentText(currentValue);
}

void TimeSeriesModifierEditor::runSeries()
{
    handleExceptions([&]() {
        TimeSeriesModifier* mod = static_object_cast<TimeSeriesModifier>(editObject());
        ModificationNode* node = modificationNode();
        if(!mod || !node)
            return;

        mod->setEnabled(true);
        mod->setRunRequestId(mod->runRequestId() + 1);
        const int startedRunRequestId = mod->runRequestId();
        const auto* seriesNode = dynamic_object_cast<const TimeSeriesModificationNode>(node);
        const int startedGenerationId = seriesNode ? seriesNode->cacheGenerationId() : 0;
        if(_runButton)
            _runButton->setEnabled(false);

        PipelineEvaluationRequest request(currentAnimationTime(), false, false);
        SharedFuture<PipelineFlowState> future = node->evaluate(request).asFuture();
        future.finally(ObjectExecutor(this), [self = QPointer<TimeSeriesModifierEditor>(this),
                                              editObject = OOWeakRef<RefTarget>(editObject()),
                                              startedRunRequestId,
                                              startedGenerationId,
                                              future](auto& task) noexcept {
            if(self.isNull() || self->editObject() != editObject.lock().get())
                return;
            TimeSeriesModifier* mod = static_object_cast<TimeSeriesModifier>(self->editObject());
            auto* seriesNode = dynamic_object_cast<TimeSeriesModificationNode>(self->modificationNode());
            if(!mod || !seriesNode || mod->runRequestId() != startedRunRequestId || seriesNode->cacheGenerationId() != startedGenerationId)
                return;
            if(task.isCanceled() || task.exceptionStore())
                seriesNode->setCompletedRunRequestId(startedRunRequestId);
            self->handleExceptions([&]() {
                (void)future.result();
            });
            if(self->_runButton)
                self->_runButton->setEnabled(true);
        });
    });
}

}   // End of namespace
