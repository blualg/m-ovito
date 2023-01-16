////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2020 OVITO GmbH, Germany
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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/properties/PropertyReference.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include "ElementTypeEditor.h"

namespace Ovito::StdObj {

IMPLEMENT_OVITO_CLASS(ElementTypeEditor);
SET_OVITO_OBJECT_EDITOR(ElementType, ElementTypeEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ElementTypeEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Element Type"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);

    QGroupBox* nameBox = new QGroupBox(tr("Type"), rollout);
    QGridLayout* gridLayout = new QGridLayout(nameBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setColumnStretch(1, 1);
    layout1->addWidget(nameBox);

    // Name.
    _namePUI = new StringParameterUI(this, PROPERTY_FIELD(ElementType::name));
    gridLayout->addWidget(new QLabel(tr("Name:")), 0, 0);
    gridLayout->addWidget(_namePUI->textBox(), 0, 1);

    // Numeric ID.
    gridLayout->addWidget(new QLabel(tr("Numeric ID:")), 1, 0);
    _numericIdLabel = new QLabel();
    gridLayout->addWidget(_numericIdLabel, 1, 1);

    QGroupBox* appearanceBox = new QGroupBox(tr("Appearance"), rollout);
    gridLayout = new QGridLayout(appearanceBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setColumnStretch(1, 1);
    layout1->addWidget(appearanceBox);

    // Display color parameter.
    ColorParameterUI* colorPUI = new ColorParameterUI(this, PROPERTY_FIELD(ElementType::color));
    gridLayout->addWidget(colorPUI->label(), 0, 0);
    gridLayout->addWidget(colorPUI->colorPicker(), 0, 1);

    // "Save as preset" button
    _setAsDefaultBtn = new QPushButton(tr("Save as preset"));
    _setAsDefaultBtn->setToolTip(tr("Set the current color as future default for this type."));
    _setAsDefaultBtn->setEnabled(false);
    gridLayout->addWidget(_setAsDefaultBtn, 1, 0, 1, 2, Qt::AlignRight);
    connect(_setAsDefaultBtn, &QPushButton::clicked, this, &ElementTypeEditor::onSaveAsDefault);
}

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ElementTypeEditor::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    PropertiesEditor::referenceReplaced(field, oldTarget, newTarget, listIndex);

    if(field == PROPERTY_FIELD(PropertiesEditor::editObject)) {

        ElementType* etype = static_object_cast<ElementType>(newTarget);

        // Update the displayed numeric ID.
        _numericIdLabel->setText(etype ? QString::number(etype->numericId()) : QString());

        // Update the placeholder text of the name input field to reflect the numeric ID of the current element type.
        if(QLineEdit* lineEdit = qobject_cast<QLineEdit*>(_namePUI->textBox()))
            lineEdit->setPlaceholderText(etype ? QStringLiteral("<%1>").arg(ElementType::generateDefaultTypeName(etype->numericId())) : QString());

        // Enable/disable the button.
        _setAsDefaultBtn->setEnabled(etype != nullptr && !etype->ownerProperty().isNull());
    }
}

/******************************************************************************
* Saves the current settings as defaults for the element type.
******************************************************************************/
void ElementTypeEditor::onSaveAsDefault()
{
    ElementType* etype = static_object_cast<ElementType>(editObject());
    if(!etype) return;

    ElementType::setDefaultColor(etype->ownerProperty(), etype->nameOrNumericId(), etype->color());

    mainWindow().showStatusBarMessage(tr("Stored current color as default value for type '%1'.").arg(etype->nameOrNumericId()), 4000);
}

}   // End of namespace
