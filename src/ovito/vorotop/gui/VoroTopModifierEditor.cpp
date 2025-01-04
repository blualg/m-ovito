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

#include <ovito/vorotop/VoroTopPlugin.h>
#include <ovito/particles/gui/modifier/analysis/StructureListParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FilenameParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include "VoroTopModifierEditor.h"

namespace Ovito::VoroTop {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoroTopModifierEditor);
SET_OVITO_OBJECT_EDITOR(VoroTopModifier, VoroTopModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void VoroTopModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("VoroTop analysis"), rolloutParams, "manual:particles.modifiers.vorotop_analysis");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(4,4,4,4);
    gridlayout->setSpacing(4);
    gridlayout->setColumnStretch(1, 1);
    int row = 0;

    // Filter filename.
    gridlayout->addWidget(new QLabel(tr("Filter:")), row++, 0, 1, 2);
    QStringList fileFilter = { tr("VoroTop filter definition file (*)") };
    FilenameParameterUI* fileFileUI = createParamUI<FilenameParameterUI>(PROPERTY_FIELD(VoroTopModifier::filterFile), fileFilter, true);
    gridlayout->addWidget(fileFileUI->selectorWidget(), row++, 0, 1, 2);

    QLabel* label = new QLabel(tr("Filter definition files available from the <a href=\"https://www.vorotop.org/download.html\">VoroTop website</a>."));
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    gridlayout->addWidget(label, row++, 0, 1, 2);

    // Atomic radii.
    BooleanParameterUI* useRadiiPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoroTopModifier::useRadii));
    gridlayout->addWidget(useRadiiPUI->checkBox(), row++, 0, 1, 2);

    // Only selected particles.
    BooleanParameterUI* onlySelectedPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(StructureIdentificationModifier::onlySelectedParticles));
    gridlayout->addWidget(onlySelectedPUI->checkBox(), row++, 0, 1, 2);

    layout->addLayout(gridlayout);

    // Status label.
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    StructureListParameterUI* structureTypesPUI = createParamUI<StructureListParameterUI>(false);
    layout->addSpacing(10);
    layout->addWidget(new QLabel(tr("Structure types:")));
    layout->addWidget(structureTypesPUI->tableWidget());
    label = new QLabel(tr("<p style=\"font-size: small;\">Double-click to change colors.</p>"));
    label->setWordWrap(true);
    layout->addWidget(label);
}

}   // End of namespace
