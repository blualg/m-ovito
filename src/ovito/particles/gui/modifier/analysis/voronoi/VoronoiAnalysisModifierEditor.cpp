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
#include <ovito/particles/modifier/analysis/voronoi/VoronoiAnalysisModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "VoronoiAnalysisModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoronoiAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(VoronoiAnalysisModifier, VoronoiAnalysisModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void VoronoiAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Voronoi analysis"), rolloutParams, "manual:particles.modifiers.voronoi_analysis");

    // Create the rollout contents.
    QVBoxLayout* mainLayout = new QVBoxLayout(rollout);
    mainLayout->setContentsMargins(4,4,4,4);
    mainLayout->setSpacing(6);

    QGroupBox* inputOptionsGroup = new QGroupBox(tr("Input"));
    QGridLayout* sublayout = new QGridLayout(inputOptionsGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    mainLayout->addWidget(inputOptionsGroup);
    int row = 0;

    // Atomic radii.
    BooleanParameterUI* useRadiiPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::useRadii));
    sublayout->addWidget(useRadiiPUI->checkBox(), row++, 0, 1, 2);

    // Only selected particles.
    BooleanParameterUI* onlySelectedPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::onlySelected));
    sublayout->addWidget(onlySelectedPUI->checkBox(), row++, 0, 1, 2);

    QGroupBox* filterSmallFacesGroup = new QGroupBox(tr("Removal of degenerate faces"));
    sublayout = new QGridLayout(filterSmallFacesGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    mainLayout->addWidget(filterSmallFacesGroup);
    row = 0;

    // Face threshold.
    FloatParameterUI* faceThresholdPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::faceThreshold));
    sublayout->addWidget(faceThresholdPUI->label(), row, 0);
    sublayout->addLayout(faceThresholdPUI->createFieldLayout(), row++, 1);
    faceThresholdPUI->spinner()->setStandardValue(0.0);
    faceThresholdPUI->textBox()->setPlaceholderText(tr("‹none›"));

    // Relative face threshold.
    FloatParameterUI* relativeFaceThresholdPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::relativeFaceThreshold));
    sublayout->addWidget(relativeFaceThresholdPUI->label(), row, 0);
    sublayout->addLayout(relativeFaceThresholdPUI->createFieldLayout(), row++, 1);

    QGroupBox* outputOptionsGroup = new QGroupBox(tr("Output"));
    sublayout = new QGridLayout(outputOptionsGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(2, 1);
    sublayout->setColumnMinimumWidth(0, 20);
    mainLayout->addWidget(outputOptionsGroup);
    row = 0;

    // Compute indices.
    BooleanParameterUI* computeIndicesPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::computeIndices));
    sublayout->addWidget(computeIndicesPUI->checkBox(), row++, 0, 1, 3);

    // Edge threshold.
    FloatParameterUI* edgeThresholdPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::edgeThreshold));
    sublayout->addWidget(edgeThresholdPUI->label(), row, 1);
    sublayout->addLayout(edgeThresholdPUI->createFieldLayout(), row++, 2);
    edgeThresholdPUI->setEnabled(false);
    connect(computeIndicesPUI->checkBox(), &QCheckBox::toggled, edgeThresholdPUI, &ParameterUI::setEnabled);
    edgeThresholdPUI->spinner()->setStandardValue(0.0);
    edgeThresholdPUI->textBox()->setPlaceholderText(tr("‹none›"));

    // Generate bonds.
    BooleanParameterUI* computeBondsPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::computeBonds));
    sublayout->addWidget(computeBondsPUI->checkBox(), row++, 0, 1, 3);

    // Generate polyhedral mesh.
    BooleanParameterUI* computePolyhedraPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoronoiAnalysisModifier::computePolyhedra));
    sublayout->addWidget(computePolyhedraPUI->checkBox(), row++, 0, 1, 3);

    // Status label.
    mainLayout->addSpacing(6);
    mainLayout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}   // End of namespace
