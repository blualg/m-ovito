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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/stdobj/gui/properties/PropertyColorMappingEditor.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/grid/objects/VoxelGridVis.h>
#include "VoxelGridVisEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(VoxelGridVisEditor);
SET_OVITO_OBJECT_EDITOR(VoxelGridVis, VoxelGridVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void VoxelGridVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Voxel grid display"), rolloutParams, "manual:visual_elements.voxel_grid");

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    VariantComboBoxParameterUI* representationModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(VoxelGridVis::representationMode));
    representationModeUI->comboBox()->addItem(tr("Boundary"), QVariant::fromValue((int)VoxelGridVis::RepresentationMode::Boundary));
    representationModeUI->comboBox()->addItem(tr("Volume"), QVariant::fromValue((int)VoxelGridVis::RepresentationMode::Volume));
    layout->addWidget(new QLabel(tr("Representation mode:")), 0, 0);
    layout->addWidget(representationModeUI->comboBox(), 0, 1);

    QWidget* container1 = new QWidget();
    QGridLayout* sublayout1 = new QGridLayout(container1);
    sublayout1->setContentsMargins(0,0,0,0);
    sublayout1->setSpacing(4);
    sublayout1->setColumnStretch(1, 1);
    layout->addWidget(container1, 1, 0, 1, 2);

    BooleanParameterUI* highlightLinesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoxelGridVis::highlightGridLines));
    sublayout1->addWidget(highlightLinesUI->checkBox(), 0, 0, 1, 2);

    BooleanParameterUI* interpolateColorsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(VoxelGridVis::interpolateColors));
    sublayout1->addWidget(interpolateColorsUI->checkBox(), 1, 0, 1, 2);

    FloatParameterUI* transparencyUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(VoxelGridVis::transparencyController));
    sublayout1->addWidget(transparencyUI->label(), 2, 0);
    sublayout1->addLayout(transparencyUI->createFieldLayout(), 2, 1);

    QWidget* container2 = new QWidget();
    QGridLayout* sublayout2 = new QGridLayout(container2);
    sublayout2->setContentsMargins(0,0,0,0);
    sublayout2->setSpacing(4);
    sublayout2->setColumnStretch(1, 1);
    layout->addWidget(container2, 2, 0, 1, 2);

    OpacityFunctionParameterUI* opacityFunctionUI = createParamUI<OpacityFunctionParameterUI>(PROPERTY_FIELD(VoxelGridVis::opacityFunction), PROPERTY_FIELD(VoxelGridVis::colorMapping));
    sublayout2->addWidget(opacityFunctionUI->plotWidget(), 0, 0, 1, 2);

    //FloatParameterUI* absorptionUnitDistanceUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(VoxelGridVis::absorptionUnitDistance));
    //sublayout2->addWidget(absorptionUnitDistanceUI->label(), 1, 0);
    //sublayout2->addLayout(absorptionUnitDistanceUI->createFieldLayout(), 1, 1);

    QLabel* label = new QLabel(tr(
        R"(<p style="font-size: small;">Note: Volume rendering is only supported by <a href="visrtx">VisRTX</a> and <a href="ospray">OSPRay</a> renderers.</p>)"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(label, &QLabel::linkActivated, this, [this](const QString& link) {
        if(link == "visrtx")
            mainWindow().actionManager()->openHelpTopic(QStringLiteral("manual:rendering.visrtx_renderer"));
        else if(link == "ospray")
            mainWindow().actionManager()->openHelpTopic(QStringLiteral("manual:rendering.ospray_renderer"));
    });
    sublayout2->addWidget(label, 1, 0, 1, 2);

    connect(this, &PropertiesEditor::contentsChanged, this, [this, container1, container2]() {
        if(VoxelGridVis* vis = static_object_cast<VoxelGridVis>(editObject())) {
            bool isVolumeRendering = vis->representationMode() == VoxelGridVis::RepresentationMode::Volume;
            if(container2->isVisible() != isVolumeRendering || container1->isVisible() == isVolumeRendering) {
                container1->setVisible(!isVolumeRendering);
                container2->setVisible(isVolumeRendering);
                container()->updateRollouts();
            }
        }
    });

    // Open a sub-editor for the property color mapping.
    SubObjectParameterUI* colorMappingParamUI = createParamUI<SubObjectParameterUI>(
        PROPERTY_FIELD(VoxelGridVis::colorMapping), rolloutParams.after(rollout).setEditorHint("HideDiscretizationOption"));

    // Whenever the pipeline input of the vis element changes, update the list of available
    // properties in the color mapping editor.
    connect(this, &PropertiesEditor::pipelineInputChanged, colorMappingParamUI, [this,colorMappingParamUI]() {
        // Retrieve the VoxelGrid object this vis element is associated with.
        DataOORef<const PropertyContainer> container = dynamic_object_cast<const PropertyContainer>(getVisDataObject());
        // We only show the color mapping panel if the VoxelGrid does not contain the RGB "Color" property.
        if(container && !container->getProperty(Property::GenericColorProperty)) {
            // Show color mapping panel.
            colorMappingParamUI->setEnabled(true);
            // Set it as property container containing the available properties the user can choose from.
            static_object_cast<PropertyColorMappingEditor>(colorMappingParamUI->subEditor())->setPropertyContainer(container);
        }
        else {
            // If the "Color" property is present, hide the color mapping panel, because the explicit RGB color values
            // take precedence during rendering of the voxel grid.
            colorMappingParamUI->setEnabled(false);
        }
    });
}

}   // End of namespace
