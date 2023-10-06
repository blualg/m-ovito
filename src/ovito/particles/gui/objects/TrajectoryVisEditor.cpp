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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/objects/TrajectoryVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/stdobj/gui/properties/PropertyColorMappingEditor.h>
#include "TrajectoryVisEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(TrajectoryVisEditor);
SET_OVITO_OBJECT_EDITOR(TrajectoryVis, TrajectoryVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void TrajectoryVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Trajectory display"), rolloutParams, "manual:visual_elements.trajectory_lines");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(2, 1);
    layout->setColumnMinimumWidth(0, 20);

    // Shading mode.
    VariantComboBoxParameterUI* shadingModeUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(TrajectoryVis::shadingMode));
    shadingModeUI->comboBox()->addItem(tr("Normal"), QVariant::fromValue((int)CylinderPrimitive::NormalShading));
    shadingModeUI->comboBox()->addItem(tr("Flat"), QVariant::fromValue((int)CylinderPrimitive::FlatShading));
    layout->addWidget(new QLabel(tr("Shading:")), 0, 0, 1, 2);
    layout->addWidget(shadingModeUI->comboBox(), 0, 2);

    // Line width.
    FloatParameterUI* lineWidthUI = new FloatParameterUI(this, PROPERTY_FIELD(TrajectoryVis::lineWidth));
    layout->addWidget(lineWidthUI->label(), 1, 0, 1, 2);
    layout->addLayout(lineWidthUI->createFieldLayout(), 1, 2);

    // Coloring mode.
    layout->addWidget(new QLabel(tr("Line coloring:")), 2, 0, 1, 3);
    _coloringModeUI = new IntegerRadioButtonParameterUI(this, PROPERTY_FIELD(TrajectoryVis::coloringMode));
    layout->addWidget(_coloringModeUI->addRadioButton(TrajectoryVis::UniformColoring, tr("Uniform:")), 3, 1);
    layout->addWidget(_coloringModeUI->addRadioButton(TrajectoryVis::PseudoColoring, tr("Color mapping")), 4, 1, 1, 2);

    // Line uniform color.
    _lineColorUI = new ColorParameterUI(this, PROPERTY_FIELD(TrajectoryVis::lineColor));
    layout->addWidget(_lineColorUI->colorPicker(), 3, 2);

    // Wrapped line display.
    BooleanParameterUI* wrappedLinesUI = new BooleanParameterUI(this, PROPERTY_FIELD(TrajectoryVis::wrappedLines));
    layout->addWidget(wrappedLinesUI->checkBox(), 5, 0, 1, 3);

    // Up to current time.
    BooleanParameterUI* showUpToCurrentTimeUI = new BooleanParameterUI(this, PROPERTY_FIELD(TrajectoryVis::showUpToCurrentTime));
    layout->addWidget(showUpToCurrentTimeUI->checkBox(), 6, 0, 1, 3);

    // Open a sub-editor for the property color mapping.
    _colorMappingParamUI = new SubObjectParameterUI(this, PROPERTY_FIELD(TrajectoryVis::colorMapping), rolloutParams.after(rollout));

    // Whenever the pipeline input of the vis element changes, update the list of available
    // properties in the color mapping editor.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &TrajectoryVisEditor::updateColoringOptions);

    // Update the coloring controls when a parameter of the vis element has been changed.
    connect(this, &PropertiesEditor::contentsChanged, this, &TrajectoryVisEditor::updateColoringOptions);
}

/******************************************************************************
* Updates the coloring controls shown in the UI.
******************************************************************************/
void TrajectoryVisEditor::updateColoringOptions()
{
    // Retrieve the TrajectoryLines this vis element is associated with.
    DataOORef<const TrajectoryLines> trajectoryObject = dynamic_object_cast<const TrajectoryLines>(getVisDataObject());

    // Do lines have explicit RGB colors assigned ("Color" property exists)?
    bool hasExplicitColors = (trajectoryObject && trajectoryObject->getProperty(TrajectoryLines::ColorProperty));

    TrajectoryVis::ColoringMode coloringMode = editObject() ? static_object_cast<TrajectoryVis>(editObject())->coloringMode() : TrajectoryVis::UniformColoring;
    if(trajectoryObject && coloringMode == TrajectoryVis::PseudoColoring && !hasExplicitColors) {
        _colorMappingParamUI->setEnabled(true);
        _lineColorUI->setEnabled(false);
        // Set trajectory lines as property container containing the available properties the user can choose from.
        static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor())->setPropertyContainer(trajectoryObject);
    }
    else {
        _colorMappingParamUI->setEnabled(false);
        _lineColorUI->setEnabled(!hasExplicitColors);
    }

    _coloringModeUI->buttonGroup()->button(TrajectoryVis::PseudoColoring)->setEnabled(trajectoryObject && !trajectoryObject->properties().isEmpty() && !hasExplicitColors);
    _coloringModeUI->buttonGroup()->button(TrajectoryVis::UniformColoring)->setEnabled(trajectoryObject && !hasExplicitColors);
}

}   // End of namespace
