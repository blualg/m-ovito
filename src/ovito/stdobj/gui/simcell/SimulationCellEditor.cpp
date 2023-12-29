////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/gui/desktop/properties/AffineTransformationParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanRadioButtonParameterUI.h>
#include "SimulationCellEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SimulationCellEditor);
SET_OVITO_OBJECT_EDITOR(SimulationCell, SimulationCellEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void SimulationCellEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create rollout.
    QWidget* rollout = createRollout(QString(), rolloutParams, "manual:scene_objects.simulation_cell");

    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(8);

    {
        QGroupBox* dimensionalityGroupBox = new QGroupBox(tr("Dimensionality"), rollout);
        layout1->addWidget(dimensionalityGroupBox);

        QGridLayout* layout2 = new QGridLayout(dimensionalityGroupBox);
        layout2->setContentsMargins(4,4,4,4);
        layout2->setSpacing(2);

        BooleanRadioButtonParameterUI* is2dPUI = new BooleanRadioButtonParameterUI(this, PROPERTY_FIELD(SimulationCell::is2D));
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

        BooleanParameterUI* pbcxPUI = new BooleanParameterUI(this, PROPERTY_FIELD(SimulationCell::pbcX));
        pbcxPUI->checkBox()->setText("X");
        layout2->addWidget(pbcxPUI->checkBox(), 0, 0);

        BooleanParameterUI* pbcyPUI = new BooleanParameterUI(this, PROPERTY_FIELD(SimulationCell::pbcY));
        pbcyPUI->checkBox()->setText("Y");
        layout2->addWidget(pbcyPUI->checkBox(), 0, 1);

        _pbczPUI = new BooleanParameterUI(this, PROPERTY_FIELD(SimulationCell::pbcZ));
        _pbczPUI->checkBox()->setText("Z");
        layout2->addWidget(_pbczPUI->checkBox(), 0, 2);
    }

    connect(this, &SimulationCellEditor::contentsChanged, this, &SimulationCellEditor::updateSimulationBoxSize);

    {
        QGroupBox* sizeGroupBox = new QGroupBox(tr("Box dimensions"), rollout);
        layout1->addWidget(sizeGroupBox);

        QGridLayout* layout2 = new QGridLayout(sizeGroupBox);
        layout2->setContentsMargins(4,4,4,4);
        layout2->setSpacing(4);
        layout2->setColumnStretch(1, 1);
        for(int i = 0; i < 3; i++) {
            _boxSizeFields[i] = new QLineEdit(rollout);
            _boxSizeFields[i]->setReadOnly(true);
            layout2->addWidget(_boxSizeFields[i], i, 1);
        }
        layout2->addWidget(new QLabel(tr("Width (X):")), 0, 0);
        layout2->addWidget(new QLabel(tr("Length (Y):")), 1, 0);
        layout2->addWidget(new QLabel(tr("Height (Z):")), 2, 0);
    }

    {
        QGroupBox* vectorsGroupBox = new QGroupBox(tr("Geometry"), rollout);
        layout1->addWidget(vectorsGroupBox);

        QVBoxLayout* sublayout = new QVBoxLayout(vectorsGroupBox);
        sublayout->setContentsMargins(4,4,4,4);
        sublayout->setSpacing(2);

        QString xyz[3] = { QStringLiteral("X: "), QStringLiteral("Y: "), QStringLiteral("Z: ") };

        {   // First cell vector.
            sublayout->addSpacing(6);
            sublayout->addWidget(new QLabel(tr("Cell vector 1:"), rollout));
            QHBoxLayout* rowLayout = new QHBoxLayout();
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(2);
            sublayout->addLayout(rowLayout);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[0][i] = new QLineEdit();
                _cellVectorFields[0][i]->setReadOnly(true);
                rowLayout->addWidget(_cellVectorFields[0][i], 1);
            }
        }

        {   // Second cell vector.
            sublayout->addSpacing(2);
            sublayout->addWidget(new QLabel(tr("Cell vector 2:"), rollout));
            QHBoxLayout* rowLayout = new QHBoxLayout();
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(2);
            sublayout->addLayout(rowLayout);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[1][i] = new QLineEdit();
                _cellVectorFields[1][i]->setReadOnly(true);
                rowLayout->addWidget(_cellVectorFields[1][i], 1);
            }
        }

        {   // Third cell vector.
            sublayout->addSpacing(2);
            sublayout->addWidget(new QLabel(tr("Cell vector 3:"), rollout));
            QHBoxLayout* rowLayout = new QHBoxLayout();
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(2);
            sublayout->addLayout(rowLayout);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[2][i] = new QLineEdit();
                _cellVectorFields[2][i]->setReadOnly(true);
                rowLayout->addWidget(_cellVectorFields[2][i], 1);
            }
        }

        {   // Cell origin.
            sublayout->addSpacing(8);
            sublayout->addWidget(new QLabel(tr("Cell origin:"), rollout));
            QHBoxLayout* rowLayout = new QHBoxLayout();
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(2);
            sublayout->addLayout(rowLayout);
            for(int i = 0; i < 3; i++) {
                _cellVectorFields[3][i] = new QLineEdit();
                _cellVectorFields[3][i]->setReadOnly(true);
                rowLayout->addWidget(_cellVectorFields[3][i], 1);
            }
        }
    }
}

/******************************************************************************
* After the simulation cell size has changed, updates the UI controls.
******************************************************************************/
void SimulationCellEditor::updateSimulationBoxSize()
{
    SimulationCell* cell = static_object_cast<SimulationCell>(editObject());
    if(!cell) return;

    const AffineTransformation& cellTM = cell->cellMatrix();
    ParameterUnit* worldUnit = mainWindow().unitsManager().worldUnit();

    for(size_t dim = 0; dim < 3; dim++) {
        _boxSizeFields[dim]->setText(worldUnit->formatValue(cellTM(dim, dim)));
        for(size_t col = 0; col < 4; col++)
            _cellVectorFields[col][dim]->setText(worldUnit->formatValue(cellTM(dim, col)));
    }

    _pbczPUI->setEnabled(!cell->is2D());
}

}   // End of namespace
