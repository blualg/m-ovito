////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/stdobj/gui/properties/PropertyColorMappingEditor.h>
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include "SurfaceMeshVisEditor.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(SurfaceMeshVisEditor);
SET_OVITO_OBJECT_EDITOR(SurfaceMeshVis, SurfaceMeshVisEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void SurfaceMeshVisEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(QString(), rolloutParams, "manual:visual_elements.surface_mesh");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* coloringGroupBox = new QGroupBox(tr("Color mapping mode"));
    QGridLayout* sublayout = new QGridLayout(coloringGroupBox);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    layout->addWidget(coloringGroupBox);

    // Coloring mode.
    _coloringModeUI = new IntegerRadioButtonParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::colorMappingMode));
    sublayout->addWidget(_coloringModeUI->addRadioButton(SurfaceMeshVis::NoPseudoColoring, tr("Uniform color:")), 0, 0);
    QHBoxLayout* boxlayout = new QHBoxLayout();
    boxlayout->setContentsMargins(0,0,0,0);
    sublayout->addLayout(boxlayout, 1, 0, 1, 2);
    boxlayout->addWidget(_coloringModeUI->addRadioButton(SurfaceMeshVis::VertexPseudoColoring, tr("Vertices")), 1);
    boxlayout->addWidget(_coloringModeUI->addRadioButton(SurfaceMeshVis::FacePseudoColoring, tr("Faces")), 1);
    boxlayout->addWidget(_coloringModeUI->addRadioButton(SurfaceMeshVis::RegionPseudoColoring, tr("Regions")), 1);

    _surfaceColorUI = new ColorParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::surfaceColor));
    sublayout->addWidget(_surfaceColorUI->colorPicker(), 0, 1);

    FloatParameterUI* surfaceTransparencyUI = new FloatParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::surfaceTransparencyController));
    sublayout->addWidget(new QLabel(tr("Transparency:")), 2, 0);
    sublayout->addLayout(surfaceTransparencyUI->createFieldLayout(), 2, 1);

    // Rendering options
    QGroupBox* renderingOptionsGroupBox = new QGroupBox(tr("Rendering options"));
    sublayout = new QGridLayout(renderingOptionsGroupBox);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    layout->addWidget(renderingOptionsGroupBox);

    BooleanParameterUI* smoothShadingUI = new BooleanParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::smoothShading));
    sublayout->addWidget(smoothShadingUI->checkBox(), 0, 0, 1, 2);

    BooleanParameterUI* reverseOrientationUI = new BooleanParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::reverseOrientation));
    sublayout->addWidget(reverseOrientationUI->checkBox(), 1, 0, 1, 2);

    _clipAtDomainBoundariesUI = new BooleanParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::clipAtDomainBoundaries));
    sublayout->addWidget(_clipAtDomainBoundariesUI->checkBox(), 2, 0, 1, 2);

    BooleanParameterUI* highlightEdgesUI = new BooleanParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::highlightEdges));
    sublayout->addWidget(highlightEdgesUI->checkBox(), 3, 0, 1, 2);

    _capGroupUI = new BooleanGroupBoxParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::showCap));
    _capGroupUI->groupBox()->setTitle(tr("Cap polygons"));
    sublayout = new QGridLayout(_capGroupUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(4);
    sublayout->setColumnStretch(1, 1);
    layout->addWidget(_capGroupUI->groupBox());

    ColorParameterUI* capColorUI = new ColorParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::capColor));
    sublayout->addWidget(capColorUI->label(), 0, 0);
    sublayout->addWidget(capColorUI->colorPicker(), 0, 1);

    FloatParameterUI* capTransparencyUI = new FloatParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::capTransparencyController));
    sublayout->addWidget(new QLabel(tr("Transparency:")), 1, 0);
    sublayout->addLayout(capTransparencyUI->createFieldLayout(), 1, 1);

    // Open a sub-editor for the property color mapping.
    _colorMappingParamUI = new SubObjectParameterUI(this, PROPERTY_FIELD(SurfaceMeshVis::surfaceColorMapping), rolloutParams.after(rollout));

    // Whenever the pipeline input of the vis element changes, update the list of available
    // properties in the color mapping editor.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &SurfaceMeshVisEditor::updateColoringOptions);

    // Update the coloring controls when a parameter of the vis element has been changed.
    connect(this, &PropertiesEditor::contentsChanged, this, &SurfaceMeshVisEditor::updateColoringOptions);
}

/******************************************************************************
* Updates the coloring controls shown in the UI.
******************************************************************************/
void SurfaceMeshVisEditor::updateColoringOptions()
{
    // Retrieve the SurfaceMesh object this vis element is associated with.
    DataOORef<const SurfaceMesh> surfaceMesh = dynamic_object_cast<const SurfaceMesh>(getVisDataObject());

    // Do vertices/faces/regions have the explicit colors assigned ("Color" property exists)?
    bool hasExplicitColors = false;
    if(surfaceMesh) {
        hasExplicitColors |= (surfaceMesh->vertices() && surfaceMesh->vertices()->getProperty(SurfaceMeshVertices::ColorProperty));
        hasExplicitColors |= (surfaceMesh->faces()    && surfaceMesh->faces()->getProperty(SurfaceMeshFaces::ColorProperty));
        hasExplicitColors |= (surfaceMesh->regions()  && surfaceMesh->regions()->getProperty(SurfaceMeshRegions::ColorProperty));
    }

    SurfaceMeshVis::ColorMappingMode mappingSource = editObject() ? static_object_cast<SurfaceMeshVis>(editObject())->colorMappingMode() : SurfaceMeshVis::NoPseudoColoring;
    if(surfaceMesh && mappingSource == SurfaceMeshVis::VertexPseudoColoring && !hasExplicitColors) {
        _colorMappingParamUI->setEnabled(true);
        _surfaceColorUI->setEnabled(false);
        // Set surface vertices as property container containing the available properties the user can choose from.
        static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor())->setPropertyContainer(surfaceMesh->vertices());
    }
    else if(surfaceMesh && mappingSource == SurfaceMeshVis::FacePseudoColoring && !hasExplicitColors) {
        // Show color mapping panel.
        _colorMappingParamUI->setEnabled(true);
        _surfaceColorUI->setEnabled(false);
        // Set surface faces as property container containing the available properties the user can choose from.
        static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor())->setPropertyContainer(surfaceMesh->faces());
    }
    else if(surfaceMesh && mappingSource == SurfaceMeshVis::RegionPseudoColoring && !hasExplicitColors) {
        // Show color mapping panel.
        _colorMappingParamUI->setEnabled(true);
        _surfaceColorUI->setEnabled(false);
        // Set surface regions as property container containing the available properties the user can choose from.
        static_object_cast<PropertyColorMappingEditor>(_colorMappingParamUI->subEditor())->setPropertyContainer(surfaceMesh->regions());
    }
    else {
        _colorMappingParamUI->setEnabled(false);
        _surfaceColorUI->setEnabled(!hasExplicitColors);
    }

    _coloringModeUI->buttonGroup()->button(SurfaceMeshVis::VertexPseudoColoring)->setEnabled(surfaceMesh && surfaceMesh->vertices() && !surfaceMesh->vertices()->properties().isEmpty() && !hasExplicitColors);
    _coloringModeUI->buttonGroup()->button(SurfaceMeshVis::FacePseudoColoring  )->setEnabled(surfaceMesh && surfaceMesh->faces()    && !surfaceMesh->faces()->properties().isEmpty() && !hasExplicitColors);
    _coloringModeUI->buttonGroup()->button(SurfaceMeshVis::RegionPseudoColoring)->setEnabled(surfaceMesh && surfaceMesh->regions()  && !surfaceMesh->regions()->properties().isEmpty() && !hasExplicitColors);
    _coloringModeUI->buttonGroup()->button(SurfaceMeshVis::NoPseudoColoring)->setEnabled(surfaceMesh && !hasExplicitColors);

    if(surfaceMesh && surfaceMesh->topology()) {
        if(surfaceMesh->domain()) {
            _clipAtDomainBoundariesUI->setEnabled(true);
            // Detect whether the current mesh is closed or not.
            // Depending on this we display the 'cap polygons' panel.
            bool isClosed = editObject() && static_object_cast<SurfaceMeshVis>(editObject())->surfaceIsClosed() && surfaceMesh->topology()->isClosed();
            _capGroupUI->setEnabled(isClosed);
        }
        else {
            _capGroupUI->setEnabled(false);
            _clipAtDomainBoundariesUI->setEnabled(false);
        }
    }
}

}   // End of namespace
