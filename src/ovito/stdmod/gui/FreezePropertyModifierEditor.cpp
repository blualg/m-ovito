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
#include <ovito/stdmod/modifiers/FreezePropertyModifier.h>
#include <ovito/stdobj/gui/widgets/PropertyReferenceParameterUI.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include "FreezePropertyModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(FreezePropertyModifierEditor);
SET_OVITO_OBJECT_EDITOR(FreezePropertyModifier, FreezePropertyModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void FreezePropertyModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Freeze property"), rolloutParams, "manual:particles.modifiers.freeze_property");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(2);

    DataObjectReferenceParameterUI* pclassUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(pclassUI->comboBox());
    layout->addSpacing(8);

    // Do not list data tables as available inputs.
    pclassUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
        return DataTable::OOClass().isMember(container) == false;
    });

    PropertyReferenceParameterUI* sourcePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(FreezePropertyModifier::sourceProperty), nullptr, PropertyReferenceParameterUI::ShowNoComponents, true);
    sourcePropertyUI->setContainerField(PROPERTY_FIELD(GenericPropertyModifier::subject));
    layout->addWidget(new QLabel(tr("Property to freeze:"), rollout));
    layout->addWidget(sourcePropertyUI->comboBox());
    connect(sourcePropertyUI, &PropertyReferenceParameterUI::valueEntered, this, &FreezePropertyModifierEditor::onSourcePropertyChanged);
    layout->addSpacing(8);

    PropertyReferenceParameterUI* destPropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(FreezePropertyModifier::destinationProperty), nullptr, PropertyReferenceParameterUI::ShowNoComponents, false);
    destPropertyUI->setContainerField(PROPERTY_FIELD(GenericPropertyModifier::subject));
    layout->addWidget(new QLabel(tr("Output property:"), rollout));
    layout->addWidget(destPropertyUI->comboBox());
    layout->addSpacing(8);

    QGridLayout* gridlayout = new QGridLayout();
    gridlayout->setContentsMargins(0,0,0,0);
    gridlayout->setColumnStretch(1, 1);
    gridlayout->setVerticalSpacing(4);

    IntegerParameterUI* freezeTimePUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(FreezePropertyModifier::freezeTime));
    gridlayout->addWidget(freezeTimePUI->label(), 0, 0);
    gridlayout->addLayout(freezeTimePUI->createFieldLayout(), 0, 1);

    gridlayout->setRowMinimumHeight(1, 6);
    BooleanParameterUI* tolerateNewElementsPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FreezePropertyModifier::tolerateNewElements));
    gridlayout->addWidget(tolerateNewElementsPUI->checkBox(), 2, 0, 1, 2);

    BooleanParameterUI* selectNewElementsPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(FreezePropertyModifier::selectNewElements));
    gridlayout->addWidget(selectNewElementsPUI->checkBox(), 3, 0, 1, 2);

    connect(this, &PropertiesEditor::contentsChanged, this, [=](RefTarget* editObject) {
        if(FreezePropertyModifier* modifier = static_object_cast<FreezePropertyModifier>(editObject)) {
            if(modifier->subject().dataClass()) {
                PropertyContainerClassPtr classPtr = modifier->subject().dataClass();
                tolerateNewElementsPUI->checkBox()->setText(tr("Tolerate newly appearing %1").arg(classPtr->elementDescriptionName()));
                selectNewElementsPUI->checkBox()->setText(tr("Select newly appearing %1").arg(classPtr->elementDescriptionName()));
            }
            selectNewElementsPUI->setEnabled(modifier->tolerateNewElements());
        }
    });

    layout->addLayout(gridlayout);
    layout->addSpacing(8);

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

/******************************************************************************
* Is called when the user has selected a different source property.
******************************************************************************/
void FreezePropertyModifierEditor::onSourcePropertyChanged()
{
    FreezePropertyModifier* mod = static_object_cast<FreezePropertyModifier>(editObject());
    if(!mod) return;

    performTransaction(tr("Freeze property"), [mod]() {
        // When the user selects a different source property, adjust the destination property automatically.
        mod->setDestinationProperty(mod->sourceProperty());
    });
}

}   // End of namespace
