////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/particles/objects/BondsVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerCheckBoxParameterUI.h>
#include <ovito/particles/gui/util/BondInspectionApplet.h>
#include "BondsVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(BondsVisEditor);
SET_OVITO_OBJECT_EDITOR(BondsVis, BondsVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void BondsVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Bonds display"), rolloutParams, "manual:visual_elements.bonds");
    QVBoxLayout* mainLayout = new QVBoxLayout(rollout);
    mainLayout->setContentsMargins(4,4,4,4);

    // Appearance box
    QGroupBox* appearanceBox = new QGroupBox(tr("Appearance"));
    QGridLayout* layout = new QGridLayout(appearanceBox);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);
    mainLayout->addWidget(appearanceBox);

    // Bond width.
    FloatParameterUI* bondWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(BondsVis::bondWidth));
    layout->addWidget(bondWidthUI->label(), 0, 0);
    layout->addLayout(bondWidthUI->createFieldLayout(), 0, 1);

    // Shading mode.
    IntegerCheckBoxParameterUI* shadingModeUI = createParamUI<IntegerCheckBoxParameterUI>(PROPERTY_FIELD(BondsVis::shadingMode), BondsVis::NormalShading, BondsVis::FlatShading);
    shadingModeUI->checkBox()->setText(tr("Flat shading"));
    layout->addWidget(shadingModeUI->checkBox(), 1, 1);

    // Coloring box
    QGroupBox* coloringBox = new QGroupBox(tr("Coloring"));
    layout = new QGridLayout(coloringBox);
    layout->setContentsMargins(4,4,4,4);
    layout->setHorizontalSpacing(4);
    layout->setVerticalSpacing(5);
    layout->setColumnStretch(1, 1);
    layout->setRowMinimumHeight(3, 1); // Extra space below the last option to better align the uniform color picker.
    mainLayout->addWidget(coloringBox);

    // Coloring mode.
    _coloringModeUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(BondsVis::coloringMode));

    // By-type coloring mode.
    layout->addWidget(_coloringModeUI->addRadioButton(BondsVis::ByTypeColoring, tr("By bond type")), 0, 0);
    QLabel* showTypesLabel = new QLabel(tr("<a href=\"show_types\">(see inspector)</a>"));
    showTypesLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(showTypesLabel, &QLabel::linkActivated, this, [&]() {
        if(ui().mainWindow()->openDataInspector(BondInspectionApplet::OOClass()) == false)
            QToolTip::showText(QCursor::pos(), tr("Could not open the 'Bonds' page of the data inspector, because there are no bonds found in the current pipeline."), nullptr, QRect(), 3000);
    });
    layout->addWidget(showTypesLabel, 0, 1);

    // By-particle coloring mode.
    layout->addWidget(_coloringModeUI->addRadioButton(BondsVis::ParticleBasedColoring, tr("By particle")), 1, 0);

    // Uniform color.
    layout->addWidget(_coloringModeUI->addRadioButton(BondsVis::UniformColoring, tr("Uniform color:")), 2, 0);
    _bondColorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(BondsVis::bondColor));
    QVBoxLayout* sublayout = new QVBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->setSpacing(0);
    sublayout->addStretch(1);
    sublayout->addWidget(_bondColorUI->colorPicker());
    layout->addLayout(sublayout, 1, 1, -1, -1);

    // Whenever the pipeline input of the vis element changes, update the list of available coloring options.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &BondsVisEditor::updateColoringOptions);

    // Update the coloring controls when a parameter of the vis element has been changed.
    connect(this, &PropertiesEditor::contentsChanged, this, &BondsVisEditor::updateColoringOptions);
}

/******************************************************************************
* Updates the coloring controls shown in the UI.
******************************************************************************/
void BondsVisEditor::updateColoringOptions()
{
    // Retrieve the Bonds this vis element is associated with.
    DataOORef<const Bonds> bonds = dynamic_object_cast<const Bonds>(getVisDataObject());

    // Do the bonds have explicit RGB colors assigned ("Color" property exists)?
    bool hasExplicitColors = (bonds && bonds->getProperty(Bonds::ColorProperty));

    BondsVis::ColoringMode coloringMode = editObject() ? static_object_cast<BondsVis>(editObject())->coloringMode() : BondsVis::UniformColoring;

    _bondColorUI->setEnabled(editObject() && !hasExplicitColors && coloringMode == BondsVis::UniformColoring);

    _coloringModeUI->buttonGroup()->button(BondsVis::UniformColoring)->setEnabled(editObject() && !hasExplicitColors);
    _coloringModeUI->buttonGroup()->button(BondsVis::ByTypeColoring)->setEnabled(bonds && !hasExplicitColors && bonds->getProperty(Bonds::TypeProperty));
    _coloringModeUI->buttonGroup()->button(BondsVis::ParticleBasedColoring)->setEnabled(!hasExplicitColors);
}

}   // End of namespace
