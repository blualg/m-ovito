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
    QWidget* rollout = createRollout(tr("Spatial binning"), rolloutParams, "manual:particles.modifiers.bin_and_reduce");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* propertyUI = createParamUI<PropertyReferenceParameterUI>(
        PROPERTY_FIELD(SpatialBinningModifier::sourceProperty),
        &Particles::OOClass(),
        PropertyReferenceParameterUI::ShowComponentsAndVectorProperties);
    propertyUI->setNullPropertyItem(tr("<None> (count particles)"));
    layout->addWidget(new QLabel(tr("Input property")));
    layout->addWidget(propertyUI->comboBox());

    auto* reductionUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::reductionOperation));
    reductionUI->comboBox()->addItem(tr("sum"), QVariant::fromValue((int)SpatialBinningModifier::Sum));
    reductionUI->comboBox()->addItem(tr("mean"), QVariant::fromValue((int)SpatialBinningModifier::Mean));
    reductionUI->comboBox()->addItem(tr("min"), QVariant::fromValue((int)SpatialBinningModifier::Min));
    reductionUI->comboBox()->addItem(tr("max"), QVariant::fromValue((int)SpatialBinningModifier::Max));
    reductionUI->comboBox()->addItem(tr("sum divided by bin volume"), QVariant::fromValue((int)SpatialBinningModifier::SumDividedByBinVolume));
    layout->addWidget(new QLabel(tr("Reduction operation")));
    layout->addWidget(reductionUI->comboBox());

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::onlySelectedParticles));
    layout->addWidget(onlySelectedUI->checkBox());

    auto* firstDerivativeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::firstDerivative));
    layout->addWidget(firstDerivativeUI->checkBox());

    auto* directionsBox = new QGroupBox(tr("Binning directions"));
    auto* directionsLayout = new QGridLayout(directionsBox);
    directionsLayout->setContentsMargins(4, 4, 4, 4);
    directionsLayout->setColumnStretch(2, 1);

    auto* dir1UI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::binDirection1));
    auto* bins1UI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::numberOfBins1));
    directionsLayout->addWidget(dir1UI->checkBox(), 0, 0);
    directionsLayout->addWidget(new QLabel(tr("Bins")), 0, 1);
    directionsLayout->addLayout(bins1UI->createFieldLayout(), 0, 2);

    auto* dir2UI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::binDirection2));
    auto* bins2UI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::numberOfBins2));
    directionsLayout->addWidget(dir2UI->checkBox(), 1, 0);
    directionsLayout->addWidget(new QLabel(tr("Bins")), 1, 1);
    directionsLayout->addLayout(bins2UI->createFieldLayout(), 1, 2);
    bins2UI->setEnabled(false);
    connect(dir2UI->checkBox(), &QCheckBox::toggled, bins2UI, &IntegerParameterUI::setEnabled);

    auto* dir3UI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::binDirection3));
    auto* bins3UI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(SpatialBinningModifier::numberOfBins3));
    directionsLayout->addWidget(dir3UI->checkBox(), 2, 0);
    directionsLayout->addWidget(new QLabel(tr("Bins")), 2, 1);
    directionsLayout->addLayout(bins3UI->createFieldLayout(), 2, 2);
    bins3UI->setEnabled(false);
    connect(dir3UI->checkBox(), &QCheckBox::toggled, bins3UI, &IntegerParameterUI::setEnabled);

    layout->addWidget(directionsBox);

    _plotTitleLabel = new QLabel(tr("1D binned profile:"));
    layout->addWidget(_plotTitleLabel);

    _plotWidget = new DataTablePlotWidget();
    _plotWidget->setMinimumHeight(200);
    _plotWidget->setMaximumHeight(200);
    layout->addWidget(_plotWidget);

    layout->addWidget(new OpenDataInspectorButton(this, tr("Show in data inspector")));
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &SpatialBinningModifierEditor::plotBinning);
    connect(this, &PropertiesEditor::contentsReplaced, this, &SpatialBinningModifierEditor::updatePlotVisibility);
    connect(dir1UI->checkBox(), &QCheckBox::toggled, this, &SpatialBinningModifierEditor::updatePlotVisibility);
    connect(dir2UI->checkBox(), &QCheckBox::toggled, this, &SpatialBinningModifierEditor::updatePlotVisibility);
    connect(dir3UI->checkBox(), &QCheckBox::toggled, this, &SpatialBinningModifierEditor::updatePlotVisibility);
    connect(dir1UI->checkBox(), &QCheckBox::toggled, firstDerivativeUI, [this, firstDerivativeUI]() {
        firstDerivativeUI->setEnabled(_plotWidget && _plotWidget->isVisible());
    });
    connect(dir2UI->checkBox(), &QCheckBox::toggled, firstDerivativeUI, [this, firstDerivativeUI]() {
        firstDerivativeUI->setEnabled(_plotWidget && _plotWidget->isVisible());
    });
    connect(dir3UI->checkBox(), &QCheckBox::toggled, firstDerivativeUI, [this, firstDerivativeUI]() {
        firstDerivativeUI->setEnabled(_plotWidget && _plotWidget->isVisible());
    });

    updatePlotVisibility();
    firstDerivativeUI->setEnabled(_plotWidget && _plotWidget->isVisible());
}

void SpatialBinningModifierEditor::plotBinning()
{
    handleExceptions([&]() {
        DataOORef<const DataTable> table =
            getPipelineOutput().getObjectBy<DataTable>(static_cast<const PipelineNode*>(modificationNode()),
                                                       SpatialBinningModifier::OutputIdentifier);
        _plotWidget->setTable(std::move(table));
        updatePlotVisibility();
    });
}

void SpatialBinningModifierEditor::updatePlotVisibility()
{
    bool showPlot = false;
    if(const auto* modifier = static_object_cast<SpatialBinningModifier>(editObject())) {
        int activeDirections = int(modifier->binDirection1()) + int(modifier->binDirection2()) + int(modifier->binDirection3());
        showPlot = (activeDirections == 1);
    }

    if(_plotTitleLabel)
        _plotTitleLabel->setVisible(showPlot);
    if(_plotWidget)
        _plotWidget->setVisible(showPlot);
}

}  // namespace Ovito
