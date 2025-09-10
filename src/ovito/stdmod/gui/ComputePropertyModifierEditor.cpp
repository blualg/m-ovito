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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/stdmod/modifiers/ComputePropertyModifier.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/gui/desktop/properties/ModifierDelegateParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanGroupBoxParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/SubObjectParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteLineEdit.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/gui/desktop/app/GuiApplication.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include "ComputePropertyModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ComputePropertyModifierEditor);
SET_OVITO_OBJECT_EDITOR(ComputePropertyModifier, ComputePropertyModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ComputePropertyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Compute property"), rolloutParams, "manual:particles.modifiers.compute_property");

    // Create the rollout contents.
    QVBoxLayout* mainLayout = new QVBoxLayout(rollout);
    mainLayout->setContentsMargins(4,4,4,4);
    mainLayout->setSpacing(6);

    QGroupBox* operateOnGroup = new QGroupBox(tr("Operate on"));
    QVBoxLayout* sublayout = new QVBoxLayout(operateOnGroup);
    sublayout->setContentsMargins(4,4,4,4);
    sublayout->setSpacing(6);
    mainLayout->addWidget(operateOnGroup);

    ModifierDelegateParameterUI* delegateUI = createParamUI<ModifierDelegateParameterUI>(ComputePropertyModifierDelegate::OOClass());
    sublayout->addWidget(delegateUI->comboBox());

    // Create the check box for the selection flag.
    onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ComputePropertyModifier::onlySelectedElements));
    sublayout->addWidget(onlySelectedUI->checkBox());

    QGroupBox* propertiesGroupBox = new QGroupBox(tr("Output"), rollout);
    mainLayout->addWidget(propertiesGroupBox);
    QGridLayout* propertiesLayout = new QGridLayout(propertiesGroupBox);
    propertiesLayout->setContentsMargins(6,6,6,6);
    propertiesLayout->setColumnStretch(1,1);
    propertiesLayout->setSpacing(4);

    // Output property
    outputPropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(ComputePropertyModifier::outputProperty), nullptr, PropertyReferenceParameterUI::ShowNoComponents, false);
    propertiesLayout->addWidget(new QLabel(tr("Property name:")), 0, 0);
    propertiesLayout->addWidget(outputPropertyUI->comboBox(), 0, 1);
    propertiesLayout->addWidget(new QLabel(tr("Components:")), 1, 0);
    componentNamesEdit = new EnterLineEdit();
    componentNamesEdit->setPlaceholderText(tr("‹scalar›"));
    propertiesLayout->addWidget(componentNamesEdit, 1, 1);
    connect(this, &PropertiesEditor::contentsChanged, this, [this](RefTarget* editObject) {
        ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(editObject);
        if(modifier && modifier->delegate()) {
            outputPropertyUI->setContainerRef(modifier->delegate()->inputContainerRef());
            int typeId = modifier->outputProperty().standardTypeId(modifier->delegate()->inputContainerClass());
            componentNamesEdit->setEnabled(typeId == Property::GenericUserProperty);
            componentNamesEdit->setText(modifier->effectiveComponentNames().join(QStringLiteral(", ")));
        }
        else {
            outputPropertyUI->setContainerRef({});
            componentNamesEdit->setEnabled(false);
            componentNamesEdit->clear();
        }
    });
    connect(componentNamesEdit, &QLineEdit::editingFinished, this, [this]() {
        if(ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(editObject())) {
            QStringList componentNames = componentNamesEdit->text().trimmed().split(QChar(','));
            for(QString& name : componentNames) name = name.trimmed();
            componentNames.removeIf([](const QString& name) { return name.isEmpty(); });
            if(modifier->delegate()) {
                int typeId = modifier->outputProperty().standardTypeId(modifier->delegate()->inputContainerClass());
                if(typeId == Property::GenericUserProperty) {
                    performTransaction(tr("Set components list"), [&]() {
                        modifier->setPropertyComponentCount(std::max(1, (int)componentNames.size()), componentNames);
                    });
                }
            }
        }
    });

    expressionsGroupBox = new QGroupBox(tr("Expression"));
    mainLayout->addWidget(expressionsGroupBox);
    expressionsLayout = new QGridLayout(expressionsGroupBox);
    expressionsLayout->setContentsMargins(4,4,4,4);
    expressionsLayout->setSpacing(1);
    expressionsLayout->setRowMinimumHeight(1,4);
    expressionsLayout->setColumnStretch(1,1);

    // Show multi-line fields.
    expandFieldsLabel = new QLabel();
    expandFieldsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    expressionsLayout->addWidget(expandFieldsLabel, 0, 1, Qt::AlignRight | Qt::AlignBottom);
    connect(expandFieldsLabel, &QLabel::linkActivated, this, [this]() {
        performTransaction(tr("Collapse/expand input fields"), [&]() {
            if(ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(editObject()))
                modifier->setUseMultilineFields(!modifier->useMultilineFields());
        });
    });
    connect(this, &PropertiesEditor::contentsChanged, this, [this](RefTarget* editObject) {
        ComputePropertyModifier* modifier = static_object_cast<ComputePropertyModifier>(editObject);
        if(modifier && modifier->useMultilineFields()) {
            expandFieldsLabel->setText(tr("<a href=\"expand\">↑</a>"));
            expandFieldsLabel->setToolTip(tr("Switch to single-line input fields"));
        }
        else {
            expandFieldsLabel->setText(tr("<a href=\"collapse\">↓<a>"));
            expandFieldsLabel->setToolTip(tr("Expand the input field(s)"));
        }
    });

    // Show editor of modifier delegate as an embedded widget.
    QWidget* delegateEditorContainer = new QWidget();
    mainLayout->addWidget(delegateEditorContainer);
    QVBoxLayout* delegateEditorLayout = new QVBoxLayout(delegateEditorContainer);
    delegateEditorLayout->setContentsMargins(0,0,0,0);
    delegateEditorLayout->setSpacing(0);
    createParamUI<SubObjectParameterUI>(PROPERTY_FIELD(DelegatingModifier::delegate), RolloutInsertionParameters().insertInto(delegateEditorContainer));

    // Status label.
    mainLayout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    // List of available input variables.
    QWidget* variablesRollout = createRollout(tr("Expression variables"), rolloutParams.after(rollout), "manual:particles.modifiers.compute_property");
    QVBoxLayout* variablesLayout = new QVBoxLayout(variablesRollout);
    variablesLayout->setContentsMargins(4,4,4,4);
    variableNamesDisplay = new QLabel();
    variableNamesDisplay->setWordWrap(true);
    variableNamesDisplay->setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
    variablesLayout->addWidget(variableNamesDisplay);

    // Update input variables list if another modifier has been loaded into the editor.
    connect(this, &ComputePropertyModifierEditor::contentsReplaced, this, &ComputePropertyModifierEditor::updateExpressionFields);
    connect(this, &ComputePropertyModifierEditor::contentsReplaced, this, &ComputePropertyModifierEditor::updateVariablesList);
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ComputePropertyModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::TargetChanged) {
        updateExpressionFieldsLater(this, ui());
    }
    else if(source == editObject() && event.type() == ReferenceEvent::ObjectStatusChanged) {
        updateVariablesListLater(this, ui());
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Updates the editor's display of the available expression variables.
******************************************************************************/
void ComputePropertyModifierEditor::updateVariablesList()
{
    ComputePropertyModifier* mod = static_object_cast<ComputePropertyModifier>(editObject());
    if(!mod)
        return;

    if(ComputePropertyModificationNode* modNode = dynamic_object_cast<ComputePropertyModificationNode>(modificationNode())) {
        const QStringList& inputVariableNames = modNode->inputVariableNames();
        for(AutocompleteLineEdit* box : expressionLineEdits)
            box->setWordList(inputVariableNames);
        for(AutocompleteTextEdit* box : expressionTextEdits)
            box->setWordList(inputVariableNames);
        QString descriptionStyle = GuiApplication::instance()->usingDarkTheme()
            ? QStringLiteral("color: #aaa; font-style: italic;")
            : QStringLiteral("color: #555; font-style: italic;");
        variableNamesDisplay->setText(
            QStringLiteral("%1<p></p>").arg(modNode->inputVariableTable()).replace(QStringLiteral("DESCRIPTION_STYLE_PLACEHOLDER"), descriptionStyle));
    }

    container()->updateRolloutsLater();
}

/******************************************************************************
* Updates the editor's input fields for the expressions.
******************************************************************************/
void ComputePropertyModifierEditor::updateExpressionFields()
{
    ComputePropertyModifier* mod = static_object_cast<ComputePropertyModifier>(editObject());
    if(!mod || !mod->delegate()) return;

    const QStringList& expr = mod->expressions();
    expressionsGroupBox->setTitle(mod->delegate()->expressionUITitle(expr.size()));
    while(expr.size() > expressionLineEdits.size()) {
        QLabel* label = new QLabel();
        AutocompleteLineEdit* lineEdit = new AutocompleteLineEdit();
        AutocompleteTextEdit* textEdit = new AutocompleteTextEdit();
        expressionsLayout->addWidget(label, expressionTextEdits.size()+2, 0);
        expressionsLayout->addWidget(lineEdit, expressionLineEdits.size()+2, 1);
        expressionsLayout->addWidget(textEdit, expressionTextEdits.size()+2, 1);
        expressionLineEdits.push_back(lineEdit);
        expressionTextEdits.push_back(textEdit);
        expressionLabels.push_back(label);
        connect(lineEdit, &AutocompleteLineEdit::editingFinished, this, &ComputePropertyModifierEditor::onExpressionEditingFinished);
        connect(textEdit, &AutocompleteTextEdit::editingFinished, this, &ComputePropertyModifierEditor::onExpressionEditingFinished);
    }
    while(expr.size() < expressionLineEdits.size()) {
        delete expressionLineEdits.takeLast();
        delete expressionTextEdits.takeLast();
        delete expressionLabels.takeLast();
    }
    OVITO_ASSERT(expressionLineEdits.size() == expr.size());
    OVITO_ASSERT(expressionTextEdits.size() == expr.size());
    OVITO_ASSERT(expressionLabels.size() == expr.size());
    if(mod->useMultilineFields()) {
        for(AutocompleteLineEdit* lineEdit : expressionLineEdits) lineEdit->setVisible(false);
        for(AutocompleteTextEdit* textEdit : expressionTextEdits) textEdit->setVisible(true);
        for(QLabel* label : expressionLabels) {
            label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            label->setMargin(4);
        }
    }
    else {
        for(AutocompleteLineEdit* lineEdit : expressionLineEdits) lineEdit->setVisible(true);
        for(AutocompleteTextEdit* textEdit : expressionTextEdits) textEdit->setVisible(false);
        for(QLabel* label : expressionLabels) {
            label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            label->setMargin(0);
        }
    }

    const QStringList propertyComponentNames = mod->effectiveComponentNames();
    for(int i = 0; i < expr.size(); i++) {
        expressionLineEdits[i]->setText(expr[i]);
        if(expressionTextEdits[i]->toPlainText() != expr[i])
            expressionTextEdits[i]->setPlainText(expr[i]);
        if(expr.size() == 1 && propertyComponentNames.empty())
            expressionLabels[i]->hide();
        else {
            if(i < propertyComponentNames.size())
                expressionLabels[i]->setText(tr("%1:").arg(propertyComponentNames[i]));
            else
                expressionLabels[i]->setText(tr("%1:").arg(i+1));
            expressionLabels[i]->show();
        }
    }

    onlySelectedUI->checkBox()->setText(tr("Compute only for selected %1").arg(mod->delegate()->elementLabel()));
    onlySelectedUI->setEnabled(mod->delegate()->inputContainerClass()->isValidStandardPropertyId(Property::GenericSelectionProperty));

    container()->updateRolloutsLater();
}

/******************************************************************************
* Is called when the user has typed in an expression.
******************************************************************************/
void ComputePropertyModifierEditor::onExpressionEditingFinished()
{
    ComputePropertyModifier* mod = static_object_cast<ComputePropertyModifier>(editObject());
    int index;
    QString expr;
    if(mod->useMultilineFields()) {
        AutocompleteTextEdit* edit = static_cast<AutocompleteTextEdit*>(sender());
        index = expressionTextEdits.indexOf(edit);
        expr = edit->toPlainText();
    }
    else {
        AutocompleteLineEdit* edit = static_cast<AutocompleteLineEdit*>(sender());
        index = expressionLineEdits.indexOf(edit);
        expr = edit->text();
    }
    OVITO_ASSERT(index >= 0);
    performTransaction(tr("Change expression"), [mod, expr, index]() {
        QStringList expressions = mod->expressions();
        expressions[index] = expr;
        mod->setExpressions(expressions);
    });
}

}   // End of namespace
