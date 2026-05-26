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
#include <ovito/particles/modifier/properties/CalculatePropertyModifier.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/particles/gui/util/ParticleSelectorPopupEditor.h>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include "CalculatePropertyModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(CalculatePropertyModifierEditor);
SET_OVITO_OBJECT_EDITOR(CalculatePropertyModifier, CalculatePropertyModifierEditor);

/******************************************************************************
 * Sets up the UI widgets of the editor.
 ******************************************************************************/
void CalculatePropertyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Calculate property"), rolloutParams, "");

    auto* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setColumnStretch(1, 1);

    _selectorWidget = new QWidget();
    auto* selectorLayout = new QGridLayout(_selectorWidget);
    selectorLayout->setContentsMargins(0, 0, 0, 0);
    selectorLayout->setColumnStretch(1, 1);

    auto* fromTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::fromTypes));
    fromTypesUI->lineEdit()->setPlaceholderText(tr("e.g. O,H or 1,2"));
    auto* fromExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::fromExpression));
    selectorLayout->addWidget(new QLabel(tr("From atom type(s)")), 0, 0);
    selectorLayout->addWidget(createSelectorPopupRow(_selectorWidget,
                                                     fromTypesUI->lineEdit(),
                                                     fromExpressionUI,
                                                     tr("From selector expression override"),
                                                     tr("Uses this expression instead of the 'From atom type(s)' field. Leave it empty to use the type list.")),
                              0, 1);

    auto* toTypesUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::toTypes));
    toTypesUI->lineEdit()->setPlaceholderText(tr("e.g. H or 8"));
    auto* toExpressionUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::toExpression));
    selectorLayout->addWidget(new QLabel(tr("To atom type(s)")), 1, 0);
    selectorLayout->addWidget(createSelectorPopupRow(_selectorWidget,
                                                     toTypesUI->lineEdit(),
                                                     toExpressionUI,
                                                     tr("To selector expression override"),
                                                     tr("Uses this expression instead of the 'To atom type(s)' field. Leave it empty to use the type list.")),
                              1, 1);

    _selectorDescriptionLabel = new QLabel();
    _selectorDescriptionLabel->setWordWrap(true);
    selectorLayout->addWidget(_selectorDescriptionLabel, 2, 0, 1, 2);
    gridLayout->addWidget(_selectorWidget, 0, 0, 1, 2);

    _outputWidget = new QWidget();
    auto* outputLayout = new QGridLayout(_outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setColumnStretch(1, 1);
    auto* outputNameUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::outputPropertyName));
    outputNameUI->lineEdit()->setPlaceholderText(tr("Leave empty to use the default name"));
    outputLayout->addWidget(new QLabel(tr("Output property name")), 0, 0);
    outputLayout->addWidget(outputNameUI->lineEdit(), 0, 1);
    gridLayout->addWidget(_outputWidget, 1, 0, 1, 2);

    _expressionWidget = new QWidget();
    auto* expressionLayout = new QGridLayout(_expressionWidget);
    expressionLayout->setContentsMargins(0, 0, 0, 0);
    expressionLayout->setColumnStretch(1, 1);
    auto* scriptUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::script));
    auto* scriptEdit = new AutocompleteTextEdit();
    scriptEdit->setCommitOnReturn(false);
    scriptEdit->setPlaceholderText(tr("mx = Charge*Position.X\nmy = Charge*Position.Y\nmz = Charge*Position.Z\nX = mx\nY = my\nZ = mz\n\nor\n\nr2 = Position.X^2 + Position.Y^2 + Position.Z^2\nresult = sqrt(r2)"));
    scriptUI->setTextBox(scriptEdit);
    expressionLayout->addWidget(new QLabel(tr("Script")), 0, 0);
    expressionLayout->addWidget(scriptEdit, 0, 1);
    auto* helperLabel = new QLabel(tr("Write one assignment per line. Use <code>result = ...</code> for a scalar output or all three of "
                                      "<code>X = ...</code>, <code>Y = ...</code>, <code>Z = ...</code> for a vector output. "
                                      "Later lines can reuse variables defined on earlier lines. Pair scripts can use <code>@i.*</code>, <code>@j.*</code>, "
                                      "<code>Distance</code>, and <code>Delta.X/Y/Z</code>."));
    helperLabel->setWordWrap(true);
    helperLabel->setTextFormat(Qt::RichText);
    expressionLayout->addWidget(helperLabel, 1, 0, 1, 2);
    gridLayout->addWidget(_expressionWidget, 2, 0, 1, 2);
    _vectorExpressionWidget = nullptr;

    _groupingWidget = new QWidget();
    auto* groupingLayout = new QGridLayout(_groupingWidget);
    groupingLayout->setContentsMargins(0, 0, 0, 0);
    groupingLayout->setColumnStretch(1, 1);
    auto* groupingUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::groupingMode));
    groupingUI->comboBox()->addItem(tr("None"), QVariant::fromValue((int)CalculatePropertyModifier::NoGrouping));
    groupingUI->comboBox()->addItem(tr("Molecule"), QVariant::fromValue((int)CalculatePropertyModifier::GroupByMolecule));
    groupingLayout->addWidget(new QLabel(tr("Group by")), 0, 0);
    groupingLayout->addWidget(groupingUI->comboBox(), 0, 1);
    auto* groupingNote = new QLabel(tr("Grouping is applied after the script has computed the per-particle contribution. "
                                       "For example, a script can define a per-particle dipole contribution and <code>Group by = Molecule</code> will sum it within each molecule."));
    groupingNote->setWordWrap(true);
    groupingLayout->addWidget(groupingNote, 1, 0, 1, 2);
    gridLayout->addWidget(_groupingWidget, 3, 0, 1, 2);

    _reductionWidget = new QWidget();
    auto* reductionLayout = new QGridLayout(_reductionWidget);
    reductionLayout->setContentsMargins(0, 0, 0, 0);
    reductionLayout->setColumnStretch(1, 1);
    auto* reductionUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::reductionOperation));
    reductionUI->comboBox()->addItem(tr("None"), QVariant::fromValue((int)CalculatePropertyModifier::NoReduction));
    reductionUI->comboBox()->addItem(tr("Sum"), QVariant::fromValue((int)CalculatePropertyModifier::SumReduction));
    reductionUI->comboBox()->addItem(tr("Mean"), QVariant::fromValue((int)CalculatePropertyModifier::MeanReduction));
    reductionUI->comboBox()->addItem(tr("Min"), QVariant::fromValue((int)CalculatePropertyModifier::MinReduction));
    reductionUI->comboBox()->addItem(tr("Max"), QVariant::fromValue((int)CalculatePropertyModifier::MaxReduction));
    reductionLayout->addWidget(new QLabel(tr("Reduction")), 0, 0);
    reductionLayout->addWidget(reductionUI->comboBox(), 0, 1);
    auto* reductionNote = new QLabel(tr("Reduction is applied after any optional grouping step. Scalar scripts write a one-point data table and global attribute; vector scripts reduce the X/Y/Z components together."));
    reductionNote->setWordWrap(true);
    reductionLayout->addWidget(reductionNote, 1, 0, 1, 2);
    gridLayout->addWidget(_reductionWidget, 4, 0, 1, 2);

    auto* onlySelectedUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(CalculatePropertyModifier::onlySelectedParticles));
    gridLayout->addWidget(onlySelectedUI->checkBox(), 5, 0, 1, 2);

    layout->addLayout(gridLayout);
    layout->addSpacing(6);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());

    connect(this, &PropertiesEditor::contentsChanged, this, &CalculatePropertyModifierEditor::updateVisibleControls);
    connect(this, &PropertiesEditor::contentsReplaced, this, &CalculatePropertyModifierEditor::updateVisibleControls);

    updateVisibleControls();
}

void CalculatePropertyModifierEditor::updateVisibleControls()
{
    const CalculatePropertyModifier* modifier = static_object_cast<CalculatePropertyModifier>(editObject());
    Q_UNUSED(modifier);

    _selectorWidget->setVisible(true);
    _outputWidget->setVisible(true);
    _expressionWidget->setVisible(true);
    if(_vectorExpressionWidget)
        _vectorExpressionWidget->setVisible(false);
    _groupingWidget->setVisible(true);
    _reductionWidget->setVisible(true);

    if(_selectorDescriptionLabel) {
        _selectorDescriptionLabel->setText(tr("These selectors are optional and are used only for pair scripts. "
                                              "Use <code>@i.*</code> and <code>@j.*</code> in the script to evaluate over pairs. "
                                              "If both selectors are the same, only unique unordered pairs <code>i &lt; j</code> are generated."));
        _selectorDescriptionLabel->setTextFormat(Qt::RichText);
    }

    for(QWidget* widget = _selectorWidget ? _selectorWidget->parentWidget() : nullptr; widget; widget = widget->parentWidget()) {
        if(QLayout* widgetLayout = widget->layout()) {
            widgetLayout->invalidate();
            widgetLayout->activate();
        }
        widget->updateGeometry();
    }
}

}  // namespace Ovito
