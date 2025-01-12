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
#include <ovito/particles/modifier/properties/ParticlesComputePropertyModifierDelegate.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerRadioButtonParameterUI.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include "ParticlesComputePropertyModifierDelegateEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesComputePropertyModifierDelegateEditor);
SET_OVITO_OBJECT_EDITOR(ParticlesComputePropertyModifierDelegate, ParticlesComputePropertyModifierDelegateEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ParticlesComputePropertyModifierDelegateEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* neighborRollout = createRollout(tr("Neighbor particles"), rolloutParams, "manual:particles.modifiers.compute_property");

    QVBoxLayout* mainLayout = new QVBoxLayout(neighborRollout);
    if(!rolloutParams.container())
        mainLayout->setContentsMargins(4,4,4,4);
    else
        mainLayout->setContentsMargins(0,0,0,0);

    neighborExpressionsGroupBox = new QGroupBox(tr("Neighbor particle expression"));
    mainLayout->addWidget(neighborExpressionsGroupBox);
    QGridLayout* groupBoxLayout = new QGridLayout(neighborExpressionsGroupBox);
    groupBoxLayout->setContentsMargins(4,4,4,4);
    groupBoxLayout->setSpacing(2);
    groupBoxLayout->setRowMinimumHeight(2, 4);
    groupBoxLayout->setColumnStretch(1, 1);

    IntegerRadioButtonParameterUI* neighborModePUI = createParamUI<IntegerRadioButtonParameterUI>(PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate::neighborMode));
    QRadioButton* cutoffModeBtn = neighborModePUI->addRadioButton(ParticlesComputePropertyModifierDelegate::Cutoff, tr("Cutoff range:"));
    groupBoxLayout->addWidget(cutoffModeBtn, 0, 0);
    QRadioButton* bondModeBtn = neighborModePUI->addRadioButton(ParticlesComputePropertyModifierDelegate::Bonded, tr("Bonded"));
    groupBoxLayout->addWidget(bondModeBtn, 1, 0, 1, 2);

    // Cutoff parameter.
    FloatParameterUI* cutoffRadiusUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate::cutoff));
    groupBoxLayout->addLayout(cutoffRadiusUI->createFieldLayout(), 0, 1);
    cutoffRadiusUI->setEnabled(false);
    connect(cutoffModeBtn, &QRadioButton::toggled, cutoffRadiusUI, &FloatParameterUI::setEnabled);

    // Show multiline fields.
    BooleanParameterUI* multilineFieldsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticlesComputePropertyModifierDelegate::useMultilineFields));
    groupBoxLayout->addWidget(multilineFieldsUI->checkBox(), 2, 0, 1, 2, Qt::AlignRight | Qt::AlignBottom);

    // Sublayout for the expression fields.
    neighborExpressionsLayout = new QGridLayout();
    neighborExpressionsLayout->setColumnStretch(1, 1);
    neighborExpressionsLayout->setContentsMargins(0,0,0,0);
    neighborExpressionsLayout->setSpacing(1);
    groupBoxLayout->addLayout(neighborExpressionsLayout, 3, 0, 1, 2);

    // Update input variables list if another modifier has been loaded into the editor.
    connect(this, &ParticlesComputePropertyModifierDelegateEditor::contentsReplaced, this, &ParticlesComputePropertyModifierDelegateEditor::updateExpressionFields);
    connect(this, &ParticlesComputePropertyModifierDelegateEditor::contentsReplaced, this, &ParticlesComputePropertyModifierDelegateEditor::updateVariablesList);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ParticlesComputePropertyModifierDelegateEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject()) {
        if(event.type() == ReferenceEvent::TargetChanged) {
            updateExpressionFieldsLater(this, mainWindow());
        }
        else if(event.type() == ReferenceEvent::ObjectStatusChanged) {
            updateVariablesListLater(this, mainWindow());
        }
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Updates the editor's display of the available expression variables.
******************************************************************************/
void ParticlesComputePropertyModifierDelegateEditor::updateVariablesList()
{
    if(ComputePropertyModificationNode* modNode = dynamic_object_cast<ComputePropertyModificationNode>(modificationNode())) {
        const QStringList& inputVariableNames = modNode->delegateInputVariableNames();
        for(AutocompleteLineEdit* box : neighborExpressionLineEdits)
            box->setWordList(inputVariableNames);
        for(AutocompleteTextEdit* box : neighborExpressionTextEdits)
            box->setWordList(inputVariableNames);
    }
}

/******************************************************************************
* Updates the editor's input fields for the expressions.
******************************************************************************/
void ParticlesComputePropertyModifierDelegateEditor::updateExpressionFields()
{
    ParticlesComputePropertyModifierDelegate* delegate = static_object_cast<ParticlesComputePropertyModifierDelegate>(editObject());
    if(!delegate) return;

    const QStringList& neighExpr = delegate->neighborExpressions();
    while(neighExpr.size() > neighborExpressionLineEdits.size()) {
        QLabel* label = new QLabel();
        AutocompleteLineEdit* lineEdit = new AutocompleteLineEdit();
        AutocompleteTextEdit* textEdit = new AutocompleteTextEdit();
        neighborExpressionsLayout->addWidget(label, neighborExpressionLineEdits.size(), 0);
        neighborExpressionsLayout->addWidget(lineEdit, neighborExpressionLineEdits.size(), 1);
        neighborExpressionsLayout->addWidget(textEdit, neighborExpressionTextEdits.size(), 1);
        neighborExpressionLineEdits.push_back(lineEdit);
        neighborExpressionTextEdits.push_back(textEdit);
        neighborExpressionLabels.push_back(label);
        connect(lineEdit, &AutocompleteLineEdit::editingFinished, this, &ParticlesComputePropertyModifierDelegateEditor::onExpressionEditingFinished);
        connect(textEdit, &AutocompleteTextEdit::editingFinished, this, &ParticlesComputePropertyModifierDelegateEditor::onExpressionEditingFinished);
    }
    while(neighExpr.size() < neighborExpressionLineEdits.size()) {
        delete neighborExpressionLineEdits.takeLast();
        delete neighborExpressionTextEdits.takeLast();
        delete neighborExpressionLabels.takeLast();
    }
    OVITO_ASSERT(neighborExpressionLineEdits.size() == neighExpr.size());
    OVITO_ASSERT(neighborExpressionTextEdits.size() == neighExpr.size());
    OVITO_ASSERT(neighborExpressionLabels.size() == neighExpr.size());
    if(delegate->useMultilineFields()) {
        for(AutocompleteLineEdit* lineEdit : neighborExpressionLineEdits) lineEdit->setVisible(false);
        for(AutocompleteTextEdit* textEdit : neighborExpressionTextEdits) textEdit->setVisible(true);
    }
    else {
        for(AutocompleteLineEdit* lineEdit : neighborExpressionLineEdits) lineEdit->setVisible(true);
        for(AutocompleteTextEdit* textEdit : neighborExpressionTextEdits) textEdit->setVisible(false);
    }

    QStringList standardPropertyComponentNames;
    if(ComputePropertyModifier* modifier = dynamic_object_cast<ComputePropertyModifier>(delegate->modifier())) {
        if(int typeId = modifier->outputProperty().standardTypeId(modifier->delegate()->inputContainerClass()))
            standardPropertyComponentNames = modifier->delegate()->inputContainerClass()->standardPropertyComponentNames(typeId);
    }

    for(int i = 0; i < neighExpr.size(); i++) {
        neighborExpressionLineEdits[i]->setText(neighExpr[i]);
        neighborExpressionTextEdits[i]->setPlainText(neighExpr[i]);
        if(neighExpr.size() == 1)
            neighborExpressionLabels[i]->hide();
        else {
            if(i < standardPropertyComponentNames.size())
                neighborExpressionLabels[i]->setText(tr("%1:").arg(standardPropertyComponentNames[i]));
            else
                neighborExpressionLabels[i]->setText(tr("%1:").arg(i+1));
            neighborExpressionLabels[i]->show();
        }
    }
}

/******************************************************************************
* Is called when the user has typed in an expression.
******************************************************************************/
void ParticlesComputePropertyModifierDelegateEditor::onExpressionEditingFinished()
{
    ParticlesComputePropertyModifierDelegate* delegate = static_object_cast<ParticlesComputePropertyModifierDelegate>(editObject());
    int index;
    QString expr;
    if(AutocompleteLineEdit* edit = dynamic_cast<AutocompleteLineEdit*>(sender())) {
        index = neighborExpressionLineEdits.indexOf(edit);
        expr = edit->text();
    }
    else if(AutocompleteTextEdit* edit = dynamic_cast<AutocompleteTextEdit*>(sender())) {
        index = neighborExpressionTextEdits.indexOf(edit);
        expr = edit->toPlainText();
    }
    else return;
    OVITO_ASSERT(index >= 0);
    performTransaction(tr("Change neighbor expression"), [delegate, expr, index]() {
        QStringList expressions = delegate->neighborExpressions();
        expressions[index] = expr;
        delegate->setNeighborExpressions(expressions);
    });
}

}   // End of namespace
