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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "ParticlesVisEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ParticlesVisEditor);
SET_OVITO_OBJECT_EDITOR(ParticlesVis, ParticlesVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ParticlesVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Particle display"), rolloutParams, "manual:visual_elements.particles");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    // Shape.
    VariantComboBoxParameterUI* particleShapeUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(ParticlesVis::particleShape));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_sphere.png"), tr("Sphere/Ellipsoid"), QVariant::fromValue((int)ParticlesVis::Sphere));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_circle.png"), tr("Circle"), QVariant::fromValue((int)ParticlesVis::Circle));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cube.png"), tr("Cube/Box"), QVariant::fromValue((int)ParticlesVis::Box));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_square.png"), tr("Square"), QVariant::fromValue((int)ParticlesVis::Square));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cylinder.png"), tr("Cylinder"), QVariant::fromValue((int)ParticlesVis::Cylinder));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_spherocylinder.png"), tr("Spherocylinder"), QVariant::fromValue((int)ParticlesVis::Spherocylinder));
    layout->addWidget(new QLabel(tr("Standard shape:")), 1, 0);
    layout->addWidget(particleShapeUI->comboBox(), 1, 1);

    // Default radius.
    FloatParameterUI* defaultRadiusUI = new FloatParameterUI(this, PROPERTY_FIELD(ParticlesVis::defaultParticleRadius));
    layout->addWidget(defaultRadiusUI->label(), 2, 0);
    layout->addLayout(defaultRadiusUI->createFieldLayout(), 2, 1);

    // Radius scaling factor.
    FloatParameterUI* radiusScalingUI = new FloatParameterUI(this, PROPERTY_FIELD(ParticlesVis::radiusScaleFactor));
    layout->addWidget(radiusScalingUI->label(), 3, 0);
    layout->addLayout(radiusScalingUI->createFieldLayout(), 3, 1);

    // Rendering quality.
    VariantComboBoxParameterUI* renderingQualityUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(ParticlesVis::renderingQuality));
    renderingQualityUI->comboBox()->addItem(tr("Low"), QVariant::fromValue((int)ParticlePrimitive::LowQuality));
    renderingQualityUI->comboBox()->addItem(tr("Medium"), QVariant::fromValue((int)ParticlePrimitive::MediumQuality));
    renderingQualityUI->comboBox()->addItem(tr("High"), QVariant::fromValue((int)ParticlePrimitive::HighQuality));
    renderingQualityUI->comboBox()->addItem(tr("Automatic"), QVariant::fromValue((int)ParticlePrimitive::AutoQuality));
    layout->addWidget(new QLabel(tr("Rendering quality:")), 4, 0);
    layout->addWidget(renderingQualityUI->comboBox(), 4, 1);
}

}   // End of namespace
