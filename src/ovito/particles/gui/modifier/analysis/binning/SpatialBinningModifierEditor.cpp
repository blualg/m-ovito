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
#include <ovito/particles/modifier/analysis/binning/SpatialBinningModifier.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "SpatialBinningModifierEditor.h"

#include <QLabel>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SpatialBinningModifierEditor);
SET_OVITO_OBJECT_EDITOR(SpatialBinningModifier, SpatialBinningModifierEditor);

void SpatialBinningModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Bin and reduce"), rolloutParams, "manual:particles.modifiers.bin_and_reduce");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    PropertyReferenceParameterUI* propertyUI = createParamUI<PropertyReferenceParameterUI>(
        PROPERTY_FIELD(SpatialBinningModifier::sourceProperty),
        &Particles::OOClass(),
        PropertyReferenceParameterUI::ShowComponentsAndVectorProperties);
    layout->addWidget(new QLabel(tr("Particle property:")));
    layout->addWidget(propertyUI->comboBox());

    auto* reductionLayout = new QGridLayout();
    reductionLayout->setContentsMargins(0, 0, 0, 0);
    reductionLayout->setColumnStretch(1, 1);

    reductionLayout->addWidget(new QLabel(tr("Reduction operation:")), 0, 0);
    VariantComboBoxParameterUI* reductionUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::reductionOperation));
    reductionUI->comboBox()->addItem(tr("mean"), QVariant::fromValue((int)SpatialBinningModifier::Mean));
    reductionUI->comboBox()->addItem(tr("sum"), QVariant::fromValue((int)SpatialBinningModifier::Sum));
    reductionUI->comboBox()->addItem(tr("sum divided by bin volume"), QVariant::fromValue((int)SpatialBinningModifier::SumDividedByBinVolume));
    reductionUI->comboBox()->addItem(tr("min"), QVariant::fromValue((int)SpatialBinningModifier::Min));
    reductionUI->comboBox()->addItem(tr("max"), QVariant::fromValue((int)SpatialBinningModifier::Max));
    reductionLayout->addWidget(reductionUI->comboBox(), 0, 1);
    layout->addLayout(reductionLayout);

    auto* directionLayout = new QGridLayout();
    directionLayout->setContentsMargins(0, 0, 0, 0);
    directionLayout->setColumnStretch(1, 1);
    directionLayout->setColumnStretch(2, 1);

    directionLayout->addWidget(new QLabel(tr("Binning direction:")), 0, 0);
    VariantComboBoxParameterUI* directionUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::binDirection));
    directionUI->comboBox()->addItem(tr("cell vector 1"), QVariant::fromValue((int)SpatialBinningModifier::CellVector1));
    directionUI->comboBox()->addItem(tr("cell vector 2"), QVariant::fromValue((int)SpatialBinningModifier::CellVector2));
    directionUI->comboBox()->addItem(tr("cell vector 3"), QVariant::fromValue((int)SpatialBinningModifier::CellVector3));
    directionUI->comboBox()->addItem(tr("vectors 1 and 2"), QVariant::fromValue((int)SpatialBinningModifier::CellVectors12));
    directionUI->comboBox()->addItem(tr("vectors 1 and 3"), QVariant::fromValue((int)SpatialBinningModifier::CellVectors13));
    directionUI->comboBox()->addItem(tr("vectors 2 and 3"), QVariant::fromValue((int)SpatialBinningModifier::CellVectors23));
    directionLayout->addWidget(directionUI->comboBox(), 0, 1, 1, 2);

    IntegerParameterUI* binsXUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::numberOfBinsX));
    directionLayout->addWidget(binsXUI->label(), 1, 0);
    directionLayout->addLayout(binsXUI->createFieldLayout(), 1, 1);

    _binsYUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::numberOfBinsY));
    directionLayout->addLayout(_binsYUI->createFieldLayout(), 1, 2);
    layout->addLayout(directionLayout);

    _firstDerivativeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::firstDerivative));
    layout->addWidget(_firstDerivativeUI->checkBox());

    _plotTitleLabel = new QLabel(tr("Reduction:"));
    layout->addWidget(_plotTitleLabel);

    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(220);
    _plotWidget->setMaximumHeight(220);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &SpatialBinningModifierEditor::plotBinning);
    connect(this, &PropertiesEditor::contentsReplaced, this, &SpatialBinningModifierEditor::updateWidgets);
    connect(this, &PropertiesEditor::contentsChanged, this, &SpatialBinningModifierEditor::updateWidgets);
    connect(directionUI->comboBox(), &QComboBox::currentIndexChanged, this, [this](int) { updateWidgets(); });

    updateWidgets();
}

void SpatialBinningModifierEditor::plotBinning()
{
    handleExceptions([&]() {
        const bool showPlot = editObject() && static_object_cast<SpatialBinningModifier>(editObject())->is1D();
        if(showPlot)
            _plotWidget->setTable(getPipelineOutput().getObjectBy<DataTable>(modificationNode(), SpatialBinningModifier::OutputIdentifier));
        else
            _plotWidget->setTable({});
        updateWidgets();
    });
}

void SpatialBinningModifierEditor::updateWidgets()
{
    bool is1D = true;
    if(const auto* modifier = static_object_cast<SpatialBinningModifier>(editObject()))
        is1D = modifier->is1D();

    if(_binsYUI)
        _binsYUI->setEnabled(!is1D);
    if(_firstDerivativeUI)
        _firstDerivativeUI->setEnabled(is1D);
    if(_plotTitleLabel)
        _plotTitleLabel->setVisible(is1D);
    if(_plotWidget)
        _plotWidget->setVisible(is1D);
}

}  // namespace Ovito
