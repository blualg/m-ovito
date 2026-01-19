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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/AffineTransformationParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "EditSimulationCellModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(EditSimulationCellModifierEditor);
SET_OVITO_OBJECT_EDITOR(EditSimulationCellModifier, EditSimulationCellModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void EditSimulationCellModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Edit simulation cell"), rolloutParams, "manual:particles.modifiers.edit_simulation_cell");

    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(8);

    {
        QGroupBox* dimensionalityGroupBox = new QGroupBox(tr("Dimensionality"), rollout);
        layout1->addWidget(dimensionalityGroupBox);

        QGridLayout* layout2 = new QGridLayout(dimensionalityGroupBox);
        layout2->setContentsMargins(4,4,4,4);
        layout2->setSpacing(2);

        BooleanRadioButtonParameterUI* is2dPUI = createParamUI<BooleanRadioButtonParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::is2D));
        is2dPUI->buttonTrue()->setText("2D");
        is2dPUI->buttonFalse()->setText("3D");
        layout2->addWidget(is2dPUI->buttonTrue(), 0, 0);
        layout2->addWidget(is2dPUI->buttonFalse(), 0, 1);
    }

    {
        QGroupBox* pbcGroupBox = new QGroupBox(tr("Periodic boundary conditions"), rollout);
        layout1->addWidget(pbcGroupBox);

        QGridLayout* layout2 = new QGridLayout(pbcGroupBox);
        layout2->setContentsMargins(4,4,4,4);
        layout2->setSpacing(2);

        BooleanParameterUI* pbcxPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::pbcX));
        pbcxPUI->checkBox()->setText("X");
        layout2->addWidget(pbcxPUI->checkBox(), 0, 0);

        BooleanParameterUI* pbcyPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::pbcY));
        pbcyPUI->checkBox()->setText("Y");
        layout2->addWidget(pbcyPUI->checkBox(), 0, 1);

        _pbczPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::pbcZ));
        _pbczPUI->checkBox()->setText("Z");
        layout2->addWidget(_pbczPUI->checkBox(), 0, 2);
    }

    {
        BooleanGroupBoxParameterUI* replaceCellPUI = createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::replaceCell));

        QGroupBox* vectorsGroupBox = replaceCellPUI->groupBox();
        vectorsGroupBox->setTitle(tr("Edit cell geometry"));
        layout1->addWidget(vectorsGroupBox);

        QVBoxLayout* layout = new QVBoxLayout(replaceCellPUI->childContainer());
        layout->setContentsMargins(4,4,4,4);
        layout->setSpacing(6);

        QGridLayout* sublayout = new QGridLayout();
        sublayout->setContentsMargins(0,0,0,0);
        sublayout->setSpacing(2);
        sublayout->setColumnStretch(1, 1);
        sublayout->setColumnStretch(2, 1);
        sublayout->setColumnStretch(3, 1);
        layout->addLayout(sublayout);

        int layoutRow = 0;
        sublayout->addWidget(new QLabel(tr("Cell vectors:")), layoutRow++, 1, 1, 3);

        sublayout->addWidget(new QLabel(tr("<b>a</b>:")), layoutRow+0, 0, 1, 1, Qt::AlignRight);
        sublayout->addWidget(new QLabel(tr("<b>b</b>:")), layoutRow+1, 0, 1, 1, Qt::AlignRight);
        sublayout->addWidget(new QLabel(tr("<b>c</b>:")), layoutRow+2, 0, 1, 1, Qt::AlignRight);
        for(size_t v = 0; v < 3; v++) {
            for(size_t r = 0; r < 3; r++) {
                AffineTransformationParameterUI* destinationCellUI = createParamUI<AffineTransformationParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::cellMatrix), r, v);
                sublayout->addLayout(destinationCellUI->createFieldLayout(), layoutRow, r+1);
                _cellUnits[r][v].emplace(unitsManager().getUnit(destinationCellUI->parameterUnitType()));
                destinationCellUI->spinner()->setUnit(&_cellUnits[r][v].value());
                destinationCellUI->textBox()->setPlaceholderText(_cellUnits[r][v]->formatValue(0));
                destinationCellUI->spinner()->setStandardValue(0);
                _cellVectorFields[v][r] = destinationCellUI->textBox();
            }
            layoutRow++;
        }

        sublayout->addWidget(new QLabel(tr("Cell origin:")), layoutRow++, 1, 1, 3);
        sublayout->addWidget(new QLabel(tr("<b>o</b>:")), layoutRow, 0, 1, 1, Qt::AlignRight);
        for(size_t r = 0; r < 3; r++) {
            AffineTransformationParameterUI* destinationCellUI = createParamUI<AffineTransformationParameterUI>(PROPERTY_FIELD(EditSimulationCellModifier::cellMatrix), r, 3);
            sublayout->addLayout(destinationCellUI->createFieldLayout(), layoutRow, r+1);
            _originUnits[r].emplace(unitsManager().getUnit(destinationCellUI->parameterUnitType()));
            destinationCellUI->spinner()->setUnit(&_originUnits[r].value());
            destinationCellUI->textBox()->setPlaceholderText(_originUnits[r]->formatValue(0));
            destinationCellUI->spinner()->setStandardValue(0);
            _cellVectorFields[3][r] = destinationCellUI->textBox();
        }
    }

    // Update the palette of the cell vector fields when their values change.
    connect(this, &PropertiesEditor::contentsChanged, this, &EditSimulationCellModifierEditor::updateSimulationCellFields);

    // Whenever the pipeline input of the modifier changes, update the increment step sizes of the cell parameters.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &EditSimulationCellModifierEditor::updateParameterUnitScales);
}

/******************************************************************************
* After the simulation cell size has changed, updates the UI controls.
******************************************************************************/
void EditSimulationCellModifierEditor::updateSimulationCellFields()
{
    _pbczPUI->setEnabled(modifier() && !modifier()->is2D());
}

/******************************************************************************
* Auto-adjusts the increment steps of the numeric parameter spinner widgets.
******************************************************************************/
void EditSimulationCellModifierEditor::updateParameterUnitScales()
{
    _originUnits[0]->setScaleReference(0);
    _originUnits[1]->setScaleReference(0);
    _originUnits[2]->setScaleReference(0);

    if(DataOORef<const SimulationCell> cell = getPipelineInput().getObject<SimulationCell>()) {
        // Let origin translation increments depend on the cell vector lengths.
        _originUnits[0]->setScaleReference(cell->cellMatrix().column(0).length());
        _originUnits[1]->setScaleReference(cell->cellMatrix().column(1).length());
        _originUnits[2]->setScaleReference(cell->cellMatrix().column(2).length());

        // Also let shear increments (off-diagonal cell matrix elements) depend on the cell vector lengths.
        _cellUnits[1][0]->setScaleReference(0.3 * cell->cellMatrix().column(0).length());
        _cellUnits[2][0]->setScaleReference(0.3 * cell->cellMatrix().column(0).length());
        _cellUnits[0][1]->setScaleReference(0.3 * cell->cellMatrix().column(1).length());
        _cellUnits[2][1]->setScaleReference(0.3 * cell->cellMatrix().column(1).length());
        _cellUnits[0][2]->setScaleReference(0.3 * cell->cellMatrix().column(2).length());
        _cellUnits[1][2]->setScaleReference(0.3 * cell->cellMatrix().column(2).length());
    }
}

}   // End of namespace
