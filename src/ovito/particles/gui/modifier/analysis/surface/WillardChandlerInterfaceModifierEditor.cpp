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
#include <ovito/particles/modifier/analysis/surface/WillardChandlerInterfaceModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include "WillardChandlerInterfaceModifierEditor.h"

#include <QLabel>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(WillardChandlerInterfaceModifierEditor);
SET_OVITO_OBJECT_EDITOR(WillardChandlerInterfaceModifier, WillardChandlerInterfaceModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void WillardChandlerInterfaceModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Willard-Chandler interface"), rolloutParams, "manual:particles.modifiers.willard_chandler_interface");

    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGroupBox* surfaceGroup = new QGroupBox(tr("Surface construction"));
    layout->addWidget(surfaceGroup);

    QGridLayout* surfaceLayout = new QGridLayout(surfaceGroup);
    surfaceLayout->setContentsMargins(4, 4, 4, 4);
    surfaceLayout->setSpacing(6);
    surfaceLayout->setColumnStretch(1, 1);

    int row = 0;

    FloatParameterUI* gaussianWidthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::gaussianWidth));
    surfaceLayout->addWidget(gaussianWidthUI->label(), row, 0);
    surfaceLayout->addLayout(gaussianWidthUI->createFieldLayout(), row++, 1);

    FloatParameterUI* isoValueUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::isoValue));
    surfaceLayout->addWidget(isoValueUI->label(), row, 0);
    surfaceLayout->addLayout(isoValueUI->createFieldLayout(), row++, 1);

    IntegerParameterUI* gridResolutionUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::gridResolution));
    surfaceLayout->addWidget(gridResolutionUI->label(), row, 0);
    surfaceLayout->addLayout(gridResolutionUI->createFieldLayout(), row++, 1);

    QGroupBox* classificationGroup = new QGroupBox(tr("Phase classification"));
    layout->addWidget(classificationGroup);

    QGridLayout* classificationLayout = new QGridLayout(classificationGroup);
    classificationLayout->setContentsMargins(4, 4, 4, 4);
    classificationLayout->setSpacing(6);
    classificationLayout->setColumnStretch(1, 1);

    row = 0;

    FloatParameterUI* interfacialThicknessUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::interfacialThickness));
    classificationLayout->addWidget(interfacialThicknessUI->label(), row, 0);
    classificationLayout->addLayout(interfacialThicknessUI->createFieldLayout(), row++, 1);

    BooleanParameterUI* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::onlySelectedParticles));
    classificationLayout->addWidget(onlySelectedUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* selectInterfacialUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::selectInterfacialParticles));
    classificationLayout->addWidget(selectInterfacialUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* selectVaporUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::selectVaporParticles));
    classificationLayout->addWidget(selectVaporUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* extendSelectionUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::extendSelection));
    classificationLayout->addWidget(extendSelectionUI->checkBox(), row++, 0, 1, 2);

    StringParameterUI* selectionExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::selectionExpression));
    selectionExpressionUI->lineEdit()->setPlaceholderText(tr("e.g. ParticleType==1 && Position.Z>0"));
    classificationLayout->addWidget(new QLabel(tr("Additional selector"), classificationGroup), row, 0);
    classificationLayout->addWidget(createSelectorPopupRow(
        classificationGroup,
        selectionExpressionUI->textBox(),
        selectionExpressionUI,
        tr("Additional selection expression"),
        tr("Select additional particles with this expression and union them with the Willard-Chandler selection. "
           "Leave it empty to use only the phase-based selection.")), row++, 1);

    QGroupBox* detachedGroup = new QGroupBox(tr("Detached-cluster correction"));
    layout->addWidget(detachedGroup);

    QGridLayout* detachedLayout = new QGridLayout(detachedGroup);
    detachedLayout->setContentsMargins(4, 4, 4, 4);
    detachedLayout->setSpacing(6);
    detachedLayout->setColumnStretch(1, 1);

    row = 0;

    BooleanParameterUI* correctDetachedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::correctDetachedClusters));
    detachedLayout->addWidget(correctDetachedUI->checkBox(), row++, 0, 1, 2);

    VariantComboBoxParameterUI* plateNormalUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateNormalDirection));
    plateNormalUI->comboBox()->addItem(tr("+X"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::PositiveX)));
    plateNormalUI->comboBox()->addItem(tr("-X"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::NegativeX)));
    plateNormalUI->comboBox()->addItem(tr("+Y"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::PositiveY)));
    plateNormalUI->comboBox()->addItem(tr("-Y"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::NegativeY)));
    plateNormalUI->comboBox()->addItem(tr("+Z"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::PositiveZ)));
    plateNormalUI->comboBox()->addItem(tr("-Z"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::NegativeZ)));
    detachedLayout->addWidget(new QLabel(tr("Plate normal"), detachedGroup), row, 0);
    detachedLayout->addWidget(plateNormalUI->comboBox(), row++, 1);

    VariantComboBoxParameterUI* plateReferenceSourceUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateReferenceSource));
    plateReferenceSourceUI->comboBox()->addItem(tr("Plate atoms from expression"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::PlateAtomsExpression)));
    plateReferenceSourceUI->comboBox()->addItem(tr("Fixed coordinate"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::FixedCoordinate)));
    detachedLayout->addWidget(new QLabel(tr("Plate reference source"), detachedGroup), row, 0);
    detachedLayout->addWidget(plateReferenceSourceUI->comboBox(), row++, 1);

    VariantComboBoxParameterUI* plateGapModeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateGapMode));
    plateGapModeUI->comboBox()->addItem(tr("Global plate top level"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::GlobalPlateTop)));
    plateGapModeUI->comboBox()->addItem(tr("Local plate top under cluster footprint"), QVariant::fromValue(static_cast<int>(WillardChandlerInterfaceModifier::LocalPlateTop)));
    detachedLayout->addWidget(new QLabel(tr("Plate top reference"), detachedGroup), row, 0);
    detachedLayout->addWidget(plateGapModeUI->comboBox(), row++, 1);

    FloatParameterUI* detachedGapCutoffUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::detachedClusterGapCutoff));
    detachedLayout->addWidget(detachedGapCutoffUI->label(), row, 0);
    detachedLayout->addLayout(detachedGapCutoffUI->createFieldLayout(), row++, 1);

    FloatParameterUI* detachedBottomPercentileUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::detachedClusterBottomPercentile));
    detachedLayout->addWidget(detachedBottomPercentileUI->label(), row, 0);
    detachedLayout->addLayout(detachedBottomPercentileUI->createFieldLayout(), row++, 1);

    FloatParameterUI* plateTopPercentileUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateTopPercentile));
    detachedLayout->addWidget(plateTopPercentileUI->label(), row, 0);
    detachedLayout->addLayout(plateTopPercentileUI->createFieldLayout(), row++, 1);

    IntegerParameterUI* minimumSupportAtomsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::detachedClusterMinimumSupportAtoms));
    detachedLayout->addWidget(minimumSupportAtomsUI->label(), row, 0);
    detachedLayout->addLayout(minimumSupportAtomsUI->createFieldLayout(), row++, 1);

    FloatParameterUI* plateTopCoordinateUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateTopCoordinate));
    detachedLayout->addWidget(plateTopCoordinateUI->label(), row, 0);
    detachedLayout->addLayout(plateTopCoordinateUI->createFieldLayout(), row++, 1);

    StringParameterUI* plateSelectionExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::plateSelectionExpression));
    plateSelectionExpressionUI->lineEdit()->setPlaceholderText(tr("e.g. ParticleType==3 && Position.Z<5"));
    detachedLayout->addWidget(new QLabel(tr("Plate selector"), detachedGroup), row, 0);
    detachedLayout->addWidget(createSelectorPopupRow(
        detachedGroup,
        plateSelectionExpressionUI->textBox(),
        plateSelectionExpressionUI,
        tr("Plate selection expression"),
        tr("Select the plate atoms that define the solid surface reference used to evaluate detached liquid clusters. "
           "The current Willard-Chandler classification stays unchanged unless detached-cluster correction is enabled.")), row++, 1);

    auto updateDetachedWidgets = [plateNormalUI, plateReferenceSourceUI, plateGapModeUI, detachedGapCutoffUI, detachedBottomPercentileUI, plateTopPercentileUI, minimumSupportAtomsUI, plateTopCoordinateUI, plateSelectionExpressionUI, correctDetachedUI]() {
        const bool enabled = correctDetachedUI->checkBox()->isChecked();
        const bool useExpression = plateReferenceSourceUI->comboBox()->currentData().toInt() == static_cast<int>(WillardChandlerInterfaceModifier::PlateAtomsExpression);
        plateNormalUI->setEnabled(enabled);
        plateReferenceSourceUI->setEnabled(enabled);
        plateGapModeUI->setEnabled(enabled);
        detachedGapCutoffUI->setEnabled(enabled);
        detachedBottomPercentileUI->setEnabled(enabled);
        plateTopPercentileUI->setEnabled(enabled);
        minimumSupportAtomsUI->setEnabled(enabled);
        plateTopCoordinateUI->setEnabled(enabled && !useExpression);
        plateSelectionExpressionUI->setEnabled(enabled && useExpression);
    };
    connect(correctDetachedUI->checkBox(), &QCheckBox::toggled, this, updateDetachedWidgets);
    connect(plateReferenceSourceUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged), this, updateDetachedWidgets);
    updateDetachedWidgets();

    OpenDataInspectorButton* phaseCountsButton =
        new OpenDataInspectorButton(this, tr("Phase counts"), WillardChandlerInterfaceModifier::PhaseCountsTableId.toString());
    classificationLayout->addWidget(phaseCountsButton, row++, 0, 1, 2);

    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    layout->addWidget(statusWidget);
    statusWidget->setMinimumHeight(56);

    createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(WillardChandlerInterfaceModifier::surfaceMeshVis),
                                        rolloutParams.after(rollout).setTitle(tr("Surface display")));
}

}   // End of namespace
