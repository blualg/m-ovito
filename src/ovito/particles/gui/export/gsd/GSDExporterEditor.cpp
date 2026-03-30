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
#include <ovito/particles/export/gsd/GSDExporter.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include "GSDExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(GSDExporterEditor);
SET_OVITO_OBJECT_EDITOR(GSDExporter, GSDExporterEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void GSDExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("GSD format"), rolloutParams, "manual:file_formats.output.gsd");

    // Create the rollout contents.
    QHBoxLayout* layout = new QHBoxLayout(rollout);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    QGridLayout* sublayout = new QGridLayout();
    sublayout->setColumnStretch(3, 1);
    layout->addLayout(sublayout);

    // Floating point precision
    int row = 0;
    sublayout->addWidget(new QLabel(tr("%1:").arg(PROPERTY_FIELD(GSDExporter::dataType)->displayName())), row, 0);
    auto* dataTypeUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(GSDExporter::dataType));
    sublayout->addWidget(dataTypeUI->addRadioButton(GSDExporter::Float32, tr("float32")), row, 1);
    sublayout->addWidget(dataTypeUI->addRadioButton(GSDExporter::Float64, tr("float64")), row, 2);
}

}  // namespace Ovito
