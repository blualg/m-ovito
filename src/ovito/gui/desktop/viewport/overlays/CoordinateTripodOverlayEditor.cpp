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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FontParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/Vector3ParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/viewport/overlays/MoveOverlayInputMode.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/general/ViewportModeButton.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/core/viewport/overlays/CoordinateTripodOverlay.h>
#include "CoordinateTripodOverlayEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CoordinateTripodOverlayEditor);
DEFINE_REFERENCE_FIELD(CoordinateTripodOverlayEditor, viewport);
SET_OVITO_OBJECT_EDITOR(CoordinateTripodOverlay, CoordinateTripodOverlayEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void CoordinateTripodOverlayEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Coordinate tripod"), rolloutParams, "manual:viewport_layers.coordinate_tripod");

    // Create the rollout contents.
    QVBoxLayout* parentLayout = new QVBoxLayout(rollout);
    parentLayout->setContentsMargins(4,4,4,4);
    parentLayout->setSpacing(4);

    QGroupBox* positionBox = new QGroupBox(tr("Positioning"));
    QGridLayout* positionLayout = new QGridLayout(positionBox);
    positionLayout->setContentsMargins(4,4,4,4);
    positionLayout->setColumnStretch(1, 1);
    positionLayout->setColumnStretch(2, 1);
    positionLayout->setSpacing(2);
    positionLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(positionBox);

    VariantComboBoxParameterUI* alignmentPUI = new VariantComboBoxParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::alignment));
    positionLayout->addWidget(new QLabel(tr("Alignment:")), 0, 0);
    positionLayout->addWidget(alignmentPUI->comboBox(), 0, 1, 1, 2);
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_left"), tr("Top left"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_right"), tr("Top right"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_right"), tr("Bottom right"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_left"), tr("Bottom left"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignLeft)));

    FloatParameterUI* offsetXPUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::offsetX));
    positionLayout->addWidget(new QLabel(tr("XY offset:")), 1, 0);
    positionLayout->addLayout(offsetXPUI->createFieldLayout(), 1, 1);
    FloatParameterUI* offsetYPUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::offsetY));
    positionLayout->addLayout(offsetYPUI->createFieldLayout(), 1, 2);

    OORef<MoveOverlayInputMode> moveOverlayMode = OORef<MoveOverlayInputMode>::create(this);
    connect(this, &QObject::destroyed, moveOverlayMode, &ViewportInputMode::removeMode);
    ViewportModeAction* moveOverlayAction = new ViewportModeAction(mainWindow(), tr("Move"), this, std::move(moveOverlayMode));
    moveOverlayAction->setIcon(QIcon::fromTheme("edit_mode_move"));
    moveOverlayAction->setToolTip(tr("Reposition the axes tripod in the viewport using the mouse"));
    positionLayout->addWidget(new ViewportModeButton(moveOverlayAction), 2, 1, 1, 2, Qt::AlignRight | Qt::AlignTop);

    QGroupBox* styleBox = new QGroupBox(tr("Style"));
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    styleLayout->setContentsMargins(4,4,4,4);
    styleLayout->setColumnStretch(1, 1);
    styleLayout->setSpacing(2);
    styleLayout->setHorizontalSpacing(6);
    parentLayout->addWidget(styleBox);

    int row = 0;

    // Perspective distortion.
    _perspectiveDistortionUI = new BooleanParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::perspectiveDistortion));
    styleLayout->addWidget(_perspectiveDistortionUI->checkBox(), row++, 0, 1, 2);
    styleLayout->setRowMinimumHeight(row++, 4);
    connect(this, &PropertiesEditor::contentsReplaced, this, [this]() {
        setViewport(editObject() ? this->activeViewport() : nullptr);
        _perspectiveDistortionUI->setEnabled(viewport() && viewport()->isPerspectiveProjection());
    });

    FloatParameterUI* sizePUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::tripodSize));
    styleLayout->addWidget(sizePUI->label(), row, 0);
    styleLayout->addLayout(sizePUI->createFieldLayout(), row++, 1);

    FloatParameterUI* lineWidthPUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::lineWidth));
    styleLayout->addWidget(lineWidthPUI->label(), row, 0);
    styleLayout->addLayout(lineWidthPUI->createFieldLayout(), row++, 1);

    FloatParameterUI* fontSizePUI = new FloatParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::fontSize));
    styleLayout->addWidget(fontSizePUI->label(), row, 0);
    styleLayout->addLayout(fontSizePUI->createFieldLayout(), row++, 1);

    BooleanParameterUI* outlineEnabledPUI = new BooleanParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::outlineEnabled));
    styleLayout->addWidget(outlineEnabledPUI->checkBox(), row, 0);
    outlineEnabledPUI->checkBox()->setText(tr("Text outline:"));

    ColorParameterUI* outlineColorPUI = new ColorParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::outlineColor));
    styleLayout->addWidget(outlineColorPUI->colorPicker(), row++, 1);

    FontParameterUI* labelFontPUI = new FontParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::font));
    styleLayout->addWidget(labelFontPUI->label(), row, 0);
    styleLayout->addWidget(labelFontPUI->fontPicker(), row++, 1);

#if 0 // Deprecated since OVITO 3.9.2
    styleLayout->setRowMinimumHeight(row++, 8);
    IntegerRadioButtonParameterUI* tripodStyleUI = new IntegerRadioButtonParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::tripodStyle));
    styleLayout->addWidget(new QLabel(tr("Axis style:")), row, 0);
    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->addWidget(tripodStyleUI->addRadioButton(CoordinateTripodOverlay::FlatArrows, tr("Flat")));
    hlayout->addWidget(tripodStyleUI->addRadioButton(CoordinateTripodOverlay::SolidArrows, tr("Solid")));
    styleLayout->addLayout(hlayout, row++, 1);
#endif

    // Create a second rollout.
    rollout = createRollout(tr("Coordinate axes"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);
    layout->setColumnStretch(1, 1);

    row = 0;
    QGridLayout* sublayout;
    StringParameterUI* axisLabelPUI;
    ColorParameterUI* axisColorPUI;
    BooleanGroupBoxParameterUI* axisPUI;

    // Axis 1.
    axisPUI = new BooleanGroupBoxParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis1Enabled));
    axisPUI->groupBox()->setTitle("Axis 1");
    layout->addWidget(axisPUI->groupBox(), row++, 0, 1, 2);
    sublayout = new QGridLayout(axisPUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(2);

    // Axis label.
    axisLabelPUI = new StringParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis1Label));
    sublayout->addWidget(new QLabel(tr("Label:")), 0, 0);
    sublayout->addWidget(axisLabelPUI->textBox(), 0, 1, 1, 2);

    // Axis color.
    axisColorPUI = new ColorParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis1Color));
    sublayout->addWidget(new QLabel(tr("Color:")), 1, 0);
    sublayout->addWidget(axisColorPUI->colorPicker(), 1, 1, 1, 2);

    // Axis direction.
    sublayout->addWidget(new QLabel(tr("Cartesian direction:")), 2, 0, 1, 3);
    for(int dim = 0; dim < 3; dim++) {
        Vector3ParameterUI* axisDirPUI = new Vector3ParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis1Dir), dim);
        sublayout->addLayout(axisDirPUI->createFieldLayout(), 3, dim, 1, 1);
    }

    // Axis 2
    axisPUI = new BooleanGroupBoxParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis2Enabled));
    axisPUI->groupBox()->setTitle("Axis 2");
    layout->addWidget(axisPUI->groupBox(), row++, 0, 1, 2);
    sublayout = new QGridLayout(axisPUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(2);

    // Axis label.
    axisLabelPUI = new StringParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis2Label));
    sublayout->addWidget(new QLabel(tr("Label:")), 0, 0);
    sublayout->addWidget(axisLabelPUI->textBox(), 0, 1, 1, 2);

    // Axis color.
    axisColorPUI = new ColorParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis2Color));
    sublayout->addWidget(new QLabel(tr("Color:")), 1, 0);
    sublayout->addWidget(axisColorPUI->colorPicker(), 1, 1, 1, 2);

    // Axis direction.
    sublayout->addWidget(new QLabel(tr("Cartesian direction:")), 2, 0, 1, 3);
    for(int dim = 0; dim < 3; dim++) {
        Vector3ParameterUI* axisDirPUI = new Vector3ParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis2Dir), dim);
        sublayout->addLayout(axisDirPUI->createFieldLayout(), 3, dim, 1, 1);
    }

    // Axis 3.
    axisPUI = new BooleanGroupBoxParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis3Enabled));
    axisPUI->groupBox()->setTitle("Axis 3");
    layout->addWidget(axisPUI->groupBox(), row++, 0, 1, 2);
    sublayout = new QGridLayout(axisPUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(2);

    // Axis label.
    axisLabelPUI = new StringParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis3Label));
    sublayout->addWidget(new QLabel(tr("Label:")), 0, 0);
    sublayout->addWidget(axisLabelPUI->textBox(), 0, 1, 1, 2);

    // Axis color.
    axisColorPUI = new ColorParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis3Color));
    sublayout->addWidget(new QLabel(tr("Color:")), 1, 0);
    sublayout->addWidget(axisColorPUI->colorPicker(), 1, 1, 1, 2);

    // Axis direction.
    sublayout->addWidget(new QLabel(tr("Cartesian direction:")), 2, 0, 1, 3);
    for(int dim = 0; dim < 3; dim++) {
        Vector3ParameterUI* axisDirPUI = new Vector3ParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis3Dir), dim);
        sublayout->addLayout(axisDirPUI->createFieldLayout(), 3, dim, 1, 1);
    }

    // Axis 4.
    axisPUI = new BooleanGroupBoxParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis4Enabled));
    axisPUI->groupBox()->setTitle("Axis 4");
    layout->addWidget(axisPUI->groupBox(), row++, 0, 1, 2);
    sublayout = new QGridLayout(axisPUI->childContainer());
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(2);

    // Axis label.
    axisLabelPUI = new StringParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis4Label));
    sublayout->addWidget(new QLabel(tr("Label:")), 0, 0);
    sublayout->addWidget(axisLabelPUI->textBox(), 0, 1, 1, 2);

    // Axis color.
    axisColorPUI = new ColorParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis4Color));
    sublayout->addWidget(new QLabel(tr("Color:")), 1, 0);
    sublayout->addWidget(axisColorPUI->colorPicker(), 1, 1, 1, 2);

    // Axis direction.
    sublayout->addWidget(new QLabel(tr("Cartesian direction:")), 2, 0, 1, 3);
    for(int dim = 0; dim < 3; dim++) {
        Vector3ParameterUI* axisDirPUI = new Vector3ParameterUI(this, PROPERTY_FIELD(CoordinateTripodOverlay::axis4Dir), dim);
        sublayout->addLayout(axisDirPUI->createFieldLayout(), 3, dim, 1, 1);
    }
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool CoordinateTripodOverlayEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == viewport() && event.type() == ReferenceEvent::TargetChanged && static_cast<const TargetChangedEvent&>(event).field() == PROPERTY_FIELD(Viewport::viewType)) {
        _perspectiveDistortionUI->setEnabled(viewport() && viewport()->isPerspectiveProjection());
    }
    return PropertiesEditor::referenceEvent(source, event);
}

}   // End of namespace
