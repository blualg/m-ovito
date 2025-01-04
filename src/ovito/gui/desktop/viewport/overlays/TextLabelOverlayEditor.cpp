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
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FontParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/PipelineSelectionParameterUI.h>
#include <ovito/gui/desktop/viewport/overlays/MoveOverlayInputMode.h>
#include <ovito/gui/desktop/widgets/general/AutocompleteTextEdit.h>
#include <ovito/gui/desktop/widgets/general/ViewportModeButton.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ViewportModeAction.h>
#include <ovito/core/viewport/overlays/TextLabelOverlay.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include "TextLabelOverlayEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TextLabelOverlayEditor);
SET_OVITO_OBJECT_EDITOR(TextLabelOverlay, TextLabelOverlayEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void TextLabelOverlayEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Text label"), rolloutParams, "manual:viewport_layers.text_label");

    // Create the rollout contents.
    QVBoxLayout* parentLayout = new QVBoxLayout(rollout);
    parentLayout->setContentsMargins(4,4,4,4);
    parentLayout->setSpacing(4);

    // Label text.
    parentLayout->addWidget(new QLabel(tr("Text:")));
    StringParameterUI* labelTextPUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(TextLabelOverlay::labelText));
    _textEdit = new AutocompleteTextEdit();
    labelTextPUI->setTextBox(_textEdit);
    parentLayout->addWidget(labelTextPUI->textBox());

    QGroupBox* positionBox = new QGroupBox(tr("Positioning"));
    QGridLayout* positionLayout = new QGridLayout(positionBox);
    positionLayout->setContentsMargins(4,4,4,4);
    positionLayout->setColumnStretch(1, 1);
    positionLayout->setColumnStretch(2, 1);
    positionLayout->setSpacing(2);
    positionLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(positionBox);

    VariantComboBoxParameterUI* alignmentPUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(TextLabelOverlay::alignment));
    positionLayout->addWidget(new QLabel(tr("Alignment:")), 0, 0);
    positionLayout->addWidget(alignmentPUI->comboBox(), 0, 1, 1, 2);
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_left"), tr("Top left"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top"), tr("Top"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_top_right"), tr("Top right"), QVariant::fromValue((int)(Qt::AlignTop | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_right"), tr("Right"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_right"), tr("Bottom right"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignRight)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom"), tr("Bottom"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignHCenter)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_bottom_left"), tr("Bottom left"), QVariant::fromValue((int)(Qt::AlignBottom | Qt::AlignLeft)));
    alignmentPUI->comboBox()->addItem(QIcon::fromTheme("overlay_alignment_left"), tr("Left"), QVariant::fromValue((int)(Qt::AlignVCenter | Qt::AlignLeft)));

    FloatParameterUI* offsetXPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelOverlay::offsetX));
    positionLayout->addWidget(new QLabel(tr("XY offset:")), 1, 0);
    positionLayout->addLayout(offsetXPUI->createFieldLayout(), 1, 1);
    FloatParameterUI* offsetYPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelOverlay::offsetY));
    positionLayout->addLayout(offsetYPUI->createFieldLayout(), 1, 2);

    OORef<MoveOverlayInputMode> moveOverlayMode = OORef<MoveOverlayInputMode>::create(this);
    connect(this, &QObject::destroyed, moveOverlayMode, &ViewportInputMode::removeMode);
    ViewportModeAction* moveOverlayAction = new ViewportModeAction(mainWindow(), tr("Move"), this, std::move(moveOverlayMode));
    moveOverlayAction->setIcon(QIcon::fromTheme("edit_mode_move"));
    moveOverlayAction->setToolTip(tr("Reposition the label in the viewport using the mouse"));
    positionLayout->addWidget(new ViewportModeButton(moveOverlayAction), 2, 1, 1, 2, Qt::AlignRight | Qt::AlignTop);

    QGroupBox* styleBox = new QGroupBox(tr("Style"));
    QGridLayout* styleLayout = new QGridLayout(styleBox);
    styleLayout->setContentsMargins(4,4,4,4);
    styleLayout->setColumnStretch(1, 1);
    styleLayout->setSpacing(2);
    styleLayout->setHorizontalSpacing(4);
    parentLayout->addWidget(styleBox);

    int row = 0;
    FloatParameterUI* fontSizePUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TextLabelOverlay::fontSize));
    styleLayout->addWidget(new QLabel(tr("Font size:")), row, 0);
    styleLayout->addLayout(fontSizePUI->createFieldLayout(), row++, 1);

    // Text color.
    ColorParameterUI* textColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TextLabelOverlay::textColor));
    styleLayout->addWidget(new QLabel(tr("Color:")), row, 0);
    styleLayout->addWidget(textColorPUI->colorPicker(), row++, 1);

    BooleanParameterUI* outlineEnabledPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TextLabelOverlay::outlineEnabled));
    styleLayout->addWidget(outlineEnabledPUI->checkBox(), row, 0);
    outlineEnabledPUI->checkBox()->setText(tr("Outline:"));

    ColorParameterUI* outlineColorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(TextLabelOverlay::outlineColor));
    styleLayout->addWidget(outlineColorPUI->colorPicker(), row++, 1);

    FontParameterUI* labelFontPUI = createParamUI<FontParameterUI>(PROPERTY_FIELD(TextLabelOverlay::font));
    styleLayout->addWidget(labelFontPUI->label(), row, 0);
    styleLayout->addWidget(labelFontPUI->fontPicker(), row++, 1);

    QWidget* variablesRollout = createRollout(tr("Variables"), rolloutParams.after(rollout), "manual:viewport_layers.text_label");
    QGridLayout* variablesLayout = new QGridLayout(variablesRollout);
    variablesLayout->setContentsMargins(4,4,4,4);
    variablesLayout->setSpacing(4);
    variablesLayout->setColumnStretch(1, 1);

    PipelineSelectionParameterUI* pipelineUI = createParamUI<PipelineSelectionParameterUI>(PROPERTY_FIELD(ViewportOverlay::pipeline));
    variablesLayout->addWidget(new QLabel(tr("Source pipeline:")), 0, 0, 1, 2);
    variablesLayout->addWidget(pipelineUI->comboBox(), 1, 0, 1, 2);

    StringParameterUI* valueFormatStringPUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(TextLabelOverlay::valueFormatString));
    variablesLayout->addWidget(new QLabel(tr("Value format string:")), 2, 0);
    variablesLayout->addWidget(valueFormatStringPUI->textBox(), 2, 1);

    _attributeNamesList = new QLabel();
    _attributeNamesList->setWordWrap(true);
    _attributeNamesList->setTextInteractionFlags(Qt::TextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard));
    variablesLayout->addWidget(_attributeNamesList, 3, 0, 1, 2);

    // Whenever the pipeline input changes, update the list of available input variables.
    connect(this, &PropertiesEditor::pipelineInputChanged, this, &TextLabelOverlayEditor::updateVariablesList);
}

/******************************************************************************
* Updates the UI.
******************************************************************************/
void TextLabelOverlayEditor::updateVariablesList()
{
    QString str;
    QStringList variableNames;
    if(const PipelineFlowState& flowState = getPipelineInput()) {
        str.append(tr("<p>Dynamic attributes that can be referenced in the label text:</b><ul>"));
        for(const QString& attrName : flowState.buildAttributesMap().keys()) {
            str.append(QStringLiteral("<li>[%1]</li>").arg(attrName.toHtmlEscaped()));
            variableNames.push_back(QStringLiteral("[") + attrName + QStringLiteral("]"));
        }
        str.append(QStringLiteral("</ul></p><p></p>"));
    }
    _attributeNamesList->setText(str);
    _attributeNamesList->updateGeometry();
    _textEdit->setWordList(variableNames);
    container()->updateRolloutsLater();
}

}   // End of namespace
