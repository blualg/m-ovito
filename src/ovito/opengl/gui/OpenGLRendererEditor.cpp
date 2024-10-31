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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/core/viewport/ViewportWindow.h>
#include <ovito/opengl/OpenGLRenderer.h>
#include "OpenGLRendererEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OpenGLRendererEditor);
SET_OVITO_OBJECT_EDITOR(OpenGLRenderer, OpenGLRendererEditor);

/******************************************************************************
* Constructor that creates the UI controls for the editor.
******************************************************************************/
void OpenGLRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create the rollout.
    QWidget* rollout = createRollout(tr("OpenGL settings"), rolloutParams, "manual:rendering.opengl_renderer");

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
    IntegerParameterUI* antialiasingLevelUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OpenGLRenderer::antialiasingLevel));
    gridLayout->addWidget(antialiasingLevelUI->label(), 0, 0);
    gridLayout->addLayout(antialiasingLevelUI->createFieldLayout(), 0, 1);

    // Transparency rendering method
    QGroupBox* transparencyBox = new QGroupBox(tr("Transparency rendering method"), rollout);
    rootLayout->addWidget(transparencyBox);
    QHBoxLayout* boxLayout = new QHBoxLayout(transparencyBox);
    boxLayout->setContentsMargins(4,4,4,4);

    VariantComboBoxParameterUI* transparencyMethodUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(OpenGLRenderer::orderIndependentTransparency));
    transparencyMethodUI->comboBox()->addItem(tr("Back-to-Front Ordered (default)"), QVariant::fromValue(false));
    transparencyMethodUI->comboBox()->addItem(tr("Weighted Blended Order-Independent"), QVariant::fromValue(true));
    boxLayout->addWidget(transparencyMethodUI->comboBox());

    // Settings management functions.
    rootLayout->addWidget(createCopySettingsBetweenRenderersWidget());

    // Conditionally hide the "Quality" group box if this editor is for the interactive viewport renderer and not the final frame renderer.
    connect(this, &BaseSceneRendererEditor::editingInteractiveRenderer, qualityBox, &QWidget::hide);
}

/******************************************************************************
* Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
******************************************************************************/
void OpenGLRendererEditor::transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final)
{
    OpenGLRenderer* sourceRenderer = dynamic_object_cast<OpenGLRenderer>(source);
    OpenGLRenderer* targetRenderer = dynamic_object_cast<OpenGLRenderer>(target);
    if(sourceRenderer && targetRenderer) {
        targetRenderer->setOrderIndependentTransparency(sourceRenderer->orderIndependentTransparency());
    }
}

}   // End of namespace
