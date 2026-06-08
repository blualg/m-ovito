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
#include <ovito/particles/modifier/analysis/orientation/GeometryDescriptorModifier.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QLabel>
#include <QSizePolicy>
#include "GeometryDescriptorModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GeometryDescriptorModifierEditor);
SET_OVITO_OBJECT_EDITOR(GeometryDescriptorModifier, GeometryDescriptorModifierEditor);

void GeometryDescriptorModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Geometry descriptors"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setColumnStretch(1, 1);

    auto* modeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::descriptorMode));
    modeUI->comboBox()->addItem(tr("Vector: atom 1 -> atom 2"), QVariant::fromValue((int)GeometryDescriptorModifier::VectorDescriptor));
    modeUI->comboBox()->addItem(tr("Angle: atom 1 - atom 2 - atom 3"), QVariant::fromValue((int)GeometryDescriptorModifier::AngleDescriptor));
    modeUI->comboBox()->addItem(tr("Dihedral: atom 1 - atom 2 - atom 3 - atom 4"), QVariant::fromValue((int)GeometryDescriptorModifier::DihedralDescriptor));
    grid->addWidget(new QLabel(tr("Descriptor")), 0, 0);
    grid->addWidget(modeUI->comboBox(), 0, 1);

    auto* atomRoleSelectionUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atomRoleSelectionMode));
    atomRoleSelectionUI->comboBox()->addItem(tr("Type/expression selectors"), QVariant::fromValue((int)GeometryDescriptorModifier::TypeExpressionSelection));
    atomRoleSelectionUI->comboBox()->addItem(tr("Template molecule atom IDs"), QVariant::fromValue((int)GeometryDescriptorModifier::TemplateMoleculeAtomIds));
    grid->addWidget(new QLabel(tr("Atom role selection")), 1, 0);
    grid->addWidget(atomRoleSelectionUI->comboBox(), 1, 1);

    auto addSelectorRow = [this, rollout, grid](int row,
                                                QWidget** labelWidget,
                                                const QString& label,
                                                const QString& expressionTitle,
                                                const QString& expressionDescription,
                                                StringParameterUI* typesUI,
                                                StringParameterUI* expressionUI) -> QWidget* {
        typesUI->lineEdit()->setPlaceholderText(tr("e.g. O or 2"));
        auto* labelObject = new QLabel(label);
        if(labelWidget)
            *labelWidget = labelObject;
        grid->addWidget(labelObject, row, 0);
        QWidget* selectorWidget = createSelectorPopupRow(
            rollout,
            typesUI->textBox(),
            expressionUI,
            expressionTitle,
            expressionDescription);
        grid->addWidget(selectorWidget, row, 1);
        return selectorWidget;
    };

    addSelectorRow(
        2,
        &_atom1Label,
        tr("Atom 1"),
        tr("Atom 1 expression override"),
        tr("Use this expression instead of the atom 1 type list."),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom1Types)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom1Expression)));

    addSelectorRow(
        3,
        &_atom2Label,
        tr("Atom 2"),
        tr("Atom 2 expression override"),
        tr("Use this expression instead of the atom 2 type list."),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom2Types)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom2Expression)));

    _atom3Widget = addSelectorRow(
        4,
        &_atom3Label,
        tr("Atom 3"),
        tr("Atom 3 expression override"),
        tr("Use this expression instead of the atom 3 type list."),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom3Types)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom3Expression)));

    _atom4Widget = addSelectorRow(
        5,
        &_atom4Label,
        tr("Atom 4"),
        tr("Atom 4 expression override"),
        tr("Use this expression instead of the atom 4 type list."),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom4Types)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom4Expression)));

    _atom1Widget = qobject_cast<QWidget*>(grid->itemAtPosition(2, 1)->widget());
    _atom2Widget = qobject_cast<QWidget*>(grid->itemAtPosition(3, 1)->widget());

    _templateWidget = new QWidget();
    auto* templateLayout = new QVBoxLayout(_templateWidget);
    templateLayout->setContentsMargins(0, 0, 0, 0);
    templateLayout->setSpacing(4);

    auto createIntegerRow = [this](const QString& label, IntegerParameterUI* ui) {
        auto* row = new QWidget();
        auto* rowLayout = new QGridLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setColumnStretch(1, 1);
        rowLayout->addWidget(new QLabel(label), 0, 0);
        rowLayout->addLayout(ui->createFieldLayout(), 0, 1);
        return row;
    };

    templateLayout->addWidget(createIntegerRow(
        tr("Template molecule ID"),
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::templateMoleculeId))));
    templateLayout->addWidget(createIntegerRow(
        tr("Atom 1 particle ID"),
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom1ParticleId))));
    templateLayout->addWidget(createIntegerRow(
        tr("Atom 2 particle ID"),
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom2ParticleId))));
    _templateAtom3Row = createIntegerRow(
        tr("Atom 3 particle ID"),
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom3ParticleId)));
    templateLayout->addWidget(_templateAtom3Row);
    _templateAtom4Row = createIntegerRow(
        tr("Atom 4 particle ID"),
        createParamUI<IntegerParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::atom4ParticleId)));
    templateLayout->addWidget(_templateAtom4Row);

    grid->addWidget(_templateWidget, 6, 0, 1, 2);

    _normalizeWidget = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::normalizeVectors))->checkBox();
    grid->addWidget(_normalizeWidget, 7, 0, 1, 2);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(GeometryDescriptorModifier::onlySelectedParticles));
    grid->addWidget(onlySelectedUI->checkBox(), 8, 0, 1, 2);

    layout->addLayout(grid);
    layout->addSpacing(4);

    layout->addWidget(new QLabel(tr("Descriptor values:")));

    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(180);
    _plotWidget->setMaximumHeight(180);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show in data inspector"), GeometryDescriptorModifier::TableIdentifier, 1));

    ObjectStatusDisplay* statusDisplay = createParamUI<ObjectStatusDisplay>();
    statusDisplay->statusWidget()->setMinimumHeight(64);
    statusDisplay->statusWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(statusDisplay->statusWidget());

    connect(this, &PropertiesEditor::contentsChanged, this, &GeometryDescriptorModifierEditor::updateModeControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &GeometryDescriptorModifierEditor::updateModeControls);
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &GeometryDescriptorModifierEditor::plotValues);

    updateModeControls();
}

void GeometryDescriptorModifierEditor::updateModeControls()
{
    const GeometryDescriptorModifier* modifier = static_object_cast<GeometryDescriptorModifier>(editObject());
    const GeometryDescriptorModifier::DescriptorMode mode = modifier
        ? modifier->descriptorMode()
        : GeometryDescriptorModifier::VectorDescriptor;
    const bool templateMode = modifier && modifier->atomRoleSelectionMode() == GeometryDescriptorModifier::TemplateMoleculeAtomIds;

    if(_atom1Label)
        _atom1Label->setVisible(!templateMode);
    if(_atom1Widget)
        _atom1Widget->setVisible(!templateMode);
    if(_atom2Label)
        _atom2Label->setVisible(!templateMode);
    if(_atom2Widget)
        _atom2Widget->setVisible(!templateMode);
    if(_atom3Label)
        _atom3Label->setVisible(!templateMode && (mode == GeometryDescriptorModifier::AngleDescriptor || mode == GeometryDescriptorModifier::DihedralDescriptor));
    if(_atom3Widget)
        _atom3Widget->setVisible(!templateMode && (mode == GeometryDescriptorModifier::AngleDescriptor || mode == GeometryDescriptorModifier::DihedralDescriptor));
    if(_atom4Label)
        _atom4Label->setVisible(!templateMode && mode == GeometryDescriptorModifier::DihedralDescriptor);
    if(_atom4Widget)
        _atom4Widget->setVisible(!templateMode && mode == GeometryDescriptorModifier::DihedralDescriptor);
    if(_templateWidget)
        _templateWidget->setVisible(templateMode);
    if(_templateAtom3Row)
        _templateAtom3Row->setVisible(mode == GeometryDescriptorModifier::AngleDescriptor || mode == GeometryDescriptorModifier::DihedralDescriptor);
    if(_templateAtom4Row)
        _templateAtom4Row->setVisible(mode == GeometryDescriptorModifier::DihedralDescriptor);
    if(_normalizeWidget)
        _normalizeWidget->setVisible(mode == GeometryDescriptorModifier::VectorDescriptor);

    if(QWidget* rollout = _atom3Widget ? _atom3Widget->parentWidget() : nullptr) {
        if(QLayout* layout = rollout->layout()) {
            layout->invalidate();
            layout->activate();
        }
        rollout->updateGeometry();
    }
}

void GeometryDescriptorModifierEditor::plotValues()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), GeometryDescriptorModifier::TableIdentifier);
        _plotWidget->setTable(std::move(table));
    });
}

}  // namespace Ovito
