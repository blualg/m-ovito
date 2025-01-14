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
#include <ovito/particles/modifier/analysis/cluster/ClusterAnalysisModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "ClusterAnalysisModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ClusterAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(ClusterAnalysisModifier, ClusterAnalysisModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ClusterAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Cluster analysis"), rolloutParams, "manual:particles.modifiers.cluster_analysis");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(4,4,4,4);
    gridlayout->setVerticalSpacing(6);
    gridlayout->setColumnStretch(2, 1);
    gridlayout->setColumnMinimumWidth(0, 10);
    gridlayout->setRowMinimumHeight(3, 6);

    gridlayout->addWidget(new QLabel(tr("Connectivity criterion:")), 0, 0, 1, 3);

    IntegerRadioButtonParameterUI* neighborModePUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::neighborMode));
    QRadioButton* cutoffModeBtn = neighborModePUI->addRadioButton(ClusterAnalysisModifier::CutoffRange, tr("Cutoff distance:"));
    gridlayout->addWidget(cutoffModeBtn, 1, 1);
    QRadioButton* bondModeBtn = neighborModePUI->addRadioButton(ClusterAnalysisModifier::Bonding, tr("Bonds"));
    gridlayout->addWidget(bondModeBtn, 2, 1, 1, 2);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::cutoff));
    gridlayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 1, 2);
    cutoffRadiusPUI->setEnabled(false);
    connect(cutoffModeBtn, &QRadioButton::toggled, cutoffRadiusPUI, &FloatParameterUI::setEnabled);

    // Sort by size.
    BooleanParameterUI* sortBySizeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::sortBySize));
    gridlayout->addWidget(sortBySizeUI->checkBox(), 4, 0, 1, 3);

    // Compute centers of mass.
    BooleanParameterUI* computeCentersOfMassUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::computeCentersOfMass));
    gridlayout->addWidget(computeCentersOfMassUI->checkBox(), 5, 0, 1, 3);

    // Compute centers of mass.
    BooleanParameterUI* computeRadiusOfGyrationUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::computeRadiusOfGyration));
    gridlayout->addWidget(computeRadiusOfGyrationUI->checkBox(), 6, 0, 1, 3);

    // Unwrap particle coordinates.
    BooleanParameterUI* unwrapParticleCoordinatesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::unwrapParticleCoordinates));
    gridlayout->addWidget(unwrapParticleCoordinatesUI->checkBox(), 7, 0, 1, 3);

    // Color particles by cluster.
    BooleanParameterUI* colorParticlesByClusterUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::colorParticlesByCluster));
    gridlayout->addWidget(colorParticlesByClusterUI->checkBox(), 8, 0, 1, 3);

    // Use only selected particles.
    BooleanParameterUI* onlySelectedParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ClusterAnalysisModifier::onlySelectedParticles));
    gridlayout->addWidget(onlySelectedParticlesUI->checkBox(), 9, 0, 1, 3);

    layout->addLayout(gridlayout);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    OpenDataInspectorButton* openDataInspectorBtn = new OpenDataInspectorButton(this, tr("Show list of clusters"), QStringLiteral("clusters"), 1); // Note: Mode hint "1" is used to switch to the data table view.
    layout->addWidget(openDataInspectorBtn);
}

}   // End of namespace
