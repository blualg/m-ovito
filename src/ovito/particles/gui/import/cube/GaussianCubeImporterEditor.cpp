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

    QGroupBox* gridOptionsBox = new QGroupBox(tr("Volumetric grid"), rollout);
    QGridLayout* sublayout1 = new QGridLayout(gridOptionsBox);
    sublayout1->setContentsMargins(4,4,4,4);
    sublayout1->setColumnStretch(1, 1);
    layout->addWidget(gridOptionsBox);

    // Grid type
    sublayout1->addWidget(new QLabel(tr("Grid type:")), 0, 0);
    IntegerRadioButtonParameterUI* gridTypeUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(GaussianCubeImporter::gridType));
    sublayout1->addWidget(gridTypeUI->addRadioButton(VoxelGrid::GridType::PointData, tr("Point-based")), 0, 1);
    sublayout1->addWidget(gridTypeUI->addRadioButton(VoxelGrid::GridType::CellData, tr("Cell-based")), 1, 1);

    // Convert field values from Bohr units to Angstroms.
    BooleanParameterUI* convertFieldBohrToAngstromUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(GaussianCubeImporter::convertFieldBohrToAngstrom));
    sublayout1->addWidget(convertFieldBohrToAngstromUI->checkBox(), 2, 0, 1, 2);

    QGroupBox* atomicOptionsBox = new QGroupBox(tr("Atomic structure"), rollout);
    QVBoxLayout* sublayout2 = new QVBoxLayout(atomicOptionsBox);
    sublayout2->setContentsMargins(4,4,4,4);
    layout->addWidget(atomicOptionsBox);

    // Generate bonds
    BooleanParameterUI* generateBondsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::generateBonds));
    sublayout2->addWidget(generateBondsUI->checkBox());
}

}   // End of namespace
