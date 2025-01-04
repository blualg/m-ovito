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
#include <ovito/particles/export/lammps/LAMMPSDataExporter.h>
#include <ovito/particles/import/lammps/LAMMPSDataImporter.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include "LAMMPSDataExporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSDataExporterEditor);
SET_OVITO_OBJECT_EDITOR(LAMMPSDataExporter, LAMMPSDataExporterEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LAMMPSDataExporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("LAMMPS Data File"), rolloutParams);

    // Create the rollout contents.
    QGridLayout* layout = new QGridLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    layout->addWidget(new QLabel(tr("LAMMPS atom style:")), 0, 0);
    VariantComboBoxParameterUI* atomStyleUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::atomStyle));
    for(int i = 1; i < LAMMPSDataImporter::AtomStyle_COUNT; i++) {
        LAMMPSDataImporter::LAMMPSAtomStyle atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(i);
        atomStyleUI->comboBox()->addItem(LAMMPSDataImporter::atomStyleName(atomStyle), QVariant::fromValue(i));
    }
    atomStyleUI->comboBox()->model()->sort(0);
    layout->addWidget(atomStyleUI->comboBox(), 0, 1);

    layout->addWidget(new QLabel(tr("Hybrid sub-styles:")), 1, 0);
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->setSpacing(6);
    sublayout->setContentsMargins(0,0,0,0);
    for(QComboBox*& substyleList : _subStyleLists) {
        substyleList = new QComboBox(rollout);
        substyleList->setEditable(false);
        for(int i = 1; i < LAMMPSDataImporter::AtomStyle_COUNT; i++) {
            LAMMPSDataImporter::LAMMPSAtomStyle atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(i);
            if(atomStyle != LAMMPSDataImporter::AtomStyle_Hybrid)
                substyleList->addItem(LAMMPSDataImporter::atomStyleName(atomStyle), QVariant::fromValue(i));
        }
        substyleList->model()->sort(0);
        substyleList->insertItem(0, QString());
        substyleList->setCurrentIndex(0);
        sublayout->addWidget(substyleList);
        connect(substyleList, qOverload<int>(&QComboBox::activated), this, &LAMMPSDataExporterEditor::hybridSubStyleSelected);
    }
    layout->addLayout(sublayout, 1, 1);

    IntegerParameterUI* precisionUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(FileExporter::floatOutputPrecision));
    layout->addWidget(precisionUI->label(), 2, 0);
    layout->addLayout(precisionUI->createFieldLayout(), 2, 1);

    BooleanParameterUI* omitMassesSectionUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::omitMassesSection));
    layout->addWidget(omitMassesSectionUI->checkBox(), 3, 0, 1, 2);

    BooleanParameterUI* ignoreParticleIdentifiersUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::ignoreParticleIdentifiers));
    layout->addWidget(ignoreParticleIdentifiersUI->checkBox(), 4, 0, 1, 2);

    BooleanParameterUI* generateConsecutiveTypeIdsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::generateConsecutiveTypeIds));
    layout->addWidget(generateConsecutiveTypeIdsUI->checkBox(), 5, 0, 1, 2);

    BooleanParameterUI* exportTypeNamesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::exportTypeNames));
    layout->addWidget(exportTypeNamesUI->checkBox(), 6, 0, 1, 2);

    BooleanParameterUI* restrictedTriclinicUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(LAMMPSDataExporter::restrictedTriclinic));
    layout->addWidget(restrictedTriclinicUI->checkBox(), 7, 0, 1, 2);

    connect(this, &PropertiesEditor::contentsChanged, this, &LAMMPSDataExporterEditor::updateUI);
}

/******************************************************************************
* Updates the displayed values in the UI elements.
******************************************************************************/
void LAMMPSDataExporterEditor::updateUI()
{
    if(LAMMPSDataExporter* exporter = static_object_cast<LAMMPSDataExporter>(editObject())) {
        if(exporter->atomStyle() == LAMMPSDataImporter::AtomStyle_Hybrid) {
            auto iter = exporter->atomSubStyles().cbegin();
            for(QComboBox* substyleList : _subStyleLists) {
                substyleList->setEnabled(true);
                substyleList->setCurrentIndex(0);
                if(iter != exporter->atomSubStyles().cend()) {
                    int styleIndex = substyleList->findData(QVariant::fromValue(static_cast<int>(*iter)));
                    if(styleIndex >= 0)
                        substyleList->setCurrentIndex(styleIndex);
                    ++iter;
                }
            }
            return;
        }
    }

    for(QComboBox* substyleList : _subStyleLists) {
        substyleList->setEnabled(false);
        substyleList->setCurrentIndex(0);
    }
}

/******************************************************************************
* Is called whenever the user selects a sub-style for atom style hybrid.
******************************************************************************/
void LAMMPSDataExporterEditor::hybridSubStyleSelected()
{
    if(LAMMPSDataExporter* exporter = static_object_cast<LAMMPSDataExporter>(editObject())) {
        std::vector<LAMMPSDataImporter::LAMMPSAtomStyle> hybridSubstyles;
        for(QComboBox* substyleList : _subStyleLists) {
            LAMMPSDataImporter::LAMMPSAtomStyle substyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(substyleList->currentData().toInt());
            if(substyle != LAMMPSDataImporter::AtomStyle_Unknown)
                hybridSubstyles.push_back(substyle);
        }
        exporter->setAtomSubStyles(std::move(hybridSubstyles));
    }
}

}   // End of namespace
