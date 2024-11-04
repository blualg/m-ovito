////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/particles/import/lammps/LAMMPSDataImporter.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include "LAMMPSDataImporterEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(LAMMPSDataImporterEditor);
SET_OVITO_OBJECT_EDITOR(LAMMPSDataImporter, LAMMPSDataImporterEditor);

/******************************************************************************
* This method is called by the FileSource each time a new source
* file has been selected by the user.
******************************************************************************/
void LAMMPSDataImporterEditor::inspectNewFile(FileImporter* importer, const QUrl& sourceFile, MainWindow& mainWindow)
{
    // Inspect the data file and try to detect the LAMMPS atom style.
    LAMMPSDataImporter* dataImporter = static_object_cast<LAMMPSDataImporter>(importer);
    LAMMPSDataImporter::LAMMPSAtomStyleHints detectedAtomStyleHints = ProgressDialog::blockForFuture(dataImporter->inspectFileHeader(sourceFile), mainWindow, tr("Inspecting file header"));

    // Show dialog to ask user for the right LAMMPS atom style if it could not be detected.
    if(detectedAtomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Unknown || (detectedAtomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Hybrid && detectedAtomStyleHints.atomSubStyles.empty())) {

        QSettings settings;
        settings.beginGroup(LAMMPSDataImporter::OOClass().plugin()->pluginId());
        settings.beginGroup(LAMMPSDataImporter::OOClass().name());

        if(detectedAtomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Unknown)
            detectedAtomStyleHints.atomStyle = LAMMPSDataImporter::parseAtomStyleHint(settings.value("DefaultAtomStyle").toString());
        if(detectedAtomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Unknown)
            detectedAtomStyleHints.atomStyle = LAMMPSDataImporter::AtomStyle_Atomic;
        if(detectedAtomStyleHints.atomSubStyles.empty()) {
            for(const auto& substyleName : settings.value("DefaultAtomSubStyles").toStringList()) {
                auto substyle = LAMMPSDataImporter::parseAtomStyleHint(substyleName);
                if(substyle != LAMMPSDataImporter::AtomStyle_Unknown)
                    detectedAtomStyleHints.atomSubStyles.push_back(substyle);
            }
        }

        LAMMPSAtomStyleDialog dlg(mainWindow, detectedAtomStyleHints, &mainWindow);
        if(dlg.exec() != QDialog::Accepted)
            this_task::cancelAndThrow();

        settings.setValue("DefaultAtomStyle", LAMMPSDataImporter::atomStyleName(detectedAtomStyleHints.atomStyle));
        if(detectedAtomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Hybrid) {
            QStringList names;
            for(const auto& substyle : detectedAtomStyleHints.atomSubStyles)
                names.push_back(LAMMPSDataImporter::atomStyleName(substyle));
            settings.setValue("DefaultAtomSubStyles", names);
        }
    }
    dataImporter->setAtomStyle(detectedAtomStyleHints.atomStyle);
    dataImporter->setAtomSubStyles(std::move(detectedAtomStyleHints.atomSubStyles));
}

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void LAMMPSDataImporterEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("LAMMPS data reader"), rolloutParams, "manual:file_formats.input.lammps_data");

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(4);

    QGroupBox* optionsBox = new QGroupBox(tr("Options"), rollout);
    QVBoxLayout* sublayout = new QVBoxLayout(optionsBox);
    sublayout->setContentsMargins(4,4,4,4);
    layout->addWidget(optionsBox);

    // Sort particles
    BooleanParameterUI* sortParticlesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleImporter::sortParticles));
    sublayout->addWidget(sortParticlesUI->checkBox());
}

/******************************************************************************
* The constructor of the LAMMPSAtomStyleDialog.
******************************************************************************/
LAMMPSAtomStyleDialog::LAMMPSAtomStyleDialog(MainWindow& mainWindow, LAMMPSDataImporter::LAMMPSAtomStyleHints& atomStyleHints, QWidget* parent) : QDialog(parent), _atomStyleHints(atomStyleHints)
{
    setWindowTitle(tr("LAMMPS Data File Import"));

    QVBoxLayout* layout1 = new QVBoxLayout(this);
    layout1->setSpacing(2);
    layout1->addStrut(400);

    QLabel* label = new QLabel(
                (_atomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Unknown) ?
                tr("<html><p>Please select the right <b>atom style</b> for this LAMMPS data file. "
                "OVITO could not detect it automatically, because the file does not "
                "contain a <a href=\"https://docs.lammps.org/read_data.html#format-of-the-body-of-a-data-file\">style hint</a> in its <i>Atoms</i> section.</p>"
                "<p>If you don't know what the correct atom style is, see the <a href=\"https://docs.lammps.org/atom_style.html\">LAMMPS documentation</a> or "
                "check the value of the <i>atom_style</i> command in your LAMMPS input script.</p>"
                "<p>LAMMPS atom style:</p></html>") :
                tr("LAMMPS atom style:"), this);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
    label->setWordWrap(true);
    layout1->addWidget(label);

    _atomStyleList = new QComboBox(this);
    _atomStyleList->setEditable(false);
    for(int i = 1; i < LAMMPSDataImporter::AtomStyle_COUNT; i++) {
        LAMMPSDataImporter::LAMMPSAtomStyle atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(i);
        _atomStyleList->addItem(LAMMPSDataImporter::atomStyleName(atomStyle), QVariant::fromValue(i));
        if(_atomStyleHints.atomDataColumnCount != 0 && atomStyle != LAMMPSDataImporter::AtomStyle_Hybrid) {
            ParticleInputColumnMapping mapping = LAMMPSDataImporter::createAtomsColumnMapping(atomStyle, {}, _atomStyleHints.atomDataColumnCount);
            if(mapping.size() != _atomStyleHints.atomDataColumnCount)
                static_cast<QStandardItemModel*>(_atomStyleList->model())->item(i - 1)->setFlags(Qt::ItemIsSelectable | Qt::ItemNeverHasChildren);
        }
    }
    _atomStyleList->model()->sort(0);
    int styleIndex = _atomStyleList->findData(QVariant::fromValue(static_cast<int>(_atomStyleHints.atomStyle)));
    if(styleIndex >= 0)
        _atomStyleList->setCurrentIndex(styleIndex);
    layout1->addWidget(_atomStyleList);
    connect(_atomStyleList, qOverload<int>(&QComboBox::currentIndexChanged), this, &LAMMPSAtomStyleDialog::updateColumnList);

    _subStylesLabel = new QLabel(tr("Sub-styles:"), this);
    _subStylesLabel->setWordWrap(true);
    layout1->addWidget(_subStylesLabel);
    QHBoxLayout* sublayout = new QHBoxLayout();
    sublayout->setContentsMargins(0,0,0,0);
    sublayout->setSpacing(6);
    auto iter = _atomStyleHints.atomSubStyles.cbegin();
    for(QComboBox*& substyleList : _subStyleLists) {
        substyleList = new QComboBox(this);
        substyleList->setEditable(false);
        for(int i = 1; i < LAMMPSDataImporter::AtomStyle_COUNT; i++) {
            LAMMPSDataImporter::LAMMPSAtomStyle atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(i);
            if(atomStyle != LAMMPSDataImporter::AtomStyle_Hybrid)
                substyleList->addItem(LAMMPSDataImporter::atomStyleName(atomStyle), QVariant::fromValue(i));
        }
        substyleList->model()->sort(0);
        substyleList->insertItem(0, QString());
        substyleList->setCurrentIndex(0);
        if(iter != _atomStyleHints.atomSubStyles.cend()) {
            int styleIndex = substyleList->findData(QVariant::fromValue(static_cast<int>(*iter)));
            if(styleIndex >= 0)
                substyleList->setCurrentIndex(styleIndex);
            ++iter;
        }
        sublayout->addWidget(substyleList, 1);
        connect(substyleList, qOverload<int>(&QComboBox::currentIndexChanged), this, &LAMMPSAtomStyleDialog::updateColumnList);
    }
    layout1->addLayout(sublayout);

    label = new QLabel(tr("<html><p>The expected column order for the selected atom style is:</p></html>"), this);
    label->setWordWrap(true);
    layout1->addSpacing(16);
    layout1->addWidget(label);

    _columnListField = new QLineEdit(this);
    _columnListField->setReadOnly(true);
    _columnMismatchLabel = new QLabel();
    _columnMismatchLabel->setWordWrap(true);
    layout1->addWidget(_columnListField);
    layout1->addWidget(_columnMismatchLabel);

    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, Qt::Horizontal, this);
    connect(_buttonBox, &QDialogButtonBox::accepted, this, &LAMMPSAtomStyleDialog::onOk);
    connect(_buttonBox, &QDialogButtonBox::rejected, this, &LAMMPSAtomStyleDialog::reject);
    connect(_buttonBox, &QDialogButtonBox::helpRequested, this, [&mainWindow]() {
        mainWindow.actionManager()->openHelpTopic("manual:file_formats.input.lammps_data");
    });

    updateColumnList();

    layout1->addStretch(1);
    layout1->addSpacing(20);
    layout1->addWidget(_buttonBox);
}

/******************************************************************************
* Updates the displayed list of file data columns.
******************************************************************************/
void LAMMPSAtomStyleDialog::updateColumnList()
{
    LAMMPSDataImporter::LAMMPSAtomStyle atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(_atomStyleList->currentData().toInt());

    _subStylesLabel->setVisible(atomStyle == LAMMPSDataImporter::AtomStyle_Hybrid);
    std::vector<LAMMPSDataImporter::LAMMPSAtomStyle> hybridSubstyles;
    for(QComboBox* substyleList : _subStyleLists) {
        substyleList->setVisible(atomStyle == LAMMPSDataImporter::AtomStyle_Hybrid);
        LAMMPSDataImporter::LAMMPSAtomStyle substyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(substyleList->currentData().toInt());
        if(substyle != LAMMPSDataImporter::AtomStyle_Unknown)
            hybridSubstyles.push_back(substyle);
    }

    ParticleInputColumnMapping mapping = LAMMPSDataImporter::createAtomsColumnMapping(atomStyle, hybridSubstyles, _atomStyleHints.atomDataColumnCount);
    QString text;
    for(const InputColumnInfo& column : mapping) {
        if(!text.isEmpty())
            text += QStringLiteral(" ");
        text += column.columnName;
    }
    _columnListField->setText(std::move(text));

    if(mapping.size() != _atomStyleHints.atomDataColumnCount && _atomStyleHints.atomDataColumnCount != 0) {
        _columnMismatchLabel->setText(tr("<html><p style=\"color: red\">This does not match the actual number of columns in the data file, which is %1. Please select the correct atom style(s).</p></html>").arg(_atomStyleHints.atomDataColumnCount));
        _columnMismatchLabel->show();
        _buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else {
        _columnMismatchLabel->hide();
        _buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

/******************************************************************************
* Saves the values entered by the user and closes the dialog.
******************************************************************************/
void LAMMPSAtomStyleDialog::onOk()
{
    setFocus(); // Remove focus from child widgets to commit newly entered values in text widgets etc.

    _atomStyleHints.atomStyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(_atomStyleList->currentData().toInt());
    _atomStyleHints.atomSubStyles.clear();
    if(_atomStyleHints.atomStyle == LAMMPSDataImporter::AtomStyle_Hybrid) {
        for(QComboBox* substyleList : _subStyleLists) {
            LAMMPSDataImporter::LAMMPSAtomStyle substyle = static_cast<LAMMPSDataImporter::LAMMPSAtomStyle>(substyleList->currentData().toInt());
            if(substyle != LAMMPSDataImporter::AtomStyle_Unknown)
                _atomStyleHints.atomSubStyles.push_back(substyle);
        }
    }

    accept();
}

}   // End of namespace
