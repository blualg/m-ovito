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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ModifierDelegateFixedListParameterUI.h>
#include <ovito/stdmod/modifiers/ReplicateModifier.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include "ReplicateModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ReplicateModifierEditor);
SET_OVITO_OBJECT_EDITOR(ReplicateModifier, ReplicateModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ReplicateModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Replicate"), rolloutParams, "manual:particles.modifiers.show_periodic_images");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
#ifndef Q_OS_MACOS
    layout->setHorizontalSpacing(2);
    layout->setVerticalSpacing(2);
#else
    layout->setHorizontalSpacing(4);
#endif
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    layout->setColumnStretch(3, 1);

    layout->addWidget(new QLabel(tr("Number of images:")), 0, 0);

    IntegerParameterUI* numImagesXPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(ReplicateModifier::numImagesX));
    layout->addLayout(numImagesXPUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* numImagesYPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(ReplicateModifier::numImagesY));
    layout->addLayout(numImagesYPUI->createFieldLayout(), 0, 2);

    IntegerParameterUI* numImagesZPUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(ReplicateModifier::numImagesZ));
    layout->addLayout(numImagesZPUI->createFieldLayout(), 0, 3);

    BooleanParameterUI* adjustBoxSizeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ReplicateModifier::adjustBoxSize));
    layout->addWidget(adjustBoxSizeUI->checkBox(), 1, 0, 1, 4);

    BooleanParameterUI* uniqueIdentifiersUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ReplicateModifier::uniqueIdentifiers));
    layout->addWidget(uniqueIdentifiersUI->checkBox(), 2, 0, 1, 4);

    // Create a second rollout.
    rollout = createRollout(tr("Operate on"), rolloutParams.after(rollout), "manual:particles.modifiers.show_periodic_images");

    // Create the rollout contents.
    QVBoxLayout* topLayout = new QVBoxLayout(rollout);
    topLayout->setContentsMargins(4,4,4,4);
    topLayout->setSpacing(12);

    ModifierDelegateFixedListParameterUI* delegatesPUI = createParamUI<ModifierDelegateFixedListParameterUI>(rolloutParams.after(rollout));
    topLayout->addWidget(delegatesPUI->listWidget(103));

    // Enable Z axis only if the simulation cell is 3D.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, [this, numImagesXPUI, numImagesYPUI, numImagesZPUI]() {
        const SimulationCell* cell = getPipelineInput().getObject<SimulationCell>();
        numImagesXPUI->setEnabled(cell != nullptr);
        numImagesYPUI->setEnabled(cell != nullptr);
        numImagesZPUI->setEnabled(cell != nullptr && !cell->is2D());
    });
}

}   // End of namespace
