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
#include <ovito/particles/modifier/properties/CalculatePropertyModifier.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <QSignalBlocker>
#include "CalculatePropertyModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CalculatePropertyModifierEditor);
SET_OVITO_OBJECT_EDITOR(CalculatePropertyModifier, CalculatePropertyModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void CalculatePropertyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Calculate property"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    auto* propertyTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::propertyType));
    propertyTypeUI->comboBox()->addItem(tr("Dipole direction"), QVariant::fromValue((int)CalculatePropertyModifier::DipoleDirection));
    propertyTypeUI->comboBox()->addItem(tr("Manual molecular direction"), QVariant::fromValue((int)CalculatePropertyModifier::ManualMolecularDirection));
    gridLayout->addWidget(new QLabel(tr("Property")), 0, 0);
    gridLayout->addWidget(propertyTypeUI->comboBox(), 0, 1);

    _manualDirectionWidget = new QWidget();
    auto* manualLayout = new QGridLayout(_manualDirectionWidget);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    manualLayout->setColumnStretch(1, 1);

    _fromTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::fromTypeId));
    manualLayout->addWidget(new QLabel(tr("From atom type")), 0, 0);
    manualLayout->addWidget(_fromTypeUI->comboBox(), 0, 1);

    _toTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::toTypeId));
    manualLayout->addWidget(new QLabel(tr("To atom type")), 1, 0);
    manualLayout->addWidget(_toTypeUI->comboBox(), 1, 1);

    gridLayout->addWidget(_manualDirectionWidget, 1, 0, 1, 2);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::onlySelectedParticles));
    gridLayout->addWidget(onlySelectedUI->checkBox(), 2, 0, 1, 2);

    layout->addLayout(gridLayout);
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineInputChanged, this, &CalculatePropertyModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsChanged, this, &CalculatePropertyModifierEditor::updateManualDirectionControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &CalculatePropertyModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsReplaced, this, &CalculatePropertyModifierEditor::updateManualDirectionControls);

    updateTypeCombos();
    updateManualDirectionControls();
}

void CalculatePropertyModifierEditor::updateTypeCombos()
{
    if(!_fromTypeUI || !_toTypeUI)
        return;

    const Particles* particles = getPipelineInput().getObject<Particles>();
    const Property* typeProperty = particles ? particles->getProperty(Particles::TypeProperty) : nullptr;

    CalculatePropertyModifier* modifier = static_object_cast<CalculatePropertyModifier>(editObject());
    const int currentFromTypeId = modifier ? modifier->fromTypeId() : 0;
    const int currentToTypeId = modifier ? modifier->toTypeId() : 0;

    {
        QSignalBlocker blocker(_fromTypeUI->comboBox());
        _fromTypeUI->comboBox()->clear();
        if(typeProperty) {
            for(const ElementType* type : typeProperty->elementTypes()) {
                const QString typeName = type->nameOrNumericId();
                _fromTypeUI->comboBox()->addItem(typeName, QVariant::fromValue(type->numericId()));
            }
        }
    }
    {
        QSignalBlocker blocker(_toTypeUI->comboBox());
        _toTypeUI->comboBox()->clear();
        if(typeProperty) {
            for(const ElementType* type : typeProperty->elementTypes()) {
                const QString typeName = type->nameOrNumericId();
                _toTypeUI->comboBox()->addItem(typeName, QVariant::fromValue(type->numericId()));
            }
        }
    }

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

    if(modifier) {
        if(fromIndex >= 0) {
            const int typeId = _fromTypeUI->comboBox()->itemData(fromIndex).toInt();
            if(modifier->fromTypeId() != typeId)
                modifier->setFromTypeId(typeId);
        }
        if(toIndex >= 0) {
            const int typeId = _toTypeUI->comboBox()->itemData(toIndex).toInt();
            if(modifier->toTypeId() != typeId)
                modifier->setToTypeId(typeId);
        }
    }

    const bool hasTypes = (_fromTypeUI->comboBox()->count() > 0);
    _fromTypeUI->setEnabled(hasTypes);
    _toTypeUI->setEnabled(hasTypes);
}

void CalculatePropertyModifierEditor::updateManualDirectionControls()
{
    if(!_manualDirectionWidget)
        return;

    const CalculatePropertyModifier* modifier = static_object_cast<CalculatePropertyModifier>(editObject());
    const bool visible = modifier && modifier->propertyType() == CalculatePropertyModifier::ManualMolecularDirection;
    if(_manualDirectionWidget->isVisible() == visible)
        return;

    _manualDirectionWidget->setVisible(visible);
    _manualDirectionWidget->updateGeometry();

    for(QWidget* widget = _manualDirectionWidget->parentWidget(); widget; widget = widget->parentWidget()) {
        if(QLayout* layout = widget->layout()) {
            layout->invalidate();
            layout->activate();
        }
        widget->updateGeometry();
    }
}

}  // namespace Ovito
