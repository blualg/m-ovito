////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/particles/modifier/analysis/coordination/CoordinationAnalysisModifier.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>
#include "CoordinationAnalysisModifierEditor.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(CoordinationAnalysisModifierEditor);
SET_OVITO_OBJECT_EDITOR(CoordinationAnalysisModifier, CoordinationAnalysisModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CoordinationAnalysisModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Coordination analysis"), rolloutParams, "manual:particles.modifiers.coordination_analysis");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(4,4,4,4);
    gridlayout->setColumnStretch(1, 1);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusPUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinationAnalysisModifier::cutoff));
    gridlayout->addWidget(cutoffRadiusPUI->label(), 0, 0);
    gridlayout->addLayout(cutoffRadiusPUI->createFieldLayout(), 0, 1);

    // Number of bins parameter.
    IntegerParameterUI* numBinsPUI = new IntegerParameterUI(this, PROPERTY_FIELD(CoordinationAnalysisModifier::numberOfBins));
    gridlayout->addWidget(numBinsPUI->label(), 1, 0);
    gridlayout->addLayout(numBinsPUI->createFieldLayout(), 1, 1);
    layout->addLayout(gridlayout);

    // Partial RDFs option.
    BooleanParameterUI* partialRdfPUI = new BooleanParameterUI(this, PROPERTY_FIELD(CoordinationAnalysisModifier::computePartialRDF));
    layout->addWidget(partialRdfPUI->checkBox());

    // Only selected particles.
    BooleanParameterUI* onlySelectedPUI = new BooleanParameterUI(this, PROPERTY_FIELD(CoordinationAnalysisModifier::onlySelected));
    layout->addWidget(onlySelectedPUI->checkBox());

    _rdfPlot = new DataTablePlotWidget();
    _rdfPlot->setMinimumHeight(200);
    _rdfPlot->setMaximumHeight(200);

    layout->addSpacing(12);
    layout->addWidget(new QLabel(tr("Radial distribution function:")));
    layout->addWidget(_rdfPlot);

    OpenDataInspectorButton* openDataInspectorBtn = new OpenDataInspectorButton(this, tr("Show in data inspector"));
    layout->addWidget(openDataInspectorBtn);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget((new ObjectStatusDisplay(this))->statusWidget());

    // Update data plot whenever the modifier has calculated new results.
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &CoordinationAnalysisModifierEditor::plotRDF);
}

/******************************************************************************
* Updates the plot of the RDF computed by the modifier.
******************************************************************************/
void CoordinationAnalysisModifierEditor::plotRDF()
{
    // Look up the data table in the modifier's pipeline output.
    OORef<DataTable> table = getPipelineOutput().getObjectBy<DataTable>(modifierApplication(), QStringLiteral("coordination-rdf"));

    // Determine X plotting range.
    if(table) {
        ConstPropertyPtr x = table->getXValues();
        ConstPropertyAccessAndRef<FloatType,false> rdfXArray(x);
        ConstPropertyAccessAndRef<FloatType,true>  rdfYArray(table->y());
        double minX = 0;
        for(size_t i = 0; i < rdfYArray.size(); i++) {
            for(size_t cmpnt = 0; cmpnt < rdfYArray.componentCount(); cmpnt++) {
                if(rdfYArray.get(i, cmpnt) != 0) {
                    minX = rdfXArray[i];
                    break;
                }
            }
            if(minX) break;
        }
        _rdfPlot->setAxisScale(QwtPlot::xBottom, std::floor(minX * 9.0 / table->intervalEnd()) / 10.0 * table->intervalEnd(), table->intervalEnd());
    }
    _rdfPlot->setTable(table);
}

}   // End of namespace
