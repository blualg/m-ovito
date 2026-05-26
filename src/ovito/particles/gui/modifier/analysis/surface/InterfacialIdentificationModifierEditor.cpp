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
#include <ovito/particles/modifier/analysis/surface/InterfacialIdentificationModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/OpenDataInspectorButton.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "InterfacialIdentificationModifierEditor.h"

#include <QLabel>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(InterfacialIdentificationModifierEditor);
SET_OVITO_OBJECT_EDITOR(InterfacialIdentificationModifier, InterfacialIdentificationModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void InterfacialIdentificationModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Identify interfacial particles"), rolloutParams, "manual:particles.modifiers.identify_interfacial_particles");

    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QGroupBox* methodGroup = new QGroupBox(tr("Method"));
    layout->addWidget(methodGroup);

    QGridLayout* methodLayout = new QGridLayout(methodGroup);
    methodLayout->setContentsMargins(4, 4, 4, 4);
    methodLayout->setSpacing(6);
    methodLayout->setColumnStretch(2, 1);

    IntegerRadioButtonParameterUI* methodUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::method));
    QRadioButton* itimButton = methodUI->addRadioButton(InterfacialIdentificationModifier::ITIM, tr("ITIM (planar interface)"));
    QRadioButton* gitimButton = methodUI->addRadioButton(InterfacialIdentificationModifier::GITIM, tr("GITIM (curved interface)"));
    methodLayout->addWidget(itimButton, 0, 0, 1, 3);
    methodLayout->addWidget(gitimButton, 1, 0, 1, 3);

    QGroupBox* parameterGroup = new QGroupBox(tr("Parameters"));
    layout->addWidget(parameterGroup);

    QGridLayout* parameterLayout = new QGridLayout(parameterGroup);
    parameterLayout->setContentsMargins(4, 4, 4, 4);
    parameterLayout->setSpacing(6);
    parameterLayout->setColumnStretch(1, 1);

    int row = 0;

    FloatParameterUI* probeRadiusUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::probeSphereRadius));
    parameterLayout->addWidget(probeRadiusUI->label(), row, 0);
    parameterLayout->addLayout(probeRadiusUI->createFieldLayout(), row++, 1);

    IntegerParameterUI* maxLayersUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::maxLayers));
    parameterLayout->addWidget(maxLayersUI->label(), row, 0);
    parameterLayout->addLayout(maxLayersUI->createFieldLayout(), row++, 1);

    FloatParameterUI* radiusScaleUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::radiusScale));
    parameterLayout->addWidget(radiusScaleUI->label(), row, 0);
    parameterLayout->addLayout(radiusScaleUI->createFieldLayout(), row++, 1);

    QGroupBox* itimGroup = new QGroupBox(tr("ITIM options"));
    layout->addWidget(itimGroup);

    QGridLayout* itimLayout = new QGridLayout(itimGroup);
    itimLayout->setContentsMargins(4, 4, 4, 4);
    itimLayout->setSpacing(6);
    itimLayout->setColumnStretch(1, 1);

    row = 0;

    VariantComboBoxParameterUI* normalAxisUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::normalAxis));
    normalAxisUI->comboBox()->addItem(tr("X"), QVariant::fromValue(static_cast<int>(InterfacialIdentificationModifier::XAxis)));
    normalAxisUI->comboBox()->addItem(tr("Y"), QVariant::fromValue(static_cast<int>(InterfacialIdentificationModifier::YAxis)));
    normalAxisUI->comboBox()->addItem(tr("Z"), QVariant::fromValue(static_cast<int>(InterfacialIdentificationModifier::ZAxis)));
    itimLayout->addWidget(new QLabel(tr("Interface normal")), row, 0);
    itimLayout->addWidget(normalAxisUI->comboBox(), row++, 1);

    FloatParameterUI* meshSpacingUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::meshSpacing));
    itimLayout->addWidget(meshSpacingUI->label(), row, 0);
    itimLayout->addLayout(meshSpacingUI->createFieldLayout(), row++, 1);

    auto updateItimWidgets = [itimGroup, normalAxisUI, meshSpacingUI, itimButton]() {
        const bool enabled = itimButton->isChecked();
        itimGroup->setEnabled(enabled);
        normalAxisUI->setEnabled(enabled);
        meshSpacingUI->setEnabled(enabled);
    };
    connect(itimButton, &QRadioButton::toggled, this, updateItimWidgets);
    connect(gitimButton, &QRadioButton::toggled, this, updateItimWidgets);
    updateItimWidgets();

    QGroupBox* outputGroup = new QGroupBox(tr("Outputs"));
    layout->addWidget(outputGroup);

    QGridLayout* outputLayout = new QGridLayout(outputGroup);
    outputLayout->setContentsMargins(4, 4, 4, 4);
    outputLayout->setSpacing(6);
    outputLayout->setColumnStretch(0, 1);

    row = 0;

    BooleanParameterUI* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::onlySelectedParticles));
    outputLayout->addWidget(onlySelectedUI->checkBox(), row++, 0, 1, 2);

    BooleanParameterUI* selectInterfacialUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(InterfacialIdentificationModifier::selectInterfacialParticles));
    outputLayout->addWidget(selectInterfacialUI->checkBox(), row++, 0, 1, 2);

    OpenDataInspectorButton* openTableButton =
        new OpenDataInspectorButton(this, tr("Layer counts"), InterfacialIdentificationModifier::LayerCountsTableId.toString());
    outputLayout->addWidget(openTableButton, row++, 0, 1, 2);

    StatusWidget* statusWidget = createParamUI<ObjectStatusDisplay>()->statusWidget();
    layout->addWidget(statusWidget);
    statusWidget->setMinimumHeight(56);
}

}   // End of namespace
