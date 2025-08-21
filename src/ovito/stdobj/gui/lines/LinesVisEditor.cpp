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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/lines/Lines.h>
#include <ovito/stdobj/lines/LinesVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/stdobj/gui/properties/PropertyColorMappingEditor.h>
#include "LinesVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LinesVisEditor);
SET_OVITO_OBJECT_EDITOR(LinesVis, LinesVisEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void LinesVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Lines display"), rolloutParams, "manual:visual_elements.lines");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setColumnStretch(2, 1);
    layout->setColumnMinimumWidth(0, 20);

    int row = 0;
    // Shading mode.
    VariantComboBoxParameterUI* shadingModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(LinesVis::shadingMode));
    shadingModeUI->comboBox()->addItem(tr("Normal"), QVariant::fromValue((int)CylinderPrimitive::NormalShading));
    shadingModeUI->comboBox()->addItem(tr("Flat"), QVariant::fromValue((int)CylinderPrimitive::FlatShading));
    layout->addWidget(new QLabel(tr("Shading:")), row, 0, 1, 2);
    layout->addWidget(shadingModeUI->comboBox(), row++, 2);

    // Line width.
    FloatParameterUI* lineWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(LinesVis::lineWidth));
    layout->addWidget(lineWidthUI->label(), row, 0, 1, 2);
    layout->addLayout(lineWidthUI->createFieldLayout(), row++, 2);

    // Coloring mode.
    layout->addWidget(new QLabel(tr("Line coloring:")), row++, 0, 1, 3);
    _coloringModeUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(LinesVis::coloringMode));
    layout->addWidget(_coloringModeUI->addRadioButton(LinesVis::UniformColoring, tr("Uniform:")), row, 1);
    _lineColorUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(LinesVis::lineColor));
    layout->addWidget(_lineColorUI->colorPicker(), row++, 2);
    layout->addWidget(_coloringModeUI->addRadioButton(LinesVis::PseudoColoring, tr("Color mapping")), row++, 1, 1, 2);

    // Line end caps
    BooleanParameterUI* lineEndCapUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LinesVis::roundedCaps));
    layout->addWidget(lineEndCapUI->checkBox(), row++, 0, 1, 3);

    // Wrapped line display.
    BooleanParameterUI* wrappedLinesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LinesVis::wrappedLines));
    layout->addWidget(wrappedLinesUI->checkBox(), row++, 0, 1, 3);

    // Up to current time.
    _showUpToCurrentTimeUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LinesVis::showUpToCurrentTime));
    layout->addWidget(_showUpToCurrentTimeUI->checkBox(), row++, 0, 1, 3);

    // Open a sub-editor for the property color mapping.
    _colorMappingParamUI = createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(LinesVis::colorMapping), rolloutParams.after(rollout));

    // Whenever the pipeline input of the vis element changes, update the list of available
    // properties in the color mapping editor.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &LinesVisEditor::updateColoringOptions);

    // Update the coloring controls when a parameter of the vis element has been changed.
    connect(this, &PropertiesEditor::contentsChanged, this, &LinesVisEditor::updateColoringOptions);
}

/******************************************************************************
 * Updates the coloring controls shown in the UI.
 ******************************************************************************/
void LinesVisEditor::updateColoringOptions()
{
    bool enableUniformColor = false;
    bool enablePseudoColoring = false;
    bool enableUniformColoringOption = false;
    bool enablePseudoColoringOption = false;
    bool enableShowUpToCurrentTime = false;

    std::vector<DataOORef<const PropertyContainer>> colorMappingContainers;
    LinesVis::ColoringMode coloringMode = editObject() ? static_object_cast<LinesVis>(editObject())->coloringMode() : LinesVis::UniformColoring;

    // Inspect all Lines objects this vis element is associated with.
    for(const DataObject* dataObject : getVisDataObjects()) {
        if(const Lines* linesObject = dynamic_object_cast<Lines>(dataObject)) {

            // Do lines have explicit RGB colors assigned ("Color" property exists)?
            if(!linesObject->getProperty(Lines::ColorProperty)) {
                if(coloringMode == LinesVis::PseudoColoring) {
                    enablePseudoColoring = true;
                    colorMappingContainers.push_back(linesObject);
                }
                else {
                    enableUniformColor = true;
                }

                enablePseudoColoringOption |= !linesObject->properties().isEmpty();
                enableUniformColoringOption = true;
            }

            // Enable "Show up to current time only" option only if the lines have the "Time" property.
            enableShowUpToCurrentTime |= (bool)linesObject->getProperty(Lines::SampleTimeProperty);
        }
    }

    _lineColorUI->setEnabled(enableUniformColor);
    _colorMappingParamUI->setEnabled(enablePseudoColoring);
    if(PropertyColorMappingEditor* colorMappingEditor = static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor()))
        colorMappingEditor->setPropertyContainers(std::move(colorMappingContainers));
    _coloringModeUI->buttonGroup()->button(LinesVis::UniformColoring)->setEnabled(enableUniformColoringOption);
    _coloringModeUI->buttonGroup()->button(LinesVis::PseudoColoring)->setEnabled(enablePseudoColoringOption);
    _showUpToCurrentTimeUI->setEnabled(enableShowUpToCurrentTime);
}

}  // namespace Ovito
