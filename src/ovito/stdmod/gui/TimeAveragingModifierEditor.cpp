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
#include <ovito/stdmod/modifiers/TimeAveragingModifier.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "TimeAveragingModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TimeAveragingModifierEditor);
SET_OVITO_OBJECT_EDITOR(TimeAveragingModifier, TimeAveragingModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void TimeAveragingModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Time averaging"), rolloutParams);

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(6);

    VariantComboBoxParameterUI* targetTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TimeAveragingModifier::targetType));
    targetTypeUI->comboBox()->addItem(tr("Global attribute"), QVariant::fromValue((int)TimeAveragingModifier::Attribute));
    targetTypeUI->comboBox()->addItem(tr("Data table"), QVariant::fromValue((int)TimeAveragingModifier::Table));
    targetTypeUI->comboBox()->addItem(tr("Element property"), QVariant::fromValue((int)TimeAveragingModifier::Property));
    targetTypeUI->comboBox()->addItem(tr("Simulation cell"), QVariant::fromValue((int)TimeAveragingModifier::Cell));
    layout->addWidget(new QLabel(tr("Average:")));
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
            PROPERTY_FIELD(TimeAveragingModifier::attributeName),
            _attributeCombo,
            [this](const QVariant& value) {
                populateAttributeList(value.toString());
            },
            [this]() -> QVariant {
                return _attributeCombo ? QVariant(_attributeCombo->currentText().trimmed()) : QVariant(QString{});
            },
            [this](RefTarget*) {
                if(TimeAveragingModifier* modifier = static_object_cast<TimeAveragingModifier>(editObject()))
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
            PROPERTY_FIELD(TimeAveragingModifier::table), DataTable::OOClass());
        pageLayout->addWidget(new QLabel(tr("Data table:"), page));
        pageLayout->addWidget(tableUI->comboBox());
        _targetStack->addWidget(page);
    }

    // Property target page.
    {
        auto* page = new QWidget(_targetStack);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0,0,0,0);
        pageLayout->setSpacing(4);

        DataObjectReferenceParameterUI* containerUI = createParamUI<DataObjectReferenceParameterUI>(
            PROPERTY_FIELD(TimeAveragingModifier::propertyContainer), PropertyContainer::OOClass());
        containerUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
            return DataTable::OOClass().isMember(container) == false;
        });

        PropertyReferenceParameterUI* propertyUI = createParamUI<PropertyReferenceParameterUI>(
            PROPERTY_FIELD(TimeAveragingModifier::property), nullptr, PropertyReferenceParameterUI::ShowNoComponents, true);
        propertyUI->setContainerField(PROPERTY_FIELD(TimeAveragingModifier::propertyContainer));
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

        auto* noteLabel = new QLabel(tr("Element-wise averaging currently supports trajectories with a stable element count or stable element IDs."), page);
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

        auto* label = new QLabel(tr("Averages the full simulation cell tensor over the selected frame interval."), page);
        label->setWordWrap(true);
        pageLayout->addWidget(label);
        pageLayout->addStretch(1);
        _targetStack->addWidget(page);
    }

    BooleanGroupBoxParameterUI* intervalGroupUI = createParamUI<BooleanGroupBoxParameterUI>(
        PROPERTY_FIELD(TimeAveragingModifier::useCustomFrameInterval));
    layout->addWidget(intervalGroupUI->groupBox());

    auto* intervalLayout = new QGridLayout(intervalGroupUI->childContainer());
    intervalLayout->setContentsMargins(0,0,0,0);
    intervalLayout->setColumnStretch(1, 1);
    intervalLayout->setVerticalSpacing(4);

    IntegerParameterUI* intervalStartUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeAveragingModifier::intervalStart));
    intervalLayout->addWidget(intervalStartUI->label(), 0, 0);
    intervalLayout->addLayout(intervalStartUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* intervalEndUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeAveragingModifier::intervalEnd));
    intervalLayout->addWidget(intervalEndUI->label(), 1, 0);
    intervalLayout->addLayout(intervalEndUI->createFieldLayout(), 1, 1);

    auto* samplingLayout = new QGridLayout();
    samplingLayout->setContentsMargins(0,0,0,0);
    samplingLayout->setColumnStretch(1, 1);
    samplingLayout->setVerticalSpacing(4);

    IntegerParameterUI* samplingFrequencyUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TimeAveragingModifier::samplingFrequency));
    samplingLayout->addWidget(samplingFrequencyUI->label(), 0, 0);
    samplingLayout->addLayout(samplingFrequencyUI->createFieldLayout(), 0, 1);
    layout->addLayout(samplingLayout);

    BooleanParameterUI* overwriteUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TimeAveragingModifier::overwrite));
    layout->addWidget(overwriteUI->checkBox());

    auto* generalNoteLabel = new QLabel(tr("This first open-source implementation averages one trajectory quantity per modifier instance."), rollout);
    generalNoteLabel->setWordWrap(true);
    layout->addWidget(generalNoteLabel);

    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::contentsChanged, this, &TimeAveragingModifierEditor::updateTargetWidgets);
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &TimeAveragingModifierEditor::updateAttributeList);

    updateTargetWidgets();
    updateAttributeList();
}

/******************************************************************************
* Updates the currently visible target parameter page.
******************************************************************************/
void TimeAveragingModifierEditor::updateTargetWidgets()
{
    if(!_targetStack)
        return;

    if(TimeAveragingModifier* modifier = static_object_cast<TimeAveragingModifier>(editObject())) {
        const int pageIndex = std::clamp((int)modifier->targetType(), 0, _targetStack->count() - 1);
        _targetStack->setCurrentIndex(pageIndex);
    }
    else {
        _targetStack->setCurrentIndex(0);
    }
}

/******************************************************************************
* Refreshes the list of available attributes from the current pipeline input.
******************************************************************************/
void TimeAveragingModifierEditor::updateAttributeList()
{
    handleExceptions([this]() {
        if(TimeAveragingModifier* modifier = static_object_cast<TimeAveragingModifier>(editObject()))
            populateAttributeList(modifier->attributeName());
        else
            populateAttributeList({});
    });
}

/******************************************************************************
* Repopulates the attribute combo box and preserves the current text.
******************************************************************************/
void TimeAveragingModifierEditor::populateAttributeList(const QString& currentValue)
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

}   // End of namespace
