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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/core/dataset/data/mesh/TriangleMeshVis.h>
#include "TriMeshVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TriMeshVisEditor);
SET_OVITO_OBJECT_EDITOR(TriangleMeshVis, TriMeshVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void TriMeshVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Triangle mesh display"), rolloutParams, "manual:visual_elements.triangle_mesh");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* surfaceGroupBox = new QGroupBox(tr("Surface"));
    QGridLayout* sublayout = new QGridLayout(surfaceGroupBox);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    layout->addWidget(surfaceGroupBox);

    ColorParameterUI* colorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TriangleMeshVis::color));
    sublayout->addWidget(colorUI->label(), 0, 0);
    sublayout->addWidget(colorUI->colorPicker(), 0, 1);

    FloatParameterUI* transparencyUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TriangleMeshVis::transparencyController));
    sublayout->addWidget(transparencyUI->label(), 1, 0);
    sublayout->addLayout(transparencyUI->createFieldLayout(), 1, 1);

    // Highlight edges group box
    BooleanGroupBoxParameterUI* highlightEdgesUI = createParamUI<BooleanGroupBoxParameterUI>(PROPERTY_FIELD(TriangleMeshVis::highlightEdges));
    sublayout = new QGridLayout(highlightEdgesUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    layout->addWidget(highlightEdgesUI->groupBox());

    FloatParameterUI* wireframeWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TriangleMeshVis::wireframeWidth));
    wireframeWidthUI->spinner()->setStandardValue(0.0);
    wireframeWidthUI->textBox()->setPlaceholderText(tr("‹default›"));
    sublayout->addWidget(wireframeWidthUI->label(), 0, 0);
    sublayout->addLayout(wireframeWidthUI->createFieldLayout(), 0, 1);

    ColorParameterUI* wireframeColorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TriangleMeshVis::wireframeColor));
    sublayout->addWidget(wireframeColorUI->label(), 1, 0);
    sublayout->addWidget(wireframeColorUI->colorPicker(), 1, 1);

    BooleanParameterUI* wireframeFullyOpaqueUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TriangleMeshVis::wireframeFullyOpaque));
    sublayout->addWidget(wireframeFullyOpaqueUI->checkBox(), 2, 1);
}

}   // End of namespace
