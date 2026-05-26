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
#include <ovito/particles/modifier/analysis/orientation/MolecularOrientationModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QSignalBlocker>
#include "MolecularOrientationModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MolecularOrientationModifierEditor);
SET_OVITO_OBJECT_EDITOR(MolecularOrientationModifier, MolecularOrientationModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void MolecularOrientationModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Angular distribution"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    auto* directionModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::directionMode));
    directionModeUI->comboBox()->addItem(tr("Dipole vector"), QVariant::fromValue((int)MolecularOrientationModifier::DipoleDirection));
    directionModeUI->comboBox()->addItem(tr("Atom-type centroid vector"), QVariant::fromValue((int)MolecularOrientationModifier::ManualMolecularDirection));
    directionModeUI->comboBox()->addItem(tr("Matching pair vector"), QVariant::fromValue((int)MolecularOrientationModifier::MatchingPairVector));
    gridLayout->addWidget(new QLabel(tr("Descriptor")), 0, 0);
    gridLayout->addWidget(directionModeUI->comboBox(), 0, 1);

    _manualDirectionWidget = new QWidget();
    auto* manualLayout = new QGridLayout(_manualDirectionWidget);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    manualLayout->setColumnStretch(1, 1);

    _fromTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::fromTypeId));
    auto* fromExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::fromExpression));
    manualLayout->addWidget(new QLabel(tr("Direction start atom type")), 0, 0);
    manualLayout->addWidget(createSelectorPopupRow(
        _manualDirectionWidget,
        _fromTypeUI->comboBox(),
        fromExpressionUI,
        tr("Direction start expression override"),
        tr("Use this expression instead of the direction start atom type. Leave it empty to use the type field again.")), 0, 1);

    _toTypeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::toTypeId));
    auto* toExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::toExpression));
    manualLayout->addWidget(new QLabel(tr("Direction end atom type")), 1, 0);
    manualLayout->addWidget(createSelectorPopupRow(
        _manualDirectionWidget,
        _toTypeUI->comboBox(),
        toExpressionUI,
        tr("Direction end expression override"),
        tr("Use this expression instead of the direction end atom type. Leave it empty to use the type field again.")), 1, 1);

    gridLayout->addWidget(_manualDirectionWidget, 1, 0, 1, 2);

    auto* referenceTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::referenceTypes));
    referenceTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or O,H"));
    referenceTypesUI->setToolTip(tr("Comma-separated particle type names or numeric IDs used to define the reference site. "
                                    "A single type uses each matching atom as a reference. Multiple types use one COM per molecule unless an expression override is provided."));
    auto* referenceExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::referenceExpression));
    gridLayout->addWidget(new QLabel(tr("Orient around atom type(s)")), 2, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        referenceTypesUI->textBox(),
        referenceExpressionUI,
        tr("Reference expression override"),
        tr("Use this expression instead of the reference atom types. Leave it empty to use the type field again.")), 2, 1);

    auto* anchorTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::anchorTypes));
    anchorTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O or O,H"));
    anchorTypesUI->setToolTip(tr("Comma-separated particle type names or numeric IDs used to define the molecule anchor point. "
                                 "If multiple atoms match, their center of mass is used."));
    auto* anchorExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::anchorExpression));
    gridLayout->addWidget(new QLabel(tr("Molecule site atom type(s)")), 3, 0);
    gridLayout->addWidget(createSelectorPopupRow(
        rollout,
        anchorTypesUI->textBox(),
        anchorExpressionUI,
        tr("Molecule site expression override"),
        tr("Use this expression instead of the molecule site atom types. Leave it empty to use the type field again.")), 3, 1);

    auto* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::cutoff));
    gridLayout->addWidget(new QLabel(tr("Distance cutoff")), 4, 0);
    gridLayout->addLayout(cutoffUI->createFieldLayout(), 4, 1);

    auto* numberOfBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::numberOfBins));
    gridLayout->addWidget(new QLabel(tr("Angle histogram bins")), 5, 0);
    gridLayout->addLayout(numberOfBinsUI->createFieldLayout(), 5, 1);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(MolecularOrientationModifier::onlySelectedParticles));
    gridLayout->addWidget(onlySelectedUI->checkBox(), 6, 0, 1, 2);

    layout->addLayout(gridLayout);
    layout->addSpacing(8);
    auto* descriptorNote = new QLabel(tr("A descriptor-particle container is written for downstream histogram and correlation analysis. "
                                         "Use its selection property to restrict downstream modifiers to entries inside the reference shell."));
    descriptorNote->setWordWrap(true);
    layout->addWidget(descriptorNote);
    layout->addSpacing(6);
    layout->addWidget(new QLabel(tr("Angular distribution:")));

    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(200);
    _plotWidget->setMaximumHeight(200);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineInputChanged, this, &MolecularOrientationModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsChanged, this, &MolecularOrientationModifierEditor::updateManualDirectionControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &MolecularOrientationModifierEditor::updateTypeCombos);
    connect(this, &PropertiesEditor::contentsReplaced, this, &MolecularOrientationModifierEditor::updateManualDirectionControls);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &MolecularOrientationModifierEditor::plotDistribution);

    updateTypeCombos();
    updateManualDirectionControls();
}

void MolecularOrientationModifierEditor::updateTypeCombos()
{
    if(!_fromTypeUI || !_toTypeUI)
        return;

    const Particles* particles = getPipelineInput().getObject<Particles>();
    const Property* typeProperty = particles ? particles->getProperty(Particles::TypeProperty) : nullptr;

    const MolecularOrientationModifier* modifier = static_object_cast<MolecularOrientationModifier>(editObject());
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

    MolecularOrientationModifier* mutableModifier = static_object_cast<MolecularOrientationModifier>(editObject());
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

void MolecularOrientationModifierEditor::updateManualDirectionControls()
{
    if(!_manualDirectionWidget)
        return;

    const MolecularOrientationModifier* modifier = static_object_cast<MolecularOrientationModifier>(editObject());
    const bool visible = modifier && modifier->directionMode() != MolecularOrientationModifier::DipoleDirection;
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

void MolecularOrientationModifierEditor::plotDistribution()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), MolecularOrientationModifier::TableIdentifier);
        _plotWidget->setTable(std::move(table));
    });
}

}  // namespace Ovito
