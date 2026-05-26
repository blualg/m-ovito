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
#include <ovito/particles/modifier/modify/MoleculeCenterOfMassModifier.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include "MoleculeCenterOfMassModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(MoleculeCenterOfMassModifierEditor);
SET_OVITO_OBJECT_EDITOR(MoleculeCenterOfMassModifier, MoleculeCenterOfMassModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void MoleculeCenterOfMassModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Replace molecules with centers of mass"), rolloutParams,
                                     "manual:particles.modifiers.replace_molecules_with_centers_of_mass");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* selectionGroup = new QGroupBox(tr("Molecule selection"));
    layout->addWidget(selectionGroup);

    auto* selectionLayout = new QGridLayout(selectionGroup);
    selectionLayout->setContentsMargins(4, 4, 4, 4);
    selectionLayout->setSpacing(6);
    selectionLayout->setColumnStretch(1, 1);

    int row = 0;

    auto* sourceUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(MoleculeCenterOfMassModifier::selectionSource));
    sourceUI->comboBox()->addItem(tr("All molecules"), QVariant::fromValue((int)MoleculeCenterOfMassModifier::AllMolecules));
    sourceUI->comboBox()->addItem(tr("Current particle selection"), QVariant::fromValue((int)MoleculeCenterOfMassModifier::CurrentParticleSelection));
    sourceUI->comboBox()->addItem(tr("Atom type(s)"), QVariant::fromValue((int)MoleculeCenterOfMassModifier::AtomTypes));
    sourceUI->comboBox()->addItem(tr("Expression"), QVariant::fromValue((int)MoleculeCenterOfMassModifier::Expression));
    selectionLayout->addWidget(new QLabel(tr("Selection source")), row, 0);
    selectionLayout->addWidget(sourceUI->comboBox(), row++, 1);

    auto* typesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeCenterOfMassModifier::selectedTypes));
    typesUI->lineEdit()->setPlaceholderText(tr("e.g. O,H or 1,2"));
    auto* typesLabel = new QLabel(tr("Atom type(s)"), selectionGroup);
    auto* typesField = typesUI->textBox();
    selectionLayout->addWidget(typesLabel, row, 0);
    selectionLayout->addWidget(typesField, row++, 1);

    auto* expressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(MoleculeCenterOfMassModifier::selectionExpression));
    expressionUI->lineEdit()->setPlaceholderText(tr("e.g. ParticleType==2 && Position.X>0"));
    auto* expressionLabel = new QLabel(tr("Selection expression"), selectionGroup);
    auto* expressionRow = createSelectorPopupRow(
        selectionGroup,
        expressionUI->textBox(),
        expressionUI,
        tr("Molecule selector expression"),
        tr("Select molecules for reduction if any atom in the molecule matches this expression. "
           "Leave it empty to disable expression-based molecule selection."));
    selectionLayout->addWidget(expressionLabel, row, 0);
    selectionLayout->addWidget(expressionRow, row++, 1);

    auto updateSelectorVisibility = [this, typesLabel, typesField, expressionLabel, expressionRow]() {
        const auto* modifier = dynamic_object_cast<MoleculeCenterOfMassModifier>(editObject());
        const auto source = modifier ? modifier->selectionSource() : MoleculeCenterOfMassModifier::AllMolecules;
        const bool showTypes = (source == MoleculeCenterOfMassModifier::AtomTypes);
        const bool showExpression = (source == MoleculeCenterOfMassModifier::Expression);
        typesLabel->setVisible(showTypes);
        typesField->setVisible(showTypes);
        expressionLabel->setVisible(showExpression);
        expressionRow->setVisible(showExpression);
        container()->updateRolloutsLater();
    };

    connect(this, &PropertiesEditor::contentsChanged, this, updateSelectorVisibility);
    updateSelectorVisibility();

    auto* infoLabel = new QLabel(tr("Selected molecules are replaced by one center-of-mass particle each. "
                                    "Unselected molecules remain atomistic in the output."), rollout);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

}   // End of namespace
