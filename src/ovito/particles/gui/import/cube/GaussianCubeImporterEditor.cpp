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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/import/cube/GaussianCubeImporter.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include "GaussianCubeImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GaussianCubeImporterEditor);
SET_OVITO_OBJECT_EDITOR(GaussianCubeImporter, GaussianCubeImporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void GaussianCubeImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Gaussian Cube reader"), rolloutParams, "manual:file_formats.input.cube");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* gridOptionsBox = new QGroupBox(tr("Volumetric grid type"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(gridOptionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(gridOptionsBox);

    // Grid type
    IntegerRadioButtonParameterUI* gridTypeUI = new IntegerRadioButtonParameterUI(this, PROPERTY_FIELD(GaussianCubeImporter::gridType));
    sublayout->addWidget(gridTypeUI->addRadioButton(VoxelGrid::GridType::PointData, tr("Point-based grid")));
    sublayout->addWidget(gridTypeUI->addRadioButton(VoxelGrid::GridType::CellData, tr("Cell-based grid")));

    QGroupBox* atomicOptionsBox = new QGroupBox(tr("Atomic structure"), rollout);
    sublayout = new QVBoxLayout(atomicOptionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(atomicOptionsBox);

    // Generate bonds
    BooleanParameterUI* generateBondsUI = new BooleanParameterUI(this, PROPERTY_FIELD(ParticleImporter::generateBonds));
    sublayout->addWidget(generateBondsUI->checkBox());
}

}   // End of namespace
