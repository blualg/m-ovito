////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/particles/objects/VectorVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/Vector3ParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerCheckBoxParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/stdobj/gui/properties/PropertyColorMappingEditor.h>
#include "VectorVisEditor.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(VectorVisEditor);
SET_OVITO_OBJECT_EDITOR(VectorVis, VectorVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void VectorVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
	// Create a rollout.
	QWidget* rollout = createRollout(tr("Vector display"), rolloutParams, "manual:visual_elements.vectors");

    // Create the rollout contents.
	QGridLayout* layout = new QGridLayout(rollout);
	layout->setContentsMargins(4,4,4,4);
	layout->setSpacing(4);
	layout->setColumnStretch(2, 1);
	layout->setColumnMinimumWidth(0, 24);
	int row = 0;

	// Scaling factor.
	FloatParameterUI* scalingFactorUI = new FloatParameterUI(this, PROPERTY_FIELD(VectorVis::scalingFactor));
	layout->addWidget(scalingFactorUI->label(), row, 0, 1, 2);
	layout->addLayout(scalingFactorUI->createFieldLayout(), row++, 2);

	// Arrow width factor.
	FloatParameterUI* arrowWidthUI = new FloatParameterUI(this, PROPERTY_FIELD(VectorVis::arrowWidth));
	layout->addWidget(arrowWidthUI->label(), row, 0, 1, 2);
	layout->addLayout(arrowWidthUI->createFieldLayout(), row++, 2);

	// Arrow position.
	VariantComboBoxParameterUI* arrowPositionUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(VectorVis::arrowPosition));
	arrowPositionUI->comboBox()->addItem(QIcon(":/particles/icons/arrow_alignment_base.png"), tr("Base"), QVariant::fromValue<int>(VectorVis::Base));
	arrowPositionUI->comboBox()->addItem(QIcon(":/particles/icons/arrow_alignment_center.png"), tr("Center"), QVariant::fromValue<int>(VectorVis::Center));
	arrowPositionUI->comboBox()->addItem(QIcon(":/particles/icons/arrow_alignment_head.png"), tr("Head"), QVariant::fromValue<int>(VectorVis::Head));
	layout->addWidget(new QLabel(tr("Alignment:")), row, 0, 1, 2);
	layout->addWidget(arrowPositionUI->comboBox(), row++, 2);

	// Reverse direction.
	BooleanParameterUI* reverseArrowDirectionUI = new BooleanParameterUI(this, PROPERTY_FIELD(VectorVis::reverseArrowDirection));
	layout->addWidget(reverseArrowDirectionUI->checkBox(), row++, 2);

	// Shading mode.
	IntegerCheckBoxParameterUI* shadingModeUI = new IntegerCheckBoxParameterUI(this, PROPERTY_FIELD(VectorVis::shadingMode), CylinderPrimitive::NormalShading, CylinderPrimitive::FlatShading);
	shadingModeUI->checkBox()->setText(tr("Flat shading"));
	layout->addWidget(shadingModeUI->checkBox(), row++, 2);

	// Coloring mode.
	layout->addWidget(new QLabel(tr("Coloring:")), row++, 0, 1, 3);
	_coloringModeUI = new IntegerRadioButtonParameterUI(this, PROPERTY_FIELD(VectorVis::coloringMode));
	layout->addWidget(_coloringModeUI->addRadioButton(VectorVis::UniformColoring, tr("Uniform:")), row, 1);
	
	// Uniform color.
	_arrowColorUI = new ColorParameterUI(this, PROPERTY_FIELD(VectorVis::arrowColor));
	layout->addWidget(_arrowColorUI->colorPicker(), row++, 2);
	layout->addWidget(_coloringModeUI->addRadioButton(VectorVis::PseudoColoring, tr("Color mapping")), row++, 1, 1, 2);

	layout->setRowMinimumHeight(row++, 6);

	// Transparency.
	FloatParameterUI* transparencyUI = new FloatParameterUI(this, PROPERTY_FIELD(VectorVis::transparencyController));
	layout->addWidget(transparencyUI->label(), row, 0, 1, 2);
	layout->addLayout(transparencyUI->createFieldLayout(), row++, 2);

	layout->setRowMinimumHeight(row++, 6);

	// Offset vector.
	layout->addWidget(new QLabel(tr("Offset (XYZ):")), row++, 0, 1, 3);
	Vector3ParameterUI* offsetXUI = new Vector3ParameterUI(this, PROPERTY_FIELD(VectorVis::offset), 0);
	Vector3ParameterUI* offsetYUI = new Vector3ParameterUI(this, PROPERTY_FIELD(VectorVis::offset), 1);
	Vector3ParameterUI* offsetZUI = new Vector3ParameterUI(this, PROPERTY_FIELD(VectorVis::offset), 2);
	QHBoxLayout* sublayout = new QHBoxLayout();
	sublayout->setContentsMargins(0,0,0,0);
	sublayout->setSpacing(4);
	layout->addLayout(sublayout, row++, 0, 1, 3);
	sublayout->addLayout(offsetXUI->createFieldLayout(), 1);
	sublayout->addLayout(offsetYUI->createFieldLayout(), 1);
	sublayout->addLayout(offsetZUI->createFieldLayout(), 1);

	// Open a sub-editor for the property color mapping.
	_colorMappingParamUI = new SubObjectParameterUI(this, PROPERTY_FIELD(VectorVis::colorMapping), rolloutParams.after(rollout));

	// Whenever the pipeline input of the vis element changes, update the list of available
	// properties in the color mapping editor.
	connect(this, &PropertiesEditor::pipelineInputChanged, this, &VectorVisEditor::updateColoringOptions);

	// Update the coloring controls when a parameter of the vis element has been changed.
	connect(this, &PropertiesEditor::contentsChanged, this, &VectorVisEditor::updateColoringOptions);
}

/******************************************************************************
* Updates the coloring controls shown in the UI.
******************************************************************************/
void VectorVisEditor::updateColoringOptions()
{
	// Retrieve the PropertyContainer containing the vector property this vis element is associated with.
	ConstDataObjectRefPath path = getVisDataObjectPath();
	DataOORef<const PropertyContainer> container = path.size() >= 2 ? dynamic_object_cast<const PropertyContainer>(std::move(path[path.size() - 2])) : nullptr;

	// Do the vector arrows, which are associated with the particles, have explicit RGB colors assigned ("Vector Color" property exists)?
	bool hasExplicitColors = false;
	if(const ParticlesObject* particles = dynamic_object_cast<ParticlesObject>(container.get()))
		hasExplicitColors = particles->getProperty(ParticlesObject::VectorColorProperty) != nullptr;

	VectorVis::ColoringMode coloringMode = editObject() ? static_object_cast<VectorVis>(editObject())->coloringMode() : VectorVis::UniformColoring;
	if(container && coloringMode == VectorVis::PseudoColoring && !hasExplicitColors) {
		_colorMappingParamUI->setEnabled(true);
		_arrowColorUI->setEnabled(false);
		// Set property container containing the available properties the user can choose from.
		static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor())->setPropertyContainer(container);
	}
	else {
		_colorMappingParamUI->setEnabled(false);
		_arrowColorUI->setEnabled(!hasExplicitColors);
	}

	_coloringModeUI->buttonGroup()->button(VectorVis::PseudoColoring)->setEnabled(container && !container->properties().isEmpty() && !hasExplicitColors);
	_coloringModeUI->buttonGroup()->button(VectorVis::UniformColoring)->setEnabled(editObject() && !hasExplicitColors);
}

}	// End of namespace
