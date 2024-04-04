////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/rendering/StandardSceneRenderer.h>
#include "StandardSceneRendererEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(StandardSceneRendererEditor);
SET_OVITO_OBJECT_EDITOR(StandardSceneRenderer, StandardSceneRendererEditor);

/******************************************************************************
* Constructor that creates the UI controls for the editor.
******************************************************************************/
void StandardSceneRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create the rollout.
    QWidget* rollout = createRollout(tr("OpenGL renderer settings"), rolloutParams, "manual:rendering.opengl_renderer");

    // Create the rollout contents.
    QVBoxLayout* rootLayout = new QVBoxLayout(rollout);
    rootLayout->setContentsMargins(4,4,4,4);

    QGroupBox* qualityBox = new QGroupBox(tr("Quality"), rollout);
    rootLayout->addWidget(qualityBox);
    QGridLayout* gridLayout = new QGridLayout(qualityBox);
    gridLayout->setContentsMargins(4,4,4,4);
#ifndef Q_OS_MACOS
    gridLayout->setSpacing(2);
#endif
    gridLayout->setColumnStretch(1, 1);

    // Antialiasing level
    IntegerParameterUI* antialiasingLevelUI = new IntegerParameterUI(this, PROPERTY_FIELD(StandardSceneRenderer::antialiasingLevel));
    gridLayout->addWidget(antialiasingLevelUI->label(), 0, 0);
    gridLayout->addLayout(antialiasingLevelUI->createFieldLayout(), 0, 1);

    QGroupBox* transparencyBox = new QGroupBox(tr("Transparency rendering method"), rollout);
    rootLayout->addWidget(transparencyBox);
    QHBoxLayout* boxLayout = new QHBoxLayout(transparencyBox);
    boxLayout->setContentsMargins(4,4,4,4);

    VariantComboBoxParameterUI* transparencyMethodUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(StandardSceneRenderer::orderIndependentTransparency));
    transparencyMethodUI->comboBox()->addItem(tr("Back-to-Front Ordered (default)"), QVariant::fromValue(false));
    transparencyMethodUI->comboBox()->addItem(tr("Weighted Blended Order-Independent"), QVariant::fromValue(true));
    boxLayout->addWidget(transparencyMethodUI->comboBox());
}

}   // End of namespace
