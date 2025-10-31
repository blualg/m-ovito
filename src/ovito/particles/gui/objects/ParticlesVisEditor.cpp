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
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdmod/modifiers/EditTypesModifier.h>
#include <ovito/stdmod/modifiers/ComputePropertyModifier.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/ModifyCommandPage.h>
#include <ovito/gui/desktop/mainwin/cmdpanel/CommandPanel.h>
#include <ovito/gui/base/mainwin/PipelineListModel.h>
#include <ovito/stdobj/gui/properties/TypesInspectionApplet.h>
#include <ovito/particles/gui/util/ParticleInspectionApplet.h>
#include "ParticlesVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesVisEditor);
SET_OVITO_OBJECT_EDITOR(ParticlesVis, ParticlesVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ParticlesVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Particles display"), rolloutParams, "manual:visual_elements.particles");
    QVBoxLayout* mainLayout = new QVBoxLayout(rollout);
    mainLayout->setContentsMargins(4,4,4,4);

    // Universal settings box
    QGroupBox* universalBox = new QGroupBox(tr("Universal"));
    QGridLayout* layout = new QGridLayout(universalBox);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);
    mainLayout->addWidget(universalBox);

    // Radius scaling factor.
    FloatParameterUI* radiusScalingUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticlesVis::radiusScaleFactor));
    layout->addWidget(radiusScalingUI->label(), 0, 0);
    layout->addLayout(radiusScalingUI->createFieldLayout(), 0, 1);

    // Rendering quality.
    VariantComboBoxParameterUI* renderingQualityUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(ParticlesVis::renderingQuality));
    renderingQualityUI->comboBox()->addItem(tr("Automatic"), QVariant::fromValue((int)ParticlePrimitive::AutoQuality));
    renderingQualityUI->comboBox()->addItem(tr("Low"), QVariant::fromValue((int)ParticlePrimitive::LowQuality));
    renderingQualityUI->comboBox()->addItem(tr("Medium"), QVariant::fromValue((int)ParticlePrimitive::MediumQuality));
    renderingQualityUI->comboBox()->addItem(tr("High"), QVariant::fromValue((int)ParticlePrimitive::HighQuality));
    renderingQualityUI->createResetAction();
    layout->addWidget(new QLabel(tr("Rendering quality:")), 1, 0);
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->setSpacing(0);
    sublayout->addWidget(renderingQualityUI->comboBox(), 1);
    sublayout->addWidget(renderingQualityUI->menuToolButton());
    layout->addLayout(sublayout, 1, 1);

    // Default settings box
    QGroupBox* defaultBox = new QGroupBox(tr("Default particle shape"));
    layout = new QGridLayout(defaultBox);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);
    mainLayout->addWidget(defaultBox);

    // Shape.
    _particleShapeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(ParticlesVis::particleShape));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_sphere.png"), tr("Sphere/Ellipsoid"), QVariant::fromValue((int)ParticlesVis::Sphere));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_circle.png"), tr("Circle"), QVariant::fromValue((int)ParticlesVis::Circle));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cube.png"), tr("Cube/Box"), QVariant::fromValue((int)ParticlesVis::Box));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_square.png"), tr("Square"), QVariant::fromValue((int)ParticlesVis::Square));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cylinder.png"), tr("Cylinder"), QVariant::fromValue((int)ParticlesVis::Cylinder));
    _particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_spherocylinder.png"), tr("Spherocylinder"), QVariant::fromValue((int)ParticlesVis::Spherocylinder));
    _particleShapeUI->createResetAction();
    layout->addWidget(new QLabel(tr("Style:")), 0, 0);
    sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->setSpacing(0);
    sublayout->addWidget(_particleShapeUI->comboBox(), 1);
    sublayout->addWidget(_particleShapeUI->menuToolButton());
    layout->addLayout(sublayout, 0, 1);

    // Default radius.
    _defaultRadiusUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticlesVis::defaultParticleRadius));
    layout->addWidget(_defaultRadiusUI->label(), 1, 0);
    layout->addLayout(_defaultRadiusUI->createFieldLayout(), 1, 1);

    // Notes label.
    QLabel* label = new QLabel(tr("<p style=\"font-size: small;\">Default settings are overridden by <a href=\"show_types\">per-type settings</a>, "
        "which in turn are overridden by <a href=\"show_particles\">per-particle properties</a>. "
        "To change the appearance of individual particle types, insert an <a href=\"edit_types\">Edit Types</a> modifier into the pipeline. "
        "To set the display properties of individual particles, insert a <a href=\"compute_property\">Compute Property</a> modifier.</p>"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(label, &QLabel::linkActivated, this, &ParticlesVisEditor::handleActionLinks);
    mainLayout->addWidget(label);

    // Whenever the pipeline input of the vis element changes, update the list of available parameters.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &ParticlesVisEditor::updateOptions);
}

/******************************************************************************
* Enables or disables UI options depending on the current pipeline input.
******************************************************************************/
void ParticlesVisEditor::updateOptions()
{
    bool enableDefaultRadiusOption = false;
    bool enableDefaultShapeOption = false;

    // Inspect all Particles objects this vis element is associated with.
    std::vector<DataOORef<const PropertyContainer>> colorMappingContainers;
    for(const DataObject* dataObject : getVisDataObjects()) {
        if(const Particles* particles = dynamic_object_cast<Particles>(dataObject)) {
            if(const Property* typeProperty = particles->getProperty(Particles::TypeProperty)) {
                for(const ElementType* etype : typeProperty->elementTypes()) {
                    if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(etype)) {
                        if(ptype->shape() == ParticlesVis::ParticleShape::Default)
                            enableDefaultShapeOption = true;
                        if(ptype->radius() == 0)
                            enableDefaultRadiusOption = true;
                    }
                }
            }
            else {
                enableDefaultRadiusOption = true;
                enableDefaultShapeOption = true;
                break;
            }
        }
    }

    _particleShapeUI->setEnabled(enableDefaultShapeOption);
    _defaultRadiusUI->setEnabled(enableDefaultRadiusOption);
}

/******************************************************************************
* Handles actions links in the editor UI, e.g., by inserting the specified
* modifier into the current pipeline.
******************************************************************************/
void ParticlesVisEditor::handleActionLinks(const QString& action)
{
    // Handle actions that open the data inspector.
    if(action == "show_types") {
        if(ui().mainWindow()->openDataInspector(TypesInspectionApplet::OOClass()) == false)
            QToolTip::showText(QCursor::pos(), tr("Could not open the 'Types' page of the data inspector, because there are no typed properties found in the current pipeline."), nullptr, QRect(), 3000);
        return;
    }
    else if(action == "show_particles") {
        if(ui().mainWindow()->openDataInspector(ParticleInspectionApplet::OOClass()) == false)
            QToolTip::showText(QCursor::pos(), tr("Could not open the 'Particles' page of the data inspector, because there are no particles found in the current pipeline."), nullptr, QRect(), 3000);
        return;
    }

    // Handle actions that insert modifiers into the current pipeline.
    if(DataVis* vis = dynamic_object_cast<DataVis>(editObject())) {
        performTransaction(tr("Insert modifier"), [&]() {

            // Create the modifier of the requested type.
            OORef<Modifier> modifier;
            if(action == "edit_types")
                modifier = OORef<EditTypesModifier>::create();
            else if(action == "compute_property")
                modifier = OORef<ComputePropertyModifier>::create();
            else {
                OVITO_ASSERT(false);
                return;
            }

            // Insert the modifier into the current pipeline.
            if(Pipeline* pipeline = ui().mainWindow()->commandPanel()->modifyPage()->pipelineListModel()->selectedPipeline()) {
                if(ModificationNode* modNode = pipeline->applyModifier(currentAnimationTime(), true, modifier)) {
                    ui().mainWindow()->commandPanel()->modifyPage()->startEditingPipelineNode(modNode);
                }
            }
        });
    }
}

}   // End of namespace
