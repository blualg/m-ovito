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
#include <ovito/particles/modifier/analysis/orientation/PairMolecularAngleModifier.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include "PairMolecularAngleModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PairMolecularAngleModifierEditor);
SET_OVITO_OBJECT_EDITOR(PairMolecularAngleModifier, PairMolecularAngleModifierEditor);

void PairMolecularAngleModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Pair molecular angular distribution"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto addMoleculeSideItems = [this](VariantComboBoxParameterUI* ui) {
        ui->comboBox()->addItem(tr("mol1"), QVariant::fromValue((int)PairMolecularAngleModifier::Molecule1));
        ui->comboBox()->addItem(tr("mol2"), QVariant::fromValue((int)PairMolecularAngleModifier::Molecule2));
    };

    auto createSelectorField = [this, rollout](StringParameterUI* typesUI,
                                               StringParameterUI* expressionUI,
                                               const QString& expressionTitle,
                                               const QString& expressionDescription) -> QWidget* {
        typesUI->lineEdit()->setPlaceholderText(tr("e.g. O or 2"));
        return createSelectorPopupRow(
            rollout,
            typesUI->textBox(),
            expressionUI,
            expressionTitle,
            expressionDescription);
    };

    auto* pairBox = new QGroupBox(tr("Molecule pair"));
    auto* pairLayout = new QGridLayout(pairBox);
    pairLayout->setContentsMargins(6, 6, 6, 6);
    pairLayout->setColumnStretch(1, 1);

    auto* molecule1SiteTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::molecule1SiteTypes));
    auto* molecule1SiteExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::molecule1SiteExpression));
    pairLayout->addWidget(new QLabel(tr("mol1 pair site")), 0, 0);
    pairLayout->addWidget(createSelectorField(
        molecule1SiteTypesUI,
        molecule1SiteExpressionUI,
        tr("Molecule 1 pair site expression override"),
        tr("Use this expression instead of the molecule 1 pair site atom types.")), 0, 1);

    auto* molecule2SiteTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::molecule2SiteTypes));
    auto* molecule2SiteExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::molecule2SiteExpression));
    pairLayout->addWidget(new QLabel(tr("mol2 pair site")), 1, 0);
    pairLayout->addWidget(createSelectorField(
        molecule2SiteTypesUI,
        molecule2SiteExpressionUI,
        tr("Molecule 2 pair site expression override"),
        tr("Use this expression instead of the molecule 2 pair site atom types.")), 1, 1);

    auto* cutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::cutoff));
    pairLayout->addWidget(new QLabel(tr("Pair-site cutoff")), 2, 0);
    pairLayout->addLayout(cutoffUI->createFieldLayout(), 2, 1);
    layout->addWidget(pairBox);

    auto addEndpointRow = [&](QGridLayout* grid,
                              int row,
                              const QString& label,
                              VariantComboBoxParameterUI* moleculeUI,
                              StringParameterUI* typesUI,
                              StringParameterUI* expressionUI,
                              const QString& expressionTitle) {
        addMoleculeSideItems(moleculeUI);
        auto* rowWidget = new QWidget();
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        moleculeUI->comboBox()->setMinimumWidth(68);
        moleculeUI->comboBox()->setMaximumWidth(78);
        rowLayout->addWidget(moleculeUI->comboBox());
        rowLayout->addWidget(createSelectorField(
            typesUI,
            expressionUI,
            expressionTitle,
            tr("Use this expression instead of the atom type list for this vector endpoint.")), 1);
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(rowWidget, row, 1);
    };

    auto* direction1Box = new QGroupBox(tr("Direction 1"));
    auto* direction1Layout = new QGridLayout(direction1Box);
    direction1Layout->setContentsMargins(6, 6, 6, 6);
    direction1Layout->setColumnStretch(1, 1);
    addEndpointRow(
        direction1Layout,
        0,
        tr("Start"),
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1StartMolecule)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1StartTypes)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1StartExpression)),
        tr("Direction 1 start expression override"));
    addEndpointRow(
        direction1Layout,
        1,
        tr("End"),
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1EndMolecule)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1EndTypes)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction1EndExpression)),
        tr("Direction 1 end expression override"));
    layout->addWidget(direction1Box);

    auto* direction2Box = new QGroupBox(tr("Direction 2"));
    auto* direction2Layout = new QGridLayout(direction2Box);
    direction2Layout->setContentsMargins(6, 6, 6, 6);
    direction2Layout->setColumnStretch(1, 1);
    addEndpointRow(
        direction2Layout,
        0,
        tr("Start"),
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2StartMolecule)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2StartTypes)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2StartExpression)),
        tr("Direction 2 start expression override"));
    addEndpointRow(
        direction2Layout,
        1,
        tr("End"),
        createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2EndMolecule)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2EndTypes)),
        createParamUI<StringParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::direction2EndExpression)),
        tr("Direction 2 end expression override"));
    layout->addWidget(direction2Box);

    auto* samplingBox = new QGroupBox(tr("Sampling"));
    auto* samplingLayout = new QGridLayout(samplingBox);
    samplingLayout->setContentsMargins(6, 6, 6, 6);
    samplingLayout->setColumnStretch(1, 1);

    auto* angleSamplingUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::angleSamplingMode));
    angleSamplingUI->comboBox()->addItem(tr("Smallest angle per pair"), QVariant::fromValue((int)PairMolecularAngleModifier::SmallestAnglePerPair));
    angleSamplingUI->comboBox()->addItem(tr("All vector combinations"), QVariant::fromValue((int)PairMolecularAngleModifier::AllVectorCombinations));
    samplingLayout->addWidget(new QLabel(tr("Angle sampling")), 0, 0);
    samplingLayout->addWidget(angleSamplingUI->comboBox(), 0, 1);

    auto* pairRoleUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::pairRoleMode));
    pairRoleUI->comboBox()->addItem(tr("Ordered mol1/mol2 roles"), QVariant::fromValue((int)PairMolecularAngleModifier::OrderedMoleculePairs));
    pairRoleUI->comboBox()->addItem(tr("Unique unordered pairs"), QVariant::fromValue((int)PairMolecularAngleModifier::UniqueMoleculePairs));
    samplingLayout->addWidget(new QLabel(tr("Pair roles")), 1, 0);
    samplingLayout->addWidget(pairRoleUI->comboBox(), 1, 1);

    auto* numberOfBinsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::numberOfBins));
    samplingLayout->addWidget(new QLabel(tr("Angle histogram bins")), 2, 0);
    samplingLayout->addLayout(numberOfBinsUI->createFieldLayout(), 2, 1);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(PairMolecularAngleModifier::onlySelectedParticles));
    samplingLayout->addWidget(onlySelectedUI->checkBox(), 3, 0, 1, 2);
    layout->addWidget(samplingBox);

    layout->addWidget(new QLabel(tr("Angular distribution:")));

    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(180);
    _plotWidget->setMaximumHeight(180);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(
        this, tr("Show in data inspector"), PairMolecularAngleModifier::TableIdentifier, 1));

    ObjectStatusDisplay* statusDisplay = createParamUI<ObjectStatusDisplay>();
    statusDisplay->statusWidget()->setMinimumHeight(64);
    statusDisplay->statusWidget()->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(statusDisplay->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &PairMolecularAngleModifierEditor::plotDistribution);
}

void PairMolecularAngleModifierEditor::plotDistribution()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(modificationNode(), PairMolecularAngleModifier::TableIdentifier);
        _plotWidget->setTable(std::move(table));
    });
}

}  // namespace Ovito
